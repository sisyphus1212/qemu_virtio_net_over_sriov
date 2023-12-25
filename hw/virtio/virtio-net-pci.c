/*
 * Virtio net PCI Bindings
 *
 * Copyright IBM, Corp. 2007
 * Copyright (c) 2009 CodeSourcery
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *  Paul Brook        <paul@codesourcery.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Contributions after 2012-01-13 are licensed under the terms of the
 * GNU GPL, version 2 or (at your option) any later version.
 */

#include "qemu/osdep.h"

#include "hw/qdev-properties.h"
#include "hw/virtio/virtio-net.h"
#include "hw/virtio/virtio-pci.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "qom/object.h"
#include "hw/pci/msi.h"
#include "hw/pci/msix.h"

typedef struct VirtIONetPCI VirtIONetPCI;
typedef struct VirtIONetPCI VirtIONetVfPCI;
//static const VirtioPCIDeviceTypeInfo virtio_net_pci_vf_info;
/*
 * virtio-net-pci: This extends VirtioPCIProxy.
 */
#define TYPE_VIRTIO_NET_PCI "virtio-net-pci-base"
DECLARE_INSTANCE_CHECKER(VirtIONetPCI, VIRTIO_NET_PCI,
                         TYPE_VIRTIO_NET_PCI)

#define TYPE_VIRTIO_NET_PCI_VF "virtio-net-pci-vf-base"
#define TYPE_VIRTIO_NET_PCI_VF_GENERIC "virtio-net-pci-vf"
DECLARE_INSTANCE_CHECKER(VirtIONetVfPCI, VIRTIO_NET_PCI_VF,
                         TYPE_VIRTIO_NET_PCI_VF)

struct VirtIONetPCI {
    VirtIOPCIProxy parent_obj;
    VirtIONet vdev;
};

static Property virtio_net_properties[] = {
    DEFINE_PROP_BIT("ioeventfd", VirtIOPCIProxy, flags,
                    VIRTIO_PCI_FLAG_USE_IOEVENTFD_BIT, true),
    DEFINE_PROP_UINT32("vectors", VirtIOPCIProxy, nvectors,
                       DEV_NVECTORS_UNSPECIFIED),
    DEFINE_PROP_END_OF_LIST(),
};

#define VIRTIO_NET_VF_MMIO_SIZE      (16 * 1024)
#define VIRTIO_NET_VF_MSIX_SIZE      (16 * 1024)
#define VIRTIO_NET_CAP_SRIOV_OFFSET  (0x160)
#define VIRTIO_NET_CAP_ARI_OFFSET    (0x100)
#define VIRTIO_NET_VF_DEV_ID         (0x1041)
#define VIRTIO_NET_VF_OFFSET         (0x80)
#define VIRTIO_NET_VF_STRIDE         (2)

static void virtio_net_pci_realize(VirtIOPCIProxy *vpci_dev, Error **errp)
{
    DeviceState *qdev = DEVICE(vpci_dev);
    VirtIONetPCI *dev = VIRTIO_NET_PCI(vpci_dev);
    DeviceState *vdev = DEVICE(&dev->vdev);
    VirtIONet *net = VIRTIO_NET(vdev);

    if (vpci_dev->nvectors == DEV_NVECTORS_UNSPECIFIED) {
        vpci_dev->nvectors = 2 * MAX(net->nic_conf.peers.queues, 1)
            + 1 /* Config interrupt */
            + 1 /* Control vq */;
    }

    virtio_net_set_netclient_name(&dev->vdev, qdev->id,
                                  object_get_typename(OBJECT(qdev)));

    PCIDevice *pci_dev = &vpci_dev->pci_dev;
    pcie_ari_init(pci_dev, VIRTIO_NET_CAP_ARI_OFFSET);

    pcie_sriov_pf_init(pci_dev, VIRTIO_NET_CAP_SRIOV_OFFSET, TYPE_VIRTIO_NET_PCI_VF_GENERIC,
        VIRTIO_NET_VF_DEV_ID, 8, 8,
        VIRTIO_NET_VF_OFFSET, VIRTIO_NET_VF_STRIDE);

    pcie_sriov_pf_init_vf_bar(pci_dev, vpci_dev->modern_mem_bar_idx,
        PCI_BASE_ADDRESS_MEM_TYPE_64 | PCI_BASE_ADDRESS_MEM_PREFETCH,
        VIRTIO_NET_VF_MMIO_SIZE);

    pcie_sriov_pf_init_vf_bar(pci_dev, vpci_dev->msix_bar_idx,
        PCI_BASE_ADDRESS_MEM_TYPE_64 | PCI_BASE_ADDRESS_MEM_PREFETCH,
        VIRTIO_NET_VF_MSIX_SIZE);

    qdev_realize(vdev, BUS(&vpci_dev->bus), errp);
}

static void virtio_pci_device_write(void *opaque, hwaddr addr,
                                    uint64_t val, unsigned size)
{
    VirtIOPCIProxy *proxy = opaque;
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);

    if (vdev == NULL) {
        return;
    }

    switch (size) {
    case 1:
        virtio_config_modern_writeb(vdev, addr, val);
        break;
    case 2:
        virtio_config_modern_writew(vdev, addr, val);
        break;
    case 4:
        virtio_config_modern_writel(vdev, addr, val);
        break;
    }
}

static uint64_t virtio_net_pci_vf_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    //uint16_t bar_idx = 4;
    PCIDevice *vf = (PCIDevice *)(opaque);
    PCIDevice *pf = pcie_sriov_get_pf(vf);
    //uint16_t vf_num = pcie_sriov_vf_number(vf);
    uint32_t val;
    //pci 配置空间地址+sriov_cap offset + vf bar 所在地址
    //uint8_t *cfg = pf->config + pf->exp.sriov_cap  + PCI_SRIOV_BAR + vf_num * (bar_idx * 4);
    uint8_t * vf_common_cfg_addr = pf->config;
    //vf->config = vf_common_cfg_addr;
    //addr = vf_to_pf_addr(addr, pcie_sriov_vf_number(vf), false);
    //return addr == HWADDR_MAX ? 0 : igb_mmio_read(pf, addr, size);
    void * hw_addr = vf_common_cfg_addr + addr;
    switch (size) {
    case 1:
        val = ldub_p(hw_addr);
        break;
    case 2:
        val = lduw_le_p(hw_addr);
        break;
    case 4:
        val = ldl_le_p(hw_addr);
        break;
    default:
        val = 0;
        break;
    }
    return val;
}

static void virtio_net_pci_vf_mmio_write(void *opaque, hwaddr addr, uint64_t val,
    unsigned size)
{
    //PCIDevice *vf = PCI_DEVICE(opaque);
    //PCIDevice *pf = pcie_sriov_get_pf(vf);
    return virtio_pci_device_write(opaque, addr, val, size);
    //addr = vf_to_pf_addr(addr, pcie_sriov_vf_number(vf), true);
    //if (addr != HWADDR_MAX) {
    //    virtio_pci_device_write(pf, addr, val, size);
    //}
}

static const MemoryRegionOps mmio_ops = {
    .read =  virtio_net_pci_vf_mmio_read,
    .write =  virtio_net_pci_vf_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void virtio_net_vf_pci_cap_init(PCIDevice *dev, uint8_t cfg_type,
                                uint32_t cfg_offset, uint8_t cfg_bar,
                                int cfg_length) {
    struct virtio_pci_cap cap = {
        .cfg_type = cfg_type,
        .bar = cfg_bar,
        .offset = cfg_offset,
        .length = cfg_length,
        .cap_len = sizeof cap,
    };

    int offset = pci_add_capability(dev, PCI_CAP_ID_VNDR, 0,
                                cap.cap_len, &error_abort);

    memcpy(dev->config + offset + PCI_CAP_FLAGS, &cap.cap_len,
        cap.cap_len - PCI_CAP_FLAGS);
}

/*
static void virtio_net_vf_pci_notify_cap_init(PCIDevice *dev,
                                uint32_t cfg_offset, uint8_t cfg_bar,
                                uint8_t multiplier, int cfg_length) {

    struct virtio_pci_notify_cap notify = {
        .cap.cap_len = sizeof notify,
        .notify_off_multiplier = multiplier,
        .cap.offset = cfg_offset,
        .cap.length = cfg_length,
        .cap.cfg_type = VIRTIO_PCI_CAP_NOTIFY_CFG,
        .cap.bar = cfg_bar,
    };

    int offset = pci_add_capability(dev, PCI_CAP_ID_VNDR, 0,
                                notify.cap.cap_len, &error_abort);

    memcpy(dev->config + offset + PCI_CAP_FLAGS, &notify.cap.cap_len,
        notify.cap.cap_len - PCI_CAP_FLAGS);
}
*/
static void virtio_net_pci_vf_write_config(PCIDevice *dev, uint32_t addr, uint32_t val,
    int len)
{
    pci_default_write_config(dev, addr, val, len);
    if (object_property_get_bool(OBJECT(pcie_sriov_get_pf(dev)),
                                 "x-pcie-flr-init", &error_abort)) {
        pcie_cap_flr_write_config(dev, addr, val, len);
    }
}
static void virtio_net_pci_vf_realize(VirtIOPCIProxy *vdev, Error **errp)
{
    VirtIONetVfPCI *s = VIRTIO_NET_PCI_VF(vdev);
    //int ret;
    //int i;
    PCIDevice *dev = (PCIDevice *)vdev;
    dev->config_write = virtio_net_pci_vf_write_config;
    VirtIOPCIProxy *pf_proxy = VIRTIO_PCI(pcie_sriov_get_pf(dev));
    VirtIOPCIProxy *vf_proxy = VIRTIO_PCI(dev);
    int mmio_bar_id = 4; //pf_proxy->modern_mem_bar_idx;
    //int msix_bar_id = 1; //pf_proxy->msix_bar_idx;
    int nvectors = pf_proxy->nvectors;
    memory_region_init_io(&vf_proxy->modern_bar, OBJECT(dev), &mmio_ops, s, "virtio_net_pci_vf-mmio",
        VIRTIO_NET_VF_MMIO_SIZE);
    pcie_sriov_vf_register_bar(dev, mmio_bar_id, &vf_proxy->modern_bar);

    char* name = g_strdup_printf("%s-msix", vf_proxy->pci_dev.name);
    memory_region_init(&vf_proxy->pci_dev.msix_exclusive_bar, OBJECT(vf_proxy), name, VIRTIO_NET_VF_MSIX_SIZE);

    virtio_net_vf_pci_cap_init(dev, VIRTIO_PCI_CAP_COMMON_CFG, 0x0,    0x4, 0x1000);
    virtio_net_vf_pci_cap_init(dev, VIRTIO_PCI_CAP_ISR_CFG,    0x1000, 0x4, 0x1000);
    virtio_net_vf_pci_cap_init(dev, VIRTIO_PCI_CAP_DEVICE_CFG, 0x2000, 0x4, 0x1000);
    virtio_net_vf_pci_cap_init(dev, VIRTIO_PCI_CAP_NOTIFY_CFG, 0x3000, 0x4, 0x1000);

    //virtio_net_vf_pci_notify_cap_init(dev, 0x3000, 0x4, 4, 0x1000);

    //ret = msix_init(dev, nvectors, &vf_proxy->pci_dev.msix_exclusive_bar, msix_bar_id,
    //                0, &vf_proxy->pci_dev.msix_exclusive_bar,
    //                mmio_bar_id, 0x2000,
    //                0x00, errp);
    //if (ret) {
    //    return;
    //}

    //for (i = 0; i < nvectors; i++) {
    //    msix_vector_use(dev, i);
    //}

    if (pcie_endpoint_cap_init(dev, 0) < 0) {
        herror("Failed to initialize PCIe capability");
    }

    if (object_property_get_bool(OBJECT(pcie_sriov_get_pf(dev)),
                                 "x-pcie-flr-init", &error_abort)) {
        pcie_cap_flr_init(dev);
    }

    if (pcie_aer_init(dev, 1, 0x100, 0x40, errp) < 0) {
        herror("Failed to initialize AER capability");
    }

    pcie_ari_init(dev, 0x160);
}

static void virtio_net_pci_vf_pci_uninit(PCIDevice *dev)
{
    //VirtIONetVfPCI *s = VIRTIO_NET_PCI_VF(dev);

    pcie_aer_exit(dev);
    pcie_cap_exit(dev);
    msix_unuse_all_vectors(dev);
    //msix_uninit(dev, &s->msix, &s->msix);
}

static void virtio_net_pci_vf_qdev_reset_hold(Object *obj)
{
    //PCIDevice *vf = PCI_DEVICE(obj);
    //igb_vf_reset(pcie_sriov_get_pf(vf), pcie_sriov_vf_number(vf));
}


/*
static uint64_t virtio_pci_device_read(void *opaque, hwaddr addr,
                                       unsigned size)
{
    VirtIOPCIProxy *proxy = opaque;
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
    uint64_t val;

    if (vdev == NULL) {
        return UINT64_MAX;
    }

    switch (size) {
    case 1:
        val = virtio_config_modern_readb(vdev, addr);
        break;
    case 2:
        val = virtio_config_modern_readw(vdev, addr);
        break;
    case 4:
        val = virtio_config_modern_readl(vdev, addr);
        break;
    default:
        val = 0;
        break;
    }
    return val;
}
*/

static void virtio_net_pci_vf_instance_init(Object *obj)
{
    VirtIONetVfPCI *dev = VIRTIO_NET_PCI_VF(obj);

    virtio_instance_init_common(obj, &dev->vdev, sizeof(dev->vdev),
                                TYPE_VIRTIO_NET);
    object_property_add_alias(obj, "bootindex", OBJECT(&dev->vdev),
                              "bootindex");
}

static void virtio_net_pci_instance_init(Object *obj)
{
    VirtIONetPCI *dev = VIRTIO_NET_PCI(obj);

    virtio_instance_init_common(obj, &dev->vdev, sizeof(dev->vdev),
                                TYPE_VIRTIO_NET);
    object_property_add_alias(obj, "bootindex", OBJECT(&dev->vdev),
                              "bootindex");
}

static void virtio_net_pci_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    VirtioPCIClass *vpciklass = VIRTIO_PCI_CLASS(klass);

    k->romfile = "efi-virtio.rom";
    k->vendor_id = PCI_VENDOR_ID_REDHAT_QUMRANET;
    k->device_id = PCI_DEVICE_ID_VIRTIO_NET;
    k->revision = VIRTIO_PCI_ABI_VERSION;
    k->class_id = PCI_CLASS_NETWORK_ETHERNET;
    set_bit(DEVICE_CATEGORY_NETWORK, dc->categories);
    device_class_set_props(dc, virtio_net_properties);
    vpciklass->realize = virtio_net_pci_realize;
}

static void virtio_net_pci_vf_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    VirtioPCIClass *vpciklass = VIRTIO_PCI_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    k->vendor_id = PCI_VENDOR_ID_REDHAT_QUMRANET;
    k->device_id = PCI_DEVICE_ID_VIRTIO_NET;
    k->revision = VIRTIO_PCI_ABI_VERSION;
    k->class_id = PCI_CLASS_NETWORK_ETHERNET;
    k->exit = virtio_net_pci_vf_pci_uninit;
    rc->phases.hold = virtio_net_pci_vf_qdev_reset_hold;
    dc->desc = "virtio net pci virtual function";
    dc->user_creatable = false;
    //device_class_set_props(dc, virtio_net_pci_vf_info);
    vpciklass->realize = virtio_net_pci_vf_realize;
    set_bit(DEVICE_CATEGORY_NETWORK, dc->categories);
}

static const VirtioPCIDeviceTypeInfo virtio_net_pci_info = {
    .base_name             = TYPE_VIRTIO_NET_PCI,
    .generic_name          = "virtio-net-pci",
    .transitional_name     = "virtio-net-pci-transitional",
    .non_transitional_name = "virtio-net-pci-non-transitional",
    .instance_size = sizeof(VirtIONetPCI),
    .instance_init = virtio_net_pci_instance_init,
    .class_init    = virtio_net_pci_class_init,
};

static const VirtioPCIDeviceTypeInfo virtio_net_pci_vf_info = {
    .base_name             = TYPE_VIRTIO_NET_PCI_VF,
    .generic_name          = "virtio-net-pci-vf",
    .transitional_name     = "virtio-net-pci-vf-transitional",
    .non_transitional_name = "virtio-net-pci-vf-non-transitional",
    .instance_size = sizeof(VirtIONetVfPCI),
    .instance_init = virtio_net_pci_vf_instance_init,
    .class_init    = virtio_net_pci_vf_class_init,
};

static void virtio_net_pci_register(void)
{
    virtio_pci_types_register(&virtio_net_pci_info);
    virtio_pci_types_register(&virtio_net_pci_vf_info);
}

type_init(virtio_net_pci_register)

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_SERVICE_IMPL_H_
#define SERVICES_DEVICE_USB_USB_SERVICE_IMPL_H_

#include <stddef.h>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "services/device/usb/scoped_libusb_device_ref.h"
#include "services/device/usb/usb_context.h"
#include "services/device/usb/usb_device_impl.h"
#include "services/device/usb/usb_service.h"
#include "third_party/libusb/src/libusb/libusb.h"

struct libusb_context;

namespace device {

typedef struct libusb_context* PlatformUsbContext;

class UsbDeviceImpl;

class UsbServiceImpl final : public UsbService {
 public:
  UsbServiceImpl();

  UsbServiceImpl(const UsbServiceImpl&) = delete;
  UsbServiceImpl& operator=(const UsbServiceImpl&) = delete;

  ~UsbServiceImpl() override;

 private:
  // device::UsbService implementation
  void GetDevices(GetDevicesCallback callback) override;

  void OnUsbContext(scoped_refptr<UsbContext> context);

  // Enumerate USB devices from OS and update devices_ map.
  void RefreshDevices();
  void OnDeviceList(
      std::optional<std::vector<ScopedLibusbDeviceRef>> platform_devices);
  void RefreshDevicesComplete();

  // Creates a new UsbDevice based on the given libusb device.
  void EnumerateDevice(ScopedLibusbDeviceRef platform_device,
                       base::OnceClosure refresh_complete);

  void AddDevice(scoped_refptr<UsbDeviceImpl> device);
  void RemoveDevice(scoped_refptr<UsbDeviceImpl> device);

  // Handle hotplug events from libusb.
  static int LIBUSB_CALL HotplugCallback(libusb_context* context,
                                         libusb_device* device,
                                         libusb_hotplug_event event,
                                         void* user_data);
  // These functions release a reference to the provided platform device.
  void OnPlatformDeviceAdded(ScopedLibusbDeviceRef platform_device);
  void OnPlatformDeviceRemoved(ScopedLibusbDeviceRef platform_device);

  // Add |platform_device| to the |ignored_devices_| and
  // run |refresh_complete|.
  void EnumerationFailed(ScopedLibusbDeviceRef platform_device);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The libusb_context must outlive any references to libusb_device objects.
  scoped_refptr<UsbContext> context_;
  bool usb_unavailable_ = false;

  // When available the device list will be updated when new devices are
  // connected instead of only when a full enumeration is requested.
  libusb_hotplug_callback_handle hotplug_handle_;

  // Enumeration callbacks are queued until an enumeration completes.
  bool enumeration_ready_ = false;
  bool enumeration_in_progress_ = false;
  std::vector<GetDevicesCallback> pending_enumeration_callbacks_;

  // The map from libusb_device to UsbDeviceImpl. The key is a weak pointer to
  // the libusb_device object owned by the UsbDeviceImpl.
  typedef std::map<libusb_device*, scoped_refptr<UsbDeviceImpl>>
      PlatformDeviceMap;
  PlatformDeviceMap platform_devices_;

  // The set of devices that only need to be enumerated once and then can be
  // ignored (for example, hub devices, devices that failed enumeration, etc.).
  std::vector<ScopedLibusbDeviceRef> ignored_devices_;

  // Tracks libusb_devices that might be removed while they are being
  // enumerated. This is a weak pointer to a libusb_device object owned by a
  // UsbDeviceImpl.
  std::set<libusb_device*> devices_being_enumerated_;

  // This WeakPtr is used to safely post hotplug events back to the thread this
  // object lives on.
  base::WeakPtr<UsbServiceImpl> weak_self_;

  base::WeakPtrFactory<UsbServiceImpl> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_SERVICE_IMPL_H_

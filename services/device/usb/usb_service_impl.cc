// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_service_impl.h"

#include <stdint.h>

#include <list>
#include <memory>
#include <set>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/usb/usb_device_handle.h"
#include "services/device/usb/usb_error.h"
#include "services/device/usb/webusb_descriptors.h"
#include "third_party/libusb/src/libusb/libusb.h"

#if defined(OS_WIN)
#define INITGUID
#include <devpkey.h>
#include <setupapi.h>
#include <usbiodef.h>

#include "base/strings/string_util.h"
#include "device/base/device_info_query_win.h"
#endif  // OS_WIN

namespace device {

namespace {

// Standard USB requests and descriptor types:
const uint16_t kUsbVersion2_1 = 0x0210;

#if defined(OS_WIN)

bool IsWinUsbInterface(const std::string& device_path) {
  DeviceInfoQueryWin device_info_query;
  if (!device_info_query.device_info_list_valid()) {
    USB_PLOG(ERROR) << "Failed to create a device information set";
    return false;
  }

  // This will add the device so we can query driver info.
  if (!device_info_query.AddDevice(device_path)) {
    USB_PLOG(ERROR) << "Failed to get device interface data for "
                    << device_path;
    return false;
  }

  if (!device_info_query.GetDeviceInfo()) {
    USB_PLOG(ERROR) << "Failed to get device info for " << device_path;
    return false;
  }

  std::string buffer;
  if (!device_info_query.GetDeviceStringProperty(DEVPKEY_Device_Service,
                                                 &buffer)) {
    USB_PLOG(ERROR) << "Failed to get device service property";
    return false;
  }

  USB_LOG(DEBUG) << "Driver for " << device_path << " is " << buffer << ".";
  if (base::StartsWith(buffer, "WinUSB", base::CompareCase::INSENSITIVE_ASCII))
    return true;
  return false;
}

#endif  // OS_WIN

scoped_refptr<UsbContext> InitializeUsbContextBlocking() {
  PlatformUsbContext platform_context = nullptr;
  int rv = libusb_init(&platform_context);
  if (rv == LIBUSB_SUCCESS && platform_context) {
    return base::MakeRefCounted<UsbContext>(platform_context);
  }

  USB_LOG(DEBUG) << "Failed to initialize libusb: "
                 << ConvertPlatformUsbErrorToString(rv);
  return nullptr;
}

base::Optional<std::vector<ScopedLibusbDeviceRef>> GetDeviceListBlocking(
    const std::string& new_device_path,
    scoped_refptr<UsbContext> usb_context) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

#if defined(OS_WIN)
  if (!new_device_path.empty()) {
    if (!IsWinUsbInterface(new_device_path)) {
      // Wait to call libusb_get_device_list until libusb will be able to find
      // a WinUSB interface for the device.
      return base::nullopt;
    }
  }
#endif  // defined(OS_WIN)

  libusb_device** platform_devices = NULL;
  const ssize_t device_count =
      libusb_get_device_list(usb_context->context(), &platform_devices);
  if (device_count < 0) {
    USB_LOG(ERROR) << "Failed to get device list: "
                   << ConvertPlatformUsbErrorToString(device_count);
    return base::nullopt;
  }

  std::vector<ScopedLibusbDeviceRef> scoped_devices;
  scoped_devices.reserve(device_count);
  for (ssize_t i = 0; i < device_count; ++i)
    scoped_devices.emplace_back(platform_devices[i], usb_context);

  // Free the list but don't unref the devices because ownership has been
  // been transfered to the elements of |scoped_devices|.
  libusb_free_device_list(platform_devices, false);

  return scoped_devices;
}

void CloseHandleAndRunContinuation(scoped_refptr<UsbDeviceHandle> device_handle,
                                   base::OnceClosure continuation) {
  device_handle->Close();
  std::move(continuation).Run();
}

void SaveStringsAndRunContinuation(
    scoped_refptr<UsbDeviceImpl> device,
    uint8_t manufacturer,
    uint8_t product,
    uint8_t serial_number,
    const base::Closure& continuation,
    std::unique_ptr<std::map<uint8_t, base::string16>> string_map) {
  if (manufacturer != 0)
    device->set_manufacturer_string((*string_map)[manufacturer]);
  if (product != 0)
    device->set_product_string((*string_map)[product]);
  if (serial_number != 0)
    device->set_serial_number((*string_map)[serial_number]);
  continuation.Run();
}

void OnReadBosDescriptor(scoped_refptr<UsbDeviceHandle> device_handle,
                         const base::Closure& barrier,
                         const GURL& landing_page) {
  scoped_refptr<UsbDeviceImpl> device =
      static_cast<UsbDeviceImpl*>(device_handle->GetDevice().get());

  if (landing_page.is_valid())
    device->set_webusb_landing_page(landing_page);

  barrier.Run();
}

void OnDeviceOpenedReadDescriptors(
    uint8_t manufacturer,
    uint8_t product,
    uint8_t serial_number,
    bool read_bos_descriptors,
    base::OnceClosure success_closure,
    base::OnceClosure failure_closure,
    scoped_refptr<UsbDeviceHandle> device_handle) {
  if (device_handle) {
    std::unique_ptr<std::map<uint8_t, base::string16>> string_map(
        new std::map<uint8_t, base::string16>());
    if (manufacturer != 0)
      (*string_map)[manufacturer] = base::string16();
    if (product != 0)
      (*string_map)[product] = base::string16();
    if (serial_number != 0)
      (*string_map)[serial_number] = base::string16();

    int count = 0;
    if (!string_map->empty())
      count++;
    if (read_bos_descriptors)
      count++;
    DCHECK_GT(count, 0);

    base::RepeatingClosure barrier = base::BarrierClosure(
        count, base::BindOnce(&CloseHandleAndRunContinuation, device_handle,
                              std::move(success_closure)));

    if (!string_map->empty()) {
      scoped_refptr<UsbDeviceImpl> device =
          static_cast<UsbDeviceImpl*>(device_handle->GetDevice().get());

      ReadUsbStringDescriptors(
          device_handle, std::move(string_map),
          base::BindOnce(&SaveStringsAndRunContinuation, device, manufacturer,
                         product, serial_number, barrier));
    }

    if (read_bos_descriptors) {
      ReadWebUsbDescriptors(
          device_handle,
          base::BindOnce(&OnReadBosDescriptor, device_handle, barrier));
    }
  } else {
    std::move(failure_closure).Run();
  }
}

}  // namespace

UsbServiceImpl::UsbServiceImpl()
    : task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  weak_self_ = weak_factory_.GetWeakPtr();
  base::PostTaskAndReplyWithResult(
      FROM_HERE, kBlockingTaskTraits,
      base::BindOnce(&InitializeUsbContextBlocking),
      base::BindOnce(&UsbServiceImpl::OnUsbContext,
                     weak_factory_.GetWeakPtr()));
}

UsbServiceImpl::~UsbServiceImpl() {
  NotifyWillDestroyUsbService();
  if (hotplug_enabled_)
    libusb_hotplug_deregister_callback(context_->context(), hotplug_handle_);
}

void UsbServiceImpl::GetDevices(const GetDevicesCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (usb_unavailable_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(callback, std::vector<scoped_refptr<UsbDevice>>()));
    return;
  }

  if (hotplug_enabled_ && !enumeration_in_progress_) {
    // The device list is updated live when hotplug events are supported.
    UsbService::GetDevices(callback);
  } else {
    pending_enumeration_callbacks_.push_back(callback);
    RefreshDevices();
  }
}

#if defined(OS_WIN)

void UsbServiceImpl::OnDeviceAdded(const GUID& class_guid,
                                   const std::string& device_path) {
  // Only the root node of a composite USB device has the class GUID
  // GUID_DEVINTERFACE_USB_DEVICE but we want to wait until WinUSB is loaded.
  // This first pass filter will catch anything that's sitting on the USB bus
  // (including devices on 3rd party USB controllers) to avoid the more
  // expensive driver check that needs to be done on the FILE thread.
  if (device_path.find("usb") != std::string::npos) {
    pending_path_enumerations_.push(device_path);
    RefreshDevices();
  }
}

void UsbServiceImpl::OnDeviceRemoved(const GUID& class_guid,
                                     const std::string& device_path) {
  // The root USB device node is removed last.
  if (class_guid == GUID_DEVINTERFACE_USB_DEVICE) {
    RefreshDevices();
  }
}

#endif  // OS_WIN

void UsbServiceImpl::OnUsbContext(scoped_refptr<UsbContext> context) {
  if (!context) {
    usb_unavailable_ = true;
    return;
  }

  context_ = std::move(context);

  int rv = libusb_hotplug_register_callback(
      context_->context(),
      static_cast<libusb_hotplug_event>(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                                        LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
      static_cast<libusb_hotplug_flag>(0), LIBUSB_HOTPLUG_MATCH_ANY,
      LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
      &UsbServiceImpl::HotplugCallback, this, &hotplug_handle_);
  if (rv == LIBUSB_SUCCESS)
    hotplug_enabled_ = true;

  // This will call any enumeration callbacks queued while initializing.
  RefreshDevices();

#if defined(OS_WIN)
  DeviceMonitorWin* device_monitor = DeviceMonitorWin::GetForAllInterfaces();
  if (device_monitor)
    device_observer_.Add(device_monitor);
#endif  // OS_WIN
}

void UsbServiceImpl::RefreshDevices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!context_ || enumeration_in_progress_)
    return;

  enumeration_in_progress_ = true;
  DCHECK(devices_being_enumerated_.empty());

  std::string device_path;
  if (!pending_path_enumerations_.empty()) {
    device_path = pending_path_enumerations_.front();
    pending_path_enumerations_.pop();
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE, kBlockingTaskTraits,
      base::BindOnce(&GetDeviceListBlocking, device_path, context_),
      base::BindOnce(&UsbServiceImpl::OnDeviceList,
                     weak_factory_.GetWeakPtr()));
}

void UsbServiceImpl::OnDeviceList(
    base::Optional<std::vector<ScopedLibusbDeviceRef>> devices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!devices) {
    RefreshDevicesComplete();
    return;
  }

  std::vector<ScopedLibusbDeviceRef> new_devices;

  // Look for new and existing devices.
  for (auto& device : *devices) {
    // Ignore devices that have failed enumeration previously.
    if (base::Contains(ignored_devices_, device.get()))
      continue;

    auto it = platform_devices_.find(device.get());
    if (it == platform_devices_.end()) {
      new_devices.push_back(std::move(device));
    } else {
      // Mark the existing device object visited and remove it from the list so
      // it will not be ignored.
      it->second->set_visited(true);
      device.Reset();
    }
  }

  // Remove devices not seen in this enumeration.
  for (PlatformDeviceMap::iterator it = platform_devices_.begin();
       it != platform_devices_.end();
       /* incremented internally */) {
    PlatformDeviceMap::iterator current = it++;
    const scoped_refptr<UsbDeviceImpl>& device = current->second;
    if (device->was_visited()) {
      device->set_visited(false);
    } else {
      RemoveDevice(device);
    }
  }

  // Remaining devices are being ignored. Clear the old list so that devices
  // that have been removed don't remain in |ignored_devices_| indefinitely.
  ignored_devices_.clear();
  for (auto& device : *devices) {
    if (device.IsValid())
      ignored_devices_.push_back(std::move(device));
  }

  // Enumerate new devices.
  base::RepeatingClosure refresh_complete = base::BarrierClosure(
      new_devices.size(),
      base::BindOnce(&UsbServiceImpl::RefreshDevicesComplete,
                     weak_factory_.GetWeakPtr()));
  for (auto& device : new_devices)
    EnumerateDevice(std::move(device), refresh_complete);
}

void UsbServiceImpl::RefreshDevicesComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(enumeration_in_progress_);

  enumeration_ready_ = true;
  enumeration_in_progress_ = false;
  devices_being_enumerated_.clear();

  if (!pending_enumeration_callbacks_.empty()) {
    std::vector<scoped_refptr<UsbDevice>> result;
    result.reserve(devices().size());
    for (const auto& map_entry : devices())
      result.push_back(map_entry.second);

    std::vector<GetDevicesCallback> callbacks;
    callbacks.swap(pending_enumeration_callbacks_);
    for (const GetDevicesCallback& callback : callbacks)
      callback.Run(result);
  }

  if (!pending_path_enumerations_.empty()) {
    RefreshDevices();
  }
}

void UsbServiceImpl::EnumerateDevice(ScopedLibusbDeviceRef platform_device,
                                     const base::Closure& refresh_complete) {
  DCHECK(context_);

  libusb_device_descriptor descriptor;
  int rv = libusb_get_device_descriptor(platform_device.get(), &descriptor);
  if (rv != LIBUSB_SUCCESS) {
    USB_LOG(EVENT) << "Failed to get device descriptor: "
                   << ConvertPlatformUsbErrorToString(rv);
    EnumerationFailed(std::move(platform_device), refresh_complete);
    return;
  }

  if (descriptor.bDeviceClass == LIBUSB_CLASS_HUB) {
    // Don't try to enumerate hubs. We never want to connect to a hub.
    EnumerationFailed(std::move(platform_device), refresh_complete);
    return;
  }

  devices_being_enumerated_.insert(platform_device.get());

  auto device = base::MakeRefCounted<UsbDeviceImpl>(std::move(platform_device),
                                                    descriptor);
  base::OnceClosure add_device =
      base::BindOnce(&UsbServiceImpl::AddDevice, weak_factory_.GetWeakPtr(),
                     refresh_complete, device);

  bool read_bos_descriptors = descriptor.bcdUSB >= kUsbVersion2_1;
  if (descriptor.iManufacturer == 0 && descriptor.iProduct == 0 &&
      descriptor.iSerialNumber == 0 && !read_bos_descriptors) {
    // Don't bother disturbing the device if it has no descriptors to offer.
    std::move(add_device).Run();
  } else {
    // Take an additional reference to the libusb_device object that will be
    // owned by this callback.
    libusb_ref_device(device->platform_device());
    base::OnceClosure enumeration_failed = base::BindOnce(
        &UsbServiceImpl::EnumerationFailed, weak_factory_.GetWeakPtr(),
        ScopedLibusbDeviceRef(device->platform_device(), context_),
        refresh_complete);

    device->Open(base::BindOnce(
        &OnDeviceOpenedReadDescriptors, descriptor.iManufacturer,
        descriptor.iProduct, descriptor.iSerialNumber, read_bos_descriptors,
        std::move(add_device), std::move(enumeration_failed)));
  }
}

void UsbServiceImpl::AddDevice(const base::Closure& refresh_complete,
                               scoped_refptr<UsbDeviceImpl> device) {
  if (!base::Contains(devices_being_enumerated_, device->platform_device())) {
    // Device was removed while being enumerated.
    refresh_complete.Run();
    return;
  }

  DCHECK(!base::Contains(platform_devices_, device->platform_device()));
  platform_devices_[device->platform_device()] = device;
  DCHECK(!base::Contains(devices(), device->guid()));
  devices()[device->guid()] = device;

  USB_LOG(USER) << "USB device added: vendor=" << device->vendor_id() << " \""
                << device->manufacturer_string()
                << "\", product=" << device->product_id() << " \""
                << device->product_string() << "\", serial=\""
                << device->serial_number() << "\", guid=" << device->guid();

  if (enumeration_ready_)
    NotifyDeviceAdded(device);

  refresh_complete.Run();
}

void UsbServiceImpl::RemoveDevice(scoped_refptr<UsbDeviceImpl> device) {
  platform_devices_.erase(device->platform_device());
  devices().erase(device->guid());

  USB_LOG(USER) << "USB device removed: guid=" << device->guid();

  NotifyDeviceRemoved(device);
  device->OnDisconnect();
}

// static
int LIBUSB_CALL UsbServiceImpl::HotplugCallback(libusb_context* context,
                                                libusb_device* device_raw,
                                                libusb_hotplug_event event,
                                                void* user_data) {
  // It is safe to access the UsbServiceImpl* here because libusb takes a lock
  // around registering, deregistering and calling hotplug callback functions
  // and so guarantees that this function will not be called by the event
  // processing thread after it has been deregistered.
  UsbServiceImpl* self = reinterpret_cast<UsbServiceImpl*>(user_data);

  // libusb does not transfer ownership of |device_raw| to this function so a
  // reference must be taken here.
  libusb_ref_device(device_raw);
  ScopedLibusbDeviceRef device(device_raw, self->context_);

  switch (event) {
    case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED:
      self->task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&UsbServiceImpl::OnPlatformDeviceAdded,
                                    self->weak_self_, std::move(device)));
      break;
    case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT:
      self->task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&UsbServiceImpl::OnPlatformDeviceRemoved,
                                    self->weak_self_, std::move(device)));
      break;
    default:
      NOTREACHED();
  }

  return 0;
}

void UsbServiceImpl::OnPlatformDeviceAdded(
    ScopedLibusbDeviceRef platform_device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(platform_devices_, platform_device.get()));
  EnumerateDevice(std::move(platform_device), base::DoNothing());
}

void UsbServiceImpl::OnPlatformDeviceRemoved(
    ScopedLibusbDeviceRef platform_device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = platform_devices_.find(platform_device.get());
  if (it == platform_devices_.end())
    devices_being_enumerated_.erase(platform_device.get());
  else
    RemoveDevice(it->second);
}

void UsbServiceImpl::EnumerationFailed(ScopedLibusbDeviceRef platform_device,
                                       const base::Closure& refresh_complete) {
  ignored_devices_.push_back(std::move(platform_device));
  refresh_complete.Run();
}

}  // namespace device

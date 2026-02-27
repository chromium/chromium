// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/usb_device_permissions_prompt.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/message_formatter.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/permission_broker/permission_broker_client.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS)

using device::mojom::UsbDeviceFilterPtr;

namespace extensions {

UsbDevicePermissionsPrompt::Prompt::DeviceInfo::DeviceInfo(
    device::mojom::UsbDeviceInfoPtr device)
    : device_(std::move(device)),
      name_(DevicePermissionsManager::GetPermissionMessage(
          device_->vendor_id,
          device_->product_id,
          device_->manufacturer_name.value_or(std::u16string()),
          device_->product_name.value_or(std::u16string()),
          u"",  // Serial number is displayed separately.
          true)) {
  if (device_->serial_number) {
    serial_number_ = *device_->serial_number;
  }
}

UsbDevicePermissionsPrompt::Prompt::DeviceInfo::~DeviceInfo() = default;

UsbDevicePermissionsPrompt::Prompt::Observer::~Observer() = default;

UsbDevicePermissionsPrompt::Prompt::Prompt(
    const Extension* extension,
    content::BrowserContext* context,
    bool multiple,
    std::vector<UsbDeviceFilterPtr> filters,
    UsbDevicePermissionsPrompt::UsbDevicesCallback callback)
    : extension_(extension),
      browser_context_(context),
      multiple_(multiple),
      filters_(std::move(filters)),
      callback_(std::move(callback)) {}

UsbDevicePermissionsPrompt::Prompt::~Prompt() = default;

void UsbDevicePermissionsPrompt::Prompt::SetObserver(Observer* observer) {
  observer_ = observer;

  if (observer) {
    auto* device_manager = UsbDeviceManager::Get(browser_context_);
    if (device_manager &&
        !manager_observation_.IsObservingSource(device_manager)) {
      device_manager->GetDevices(base::BindOnce(
          &UsbDevicePermissionsPrompt::Prompt::OnDevicesEnumerated, this));
      manager_observation_.Observe(device_manager);
    }
  }
}

std::u16string UsbDevicePermissionsPrompt::Prompt::GetDeviceName(
    size_t index) const {
  if (index >= devices_.size()) {
    return u"";
  }
  return devices_[index]->name();
}

std::u16string UsbDevicePermissionsPrompt::Prompt::GetDeviceSerialNumber(
    size_t index) const {
  if (index >= devices_.size()) {
    return u"";
  }
  return devices_[index]->serial_number();
}

void UsbDevicePermissionsPrompt::Prompt::GrantDevicePermission(size_t index) {
  if (index < devices_.size()) {
    devices_[index]->set_granted();
  }
}

void UsbDevicePermissionsPrompt::Prompt::Dismissed() {
  DevicePermissionsManager* permissions_manager =
      DevicePermissionsManager::Get(browser_context_);
  std::vector<device::mojom::UsbDeviceInfoPtr> devices;
  for (const auto& device : devices_) {
    if (device->granted()) {
      if (permissions_manager) {
        DCHECK(device->device());
        permissions_manager->AllowUsbDevice(extension_->id(),
                                            *device->device());
      }
      devices.push_back(std::move(device->device()));
    }
  }
  DCHECK(multiple() || devices.size() <= 1);
  std::move(callback_).Run(std::move(devices));
}

void UsbDevicePermissionsPrompt::Prompt::OnDeviceAdded(
    const device::mojom::UsbDeviceInfo& device) {
  MaybeAddDevice(device, /*initial_enumeration=*/false);
}

void UsbDevicePermissionsPrompt::Prompt::OnDeviceRemoved(
    const device::mojom::UsbDeviceInfo& device) {
  for (auto it = devices_.begin(); it != devices_.end(); ++it) {
    DeviceInfo* entry = static_cast<DeviceInfo*>(it->get());
    if (entry->device()->guid == device.guid) {
      if (observer_) {
        size_t index = it - devices_.begin();
        const std::u16string& device_name = entry->name();
        observer_->OnDeviceRemoved(index, device_name);
      }
      devices_.erase(it);
      return;
    }
  }
}

void UsbDevicePermissionsPrompt::Prompt::OnDevicesEnumerated(
    std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
  for (const auto& device : devices) {
    MaybeAddDevice(*device, /*initial_enumeration=*/true);
  }
}

void UsbDevicePermissionsPrompt::Prompt::MaybeAddDevice(
    const device::mojom::UsbDeviceInfo& device,
    bool initial_enumeration) {
  if (!device::UsbDeviceFilterMatchesAny(filters_, device)) {
    return;
  }

  if (initial_enumeration) {
    remaining_initial_devices_++;
  }

  auto device_info = std::make_unique<DeviceInfo>(device.Clone());
#if BUILDFLAG(IS_CHROMEOS)
  auto* device_manager = UsbDeviceManager::Get(browser_context_);
  DCHECK(device_manager);
  device_manager->CheckAccess(
      device.guid,
      base::BindOnce(&UsbDevicePermissionsPrompt::Prompt::AddCheckedDevice,
                     this, std::move(device_info), initial_enumeration));
#else
  AddCheckedDevice(std::move(device_info), initial_enumeration,
                   /*allowed=*/true);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void UsbDevicePermissionsPrompt::Prompt::AddCheckedDevice(
    std::unique_ptr<DeviceInfo> device_info,
    bool initial_enumeration,
    bool allowed) {
  if (allowed) {
    AddDevice(std::move(device_info));
  }

  if (initial_enumeration && --remaining_initial_devices_ == 0 && observer_) {
    observer_->OnDevicesInitialized();
  }
}

void UsbDevicePermissionsPrompt::Prompt::AddDevice(
    std::unique_ptr<DeviceInfo> device) {
  std::u16string device_name = device->name();
  devices_.push_back(std::move(device));
  if (observer_) {
    observer_->OnDeviceAdded(devices_.size() - 1, device_name);
  }
}

UsbDevicePermissionsPrompt::UsbDevicePermissionsPrompt(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

UsbDevicePermissionsPrompt::~UsbDevicePermissionsPrompt() = default;

void UsbDevicePermissionsPrompt::AskForDevices(
    const Extension* extension,
    content::BrowserContext* context,
    bool multiple,
    std::vector<UsbDeviceFilterPtr> filters,
    UsbDevicesCallback callback) {
  prompt_ = base::MakeRefCounted<UsbDevicePermissionsPrompt::Prompt>(
      extension, context, multiple, std::move(filters), std::move(callback));
  ShowDialog();
}

// static
scoped_refptr<UsbDevicePermissionsPrompt::Prompt>
UsbDevicePermissionsPrompt::CreateForTest(const Extension* extension,
                                          bool multiple) {
  return base::MakeRefCounted<UsbDevicePermissionsPrompt::Prompt>(
      extension, nullptr, multiple, std::vector<UsbDeviceFilterPtr>(),
      base::DoNothing());
}

}  // namespace extensions

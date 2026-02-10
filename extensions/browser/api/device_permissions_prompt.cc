// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/device_permissions_prompt.h"

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
#include "extensions/browser/api/usb/usb_device_manager.h"
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

namespace {

class UsbDeviceInfo : public DevicePermissionsPrompt::Prompt::DeviceInfo {
 public:
  explicit UsbDeviceInfo(device::mojom::UsbDeviceInfoPtr device)
      : device_(std::move(device)) {
    name_ = DevicePermissionsManager::GetPermissionMessage(
        device_->vendor_id, device_->product_id,
        device_->manufacturer_name.value_or(std::u16string()),
        device_->product_name.value_or(std::u16string()),
        u"",  // Serial number is displayed separately.
        true);
    if (device_->serial_number) {
      serial_number_ = *device_->serial_number;
    }
  }

  ~UsbDeviceInfo() override {}

  device::mojom::UsbDeviceInfoPtr& device() { return device_; }

 private:
  device::mojom::UsbDeviceInfoPtr device_;
};

class UsbDevicePermissionsPrompt : public DevicePermissionsPrompt::Prompt,
                                   public UsbDeviceManager::Observer {
 public:
  UsbDevicePermissionsPrompt(
      const Extension* extension,
      content::BrowserContext* context,
      bool multiple,
      std::vector<UsbDeviceFilterPtr> filters,
      DevicePermissionsPrompt::UsbDevicesCallback callback)
      : Prompt(extension, context, multiple),
        filters_(std::move(filters)),
        callback_(std::move(callback)) {}

 private:
  ~UsbDevicePermissionsPrompt() override { manager_observation_.Reset(); }

  // DevicePermissionsPrompt::Prompt implementation:
  void SetObserver(
      DevicePermissionsPrompt::Prompt::Observer* observer) override {
    DevicePermissionsPrompt::Prompt::SetObserver(observer);

    if (observer) {
      auto* device_manager = UsbDeviceManager::Get(browser_context());
      if (device_manager &&
          !manager_observation_.IsObservingSource(device_manager)) {
        device_manager->GetDevices(base::BindOnce(
            &UsbDevicePermissionsPrompt::OnDevicesEnumerated, this));
        manager_observation_.Observe(device_manager);
      }
    }
  }

  void Dismissed() override {
    DevicePermissionsManager* permissions_manager =
        DevicePermissionsManager::Get(browser_context());
    std::vector<device::mojom::UsbDeviceInfoPtr> devices;
    for (const auto& device : devices_) {
      if (device->granted()) {
        UsbDeviceInfo* usb_device = static_cast<UsbDeviceInfo*>(device.get());
        if (permissions_manager) {
          DCHECK(usb_device->device());
          permissions_manager->AllowUsbDevice(extension()->id(),
                                              *usb_device->device());
        }
        devices.push_back(std::move(usb_device->device()));
      }
    }
    DCHECK(multiple() || devices.size() <= 1);
    std::move(callback_).Run(std::move(devices));
  }

  // extensions::UsbDeviceManager::Observer implementation
  void OnDeviceAdded(const device::mojom::UsbDeviceInfo& device) override {
    MaybeAddDevice(device, /*initial_enumeration=*/false);
  }

  // extensions::UsbDeviceManager::Observer implementation
  void OnDeviceRemoved(const device::mojom::UsbDeviceInfo& device) override {
    for (auto it = devices_.begin(); it != devices_.end(); ++it) {
      UsbDeviceInfo* entry = static_cast<UsbDeviceInfo*>(it->get());
      if (entry->device()->guid == device.guid) {
        if (observer()) {
          size_t index = it - devices_.begin();
          const std::u16string& device_name = entry->name();
          observer()->OnDeviceRemoved(index, device_name);
        }
        devices_.erase(it);
        return;
      }
    }
  }

  void OnDevicesEnumerated(
      std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
    for (const auto& device : devices) {
      MaybeAddDevice(*device, /*initial_enumeration=*/true);
    }
  }

  void MaybeAddDevice(const device::mojom::UsbDeviceInfo& device,
                      bool initial_enumeration) {
    if (!device::UsbDeviceFilterMatchesAny(filters_, device)) {
      return;
    }

    if (initial_enumeration) {
      remaining_initial_devices_++;
    }

    auto device_info = std::make_unique<UsbDeviceInfo>(device.Clone());
#if BUILDFLAG(IS_CHROMEOS)
    auto* device_manager = UsbDeviceManager::Get(browser_context());
    DCHECK(device_manager);
    device_manager->CheckAccess(
        device.guid,
        base::BindOnce(&UsbDevicePermissionsPrompt::AddCheckedDevice, this,
                       std::move(device_info), initial_enumeration));
#else
    AddCheckedDevice(std::move(device_info), initial_enumeration,
                     /*allowed=*/true);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void AddCheckedDevice(std::unique_ptr<UsbDeviceInfo> device_info,
                        bool initial_enumeration,
                        bool allowed) {
    if (allowed) {
      AddDevice(std::move(device_info));
    }

    if (initial_enumeration && --remaining_initial_devices_ == 0 &&
        observer()) {
      observer()->OnDevicesInitialized();
    }
  }

  std::vector<UsbDeviceFilterPtr> filters_;
  size_t remaining_initial_devices_ = 0;
  DevicePermissionsPrompt::UsbDevicesCallback callback_;
  base::ScopedObservation<UsbDeviceManager, UsbDeviceManager::Observer>
      manager_observation_{this};
};

}  // namespace

DevicePermissionsPrompt::Prompt::DeviceInfo::DeviceInfo() {
}

DevicePermissionsPrompt::Prompt::DeviceInfo::~DeviceInfo() {
}

DevicePermissionsPrompt::Prompt::Observer::~Observer() {
}

DevicePermissionsPrompt::Prompt::Prompt(const Extension* extension,
                                        content::BrowserContext* context,
                                        bool multiple)
    : extension_(extension), browser_context_(context), multiple_(multiple) {
}

void DevicePermissionsPrompt::Prompt::SetObserver(Observer* observer) {
  observer_ = observer;
}

std::u16string DevicePermissionsPrompt::Prompt::GetDeviceName(
    size_t index) const {
  if (index >= devices_.size()) {
    return u"";
  }
  return devices_[index]->name();
}

std::u16string DevicePermissionsPrompt::Prompt::GetDeviceSerialNumber(
    size_t index) const {
  if (index >= devices_.size()) {
    return u"";
  }
  return devices_[index]->serial_number();
}

void DevicePermissionsPrompt::Prompt::GrantDevicePermission(size_t index) {
  if (index < devices_.size()) {
    devices_[index]->set_granted();
  }
}

DevicePermissionsPrompt::Prompt::~Prompt() {
}

void DevicePermissionsPrompt::Prompt::AddDevice(
    std::unique_ptr<DeviceInfo> device) {
  std::u16string device_name = device->name();
  devices_.push_back(std::move(device));
  if (observer_) {
    observer_->OnDeviceAdded(devices_.size() - 1, device_name);
  }
}

DevicePermissionsPrompt::DevicePermissionsPrompt(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
}

DevicePermissionsPrompt::~DevicePermissionsPrompt() {
}

void DevicePermissionsPrompt::AskForUsbDevices(
    const Extension* extension,
    content::BrowserContext* context,
    bool multiple,
    std::vector<UsbDeviceFilterPtr> filters,
    UsbDevicesCallback callback) {
  prompt_ = base::MakeRefCounted<UsbDevicePermissionsPrompt>(
      extension, context, multiple, std::move(filters), std::move(callback));
  ShowDialog();
}

// static
scoped_refptr<DevicePermissionsPrompt::Prompt>
DevicePermissionsPrompt::CreateUsbPromptForTest(const Extension* extension,
                                                bool multiple) {
  return base::MakeRefCounted<UsbDevicePermissionsPrompt>(
      extension, nullptr, multiple, std::vector<UsbDeviceFilterPtr>(),
      base::DoNothing());
}

}  // namespace extensions

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/device_permissions_prompt.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/message_formatter.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/browser/api/usb/usb_device_manager.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/hid/hid_device_filter.h"
#include "services/device/public/cpp/hid/hid_report_utils.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/permission_broker/permission_broker_client.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS)

using device::HidDeviceFilter;
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
        std::u16string(),  // Serial number is displayed separately.
        true);
    serial_number_ =
        device_->serial_number ? *(device_->serial_number) : std::u16string();
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
      UsbDeviceInfo* entry = static_cast<UsbDeviceInfo*>((*it).get());
      if (entry->device()->guid == device.guid) {
        size_t index = it - devices_.begin();
        std::u16string device_name = (*it)->name();
        devices_.erase(it);
        if (observer())
          observer()->OnDeviceRemoved(index, device_name);
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
    if (!device::UsbDeviceFilterMatchesAny(filters_, device))
      return;

    if (initial_enumeration)
      remaining_initial_devices_++;

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
    if (allowed)
      AddDevice(std::move(device_info));

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

class HidDeviceInfo : public DevicePermissionsPrompt::Prompt::DeviceInfo {
 public:
  explicit HidDeviceInfo(device::mojom::HidDeviceInfoPtr device)
      : device_(std::move(device)) {
    name_ = DevicePermissionsManager::GetPermissionMessage(
        device_->vendor_id, device_->product_id,
        std::u16string(),  // HID devices include manufacturer in product name.
        base::UTF8ToUTF16(device_->product_name),
        std::u16string(),  // Serial number is displayed separately.
        false);
    serial_number_ = base::UTF8ToUTF16(device_->serial_number);
  }

  ~HidDeviceInfo() override {}

  device::mojom::HidDeviceInfoPtr& device() { return device_; }

 private:
  device::mojom::HidDeviceInfoPtr device_;
};

DevicePermissionsPrompt::HidManagerBinder& GetHidManagerBinderOverride() {
  static base::NoDestructor<DevicePermissionsPrompt::HidManagerBinder> binder;
  return *binder;
}

class HidDevicePermissionsPrompt : public DevicePermissionsPrompt::Prompt,
                                   public device::mojom::HidManagerClient {
 public:
  HidDevicePermissionsPrompt(
      const Extension* extension,
      content::BrowserContext* context,
      bool multiple,
      const std::vector<HidDeviceFilter>& filters,
      DevicePermissionsPrompt::HidDevicesCallback callback)
      : Prompt(extension, context, multiple),
        initialized_(false),
        filters_(filters),
        callback_(std::move(callback)) {}

 private:
  ~HidDevicePermissionsPrompt() override {}

  // DevicePermissionsPrompt::Prompt implementation:
  void SetObserver(
      DevicePermissionsPrompt::Prompt::Observer* observer) override {
    DevicePermissionsPrompt::Prompt::SetObserver(observer);

    if (observer)
      LazyInitialize();
  }

  void LazyInitialize() {
    if (initialized_) {
      return;
    }

    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    auto receiver = hid_manager_.BindNewPipeAndPassReceiver();
    const auto& binder = GetHidManagerBinderOverride();
    if (binder)
      binder.Run(std::move(receiver));
    else
      content::GetDeviceService().BindHidManager(std::move(receiver));

    hid_manager_->GetDevicesAndSetClient(
        receiver_.BindNewEndpointAndPassRemote(),
        base::BindOnce(&HidDevicePermissionsPrompt::OnDevicesEnumerated, this));

    initialized_ = true;
  }

  void Dismissed() override {
    DevicePermissionsManager* permissions_manager =
        DevicePermissionsManager::Get(browser_context());
    std::vector<device::mojom::HidDeviceInfoPtr> devices;
    for (const auto& device : devices_) {
      if (device->granted()) {
        HidDeviceInfo* hid_device = static_cast<HidDeviceInfo*>(device.get());
        if (permissions_manager) {
          DCHECK(hid_device->device());
          permissions_manager->AllowHidDevice(extension()->id(),
                                              *(hid_device->device()));
        }
        devices.push_back(std::move(hid_device->device()));
      }
    }
    DCHECK(multiple() || devices.size() <= 1);
    std::move(callback_).Run(std::move(devices));
  }

  // device::mojom::HidManagerClient implementation:
  void DeviceAdded(device::mojom::HidDeviceInfoPtr device) override {
    MaybeAddDevice(std::move(device), /*initial_enumeration=*/false);
  }

  void DeviceRemoved(device::mojom::HidDeviceInfoPtr device) override {
    for (auto it = devices_.begin(); it != devices_.end(); ++it) {
      HidDeviceInfo* entry = static_cast<HidDeviceInfo*>((*it).get());
      if (entry->device()->guid == device->guid) {
        size_t index = it - devices_.begin();
        std::u16string device_name = (*it)->name();
        devices_.erase(it);
        if (observer())
          observer()->OnDeviceRemoved(index, device_name);
        return;
      }
    }
  }

  void DeviceChanged(device::mojom::HidDeviceInfoPtr device) override {
    for (const auto& device_info : devices_) {
      auto* hid_device_info =
          reinterpret_cast<HidDeviceInfo*>(device_info.get());
      if (hid_device_info->device()->guid == device->guid) {
        // The device is already present in |devices_|. Update its device
        // information.
        hid_device_info->device() = std::move(device);
        return;
      }
    }

    // The device was not previously added to |devices_|, possibly due to
    // filters or protected collections. Try adding it again.
    MaybeAddDevice(std::move(device), /*initial_enumeration=*/false);
  }

  void OnDevicesEnumerated(
      std::vector<device::mojom::HidDeviceInfoPtr> devices) {
    for (auto& device : devices)
      MaybeAddDevice(std::move(device), /*initial_enumeration=*/true);
  }

  // Returns true if `device` contains at least one unprotected report in any
  // collection.
  bool HasUnprotectedReports(const device::mojom::HidDeviceInfo& device) {
    return base::ranges::any_of(device.collections, [](const auto& collection) {
      return device::CollectionHasUnprotectedReports(*collection);
    });
  }

  void MaybeAddDevice(device::mojom::HidDeviceInfoPtr device,
                      bool initial_enumeration) {
    if (!HasUnprotectedReports(*device) ||
        (!filters_.empty() &&
         !HidDeviceFilter::MatchesAny(*device, filters_))) {
      return;
    }

    if (initial_enumeration)
      remaining_initial_devices_++;

    auto device_info = std::make_unique<HidDeviceInfo>(std::move(device));
    // TODO(huangs): Enable this for Lacros (crbug.com/1217124).
#if BUILDFLAG(IS_CHROMEOS)
    chromeos::PermissionBrokerClient::Get()->CheckPathAccess(
        device_info.get()->device()->device_node,
        base::BindOnce(&HidDevicePermissionsPrompt::AddCheckedDevice, this,
                       std::move(device_info), initial_enumeration));
#else
    AddCheckedDevice(std::move(device_info), initial_enumeration,
                     /*allowed=*/true);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void AddCheckedDevice(std::unique_ptr<HidDeviceInfo> device_info,
                        bool initial_enumeration,
                        bool allowed) {
    if (allowed)
      AddDevice(std::move(device_info));

    if (initial_enumeration && --remaining_initial_devices_ == 0 &&
        observer()) {
      observer()->OnDevicesInitialized();
    }
  }

  bool initialized_;
  std::vector<HidDeviceFilter> filters_;
  size_t remaining_initial_devices_ = 0;
  mojo::Remote<device::mojom::HidManager> hid_manager_;
  DevicePermissionsPrompt::HidDevicesCallback callback_;
  mojo::AssociatedReceiver<device::mojom::HidManagerClient> receiver_{this};
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
  DCHECK_LT(index, devices_.size());
  return devices_[index]->name();
}

std::u16string DevicePermissionsPrompt::Prompt::GetDeviceSerialNumber(
    size_t index) const {
  DCHECK_LT(index, devices_.size());
  return devices_[index]->serial_number();
}

void DevicePermissionsPrompt::Prompt::GrantDevicePermission(size_t index) {
  DCHECK_LT(index, devices_.size());
  devices_[index]->set_granted();
}

DevicePermissionsPrompt::Prompt::~Prompt() {
}

void DevicePermissionsPrompt::Prompt::AddDevice(
    std::unique_ptr<DeviceInfo> device) {
  std::u16string device_name = device->name();
  devices_.push_back(std::move(device));
  if (observer_)
    observer_->OnDeviceAdded(devices_.size() - 1, device_name);
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

void DevicePermissionsPrompt::AskForHidDevices(
    const Extension* extension,
    content::BrowserContext* context,
    bool multiple,
    const std::vector<HidDeviceFilter>& filters,
    HidDevicesCallback callback) {
  prompt_ = base::MakeRefCounted<HidDevicePermissionsPrompt>(
      extension, context, multiple, filters, std::move(callback));
  ShowDialog();
}

// static
scoped_refptr<DevicePermissionsPrompt::Prompt>
DevicePermissionsPrompt::CreateHidPromptForTest(const Extension* extension,
                                                bool multiple) {
  return base::MakeRefCounted<HidDevicePermissionsPrompt>(
      extension, nullptr, multiple, std::vector<HidDeviceFilter>(),
      base::DoNothing());
}

// static
scoped_refptr<DevicePermissionsPrompt::Prompt>
DevicePermissionsPrompt::CreateUsbPromptForTest(const Extension* extension,
                                                bool multiple) {
  return base::MakeRefCounted<UsbDevicePermissionsPrompt>(
      extension, nullptr, multiple, std::vector<UsbDeviceFilterPtr>(),
      base::DoNothing());
}

// static
void DevicePermissionsPrompt::OverrideHidManagerBinderForTesting(
    HidManagerBinder binder) {
  GetHidManagerBinderOverride() = std::move(binder);
}

}  // namespace extensions

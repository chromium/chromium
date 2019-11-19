// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/device_permissions_prompt.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/message_formatter.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/system_connector.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/browser/api/usb/usb_device_manager.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/hid/hid_device_filter.h"
#include "services/device/public/cpp/hid/hid_usage_and_page.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/permission_broker/permission_broker_client.h"
#endif  // defined(OS_CHROMEOS)

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
        device_->manufacturer_name.value_or(base::string16()),
        device_->product_name.value_or(base::string16()),
        base::string16(),  // Serial number is displayed separately.
        true);
    serial_number_ =
        device_->serial_number ? *(device_->serial_number) : base::string16();
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
      const DevicePermissionsPrompt::UsbDevicesCallback& callback)
      : Prompt(extension, context, multiple),
        filters_(std::move(filters)),
        callback_(callback),
        manager_observer_(this) {}

 private:
  ~UsbDevicePermissionsPrompt() override { manager_observer_.RemoveAll(); }

  // DevicePermissionsPrompt::Prompt implementation:
  void SetObserver(
      DevicePermissionsPrompt::Prompt::Observer* observer) override {
    DevicePermissionsPrompt::Prompt::SetObserver(observer);

    if (observer) {
      auto* device_manager = UsbDeviceManager::Get(browser_context());
      if (device_manager && !manager_observer_.IsObserving(device_manager)) {
        device_manager->GetDevices(
            base::Bind(&UsbDevicePermissionsPrompt::OnDevicesEnumerated, this));
        manager_observer_.Add(device_manager);
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
    callback_.Run(std::move(devices));
    callback_.Reset();
  }

  // extensions::UsbDeviceManager::Observer implementation
  void OnDeviceAdded(const device::mojom::UsbDeviceInfo& device) override {
    if (!device::UsbDeviceFilterMatchesAny(filters_, device))
      return;

    auto device_info = std::make_unique<UsbDeviceInfo>(device.Clone());
#if defined(OS_CHROMEOS)
    auto* device_manager = UsbDeviceManager::Get(browser_context());
    DCHECK(device_manager);
    device_manager->CheckAccess(
        device.guid,
        base::BindOnce(&UsbDevicePermissionsPrompt::AddCheckedDevice, this,
                       base::Passed(&device_info)));
#else
    AddCheckedDevice(std::move(device_info), true);
#endif  // defined(OS_CHROMEOS)
  }

  // extensions::UsbDeviceManager::Observer implementation
  void OnDeviceRemoved(const device::mojom::UsbDeviceInfo& device) override {
    for (auto it = devices_.begin(); it != devices_.end(); ++it) {
      UsbDeviceInfo* entry = static_cast<UsbDeviceInfo*>((*it).get());
      if (entry->device()->guid == device.guid) {
        size_t index = it - devices_.begin();
        base::string16 device_name = (*it)->name();
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
      OnDeviceAdded(*device);
    }
  }

  std::vector<UsbDeviceFilterPtr> filters_;
  DevicePermissionsPrompt::UsbDevicesCallback callback_;
  ScopedObserver<UsbDeviceManager, UsbDeviceManager::Observer>
      manager_observer_;
};

class HidDeviceInfo : public DevicePermissionsPrompt::Prompt::DeviceInfo {
 public:
  explicit HidDeviceInfo(device::mojom::HidDeviceInfoPtr device)
      : device_(std::move(device)) {
    name_ = DevicePermissionsManager::GetPermissionMessage(
        device_->vendor_id, device_->product_id,
        base::string16(),  // HID devices include manufacturer in product name.
        base::UTF8ToUTF16(device_->product_name),
        base::string16(),  // Serial number is displayed separately.
        false);
    serial_number_ = base::UTF8ToUTF16(device_->serial_number);
  }

  ~HidDeviceInfo() override {}

  device::mojom::HidDeviceInfoPtr& device() { return device_; }

 private:
  device::mojom::HidDeviceInfoPtr device_;
};

class HidDevicePermissionsPrompt : public DevicePermissionsPrompt::Prompt,
                                   public device::mojom::HidManagerClient {
 public:
  HidDevicePermissionsPrompt(
      const Extension* extension,
      content::BrowserContext* context,
      bool multiple,
      const std::vector<HidDeviceFilter>& filters,
      const DevicePermissionsPrompt::HidDevicesCallback& callback)
      : Prompt(extension, context, multiple),
        initialized_(false),
        filters_(filters),
        callback_(callback) {}

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
    service_manager::Connector* connector = content::GetSystemConnector();
    DCHECK(connector);
    connector->Connect(device::mojom::kServiceName,
                       hid_manager_.BindNewPipeAndPassReceiver());

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
    callback_.Run(std::move(devices));
    callback_.Reset();
  }

  // device::mojom::HidManagerClient implementation:
  void DeviceAdded(device::mojom::HidDeviceInfoPtr device) override {
    if (HasUnprotectedCollections(*device) &&
        (filters_.empty() || HidDeviceFilter::MatchesAny(*device, filters_))) {
      auto device_info = std::make_unique<HidDeviceInfo>(std::move(device));
#if defined(OS_CHROMEOS)
      chromeos::PermissionBrokerClient::Get()->CheckPathAccess(
          device_info.get()->device()->device_node,
          base::BindOnce(&HidDevicePermissionsPrompt::AddCheckedDevice, this,
                         std::move(device_info)));
#else
      AddCheckedDevice(std::move(device_info), true);
#endif  // defined(OS_CHROMEOS)
    }
  }

  void DeviceRemoved(device::mojom::HidDeviceInfoPtr device) override {
    for (auto it = devices_.begin(); it != devices_.end(); ++it) {
      HidDeviceInfo* entry = static_cast<HidDeviceInfo*>((*it).get());
      if (entry->device()->guid == device->guid) {
        size_t index = it - devices_.begin();
        base::string16 device_name = (*it)->name();
        devices_.erase(it);
        if (observer())
          observer()->OnDeviceRemoved(index, device_name);
        return;
      }
    }
  }

  void OnDevicesEnumerated(
      std::vector<device::mojom::HidDeviceInfoPtr> devices) {
    for (auto& device : devices)
      DeviceAdded(std::move(device));
  }

  bool HasUnprotectedCollections(const device::mojom::HidDeviceInfo& device) {
    for (const auto& collection : device.collections) {
      if (!device::IsProtected(*collection->usage)) {
        return true;
      }
    }
    return false;
  }

  bool initialized_;
  std::vector<HidDeviceFilter> filters_;
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

base::string16 DevicePermissionsPrompt::Prompt::GetDeviceName(
    size_t index) const {
  DCHECK_LT(index, devices_.size());
  return devices_[index]->name();
}

base::string16 DevicePermissionsPrompt::Prompt::GetDeviceSerialNumber(
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

void DevicePermissionsPrompt::Prompt::AddCheckedDevice(
    std::unique_ptr<DeviceInfo> device,
    bool allowed) {
  if (allowed) {
    base::string16 device_name = device->name();
    devices_.push_back(std::move(device));
    if (observer_)
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
    const UsbDevicesCallback& callback) {
  prompt_ = base::MakeRefCounted<UsbDevicePermissionsPrompt>(
      extension, context, multiple, std::move(filters), callback);
  ShowDialog();
}

void DevicePermissionsPrompt::AskForHidDevices(
    const Extension* extension,
    content::BrowserContext* context,
    bool multiple,
    const std::vector<HidDeviceFilter>& filters,
    const HidDevicesCallback& callback) {
  prompt_ = base::MakeRefCounted<HidDevicePermissionsPrompt>(
      extension, context, multiple, filters, callback);
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

}  // namespace extensions

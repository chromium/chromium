// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/usb/usb_device_manager.h"

#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/lazy_instance.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/common/api/usb.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/event_dispatcher.mojom-forward.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace usb = extensions::api::usb;

using content::BrowserThread;

namespace extensions {

namespace {

constexpr int kUsbClassMassStorage = 0x08;

bool IsMassStorageInterface(const device::mojom::UsbInterfaceInfo& interface) {
  for (auto& alternate : interface.alternates) {
    if (alternate->class_code == kUsbClassMassStorage)
      return true;
  }
  return false;
}

bool ShouldExposeDevice(const device::mojom::UsbDeviceInfo& device_info) {
  // ChromeOS always allows mass storage devices to be detached, but chrome.usb
  // only gets access when the specific vid/pid is listed in device policy.
  // This means that reloading policy can change the result of this function.
  for (auto& configuration : device_info.configurations) {
    for (auto& interface : configuration->interfaces) {
      if (!IsMassStorageInterface(*interface))
        return true;
    }
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (ExtensionsAPIClient::Get()->ShouldAllowDetachingUsb(
          device_info.vendor_id, device_info.product_id)) {
    return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  return false;
}

// Returns true if the given extension has permission to receive events
// regarding this device.
bool WillDispatchDeviceEvent(
    const device::mojom::UsbDeviceInfo& device_info,
    content::BrowserContext* browser_context,
    mojom::ContextType target_context,
    const Extension* extension,
    const base::Value::Dict* listener_filter,
    std::optional<base::Value::List>& event_args_out,
    mojom::EventFilteringInfoPtr& event_filtering_info_out) {
  // Check install-time and optional permissions.
  std::unique_ptr<UsbDevicePermission::CheckParam> param =
      UsbDevicePermission::CheckParam::ForUsbDevice(extension, device_info);
  if (extension->permissions_data()->CheckAPIPermissionWithParam(
          mojom::APIPermissionID::kUsbDevice, param.get())) {
    return true;
  }

  // Check permissions granted through chrome.usb.getUserSelectedDevices.
  DevicePermissions* device_permissions =
      DevicePermissionsManager::Get(browser_context)
          ->GetForExtension(extension->id());
  if (device_permissions->FindUsbDeviceEntry(device_info).get()) {
    return true;
  }

  // Check against WebUsbAllowDevicesForUrls.
  ExtensionsBrowserClient* client = ExtensionsBrowserClient::Get();
  DCHECK(client);
  if (client->IsUsbDeviceAllowedByPolicy(browser_context, extension->id(),
                                         device_info.vendor_id,
                                         device_info.product_id)) {
    return true;
  }

  return false;
}

base::LazyInstance<BrowserContextKeyedAPIFactory<UsbDeviceManager>>::Leaky
    g_event_router_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
UsbDeviceManager* UsbDeviceManager::Get(
    content::BrowserContext* browser_context) {
  return BrowserContextKeyedAPIFactory<UsbDeviceManager>::Get(browser_context);
}

// static
BrowserContextKeyedAPIFactory<UsbDeviceManager>*
UsbDeviceManager::GetFactoryInstance() {
  return g_event_router_factory.Pointer();
}

void UsbDeviceManager::Observer::OnDeviceAdded(
    const device::mojom::UsbDeviceInfo& device_info) {}

void UsbDeviceManager::Observer::OnDeviceRemoved(
    const device::mojom::UsbDeviceInfo& device_info) {}

void UsbDeviceManager::Observer::OnDeviceManagerConnectionError() {}

UsbDeviceManager::UsbDeviceManager(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->RegisterObserver(this, usb::OnDeviceAdded::kEventName);
    event_router->RegisterObserver(this, usb::OnDeviceRemoved::kEventName);
  }
}

UsbDeviceManager::~UsbDeviceManager() = default;

void UsbDeviceManager::AddObserver(Observer* observer) {
  EnsureConnectionWithDeviceManager();
  observer_list_.AddObserver(observer);
}

void UsbDeviceManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

int UsbDeviceManager::GetIdFromGuid(const std::string& guid) {
  auto iter = guid_to_id_map_.find(guid);
  if (iter == guid_to_id_map_.end()) {
    auto result = guid_to_id_map_.insert(std::make_pair(guid, next_id_++));
    DCHECK(result.second);
    iter = result.first;
    id_to_guid_map_.insert(std::make_pair(iter->second, guid));
  }
  return iter->second;
}

bool UsbDeviceManager::GetGuidFromId(int id, std::string* guid) {
  auto iter = id_to_guid_map_.find(id);
  if (iter == id_to_guid_map_.end())
    return false;
  *guid = iter->second;
  return true;
}

void UsbDeviceManager::GetApiDevice(
    const device::mojom::UsbDeviceInfo& device_in,
    api::usb::Device* device_out) {
  device_out->device = GetIdFromGuid(device_in.guid);
  device_out->vendor_id = device_in.vendor_id;
  device_out->product_id = device_in.product_id;
  device_out->version = device_in.device_version_major << 8 |
                        device_in.device_version_minor << 4 |
                        device_in.device_version_subminor;
  device_out->product_name =
      base::UTF16ToUTF8(device_in.product_name.value_or(std::u16string()));
  device_out->manufacturer_name =
      base::UTF16ToUTF8(device_in.manufacturer_name.value_or(std::u16string()));
  device_out->serial_number =
      base::UTF16ToUTF8(device_in.serial_number.value_or(std::u16string()));
}

void UsbDeviceManager::GetDevices(
    device::mojom::UsbDeviceManager::GetDevicesCallback callback) {
  if (!is_initialized_) {
    pending_get_devices_requests_.push(std::move(callback));
    EnsureConnectionWithDeviceManager();
    return;
  }

  std::vector<device::mojom::UsbDeviceInfoPtr> device_list;
  for (const auto& pair : devices_)
    device_list.push_back(pair.second->Clone());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(device_list)));
}

void UsbDeviceManager::GetDevice(
    const std::string& guid,
    mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver) {
  EnsureConnectionWithDeviceManager();
  device_manager_->GetDevice(guid, /*blocked_interface_classes=*/{},
                             std::move(device_receiver),
                             /*device_client=*/mojo::NullRemote());
}

const device::mojom::UsbDeviceInfo* UsbDeviceManager::GetDeviceInfo(
    const std::string& guid) {
  DCHECK(is_initialized_);
  auto it = devices_.find(guid);
  return it == devices_.end() ? nullptr : it->second.get();
}

bool UsbDeviceManager::UpdateActiveConfig(const std::string& guid,
                                          uint8_t config_value) {
  DCHECK(is_initialized_);
  auto it = devices_.find(guid);
  if (it == devices_.end()) {
    return false;
  }
  it->second->active_configuration = config_value;
  return true;
}

#if BUILDFLAG(IS_CHROMEOS)
void UsbDeviceManager::CheckAccess(
    const std::string& guid,
    device::mojom::UsbDeviceManager::CheckAccessCallback callback) {
  EnsureConnectionWithDeviceManager();
  device_manager_->CheckAccess(guid, std::move(callback));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void UsbDeviceManager::EnsureConnectionWithDeviceManager() {
  if (device_manager_)
    return;

  // Receive mojo::Remote<UsbDeviceManager> from DeviceService.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetDeviceService().BindUsbDeviceManager(
      device_manager_.BindNewPipeAndPassReceiver());

  SetUpDeviceManagerConnection();
}

void UsbDeviceManager::SetDeviceManagerForTesting(
    mojo::PendingRemote<device::mojom::UsbDeviceManager> fake_device_manager) {
  DCHECK(!device_manager_);
  DCHECK(fake_device_manager);
  device_manager_.Bind(std::move(fake_device_manager));
  SetUpDeviceManagerConnection();
}

void UsbDeviceManager::Shutdown() {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->UnregisterObserver(this);
  }
}

void UsbDeviceManager::OnListenerAdded(const EventListenerInfo& details) {
  EnsureConnectionWithDeviceManager();
}

void UsbDeviceManager::OnDeviceAdded(
    device::mojom::UsbDeviceInfoPtr device_info) {
  DCHECK(device_info);
  // Update the device list.
  DCHECK(!base::Contains(devices_, device_info->guid));
  if (!ShouldExposeDevice(*device_info))
    return;
  std::string guid = device_info->guid;
  auto result =
      devices_.insert(std::make_pair(std::move(guid), std::move(device_info)));
  const device::mojom::UsbDeviceInfo& stored_info = *result.first->second;

  DispatchEvent(usb::OnDeviceAdded::kEventName, stored_info);

  // Notify all observers.
  for (auto& observer : observer_list_)
    observer.OnDeviceAdded(stored_info);
}

void UsbDeviceManager::OnDeviceRemoved(
    device::mojom::UsbDeviceInfoPtr device_info) {
  DCHECK(device_info);

  // Handle if ShouldExposeDevice() returned false when the device was added.
  if (!base::Contains(devices_, device_info->guid))
    return;

  // Update the device list.
  devices_.erase(device_info->guid);

  DispatchEvent(usb::OnDeviceRemoved::kEventName, *device_info);

  // Notify all observers for OnDeviceRemoved event.
  for (auto& observer : observer_list_)
    observer.OnDeviceRemoved(*device_info);

  auto iter = guid_to_id_map_.find(device_info->guid);
  if (iter != guid_to_id_map_.end()) {
    int id = iter->second;
    guid_to_id_map_.erase(iter);
    id_to_guid_map_.erase(id);
  }

  // Remove permission entry for ephemeral USB device.
  DevicePermissionsManager* permissions_manager =
      DevicePermissionsManager::Get(browser_context_);
  DCHECK(permissions_manager);
  permissions_manager->RemoveEntryByDeviceGUID(DevicePermissionEntry::Type::USB,
                                               device_info->guid);
}

void UsbDeviceManager::SetUpDeviceManagerConnection() {
  DCHECK(device_manager_);
  device_manager_.set_disconnect_handler(
      base::BindOnce(&UsbDeviceManager::OnDeviceManagerConnectionError,
                     base::Unretained(this)));

  // Listen for added/removed device events.
  DCHECK(!client_receiver_.is_bound());
  device_manager_->EnumerateDevicesAndSetClient(
      client_receiver_.BindNewEndpointAndPassRemote(),
      base::BindOnce(&UsbDeviceManager::InitDeviceList,
                     weak_factory_.GetWeakPtr()));
}

void UsbDeviceManager::InitDeviceList(
    std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
  for (auto& device_info : devices) {
    DCHECK(device_info);
    if (!ShouldExposeDevice(*device_info))
      continue;
    std::string guid = device_info->guid;
    devices_.insert(std::make_pair(guid, std::move(device_info)));
  }
  is_initialized_ = true;

  while (!pending_get_devices_requests_.empty()) {
    std::vector<device::mojom::UsbDeviceInfoPtr> device_list;
    for (const auto& entry : devices_) {
      device_list.push_back(entry.second->Clone());
    }
    std::move(pending_get_devices_requests_.front())
        .Run(std::move(device_list));
    pending_get_devices_requests_.pop();
  }
}

void UsbDeviceManager::OnDeviceManagerConnectionError() {
  device_manager_.reset();
  client_receiver_.reset();
  devices_.clear();
  is_initialized_ = false;

  guid_to_id_map_.clear();
  id_to_guid_map_.clear();

  // Notify all observers.
  for (auto& observer : observer_list_)
    observer.OnDeviceManagerConnectionError();
}

void UsbDeviceManager::DispatchEvent(
    const std::string& event_name,
    const device::mojom::UsbDeviceInfo& device_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    usb::Device device_obj;
    GetApiDevice(device_info, &device_obj);

    std::unique_ptr<Event> event;
    if (event_name == usb::OnDeviceAdded::kEventName) {
      event = std::make_unique<Event>(events::USB_ON_DEVICE_ADDED,
                                      usb::OnDeviceAdded::kEventName,
                                      usb::OnDeviceAdded::Create(device_obj));
    } else {
      DCHECK(event_name == usb::OnDeviceRemoved::kEventName);
      event = std::make_unique<Event>(events::USB_ON_DEVICE_REMOVED,
                                      usb::OnDeviceRemoved::kEventName,
                                      usb::OnDeviceRemoved::Create(device_obj));
    }

    event->will_dispatch_callback =
        base::BindRepeating(&WillDispatchDeviceEvent, std::cref(device_info));
    event_router->BroadcastEvent(std::move(event));
  }
}

template <>
void BrowserContextKeyedAPIFactory<
    UsbDeviceManager>::DeclareFactoryDependencies() {
  DependsOn(DevicePermissionsManagerFactory::GetInstance());
  DependsOn(EventRouterFactory::GetInstance());
}

}  // namespace extensions

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/hid/hid_device_manager.h"

#include <stdint.h>

#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/event_dispatcher.mojom-forward.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/hid/hid_device_filter.h"
#include "services/device/public/cpp/hid/hid_report_type.h"
#include "services/device/public/cpp/hid/hid_report_utils.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace hid = extensions::api::hid;

namespace extensions {

namespace {

using ::device::HidDeviceFilter;
using ::device::HidReportType;
using ::device::IsAlwaysProtected;

// Return true if all reports in `device` with `report_id` are protected.
// Protected report IDs are not exposed in the API.
bool IsReportIdProtected(const device::mojom::HidDeviceInfo& device,
                         uint8_t report_id) {
  // `report_id` is not protected if there is any report with `report_id` that
  // is not protected.
  constexpr HidReportType report_types[] = {
      HidReportType::kInput, HidReportType::kOutput, HidReportType::kFeature};
  bool found_matching_report = false;
  for (const auto& report_type : report_types) {
    auto* collection =
        device::FindCollectionWithReport(device, report_id, report_type);
    if (collection) {
      found_matching_report = true;
      if (!IsAlwaysProtected(*collection->usage, report_type)) {
        return false;
      }
    }
  }
  if (!found_matching_report) {
    // No reports with `report_id` were found in any collection. This indicates
    // an error in the device's report descriptor, but the device may still be
    // usable. Consider the report protected if `device` has any collection with
    // a protected usage.
    return base::ranges::any_of(device.collections, [](const auto& collection) {
      return IsAlwaysProtected(*collection->usage, HidReportType::kInput) ||
             IsAlwaysProtected(*collection->usage, HidReportType::kOutput) ||
             IsAlwaysProtected(*collection->usage, HidReportType::kFeature);
    });
  }

  // All reports matching `report_id` are protected.
  return true;
}

void PopulateHidDeviceInfo(hid::HidDeviceInfo* output,
                           const device::mojom::HidDeviceInfo& input) {
  output->vendor_id = input.vendor_id;
  output->product_id = input.product_id;
  output->product_name = input.product_name;
  output->serial_number = input.serial_number;
  output->max_input_report_size = input.max_input_report_size;
  output->max_output_report_size = input.max_output_report_size;
  output->max_feature_report_size = input.max_feature_report_size;

  for (const auto& collection : input.collections) {
    // Omit a collection if all its reports are protected.
    if (!device::CollectionHasUnprotectedReports(*collection)) {
      continue;
    }

    // Omit IDs only used by protected reports.
    std::vector<int> filtered_report_ids;
    base::ranges::copy_if(collection->report_ids,
                          std::back_inserter(filtered_report_ids),
                          [&input](int report_id) {
                            return !IsReportIdProtected(input, report_id);
                          });

    hid::HidCollectionInfo api_collection;
    api_collection.usage_page = collection->usage->usage_page;
    api_collection.usage = collection->usage->usage;

    api_collection.report_ids = std::move(filtered_report_ids);

    output->collections.push_back(std::move(api_collection));
  }

  const std::vector<uint8_t>& report_descriptor = input.report_descriptor;
  if (report_descriptor.size() > 0) {
    output->report_descriptor.assign(report_descriptor.begin(),
                                     report_descriptor.end());
  }
}

bool WillDispatchDeviceEvent(
    base::WeakPtr<HidDeviceManager> device_manager,
    const device::mojom::HidDeviceInfo& device_info,
    content::BrowserContext* browser_context,
    mojom::ContextType target_context,
    const Extension* extension,
    const base::Value::Dict* listener_filter,
    std::optional<base::Value::List>& event_args_out,
    mojom::EventFilteringInfoPtr& event_filtering_info_out) {
  if (device_manager && extension) {
    return device_manager->HasPermission(extension, device_info, false);
  }
  return false;
}

HidDeviceManager::HidManagerBinder& GetHidManagerBinderOverride() {
  static base::NoDestructor<HidDeviceManager::HidManagerBinder> binder;
  return *binder;
}

}  // namespace

struct HidDeviceManager::GetApiDevicesParams {
 public:
  GetApiDevicesParams(const Extension* extension,
                      const std::vector<HidDeviceFilter>& filters,
                      GetApiDevicesCallback callback)
      : extension(extension), filters(filters), callback(std::move(callback)) {}
  ~GetApiDevicesParams() {}

  raw_ptr<const Extension> extension;
  std::vector<HidDeviceFilter> filters;
  GetApiDevicesCallback callback;
};

HidDeviceManager::HidDeviceManager(content::BrowserContext* context)
    : browser_context_(context) {
  event_router_ = EventRouter::Get(context);
  if (event_router_) {
    event_router_->RegisterObserver(this, hid::OnDeviceAdded::kEventName);
    event_router_->RegisterObserver(this, hid::OnDeviceRemoved::kEventName);
  }
}

HidDeviceManager::~HidDeviceManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

// static
BrowserContextKeyedAPIFactory<HidDeviceManager>*
HidDeviceManager::GetFactoryInstance() {
  static base::LazyInstance<BrowserContextKeyedAPIFactory<HidDeviceManager>>::
      DestructorAtExit factory = LAZY_INSTANCE_INITIALIZER;
  return &factory.Get();
}

void HidDeviceManager::GetApiDevices(
    const Extension* extension,
    const std::vector<HidDeviceFilter>& filters,
    GetApiDevicesCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LazyInitialize();

  if (enumeration_ready_) {
    base::Value::List devices = CreateApiDeviceList(extension, filters);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(devices)));
  } else {
    pending_enumerations_.push_back(std::make_unique<GetApiDevicesParams>(
        extension, filters, std::move(callback)));
  }
}

const device::mojom::HidDeviceInfo* HidDeviceManager::GetDeviceInfo(
    int resource_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ResourceIdToDeviceInfoMap::const_iterator device_iter =
      devices_.find(resource_id);
  if (device_iter == devices_.end()) {
    return nullptr;
  }

  return device_iter->second.get();
}

void HidDeviceManager::Connect(const std::string& device_guid,
                               ConnectCallback callback) {
  DCHECK(initialized_);

  hid_manager_->Connect(device_guid, /*connection_client=*/mojo::NullRemote(),
                        /*watcher=*/mojo::NullRemote(),
                        /*allow_protected_reports=*/true,
                        /*allow_fido_reports=*/true,
                        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                            std::move(callback), mojo::NullRemote()));
}

bool HidDeviceManager::HasPermission(
    const Extension* extension,
    const device::mojom::HidDeviceInfo& device_info,
    bool update_last_used) {
  DevicePermissionsManager* permissions_manager =
      DevicePermissionsManager::Get(browser_context_);
  CHECK(permissions_manager);
  DevicePermissions* device_permissions =
      permissions_manager->GetForExtension(extension->id());
  DCHECK(device_permissions);
  scoped_refptr<DevicePermissionEntry> permission_entry =
      device_permissions->FindHidDeviceEntry(device_info);
  if (permission_entry) {
    if (update_last_used) {
      permissions_manager->UpdateLastUsed(extension->id(), permission_entry);
    }
    return true;
  }

  std::unique_ptr<UsbDevicePermission::CheckParam> usb_param =
      UsbDevicePermission::CheckParam::ForHidDevice(
          extension, device_info.vendor_id, device_info.product_id);
  if (extension->permissions_data()->CheckAPIPermissionWithParam(
          mojom::APIPermissionID::kUsbDevice, usb_param.get())) {
    return true;
  }

  if (extension->permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kU2fDevices)) {
    HidDeviceFilter u2f_filter;
    u2f_filter.SetUsagePage(0xF1D0);
    if (u2f_filter.Matches(device_info)) {
      return true;
    }
  }

  return false;
}

void HidDeviceManager::Shutdown() {
  if (event_router_) {
    event_router_->UnregisterObserver(this);
  }
}

void HidDeviceManager::OnListenerAdded(const EventListenerInfo& details) {
  LazyInitialize();
}
void HidDeviceManager::DeviceAdded(device::mojom::HidDeviceInfoPtr device) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_LT(next_resource_id_, std::numeric_limits<int>::max());
  int new_id = next_resource_id_++;
  DCHECK(!base::Contains(resource_ids_, device->guid));
  resource_ids_[device->guid] = new_id;
  devices_[new_id] = std::move(device);

  // Don't generate events during the initial enumeration.
  if (enumeration_ready_ && event_router_) {
    api::hid::HidDeviceInfo api_device_info;
    api_device_info.device_id = new_id;

    PopulateHidDeviceInfo(&api_device_info, *devices_[new_id]);

    if (api_device_info.collections.size() > 0) {
      auto args(hid::OnDeviceAdded::Create(api_device_info));
      DispatchEvent(events::HID_ON_DEVICE_ADDED, hid::OnDeviceAdded::kEventName,
                    std::move(args), *devices_[new_id]);
    }
  }
}

void HidDeviceManager::DeviceRemoved(device::mojom::HidDeviceInfoPtr device) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const auto& resource_entry = resource_ids_.find(device->guid);
  CHECK(resource_entry != resource_ids_.end(), base::NotFatalUntil::M130);
  int resource_id = resource_entry->second;
  const auto& device_entry = devices_.find(resource_id);
  CHECK(device_entry != devices_.end(), base::NotFatalUntil::M130);
  resource_ids_.erase(resource_entry);
  devices_.erase(device_entry);

  if (event_router_) {
    DCHECK(enumeration_ready_);
    auto args(hid::OnDeviceRemoved::Create(resource_id));
    DispatchEvent(events::HID_ON_DEVICE_REMOVED,
                  hid::OnDeviceRemoved::kEventName, std::move(args), *device);
  }

  // Remove permission entry for ephemeral hid device.
  DevicePermissionsManager* permissions_manager =
      DevicePermissionsManager::Get(browser_context_);
  DCHECK(permissions_manager);
  permissions_manager->RemoveEntryByDeviceGUID(DevicePermissionEntry::Type::HID,
                                               device->guid);
}

void HidDeviceManager::DeviceChanged(device::mojom::HidDeviceInfoPtr device) {
  // Find |device| in |devices_|.
  DCHECK(thread_checker_.CalledOnValidThread());
  const auto& resource_entry = resource_ids_.find(device->guid);
  CHECK(resource_entry != resource_ids_.end(), base::NotFatalUntil::M130);
  int resource_id = resource_entry->second;
  DCHECK(base::Contains(devices_, resource_id));

  // Update the device information.
  devices_[resource_id] = std::move(device);
}

void HidDeviceManager::LazyInitialize() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (initialized_) {
    return;
  }
  // |hid_manager_| may already be initialized in tests.
  if (!hid_manager_) {
    // |hid_manager_| is initialized and safe to use whether or not the
    // connection is successful.

    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    auto receiver = hid_manager_.BindNewPipeAndPassReceiver();
    const auto& binder = GetHidManagerBinderOverride();
    if (binder) {
      binder.Run(std::move(receiver));
    } else {
      content::GetDeviceService().BindHidManager(std::move(receiver));
    }
  }
  // Enumerate HID devices and set client.
  std::vector<device::mojom::HidDeviceInfoPtr> empty_devices;
  hid_manager_->GetDevicesAndSetClient(
      receiver_.BindNewEndpointAndPassRemote(),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&HidDeviceManager::OnEnumerationComplete,
                         weak_factory_.GetWeakPtr()),
          std::move(empty_devices)));

  initialized_ = true;
}

// static
void HidDeviceManager::OverrideHidManagerBinderForTesting(
    HidManagerBinder binder) {
  GetHidManagerBinderOverride() = std::move(binder);
}

base::Value::List HidDeviceManager::CreateApiDeviceList(
    const Extension* extension,
    const std::vector<HidDeviceFilter>& filters) {
  base::Value::List api_devices;
  for (const ResourceIdToDeviceInfoMap::value_type& map_entry : devices_) {
    int resource_id = map_entry.first;
    auto& device_info = map_entry.second;

    if (!filters.empty() &&
        !HidDeviceFilter::MatchesAny(*device_info, filters)) {
      continue;
    }

    if (!HasPermission(extension, *device_info, false)) {
      continue;
    }

    hid::HidDeviceInfo api_device_info;
    api_device_info.device_id = resource_id;
    PopulateHidDeviceInfo(&api_device_info, *device_info);

    // Expose devices with which user can communicate.
    if (api_device_info.collections.size() > 0) {
      api_devices.Append(api_device_info.ToValue());
    }
  }

  return api_devices;
}

void HidDeviceManager::OnEnumerationComplete(
    std::vector<device::mojom::HidDeviceInfoPtr> devices) {
  DCHECK(resource_ids_.empty());
  DCHECK(devices_.empty());

  for (auto& device_info : devices) {
    DeviceAdded(std::move(device_info));
  }
  enumeration_ready_ = true;

  for (const auto& params : pending_enumerations_) {
    base::Value::List devices_list =
        CreateApiDeviceList(params->extension, params->filters);
    std::move(params->callback).Run(std::move(devices_list));
  }
  pending_enumerations_.clear();
}

void HidDeviceManager::DispatchEvent(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    base::Value::List event_args,
    const device::mojom::HidDeviceInfo& device_info) {
  std::unique_ptr<Event> event(
      new Event(histogram_value, event_name, std::move(event_args)));
  // The |event->will_dispatch_callback| will be called synchronously, it is
  // safe to pass |device_info| by reference.
  event->will_dispatch_callback =
      base::BindRepeating(&WillDispatchDeviceEvent, weak_factory_.GetWeakPtr(),
                          std::cref(device_info));
  event_router_->BroadcastEvent(std::move(event));
}

}  // namespace extensions

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_HID_HID_DEVICE_MANAGER_H_
#define EXTENSIONS_BROWSER_API_HID_HID_DEVICE_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/common/api/hid.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace device {
class HidDeviceFilter;
}

namespace extensions {

class Extension;

// This class maps devices enumerated by device::HidManager to resource IDs
// returned by the chrome.hid API.
class HidDeviceManager : public BrowserContextKeyedAPI,
                         public device::mojom::HidManagerClient,
                         public EventRouter::Observer {
 public:
  using GetApiDevicesCallback = base::OnceCallback<void(base::Value::List)>;

  using ConnectCallback = device::mojom::HidManager::ConnectCallback;

  explicit HidDeviceManager(content::BrowserContext* context);

  HidDeviceManager(const HidDeviceManager&) = delete;
  HidDeviceManager& operator=(const HidDeviceManager&) = delete;

  ~HidDeviceManager() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<HidDeviceManager>* GetFactoryInstance();

  // Convenience method to get the HidDeviceManager for a profile.
  static HidDeviceManager* Get(content::BrowserContext* context) {
    return BrowserContextKeyedAPIFactory<HidDeviceManager>::Get(context);
  }

  // Enumerates available devices, taking into account the permissions held by
  // the given extension and the filters provided. The provided callback will
  // be posted to the calling thread's task runner with a list of device info
  // objects.
  void GetApiDevices(const Extension* extension,
                     const std::vector<device::HidDeviceFilter>& filters,
                     GetApiDevicesCallback callback);

  const device::mojom::HidDeviceInfo* GetDeviceInfo(int resource_id);

  void Connect(const std::string& device_guid, ConnectCallback callback);

  // Checks if |extension| has permission to open |device_info|. Set
  // |update_last_used| to update the timestamp in the DevicePermissionsManager.
  bool HasPermission(const Extension* extension,
                     const device::mojom::HidDeviceInfo& device_info,
                     bool update_last_used);

  // Lazily perform an initial enumeration and set client to HidManager when
  // the first API customer makes a request or registers an event listener.
  virtual void LazyInitialize();

  // Allows tests to override where this class binds a HidManager receiver.
  using HidManagerBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<device::mojom::HidManager>)>;
  static void OverrideHidManagerBinderForTesting(HidManagerBinder binder);

 private:
  friend class BrowserContextKeyedAPIFactory<HidDeviceManager>;

  typedef std::map<int, device::mojom::HidDeviceInfoPtr>
      ResourceIdToDeviceInfoMap;
  typedef std::map<std::string, int> DeviceIdToResourceIdMap;

  struct GetApiDevicesParams;

  // KeyedService:
  void Shutdown() override;

  // BrowserContextKeyedAPI:
  static const char* service_name() { return "HidDeviceManager"; }
  static const bool kServiceHasOwnInstanceInIncognito = true;

  // EventRouter::Observer:
  void OnListenerAdded(const EventListenerInfo& details) override;

  // device::mojom::HidManagerClient implementation:
  void DeviceAdded(device::mojom::HidDeviceInfoPtr device) override;
  void DeviceRemoved(device::mojom::HidDeviceInfoPtr device) override;
  void DeviceChanged(device::mojom::HidDeviceInfoPtr device) override;

  // Builds a list of device info objects representing the currently enumerated
  // devices, taking into account the permissions held by the given extension
  // and the filters provided.
  base::Value::List CreateApiDeviceList(
      const Extension* extension,
      const std::vector<device::HidDeviceFilter>& filters);
  void OnEnumerationComplete(
      std::vector<device::mojom::HidDeviceInfoPtr> devices);

  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     base::Value::List event_args,
                     const device::mojom::HidDeviceInfo& device_info);

  base::ThreadChecker thread_checker_;
  raw_ptr<content::BrowserContext> browser_context_ = nullptr;
  raw_ptr<EventRouter> event_router_ = nullptr;
  bool initialized_ = false;
  mojo::Remote<device::mojom::HidManager> hid_manager_;
  mojo::AssociatedReceiver<device::mojom::HidManagerClient> receiver_{this};
  bool enumeration_ready_ = false;
  std::vector<std::unique_ptr<GetApiDevicesParams>> pending_enumerations_;
  int next_resource_id_ = 0;
  ResourceIdToDeviceInfoMap devices_;
  DeviceIdToResourceIdMap resource_ids_;
  base::WeakPtrFactory<HidDeviceManager> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_HID_HID_DEVICE_MANAGER_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth/bluetooth_event_router.h"

#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "extensions/browser/api/bluetooth/bluetooth_api_pairing_delegate.h"
#include "extensions/browser/api/bluetooth/bluetooth_api_utils.h"
#include "extensions/browser/api/bluetooth/bluetooth_private_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/api/bluetooth.h"
#include "extensions/common/api/bluetooth_private.h"

namespace extensions {

namespace {

void IgnoreAdapterResult(scoped_refptr<device::BluetoothAdapter> adapter) {}

void IgnoreAdapterResultAndThen(
    base::OnceClosure callback,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  std::move(callback).Run();
}

std::string GetListenerId(const extensions::EventListenerInfo& details) {
  return !details.extension_id.empty() ? details.extension_id
                                       : details.listener_url.host();
}

}  // namespace

namespace bluetooth = api::bluetooth;
namespace bt_private = api::bluetooth_private;

BluetoothEventRouter::BluetoothEventRouter(content::BrowserContext* context)
    : browser_context_(context), adapter_(nullptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  BLUETOOTH_LOG(USER) << "BluetoothEventRouter()";
  DCHECK(browser_context_);
  registrar_.Add(this, extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                 content::Source<content::BrowserContext>(browser_context_));
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

BluetoothEventRouter::~BluetoothEventRouter() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  BLUETOOTH_LOG(USER) << "~BluetoothEventRouter()";
  if (adapter_.get()) {
    adapter_->RemoveObserver(this);
    adapter_ = nullptr;
  }
  CleanUpAllExtensions();
}

bool BluetoothEventRouter::IsBluetoothSupported() const {
  return device::BluetoothAdapterFactory::IsBluetoothSupported();
}

void BluetoothEventRouter::GetAdapter(
    device::BluetoothAdapterFactory::AdapterCallback callback) {
  if (adapter_.get()) {
    std::move(callback).Run(adapter_);
    return;
  }

  // Note: On ChromeOS this will return an adapter that also supports Bluetooth
  // Low Energy.
  device::BluetoothAdapterFactory::GetClassicAdapter(
      base::BindOnce(&BluetoothEventRouter::OnAdapterInitialized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BluetoothEventRouter::StartDiscoverySession(
    device::BluetoothAdapter* adapter,
    const std::string& extension_id,
    const base::Closure& callback,
    const base::Closure& error_callback) {
  if (!adapter_.get() && IsBluetoothSupported()) {
    // If |adapter_| isn't set yet, call GetAdapter() which will synchronously
    // invoke the callback (StartDiscoverySessionImpl).
    GetAdapter(base::BindOnce(
        &IgnoreAdapterResultAndThen,
        base::BindOnce(&BluetoothEventRouter::StartDiscoverySessionImpl,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::RetainedRef(adapter), extension_id, callback,
                       error_callback)));
    return;
  }
  StartDiscoverySessionImpl(adapter, extension_id, callback, error_callback);
}

void BluetoothEventRouter::StartDiscoverySessionImpl(
    device::BluetoothAdapter* adapter,
    const std::string& extension_id,
    const base::Closure& callback,
    const base::Closure& error_callback) {
  if (!adapter_.get()) {
    BLUETOOTH_LOG(ERROR) << "Unable to get Bluetooth adapter.";
    error_callback.Run();
    return;
  }
  if (adapter != adapter_.get()) {
    BLUETOOTH_LOG(ERROR) << "Bluetooth adapter mismatch.";
    error_callback.Run();
    return;
  }
  auto iter = discovery_session_map_.find(extension_id);
  if (iter != discovery_session_map_.end() && iter->second->IsActive()) {
    BLUETOOTH_LOG(DEBUG) << "An active discovery session exists for extension: "
                         << extension_id;
    error_callback.Run();
    return;
  }

  BLUETOOTH_LOG(USER) << "StartDiscoverySession: " << extension_id;

  // Check whether user pre set discovery filter by calling SetDiscoveryFilter
  // before. If the user has set a discovery filter then start a filtered
  // discovery session, otherwise start a regular session
  auto pre_set_iter = pre_set_filter_map_.find(extension_id);
  if (pre_set_iter != pre_set_filter_map_.end()) {
    adapter->StartDiscoverySessionWithFilter(
        std::unique_ptr<device::BluetoothDiscoveryFilter>(pre_set_iter->second),
        base::Bind(&BluetoothEventRouter::OnStartDiscoverySession,
                   weak_ptr_factory_.GetWeakPtr(), extension_id, callback),
        error_callback);
    pre_set_filter_map_.erase(pre_set_iter);
    return;
  }
  adapter->StartDiscoverySession(
      base::Bind(&BluetoothEventRouter::OnStartDiscoverySession,
                 weak_ptr_factory_.GetWeakPtr(), extension_id, callback),
      error_callback);
}

void BluetoothEventRouter::StopDiscoverySession(
    device::BluetoothAdapter* adapter,
    const std::string& extension_id,
    const base::Closure& callback,
    const base::Closure& error_callback) {
  if (adapter != adapter_.get()) {
    error_callback.Run();
    return;
  }
  auto iter = discovery_session_map_.find(extension_id);
  if (iter == discovery_session_map_.end() || !iter->second->IsActive()) {
    BLUETOOTH_LOG(DEBUG) << "No active discovery session exists for extension.";
    error_callback.Run();
    return;
  }
  BLUETOOTH_LOG(USER) << "StopDiscoverySession: " << extension_id;
  device::BluetoothDiscoverySession* session = iter->second;
  session->Stop(callback, error_callback);
}

void BluetoothEventRouter::SetDiscoveryFilter(
    std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
    device::BluetoothAdapter* adapter,
    const std::string& extension_id,
    const base::Closure& callback,
    const base::Closure& error_callback) {
  BLUETOOTH_LOG(USER) << "SetDiscoveryFilter";
  if (!adapter_.get()) {
    BLUETOOTH_LOG(ERROR) << "Unable to get Bluetooth adapter.";
    error_callback.Run();
    return;
  }
  if (adapter != adapter_.get()) {
    BLUETOOTH_LOG(ERROR) << "Bluetooth adapter mismatch.";
    error_callback.Run();
    return;
  }

  auto iter = discovery_session_map_.find(extension_id);
  if (iter == discovery_session_map_.end() || !iter->second->IsActive()) {
    BLUETOOTH_LOG(DEBUG) << "No active discovery session exists for extension, "
                         << "so caching filter for later use.";
    pre_set_filter_map_[extension_id] = discovery_filter.release();
    callback.Run();
    return;
  }

  // If the session has already started simply start a new one. The callback
  // will automatically delete the old session and put the new session (with its
  // new filter) in as this extension's session
  adapter->StartDiscoverySessionWithFilter(
      std::move(discovery_filter),
      base::Bind(&BluetoothEventRouter::OnStartDiscoverySession,
                 weak_ptr_factory_.GetWeakPtr(), extension_id, callback),
      error_callback);
}

BluetoothApiPairingDelegate* BluetoothEventRouter::GetPairingDelegate(
    const std::string& extension_id) {
  return base::Contains(pairing_delegate_map_, extension_id)
             ? pairing_delegate_map_[extension_id]
             : nullptr;
}

void BluetoothEventRouter::OnAdapterInitialized(
    device::BluetoothAdapterFactory::AdapterCallback callback,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (!adapter_.get()) {
    adapter_ = adapter;
    adapter_->AddObserver(this);
  }

  std::move(callback).Run(adapter);
}

void BluetoothEventRouter::MaybeReleaseAdapter() {
  if (adapter_.get() && event_listener_count_.empty() &&
      pairing_delegate_map_.empty()) {
    BLUETOOTH_LOG(EVENT) << "Releasing Adapter.";
    adapter_->RemoveObserver(this);
    adapter_ = nullptr;
  }
}

void BluetoothEventRouter::AddPairingDelegate(const std::string& extension_id) {
  if (!adapter_.get() && IsBluetoothSupported()) {
    GetAdapter(base::BindOnce(
        &IgnoreAdapterResultAndThen,
        base::BindOnce(&BluetoothEventRouter::AddPairingDelegateImpl,
                       weak_ptr_factory_.GetWeakPtr(), extension_id)));
    return;
  }
  AddPairingDelegateImpl(extension_id);
}

void BluetoothEventRouter::AddPairingDelegateImpl(
    const std::string& extension_id) {
  if (!adapter_.get()) {
    LOG(ERROR) << "Unable to get adapter for extension_id: " << extension_id;
    return;
  }

  if (base::Contains(pairing_delegate_map_, extension_id)) {
    // For WebUI there may be more than one page open to the same url
    // (e.g. chrome://settings). These will share the same pairing delegate.
    BLUETOOTH_LOG(EVENT) << "Pairing delegate already exists for extension_id: "
                         << extension_id;
    return;
  }
  BluetoothApiPairingDelegate* delegate =
      new BluetoothApiPairingDelegate(browser_context_);
  DCHECK(adapter_.get());
  adapter_->AddPairingDelegate(
      delegate, device::BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);
  pairing_delegate_map_[extension_id] = delegate;
}

void BluetoothEventRouter::RemovePairingDelegate(
    const std::string& extension_id) {
  if (base::Contains(pairing_delegate_map_, extension_id)) {
    BluetoothApiPairingDelegate* delegate = pairing_delegate_map_[extension_id];
    if (adapter_.get())
      adapter_->RemovePairingDelegate(delegate);
    pairing_delegate_map_.erase(extension_id);
    delete delegate;
    MaybeReleaseAdapter();
  }
}

void BluetoothEventRouter::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (adapter != adapter_.get()) {
    BLUETOOTH_LOG(DEBUG) << "Ignoring event for adapter "
                         << adapter->GetAddress();
    return;
  }
  DispatchAdapterStateEvent();
}

void BluetoothEventRouter::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool has_power) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (adapter != adapter_.get()) {
    BLUETOOTH_LOG(DEBUG) << "Ignoring event for adapter "
                         << adapter->GetAddress();
    return;
  }
  DispatchAdapterStateEvent();
}

void BluetoothEventRouter::AdapterDiscoveringChanged(
    device::BluetoothAdapter* adapter,
    bool discovering) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (adapter != adapter_.get()) {
    BLUETOOTH_LOG(DEBUG) << "Ignoring event for adapter "
                         << adapter->GetAddress();
    return;
  }

  if (!discovering) {
    // If any discovery sessions are inactive, clean them up.
    DiscoverySessionMap active_session_map;
    for (auto iter = discovery_session_map_.begin();
         iter != discovery_session_map_.end(); ++iter) {
      device::BluetoothDiscoverySession* session = iter->second;
      if (session->IsActive()) {
        active_session_map[iter->first] = session;
        continue;
      }
      delete session;
    }
    discovery_session_map_.swap(active_session_map);
  }

  DispatchAdapterStateEvent();

  // Release the adapter after dispatching the event.
  if (!discovering) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&BluetoothEventRouter::MaybeReleaseAdapter,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void BluetoothEventRouter::DeviceAdded(device::BluetoothAdapter* adapter,
                                       device::BluetoothDevice* device) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (adapter != adapter_.get()) {
    BLUETOOTH_LOG(DEBUG) << "Ignoring event for adapter "
                         << adapter->GetAddress();
    return;
  }

  DispatchDeviceEvent(events::BLUETOOTH_ON_DEVICE_ADDED,
                      bluetooth::OnDeviceAdded::kEventName, device);
}

void BluetoothEventRouter::DeviceChanged(device::BluetoothAdapter* adapter,
                                         device::BluetoothDevice* device) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (adapter != adapter_.get()) {
    BLUETOOTH_LOG(DEBUG) << "Ignoring event for adapter "
                         << adapter->GetAddress();
    return;
  }

  DispatchDeviceEvent(events::BLUETOOTH_ON_DEVICE_CHANGED,
                      bluetooth::OnDeviceChanged::kEventName, device);
}

void BluetoothEventRouter::DeviceRemoved(device::BluetoothAdapter* adapter,
                                         device::BluetoothDevice* device) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (adapter != adapter_.get()) {
    BLUETOOTH_LOG(DEBUG) << "Ignoring event for adapter "
                         << adapter->GetAddress();
    return;
  }

  DispatchDeviceEvent(events::BLUETOOTH_ON_DEVICE_REMOVED,
                      bluetooth::OnDeviceRemoved::kEventName, device);
}

void BluetoothEventRouter::OnListenerAdded(const EventListenerInfo& details) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string id = GetListenerId(details);
  int count = ++event_listener_count_[id];
  BLUETOOTH_LOG(EVENT) << "Event Listener Added: " << id << " Count: " << count;
  if (!adapter_.get())
    GetAdapter(base::BindOnce(&IgnoreAdapterResult));
}

void BluetoothEventRouter::OnListenerRemoved(const EventListenerInfo& details) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string id = GetListenerId(details);
  auto iter = event_listener_count_.find(id);
  CHECK(iter != event_listener_count_.end());
  int count = --(iter->second);
  BLUETOOTH_LOG(EVENT) << "Event Listener Removed: " << id
                       << " Count: " << count;
  if (count == 0) {
    event_listener_count_.erase(iter);
    // When all listeners for a listener id have been removed, remove any
    // pairing delegate or discovery session and filters.
    CleanUpForExtension(id);
  }
  MaybeReleaseAdapter();
}

void BluetoothEventRouter::DispatchAdapterStateEvent() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  api::bluetooth::AdapterState state;
  CHECK(adapter_.get());
  PopulateAdapterState(*adapter_, &state);

  std::unique_ptr<base::ListValue> args =
      bluetooth::OnAdapterStateChanged::Create(state);
  std::unique_ptr<Event> event(
      new Event(events::BLUETOOTH_ON_ADAPTER_STATE_CHANGED,
                bluetooth::OnAdapterStateChanged::kEventName, std::move(args)));
  EventRouter::Get(browser_context_)->BroadcastEvent(std::move(event));
}

void BluetoothEventRouter::DispatchDeviceEvent(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    device::BluetoothDevice* device) {
  bluetooth::Device extension_device;
  CHECK(device);
  bluetooth::BluetoothDeviceToApiDevice(*device, &extension_device);

  std::unique_ptr<base::ListValue> args =
      bluetooth::OnDeviceAdded::Create(extension_device);
  std::unique_ptr<Event> event(
      new Event(histogram_value, event_name, std::move(args)));
  EventRouter::Get(browser_context_)->BroadcastEvent(std::move(event));
}

void BluetoothEventRouter::CleanUpForExtension(
    const std::string& extension_id) {
  BLUETOOTH_LOG(DEBUG) << "CleanUpForExtension: " << extension_id;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RemovePairingDelegate(extension_id);

  auto pre_set_iter = pre_set_filter_map_.find(extension_id);
  if (pre_set_iter != pre_set_filter_map_.end()) {
    delete pre_set_iter->second;
    pre_set_filter_map_.erase(pre_set_iter);
  }

  // Remove any discovery session initiated by the extension.
  auto session_iter = discovery_session_map_.find(extension_id);
  if (session_iter == discovery_session_map_.end())
    return;

  // discovery_session_map.erase() should happen before
  // delete session_iter->second, because deleting the
  // BluetoothDiscoverySession object may trigger a chain reaction
  // (see http://crbug.com/711484#c9) which will modify
  // discovery_session_map_ itself.
  device::BluetoothDiscoverySession* discovery_session = session_iter->second;
  discovery_session_map_.erase(session_iter);
  delete discovery_session;
}

void BluetoothEventRouter::CleanUpAllExtensions() {
  BLUETOOTH_LOG(DEBUG) << "CleanUpAllExtensions";

  for (auto& it : pre_set_filter_map_)
    delete it.second;
  pre_set_filter_map_.clear();

  for (auto& it : discovery_session_map_) {
    BLUETOOTH_LOG(DEBUG) << "Clean up Discovery Session: " << it.first;
    delete it.second;
  }
  discovery_session_map_.clear();

  auto pairing_iter = pairing_delegate_map_.begin();
  while (pairing_iter != pairing_delegate_map_.end())
    RemovePairingDelegate(pairing_iter++->first);
}

void BluetoothEventRouter::OnStartDiscoverySession(
    const std::string& extension_id,
    const base::Closure& callback,
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  BLUETOOTH_LOG(EVENT) << "OnStartDiscoverySession: " << extension_id;
  // Clean up any existing session instance for the extension.
  auto iter = discovery_session_map_.find(extension_id);
  if (iter != discovery_session_map_.end())
    delete iter->second;
  discovery_session_map_[extension_id] = discovery_session.release();
  callback.Run();
}

void BluetoothEventRouter::OnSetDiscoveryFilter(const std::string& extension_id,
                                                const base::Closure& callback) {
  BLUETOOTH_LOG(DEBUG) << "Successfully set DiscoveryFilter.";
  callback.Run();
}

void BluetoothEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED, type);
  ExtensionHost* host = content::Details<ExtensionHost>(details).ptr();
  BLUETOOTH_LOG(DEBUG) << "Host Destroyed: " << host->extension_id();
  CleanUpForExtension(host->extension_id());
}

void BluetoothEventRouter::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  CleanUpForExtension(extension->id());
}

}  // namespace extensions

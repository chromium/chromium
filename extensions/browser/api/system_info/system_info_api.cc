// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_info/system_info_api.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/values.h"
#include "components/storage_monitor/removable_storage_observer.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/browser/api/system_info/system_info_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/api/system_display.h"
#include "extensions/common/api/system_storage.h"

namespace extensions {

using api::system_storage::StorageUnitInfo;
using content::BrowserThread;
using storage_monitor::StorageMonitor;

namespace system_display = api::system_display;
namespace system_storage = api::system_storage;

namespace {

/******************************************************************************/
// Begin SystemInfoEventRouter
/******************************************************************************/

// Event router for systemInfo API. This class is responsible for managing
// display-changed and removable-storage event dispatch. The storage event
// dispatch is handled by this class, whereas display-changed event dispatch is
// delegated to the DisplayInfoProvider. This class is a singleton instance
// shared across multiple browser contexts.
class SystemInfoEventRouter : public storage_monitor::RemovableStorageObserver {
 public:
  static SystemInfoEventRouter* GetInstance();

  SystemInfoEventRouter();

  SystemInfoEventRouter(const SystemInfoEventRouter&) = delete;
  SystemInfoEventRouter& operator=(const SystemInfoEventRouter&) = delete;

  ~SystemInfoEventRouter() override;

  // The input |context| is tracked if and only if it has at least one listener
  // for display/storage events. The sets of tracked browser contexts are used
  // to determine whether or not to dispatch display/storage events.
  void CheckForDisplayListeners(content::BrowserContext* context);
  void CheckForStorageListeners(content::BrowserContext* context);

  // Stops tracking |context| as a context with display or storage listeners.
  void ShutdownForContext(content::BrowserContext* context);

  // Start/Stop display-changed event and removable-storage event dispatchers.
  // Events should be dispatched if and only if at least one browser context has
  // the relevant listeners.
  void StartOrStopDisplayEventDispatcherIfNecessary();
  void StartOrStopStorageEventDispatcherIfNecessary();

 private:
  // RemovableStorageObserver:
  void OnRemovableStorageAttached(
      const storage_monitor::StorageInfo& info) override;
  void OnRemovableStorageDetached(
      const storage_monitor::StorageInfo& info) override;

  // Called from any thread to dispatch the systemInfo event to all extension
  // processes cross multiple profiles. Currently only used for storage events.
  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     base::Value::List args) const;

  // When true, the DisplayInfoProvider is observing for changes to the display
  // and, subsequently, dispatching on-display-changed events.
  bool is_dispatching_display_events_ = false;

  // When true, the SystemInfoEventRouter is observing for removable-storage
  // changes sent by the StorageMonitor. The SystemInfoEventRouter subsequently
  // dispatches on-attached/detached events.
  bool is_dispatching_storage_events_ = false;

  base::flat_set<raw_ptr<content::BrowserContext>>
      contexts_with_display_listeners_;
  base::flat_set<raw_ptr<content::BrowserContext>>
      contexts_with_storage_listeners_;
};

static base::LazyInstance<SystemInfoEventRouter>::Leaky
    g_system_info_event_router = LAZY_INSTANCE_INITIALIZER;

// static
SystemInfoEventRouter* SystemInfoEventRouter::GetInstance() {
  return g_system_info_event_router.Pointer();
}

SystemInfoEventRouter::SystemInfoEventRouter() = default;

SystemInfoEventRouter::~SystemInfoEventRouter() {
  StorageMonitor* storage_monitor = StorageMonitor::GetInstance();
  if (storage_monitor && is_dispatching_storage_events_)
    storage_monitor->RemoveObserver(this);
}

void SystemInfoEventRouter::CheckForDisplayListeners(
    content::BrowserContext* context) {
  if (EventRouter::Get(context)->HasEventListener(
          system_display::OnDisplayChanged::kEventName)) {
    contexts_with_display_listeners_.insert(context);
  } else {
    contexts_with_display_listeners_.erase(context);
  }

  StartOrStopDisplayEventDispatcherIfNecessary();
}

void SystemInfoEventRouter::CheckForStorageListeners(
    content::BrowserContext* context) {
  EventRouter* router = EventRouter::Get(context);
  if (router->HasEventListener(system_storage::OnAttached::kEventName) ||
      router->HasEventListener(system_storage::OnDetached::kEventName)) {
    contexts_with_storage_listeners_.insert(context);
  } else {
    contexts_with_storage_listeners_.erase(context);
  }

  StartOrStopStorageEventDispatcherIfNecessary();
}

void SystemInfoEventRouter::ShutdownForContext(
    content::BrowserContext* context) {
  contexts_with_display_listeners_.erase(context);
  StartOrStopDisplayEventDispatcherIfNecessary();

  contexts_with_storage_listeners_.erase(context);
  StartOrStopStorageEventDispatcherIfNecessary();
}

void SystemInfoEventRouter::StartOrStopDisplayEventDispatcherIfNecessary() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Events should be dispatched if and only if at least one browser context has
  // the relevant listeners.
  const bool should_dispatch = !contexts_with_display_listeners_.empty();
  DisplayInfoProvider* provider = DisplayInfoProvider::Get();

  if (!provider || (should_dispatch == is_dispatching_display_events_))
    return;

  if (should_dispatch)
    provider->StartObserving();
  else
    provider->StopObserving();

  is_dispatching_display_events_ = should_dispatch;
}

void SystemInfoEventRouter::StartOrStopStorageEventDispatcherIfNecessary() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Events should be dispatched if and only if at least one browser context has
  // the relevant listeners.
  const bool should_dispatch = !contexts_with_storage_listeners_.empty();

  if (should_dispatch == is_dispatching_storage_events_)
    return;

  DCHECK(StorageMonitor::GetInstance())
      << "Missing storage monitor. Cannot start/stop storage event "
      << "dispatchers.";

  if (!StorageMonitor::GetInstance()->IsInitialized()) {
    // Because SystemInfoEventRouter is leaky, there is no need to bind with a
    // weak pointer.
    StorageMonitor::GetInstance()->EnsureInitialized(base::BindOnce(
        &SystemInfoEventRouter::StartOrStopStorageEventDispatcherIfNecessary,
        base::Unretained(this)));
    return;
  }

  if (should_dispatch)
    StorageMonitor::GetInstance()->AddObserver(this);
  else
    StorageMonitor::GetInstance()->RemoveObserver(this);

  is_dispatching_storage_events_ = should_dispatch;
}

void SystemInfoEventRouter::OnRemovableStorageAttached(
    const storage_monitor::StorageInfo& info) {
  StorageUnitInfo unit;
  systeminfo::BuildStorageUnitInfo(info, &unit);
  base::Value::List args;
  args.Append(unit.ToValue());

  DispatchEvent(events::SYSTEM_STORAGE_ON_ATTACHED,
                system_storage::OnAttached::kEventName, std::move(args));
}

void SystemInfoEventRouter::OnRemovableStorageDetached(
    const storage_monitor::StorageInfo& info) {
  base::Value::List args;
  std::string transient_id =
      StorageMonitor::GetInstance()->GetTransientIdForDeviceId(
          info.device_id());
  args.Append(transient_id);

  DispatchEvent(events::SYSTEM_STORAGE_ON_DETACHED,
                system_storage::OnDetached::kEventName, std::move(args));
}

void SystemInfoEventRouter::DispatchEvent(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    base::Value::List args) const {
  ExtensionsBrowserClient::Get()->BroadcastEventToRenderers(
      histogram_value, event_name, std::move(args), false);
}

/******************************************************************************/
// End SystemInfoEventRouter
/******************************************************************************/

void HandleListenerAddedOrRemoved(content::BrowserContext* context,
                                  const std::string& event_name) {
  // Handle display-changed listeners.
  if (event_name == system_display::OnDisplayChanged::kEventName) {
    SystemInfoEventRouter::GetInstance()->CheckForDisplayListeners(context);
    return;
  }

  // Handle removable-storage listeners.
  if (event_name == system_storage::OnAttached::kEventName ||
      event_name == system_storage::OnDetached::kEventName) {
    SystemInfoEventRouter::GetInstance()->CheckForStorageListeners(context);
    return;
  }

  NOTREACHED_IN_MIGRATION() << "Unknown event name: " << event_name;
}

}  // namespace

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<SystemInfoAPI>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<SystemInfoAPI>*
SystemInfoAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

SystemInfoAPI::SystemInfoAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* router = EventRouter::Get(browser_context_);
  router->RegisterObserver(this, system_storage::OnAttached::kEventName);
  router->RegisterObserver(this, system_storage::OnDetached::kEventName);
  router->RegisterObserver(this, system_display::OnDisplayChanged::kEventName);

  SystemInfoEventRouter::GetInstance()->CheckForDisplayListeners(context);
  SystemInfoEventRouter::GetInstance()->CheckForStorageListeners(context);
}

SystemInfoAPI::~SystemInfoAPI() = default;

void SystemInfoAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);

  SystemInfoEventRouter::GetInstance()->ShutdownForContext(browser_context_);
}

void SystemInfoAPI::OnListenerAdded(const EventListenerInfo& details) {
  HandleListenerAddedOrRemoved(browser_context_, details.event_name);
}

void SystemInfoAPI::OnListenerRemoved(const EventListenerInfo& details) {
  HandleListenerAddedOrRemoved(browser_context_, details.event_name);
}

}  // namespace extensions

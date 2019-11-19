// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_info/system_info_api.h"

#include <stdint.h>

#include <memory>
#include <set>
#include <unordered_map>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/storage_monitor/removable_storage_observer.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/browser/api/system_storage/storage_info_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/api/system_display.h"
#include "extensions/common/api/system_storage.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"

namespace extensions {

using api::system_storage::StorageUnitInfo;
using content::BrowserThread;
using storage_monitor::StorageMonitor;

typedef std::set<const content::BrowserContext*> BrowserContextSet;

namespace system_display = api::system_display;
namespace system_storage = api::system_storage;

namespace {

bool IsDisplayChangedEvent(const std::string& event_name) {
  return event_name == system_display::OnDisplayChanged::kEventName;
}

bool IsSystemStorageEvent(const std::string& event_name) {
  return (event_name == system_storage::OnAttached::kEventName ||
          event_name == system_storage::OnDetached::kEventName);
}

// Event router for systemInfo API. It is a singleton instance shared by
// multiple profiles.
class SystemInfoEventRouter : public storage_monitor::RemovableStorageObserver {
 public:
  static SystemInfoEventRouter* GetInstance();

  SystemInfoEventRouter();
  ~SystemInfoEventRouter() override;

  // Add/remove event listener for the |event_name| event.
  void AddEventListener(const content::BrowserContext* context,
                        const std::string& event_name);
  void RemoveEventListener(const content::BrowserContext* context,
                           const std::string& event_name);

  // |context| is the pointer to BrowserContext of one SystemInfoAPI instance.
  // Remove event listeners which the SystemInfoAPI instance adds.
  void ShutdownSystemInfoAPI(const content::BrowserContext* context);

 private:
  // RemovableStorageObserver implementation.
  void OnRemovableStorageAttached(
      const storage_monitor::StorageInfo& info) override;
  void OnRemovableStorageDetached(
      const storage_monitor::StorageInfo& info) override;

  // Called from any thread to dispatch the systemInfo event to all extension
  // processes cross multiple profiles.
  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     std::unique_ptr<base::ListValue> args);

  void AddEventListenerInternal(const std::string& event_name);
  void RemoveEventListenerInternal(const std::string& event_name);

  // Maps event names to the set of BrowserContexts which have SystemInfoAPI
  // instances that listen to that event.
  std::unordered_map<std::string, BrowserContextSet> watched_events_;

  bool has_storage_monitor_observer_;

  DISALLOW_COPY_AND_ASSIGN(SystemInfoEventRouter);
};

static base::LazyInstance<SystemInfoEventRouter>::Leaky
    g_system_info_event_router = LAZY_INSTANCE_INITIALIZER;

// static
SystemInfoEventRouter* SystemInfoEventRouter::GetInstance() {
  return g_system_info_event_router.Pointer();
}

SystemInfoEventRouter::SystemInfoEventRouter()
    : has_storage_monitor_observer_(false) {
}

SystemInfoEventRouter::~SystemInfoEventRouter() {
  if (has_storage_monitor_observer_) {
    StorageMonitor* storage_monitor = StorageMonitor::GetInstance();
    if (storage_monitor)
      storage_monitor->RemoveObserver(this);
  }
}

void SystemInfoEventRouter::AddEventListener(
    const content::BrowserContext* context,
    const std::string& event_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserContextSet& context_set = watched_events_[event_name];

  // Indicate whether there has been any listener listening to the
  // |event_name| event.
  const bool not_watched_before = context_set.empty();

  context_set.insert(context);
  if (not_watched_before)
    AddEventListenerInternal(event_name);
}

void SystemInfoEventRouter::RemoveEventListener(
    const content::BrowserContext* context,
    const std::string& event_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserContextSet& context_set = watched_events_[event_name];
  if (!context_set.count(context))
    return;
  context_set.erase(context);
  if (context_set.empty())
    RemoveEventListenerInternal(event_name);
}

void SystemInfoEventRouter::ShutdownSystemInfoAPI(
    const content::BrowserContext* context) {
  for (const auto& map_iter : watched_events_)
    RemoveEventListener(context, map_iter.first);
}

void SystemInfoEventRouter::OnRemovableStorageAttached(
    const storage_monitor::StorageInfo& info) {
  StorageUnitInfo unit;
  systeminfo::BuildStorageUnitInfo(info, &unit);
  std::unique_ptr<base::ListValue> args(new base::ListValue);
  args->Append(unit.ToValue());
  DispatchEvent(events::SYSTEM_STORAGE_ON_ATTACHED,
                system_storage::OnAttached::kEventName, std::move(args));
}

void SystemInfoEventRouter::OnRemovableStorageDetached(
    const storage_monitor::StorageInfo& info) {
  std::unique_ptr<base::ListValue> args(new base::ListValue);
  std::string transient_id =
      StorageMonitor::GetInstance()->GetTransientIdForDeviceId(
          info.device_id());
  args->AppendString(transient_id);

  DispatchEvent(events::SYSTEM_STORAGE_ON_DETACHED,
                system_storage::OnDetached::kEventName, std::move(args));
}

void SystemInfoEventRouter::DispatchEvent(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> args) {
  ExtensionsBrowserClient::Get()->BroadcastEventToRenderers(
      histogram_value, event_name, std::move(args), false);
}

void SystemInfoEventRouter::AddEventListenerInternal(
    const std::string& event_name) {
  if (IsDisplayChangedEvent(event_name))
    DisplayInfoProvider::Get()->StartObserving();

  if (!has_storage_monitor_observer_ && IsSystemStorageEvent(event_name)) {
    has_storage_monitor_observer_ = true;
    DCHECK(StorageMonitor::GetInstance()->IsInitialized());
    StorageMonitor::GetInstance()->AddObserver(this);
  }
}

void SystemInfoEventRouter::RemoveEventListenerInternal(
    const std::string& event_name) {
  if (IsDisplayChangedEvent(event_name))
    DisplayInfoProvider::Get()->StopObserving();

  if (IsSystemStorageEvent(event_name)) {
    const std::string& other_event_name =
        (event_name == system_storage::OnDetached::kEventName)
            ? system_storage::OnAttached::kEventName
            : system_storage::OnDetached::kEventName;
    auto map_iter = watched_events_.find(other_event_name);
    if ((map_iter == watched_events_.end()) || (map_iter->second).empty()) {
      StorageMonitor::GetInstance()->RemoveObserver(this);
      has_storage_monitor_observer_ = false;
    }
  }
}

void AddEventListener(const content::BrowserContext* context,
                      const std::string& event_name) {
  SystemInfoEventRouter::GetInstance()->AddEventListener(context, event_name);
}

void RemoveEventListener(const content::BrowserContext* context,
                         const std::string& event_name) {
  SystemInfoEventRouter::GetInstance()->RemoveEventListener(context,
                                                            event_name);
}

void ShutdownSystemInfoAPI(const content::BrowserContext* context) {
  SystemInfoEventRouter::GetInstance()->ShutdownSystemInfoAPI(context);
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
}

SystemInfoAPI::~SystemInfoAPI() {
}

void SystemInfoAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
  ShutdownSystemInfoAPI(browser_context_);
}

void SystemInfoAPI::OnListenerAdded(const EventListenerInfo& details) {
  if (IsSystemStorageEvent(details.event_name)) {
    StorageMonitor::GetInstance()->EnsureInitialized(base::BindRepeating(
        &AddEventListener, browser_context_, details.event_name));
  } else {
    AddEventListener(browser_context_, details.event_name);
  }
}

void SystemInfoAPI::OnListenerRemoved(const EventListenerInfo& details) {
  if (IsSystemStorageEvent(details.event_name)) {
    StorageMonitor::GetInstance()->EnsureInitialized(base::BindRepeating(
        &RemoveEventListener, browser_context_, details.event_name));
  } else {
    RemoveEventListener(browser_context_, details.event_name);
  }
}

}  // namespace extensions

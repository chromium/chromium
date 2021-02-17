// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_IDLE_IDLE_MANAGER_H_
#define EXTENSIONS_BROWSER_API_IDLE_IDLE_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "ui/base/idle/idle.h"

namespace base {
class Value;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

struct IdleMonitor {
  explicit IdleMonitor(ui::IdleState initial_state);

  ui::IdleState last_state;
  int listeners;
  int threshold;
};

class IdleManager : public ExtensionRegistryObserver,
                    public EventRouter::Observer,
                    public KeyedService {
 public:
  class IdleTimeProvider {
   public:
    IdleTimeProvider() {}
    virtual ~IdleTimeProvider() {}
    virtual ui::IdleState CalculateIdleState(int idle_threshold) = 0;
    virtual int CalculateIdleTime() = 0;
    virtual bool CheckIdleStateIsLocked() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(IdleTimeProvider);
  };

  class EventDelegate {
   public:
    EventDelegate() {}
    virtual ~EventDelegate() {}
    virtual void OnStateChanged(const std::string& extension_id,
                                ui::IdleState new_state) = 0;
    virtual void RegisterObserver(EventRouter::Observer* observer) = 0;
    virtual void UnregisterObserver(EventRouter::Observer* observer) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(EventDelegate);
  };

  explicit IdleManager(content::BrowserContext* context);
  ~IdleManager() override;

  void Init();

  // KeyedService implementation.
  void Shutdown() override;

  // ExtensionRegistryObserver implementation.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  ui::IdleState QueryState(int threshold);
  void SetThreshold(const std::string& extension_id, int threshold);
  // Returns the maximum time in seconds until the screen lock automatically
  // when idle.
  // Note: Currently supported on Chrome OS only. Returns a zero duration for
  // other operating systems.
  base::TimeDelta GetAutoLockDelay() const;

  static std::unique_ptr<base::Value> CreateIdleValue(ui::IdleState idle_state);

  // Override default event class. Callee assumes ownership. Used for testing.
  void SetEventDelegateForTest(std::unique_ptr<EventDelegate> event_delegate);

  // Override default idle time calculations. Callee assumes ownership. Used
  // for testing.
  void SetIdleTimeProviderForTest(
      std::unique_ptr<IdleTimeProvider> idle_provider);

 private:
  FRIEND_TEST_ALL_PREFIXES(IdleTest, ActiveToIdle);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, ActiveToLocked);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, IdleToActive);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, IdleToLocked);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, LockedToActive);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, LockedToIdle);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, MultipleExtensions);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, ReAddListener);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, SetDetectionInterval);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, SetDetectionIntervalBeforeListener);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, SetDetectionIntervalMaximum);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, SetDetectionIntervalMinimum);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, UnloadCleanup);

  typedef std::map<const std::string, IdleMonitor> MonitorMap;

  IdleMonitor* GetMonitor(const std::string& extension_id);
  void StartPolling();
  void StopPolling();
  void UpdateIdleState();

  content::BrowserContext* const context_;

  ui::IdleState last_state_;
  MonitorMap monitors_;

  base::RepeatingTimer poll_timer_;

  std::unique_ptr<IdleTimeProvider> idle_time_provider_;
  std::unique_ptr<EventDelegate> event_delegate_;

  base::ThreadChecker thread_checker_;

  // Listen to extension unloaded notification.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(IdleManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_IDLE_IDLE_MANAGER_H_

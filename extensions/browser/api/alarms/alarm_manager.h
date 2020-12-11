// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_ALARMS_ALARM_MANAGER_H_
#define EXTENSIONS_BROWSER_API_ALARMS_ALARM_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/api/alarms.h"
#include "extensions/common/extension_id.h"

namespace base {
class Clock;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class ExtensionAlarmsSchedulingTest;

struct Alarm {
  Alarm();
  Alarm(const std::string& name,
        const api::alarms::AlarmCreateInfo& create_info,
        base::TimeDelta min_granularity,
        base::Time now);
  ~Alarm();

  std::unique_ptr<api::alarms::Alarm> js_alarm;
  // The granularity isn't exposed to the extension's javascript, but we poll at
  // least as often as the shortest alarm's granularity.  It's initialized as
  // the relative delay requested in creation, even if creation uses an absolute
  // time.  This will always be at least as large as the min_granularity
  // constructor argument.
  base::TimeDelta granularity;
  // The minimum granularity is the minimum allowed polling rate. This stops
  // alarms from polling too often.
  base::TimeDelta minimum_granularity;

 private:
  DISALLOW_COPY_AND_ASSIGN(Alarm);
};

// Manages the currently pending alarms for every extension in a profile.
// There is one manager per virtual Profile.
class AlarmManager : public BrowserContextKeyedAPI,
                     public ExtensionRegistryObserver,
                     public base::SupportsWeakPtr<AlarmManager> {
 public:
  using AlarmList = std::vector<std::unique_ptr<Alarm>>;

  class Delegate {
   public:
    virtual ~Delegate() {}
    // Called when an alarm fires.
    virtual void OnAlarm(const std::string& extension_id,
                         const Alarm& alarm) = 0;
  };

  explicit AlarmManager(content::BrowserContext* context);
  ~AlarmManager() override;

  // Override the default delegate. Callee assumes onwership. Used for testing.
  void set_delegate(Delegate* delegate) { delegate_.reset(delegate); }

  using AddAlarmCallback = base::OnceClosure;
  // Adds |alarm| for the given extension, and starts the timer. Invokes
  // |callback| when done.
  void AddAlarm(const std::string& extension_id,
                std::unique_ptr<Alarm> alarm,
                AddAlarmCallback callback);

  using GetAlarmCallback = base::OnceCallback<void(Alarm*)>;
  // Passes the alarm with the given name, or NULL if none exists, to
  // |callback|.
  void GetAlarm(const std::string& extension_id,
                const std::string& name,
                GetAlarmCallback callback);

  using GetAllAlarmsCallback = base::OnceCallback<void(const AlarmList*)>;
  // Passes the list of pending alarms for the given extension, or
  // NULL if none exist, to |callback|.
  void GetAllAlarms(const std::string& extension_id,
                    GetAllAlarmsCallback callback);

  using RemoveAlarmCallback = base::OnceCallback<void(bool)>;
  // Cancels and removes the alarm with the given name. Invokes |callback| when
  // done.
  void RemoveAlarm(const std::string& extension_id,
                   const std::string& name,
                   RemoveAlarmCallback callback);

  using RemoveAllAlarmsCallback = base::OnceClosure;
  // Cancels and removes all alarms for the given extension. Invokes |callback|
  // when done.
  void RemoveAllAlarms(const std::string& extension_id,
                       RemoveAllAlarmsCallback callback);

  // Replaces AlarmManager's clock with |clock|.
  void SetClockForTesting(base::Clock* clock);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<AlarmManager>* GetFactoryInstance();

  // Convenience method to get the AlarmManager for a content::BrowserContext.
  static AlarmManager* Get(content::BrowserContext* browser_context);

 private:
  friend void RunScheduleNextPoll(AlarmManager*);
  friend class ExtensionAlarmsSchedulingTest;
  FRIEND_TEST_ALL_PREFIXES(ExtensionAlarmsSchedulingTest, PollScheduling);
  FRIEND_TEST_ALL_PREFIXES(ExtensionAlarmsSchedulingTest,
                           ReleasedExtensionPollsInfrequently);
  FRIEND_TEST_ALL_PREFIXES(ExtensionAlarmsSchedulingTest, TimerRunning);
  FRIEND_TEST_ALL_PREFIXES(ExtensionAlarmsSchedulingTest, MinimumGranularity);
  FRIEND_TEST_ALL_PREFIXES(ExtensionAlarmsSchedulingTest,
                           DifferentMinimumGranularities);
  FRIEND_TEST_ALL_PREFIXES(ExtensionAlarmsSchedulingTest,
                           RepeatingAlarmsScheduledPredictably);
  FRIEND_TEST_ALL_PREFIXES(ExtensionAlarmsSchedulingTest,
                           PollFrequencyFromStoredAlarm);
  friend class BrowserContextKeyedAPIFactory<AlarmManager>;

  using AlarmMap = std::map<ExtensionId, AlarmList>;

  using ReadyAction = base::OnceCallback<void(const std::string&)>;
  using ReadyQueue = base::queue<ReadyAction>;
  using ReadyMap = std::map<ExtensionId, ReadyQueue>;

  // Iterator used to identify a particular alarm within the Map/List pair.
  // "Not found" is represented by <alarms_.end(), invalid_iterator>.
  typedef std::pair<AlarmMap::iterator, AlarmList::iterator> AlarmIterator;

  // Part of AddAlarm that is executed after alarms are loaded.
  void AddAlarmWhenReady(std::unique_ptr<Alarm> alarm,
                         AddAlarmCallback callback,
                         const std::string& extension_id);

  // Part of GetAlarm that is executed after alarms are loaded.
  void GetAlarmWhenReady(const std::string& name,
                         GetAlarmCallback callback,
                         const std::string& extension_id);

  // Part of GetAllAlarms that is executed after alarms are loaded.
  void GetAllAlarmsWhenReady(GetAllAlarmsCallback callback,
                             const std::string& extension_id);

  // Part of RemoveAlarm that is executed after alarms are loaded.
  void RemoveAlarmWhenReady(const std::string& name,
                            RemoveAlarmCallback callback,
                            const std::string& extension_id);

  // Part of RemoveAllAlarms that is executed after alarms are loaded.
  void RemoveAllAlarmsWhenReady(RemoveAllAlarmsCallback callback,
                                const std::string& extension_id);

  // Helper to return the iterators within the AlarmMap and AlarmList for the
  // matching alarm, or an iterator to the end of the AlarmMap if none were
  // found.
  AlarmIterator GetAlarmIterator(const std::string& extension_id,
                                 const std::string& name);

  // Helper to cancel and remove the alarm at the given iterator. The iterator
  // must be valid.
  void RemoveAlarmIterator(const AlarmIterator& iter);

  // Callback for when an alarm fires.
  void OnAlarm(AlarmIterator iter);

  // Internal helper to add an alarm and start the timer with the given delay.
  void AddAlarmImpl(const std::string& extension_id,
                    std::unique_ptr<Alarm> alarm);

  // Syncs our alarm data for the given extension to/from the state storage.
  void WriteToStorage(const std::string& extension_id);
  void ReadFromStorage(const std::string& extension_id,
                       bool is_unpacked,
                       std::unique_ptr<base::Value> value);

  // Set the timer to go off at the specified |time|, and set |next_poll_time|
  // appropriately.
  void SetNextPollTime(const base::Time& time);

  // Schedules the next poll of alarms for when the next soonest alarm runs,
  // but not more often than the minimum granularity of all alarms.
  void ScheduleNextPoll();

  // Polls the alarms, running any that have elapsed. After running them and
  // rescheduling repeating alarms, schedule the next poll.
  void PollAlarms();

  // Executes |action| for given extension, making sure that the extension's
  // alarm data has been synced from the storage.
  void RunWhenReady(const std::string& extension_id, ReadyAction action);

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              extensions::UninstallReason reason) override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "AlarmManager"; }
  static const bool kServiceHasOwnInstanceInIncognito = true;

  content::BrowserContext* const browser_context_;
  base::Clock* clock_;
  std::unique_ptr<Delegate> delegate_;

  // Listen to extension load notifications.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  // The timer for this alarm manager.
  base::OneShotTimer timer_;

  // A map of our pending alarms, per extension.
  // Invariant: None of the AlarmLists are empty.
  AlarmMap alarms_;

  // A map of actions waiting for alarm data to be synced from storage, per
  // extension.
  ReadyMap ready_actions_;

  // The previous time that alarms were run.
  base::Time last_poll_time_;

  // Next poll's time.
  base::Time next_poll_time_;

  DISALLOW_COPY_AND_ASSIGN(AlarmManager);
};

}  //  namespace extensions

#endif  // EXTENSIONS_BROWSER_API_ALARMS_ALARM_MANAGER_H_

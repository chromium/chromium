// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/alarms/alarm_manager.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "extensions/browser/api/alarms/alarms_api_constants.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/state_store.h"
#include "extensions/common/api/alarms.h"

namespace extensions {

namespace alarms = api::alarms;

namespace {

// A list of alarms that this extension has set.
const char kRegisteredAlarms[] = "alarms";
const char kAlarmGranularity[] = "granularity";

const int kSecondsPerMinute = 60;

// The minimum period between polling for alarms to run.
const base::TimeDelta kDefaultMinPollPeriod() {
  return base::TimeDelta::FromDays(1);
}

class DefaultAlarmDelegate : public AlarmManager::Delegate {
 public:
  explicit DefaultAlarmDelegate(content::BrowserContext* context)
      : browser_context_(context) {}
  ~DefaultAlarmDelegate() override {}

  void OnAlarm(const std::string& extension_id, const Alarm& alarm) override {
    std::unique_ptr<base::ListValue> args(new base::ListValue());
    args->Append(alarm.js_alarm->ToValue());
    std::unique_ptr<Event> event(new Event(
        events::ALARMS_ON_ALARM, alarms::OnAlarm::kEventName, std::move(args)));
    EventRouter::Get(browser_context_)
        ->DispatchEventToExtension(extension_id, std::move(event));
  }

 private:
  content::BrowserContext* browser_context_;
};

// Creates a TimeDelta from a delay as specified in the API.
base::TimeDelta TimeDeltaFromDelay(double delay_in_minutes) {
  return base::TimeDelta::FromMicroseconds(delay_in_minutes *
                                           base::Time::kMicrosecondsPerMinute);
}

AlarmManager::AlarmList AlarmsFromValue(const std::string extension_id,
                                        bool is_unpacked,
                                        const base::ListValue* list) {
  AlarmManager::AlarmList alarms;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    const base::DictionaryValue* alarm_dict = nullptr;
    std::unique_ptr<Alarm> alarm(new Alarm());
    if (list->GetDictionary(i, &alarm_dict) &&
        alarms::Alarm::Populate(*alarm_dict, alarm->js_alarm.get())) {
      const base::Value* time_value = nullptr;
      if (alarm_dict->Get(kAlarmGranularity, &time_value)) {
        // It's okay to ignore the failure since we have minimum granularity.
        ignore_result(
            base::GetValueAsTimeDelta(*time_value, &alarm->granularity));
      }
      alarm->minimum_granularity = base::TimeDelta::FromSecondsD(
          (is_unpacked ? alarms_api_constants::kDevDelayMinimum
                       : alarms_api_constants::kReleaseDelayMinimum) *
          kSecondsPerMinute);
      if (alarm->granularity < alarm->minimum_granularity)
        alarm->granularity = alarm->minimum_granularity;
      alarms.push_back(std::move(alarm));
    }
  }
  return alarms;
}

std::unique_ptr<base::ListValue> AlarmsToValue(
    const std::vector<std::unique_ptr<Alarm>>& alarms) {
  std::unique_ptr<base::ListValue> list(new base::ListValue());
  for (size_t i = 0; i < alarms.size(); ++i) {
    std::unique_ptr<base::DictionaryValue> alarm =
        alarms[i]->js_alarm->ToValue();
    alarm->SetKey(kAlarmGranularity,
                  base::CreateTimeDeltaValue(alarms[i]->granularity));
    list->Append(std::move(alarm));
  }
  return list;
}

}  // namespace

// AlarmManager

AlarmManager::AlarmManager(content::BrowserContext* context)
    : browser_context_(context),
      clock_(base::DefaultClock::GetInstance()),
      delegate_(new DefaultAlarmDelegate(context)) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));

  StateStore* storage = ExtensionSystem::Get(browser_context_)->state_store();
  if (storage)
    storage->RegisterKey(kRegisteredAlarms);
}

AlarmManager::~AlarmManager() {
}

void AlarmManager::AddAlarm(const std::string& extension_id,
                            std::unique_ptr<Alarm> alarm,
                            AddAlarmCallback callback) {
  RunWhenReady(extension_id,
               base::BindOnce(&AlarmManager::AddAlarmWhenReady, AsWeakPtr(),
                              std::move(alarm), std::move(callback)));
}

void AlarmManager::GetAlarm(const std::string& extension_id,
                            const std::string& name,
                            GetAlarmCallback callback) {
  RunWhenReady(extension_id,
               base::BindOnce(&AlarmManager::GetAlarmWhenReady, AsWeakPtr(),
                              name, std::move(callback)));
}

void AlarmManager::GetAllAlarms(const std::string& extension_id,
                                GetAllAlarmsCallback callback) {
  RunWhenReady(extension_id,
               base::BindOnce(&AlarmManager::GetAllAlarmsWhenReady, AsWeakPtr(),
                              std::move(callback)));
}

void AlarmManager::RemoveAlarm(const std::string& extension_id,
                               const std::string& name,
                               RemoveAlarmCallback callback) {
  RunWhenReady(extension_id,
               base::BindOnce(&AlarmManager::RemoveAlarmWhenReady, AsWeakPtr(),
                              name, std::move(callback)));
}

void AlarmManager::RemoveAllAlarms(const std::string& extension_id,
                                   RemoveAllAlarmsCallback callback) {
  RunWhenReady(extension_id,
               base::BindOnce(&AlarmManager::RemoveAllAlarmsWhenReady,
                              AsWeakPtr(), std::move(callback)));
}

void AlarmManager::AddAlarmWhenReady(std::unique_ptr<Alarm> alarm,
                                     AddAlarmCallback callback,
                                     const std::string& extension_id) {
  AddAlarmImpl(extension_id, std::move(alarm));
  WriteToStorage(extension_id);
  std::move(callback).Run();
}

void AlarmManager::GetAlarmWhenReady(const std::string& name,
                                     GetAlarmCallback callback,
                                     const std::string& extension_id) {
  AlarmIterator it = GetAlarmIterator(extension_id, name);
  std::move(callback).Run(it.first != alarms_.end() ? it.second->get()
                                                    : nullptr);
}

void AlarmManager::GetAllAlarmsWhenReady(GetAllAlarmsCallback callback,
                                         const std::string& extension_id) {
  auto list = alarms_.find(extension_id);
  std::move(callback).Run(list != alarms_.end() ? &list->second : NULL);
}

void AlarmManager::RemoveAlarmWhenReady(const std::string& name,
                                        RemoveAlarmCallback callback,
                                        const std::string& extension_id) {
  AlarmIterator it = GetAlarmIterator(extension_id, name);
  if (it.first == alarms_.end()) {
    std::move(callback).Run(false);
    return;
  }

  RemoveAlarmIterator(it);
  WriteToStorage(extension_id);
  std::move(callback).Run(true);
}

void AlarmManager::RemoveAllAlarmsWhenReady(RemoveAllAlarmsCallback callback,
                                            const std::string& extension_id) {
  auto list = alarms_.find(extension_id);
  if (list != alarms_.end()) {
    // Note: I'm using indices rather than iterators here because
    // RemoveAlarmIterator will delete the list when it becomes empty.
    for (size_t i = 0, size = list->second.size(); i < size; ++i)
      RemoveAlarmIterator(AlarmIterator(list, list->second.begin()));

    CHECK(alarms_.find(extension_id) == alarms_.end());
    WriteToStorage(extension_id);
  }
  std::move(callback).Run();
}

AlarmManager::AlarmIterator AlarmManager::GetAlarmIterator(
    const std::string& extension_id,
    const std::string& name) {
  auto list = alarms_.find(extension_id);
  if (list == alarms_.end())
    return make_pair(alarms_.end(), AlarmList::iterator());

  for (auto it = list->second.begin(); it != list->second.end(); ++it) {
    if (it->get()->js_alarm->name == name)
      return make_pair(list, it);
  }

  return make_pair(alarms_.end(), AlarmList::iterator());
}

void AlarmManager::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<AlarmManager>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

template <>
void BrowserContextKeyedAPIFactory<AlarmManager>::DeclareFactoryDependencies() {
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

// static
BrowserContextKeyedAPIFactory<AlarmManager>*
AlarmManager::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
AlarmManager* AlarmManager::Get(content::BrowserContext* browser_context) {
  return BrowserContextKeyedAPIFactory<AlarmManager>::Get(browser_context);
}

void AlarmManager::RemoveAlarmIterator(const AlarmIterator& iter) {
  AlarmList& list = iter.first->second;
  list.erase(iter.second);
  if (list.empty())
    alarms_.erase(iter.first);

  // Cancel the timer if there are no more alarms.
  // We don't need to reschedule the poll otherwise, because in
  // the worst case we would just poll one extra time.
  if (alarms_.empty()) {
    timer_.Stop();
    next_poll_time_ = base::Time();
  }
}

void AlarmManager::OnAlarm(AlarmIterator it) {
  CHECK(it.first != alarms_.end());
  Alarm& alarm = **it.second;
  std::string extension_id_copy(it.first->first);
  delegate_->OnAlarm(extension_id_copy, alarm);

  // Update our scheduled time for the next alarm.
  if (double* period_in_minutes = alarm.js_alarm->period_in_minutes.get()) {
    // Get the timer's delay in JS time (i.e., convert it from minutes to
    // milliseconds).
    double period_in_js_time = *period_in_minutes *
                               base::Time::kMicrosecondsPerMinute /
                               base::Time::kMicrosecondsPerMillisecond;
    // Find out how many periods have transpired since the alarm last went off
    // (it's possible that we missed some).
    int transpired_periods =
        (last_poll_time_.ToJsTime() - alarm.js_alarm->scheduled_time) /
        period_in_js_time;
    // Schedule the alarm for the next period that is in-line with the original
    // scheduling.
    alarm.js_alarm->scheduled_time +=
        period_in_js_time * (transpired_periods + 1);
  } else {
    RemoveAlarmIterator(it);
  }
  WriteToStorage(extension_id_copy);
}

void AlarmManager::AddAlarmImpl(const std::string& extension_id,
                                std::unique_ptr<Alarm> alarm) {
  // Override any old alarm with the same name.
  AlarmIterator old_alarm =
      GetAlarmIterator(extension_id, alarm->js_alarm->name);
  if (old_alarm.first != alarms_.end())
    RemoveAlarmIterator(old_alarm);

  base::Time alarm_time =
      base::Time::FromJsTime(alarm->js_alarm->scheduled_time);
  alarms_[extension_id].push_back(std::move(alarm));
  if (next_poll_time_.is_null() || alarm_time < next_poll_time_)
    SetNextPollTime(alarm_time);
}

void AlarmManager::WriteToStorage(const std::string& extension_id) {
  StateStore* storage = ExtensionSystem::Get(browser_context_)->state_store();
  if (!storage)
    return;

  std::unique_ptr<base::Value> alarms;
  auto list = alarms_.find(extension_id);
  if (list != alarms_.end())
    alarms = AlarmsToValue(list->second);
  else
    alarms = AlarmsToValue(AlarmList());
  storage->SetExtensionValue(extension_id, kRegisteredAlarms,
                             std::move(alarms));
}

void AlarmManager::ReadFromStorage(const std::string& extension_id,
                                   bool is_unpacked,
                                   std::unique_ptr<base::Value> value) {
  base::ListValue* list = NULL;
  if (value.get() && value->GetAsList(&list)) {
    AlarmList alarm_states = AlarmsFromValue(extension_id, is_unpacked, list);
    for (size_t i = 0; i < alarm_states.size(); ++i)
      AddAlarmImpl(extension_id, std::move(alarm_states[i]));
  }

  ReadyQueue& extension_ready_queue = ready_actions_[extension_id];
  while (!extension_ready_queue.empty()) {
    std::move(extension_ready_queue.front()).Run(extension_id);
    extension_ready_queue.pop();
  }
  ready_actions_.erase(extension_id);
}

void AlarmManager::SetNextPollTime(const base::Time& time) {
  next_poll_time_ = time;
  timer_.Start(FROM_HERE,
               std::max(base::TimeDelta::FromSeconds(0), time - clock_->Now()),
               this, &AlarmManager::PollAlarms);
}

void AlarmManager::ScheduleNextPoll() {
  // If there are no alarms, stop the timer.
  if (alarms_.empty()) {
    timer_.Stop();
    next_poll_time_ = base::Time();
    return;
  }

  // Find the soonest alarm that is scheduled to run and the smallest
  // granularity of any alarm.
  // alarms_ guarantees that none of its contained lists are empty.
  base::Time soonest_alarm_time = base::Time::FromJsTime(
      alarms_.begin()->second.begin()->get()->js_alarm->scheduled_time);
  base::TimeDelta min_granularity = kDefaultMinPollPeriod();
  for (AlarmMap::const_iterator m_it = alarms_.begin(), m_end = alarms_.end();
       m_it != m_end; ++m_it) {
    for (auto l_it = m_it->second.cbegin(); l_it != m_it->second.cend();
         ++l_it) {
      base::Time cur_alarm_time =
          base::Time::FromJsTime(l_it->get()->js_alarm->scheduled_time);
      if (cur_alarm_time < soonest_alarm_time)
        soonest_alarm_time = cur_alarm_time;
      if (l_it->get()->granularity < min_granularity)
        min_granularity = l_it->get()->granularity;
      base::TimeDelta cur_alarm_delta = cur_alarm_time - last_poll_time_;
      if (cur_alarm_delta < l_it->get()->minimum_granularity)
        cur_alarm_delta = l_it->get()->minimum_granularity;
      if (cur_alarm_delta < min_granularity)
        min_granularity = cur_alarm_delta;
    }
  }

  base::Time next_poll(last_poll_time_ + min_granularity);
  // If the next alarm is more than min_granularity in the future, wait for it.
  // Otherwise, only poll as often as min_granularity.
  // As a special case, if we've never checked for an alarm before
  // (e.g. during startup), let alarms fire asap.
  if (last_poll_time_.is_null() || next_poll < soonest_alarm_time)
    next_poll = soonest_alarm_time;

  // Schedule the poll.
  SetNextPollTime(next_poll);
}

void AlarmManager::PollAlarms() {
  last_poll_time_ = clock_->Now();

  // Run any alarms scheduled in the past. OnAlarm uses vector::erase to remove
  // elements from the AlarmList, and map::erase to remove AlarmLists from the
  // AlarmMap.
  for (auto m_it = alarms_.begin(), m_end = alarms_.end(); m_it != m_end;) {
    auto cur_extension = m_it++;

    // Iterate (a) backwards so that removing elements doesn't affect
    // upcoming iterations, and (b) with indices so that if the last
    // iteration destroys the AlarmList, I'm not about to use the end
    // iterator that the destruction invalidates.
    for (size_t i = cur_extension->second.size(); i > 0; --i) {
      auto cur_alarm = cur_extension->second.begin() + i - 1;
      if (base::Time::FromJsTime(cur_alarm->get()->js_alarm->scheduled_time) <=
          last_poll_time_) {
        OnAlarm(make_pair(cur_extension, cur_alarm));
      }
    }
  }

  ScheduleNextPoll();
}

static void RemoveAllOnUninstallCallback() {
}

void AlarmManager::RunWhenReady(const std::string& extension_id,
                                ReadyAction action) {
  auto it = ready_actions_.find(extension_id);

  if (it == ready_actions_.end())
    std::move(action).Run(extension_id);
  else
    it->second.push(std::move(action));
}

void AlarmManager::OnExtensionLoaded(content::BrowserContext* browser_context,
                                     const Extension* extension) {
  StateStore* storage = ExtensionSystem::Get(browser_context_)->state_store();
  if (storage) {
    bool is_unpacked = Manifest::IsUnpackedLocation(extension->location());
    ready_actions_.insert(ReadyMap::value_type(extension->id(), ReadyQueue()));
    storage->GetExtensionValue(
        extension->id(), kRegisteredAlarms,
        base::Bind(&AlarmManager::ReadFromStorage, AsWeakPtr(), extension->id(),
                   is_unpacked));
  }
}

void AlarmManager::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  RemoveAllAlarms(extension->id(), base::Bind(RemoveAllOnUninstallCallback));
}

// AlarmManager::Alarm

Alarm::Alarm() : js_alarm(new alarms::Alarm()) {
}

Alarm::Alarm(const std::string& name,
             const alarms::AlarmCreateInfo& create_info,
             base::TimeDelta min_granularity,
             base::Time now)
    : js_alarm(new alarms::Alarm()) {
  js_alarm->name = name;
  minimum_granularity = min_granularity;

  if (create_info.when.get()) {
    // Absolute scheduling.
    js_alarm->scheduled_time = *create_info.when;
    granularity = base::Time::FromJsTime(js_alarm->scheduled_time) - now;
  } else {
    // Relative scheduling.
    double* delay_in_minutes = create_info.delay_in_minutes.get();
    if (delay_in_minutes == NULL)
      delay_in_minutes = create_info.period_in_minutes.get();
    CHECK(delay_in_minutes != NULL)
        << "ValidateAlarmCreateInfo in alarms_api.cc should have "
        << "prevented this call.";
    base::TimeDelta delay = TimeDeltaFromDelay(*delay_in_minutes);
    js_alarm->scheduled_time = (now + delay).ToJsTime();
    granularity = delay;
  }

  if (granularity < min_granularity)
    granularity = min_granularity;

  // Check for repetition.
  if (create_info.period_in_minutes.get()) {
    js_alarm->period_in_minutes.reset(
        new double(*create_info.period_in_minutes));
  }
}

Alarm::~Alarm() {
}

}  // namespace extensions

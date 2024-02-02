// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/alarms/alarm_manager.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "extensions/browser/api/alarms/alarms_api_constants.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/state_store.h"
#include "extensions/common/api/alarms.h"
#include "extensions/common/extension_id.h"

namespace extensions {

namespace alarms = api::alarms;

namespace {

// A list of alarms that this extension has set.
const char kRegisteredAlarms[] = "alarms";
const char kAlarmGranularity[] = "granularity";

// The minimum period between polling for alarms to run.
const base::TimeDelta kDefaultMinPollPeriod() {
  return base::Days(1);
}

class DefaultAlarmDelegate : public AlarmManager::Delegate {
 public:
  explicit DefaultAlarmDelegate(content::BrowserContext* context)
      : browser_context_(context) {}
  ~DefaultAlarmDelegate() override {}

  void OnAlarm(const ExtensionId& extension_id, const Alarm& alarm) override {
    base::Value::List args;
    args.Append(alarm.js_alarm->ToValue());
    auto event = std::make_unique<Event>(events::ALARMS_ON_ALARM,
                                         alarms::OnAlarm::kEventName,
                                         std::move(args), browser_context_);
    EventRouter::Get(browser_context_)
        ->DispatchEventToExtension(extension_id, std::move(event));
  }

 private:
  raw_ptr<content::BrowserContext> browser_context_;
};

// Creates a TimeDelta from a delay as specified in the API.
base::TimeDelta TimeDeltaFromDelay(double delay_in_minutes) {
  return base::Microseconds(delay_in_minutes *
                            base::Time::kMicrosecondsPerMinute);
}

AlarmManager::AlarmList AlarmsFromValue(const ExtensionId extension_id,
                                        base::TimeDelta min_delay,
                                        const base::Value::List& list) {
  AlarmManager::AlarmList alarms;
  const int max_to_create = std::min(base::saturated_cast<int>(list.size()),
                                     AlarmManager::kMaxAlarmsPerExtension);

  for (int i = 0; i < max_to_create; ++i) {
    const base::Value& alarm_value = list[i];
    Alarm alarm;
    alarm.js_alarm = alarms::Alarm::FromValue(alarm_value);
    if (alarm.js_alarm) {
      std::optional<base::TimeDelta> delta =
          base::ValueToTimeDelta(alarm_value.GetDict().Find(kAlarmGranularity));
      if (delta) {
        alarm.granularity = *delta;
        // No else branch. It's okay to ignore the failure since we have
        // minimum granularity.
      }
      alarm.minimum_granularity = min_delay;
      if (alarm.granularity < alarm.minimum_granularity)
        alarm.granularity = alarm.minimum_granularity;
      alarms.emplace_back(std::move(alarm));
    }
  }
  return alarms;
}

base::Value::List AlarmsToValue(const AlarmManager::AlarmList& alarms) {
  base::Value::List list;
  for (const auto& item : alarms) {
    base::Value::Dict alarm = item.js_alarm->ToValue();
    alarm.Set(kAlarmGranularity, base::TimeDeltaToValue(item.granularity));
    list.Append(std::move(alarm));
  }
  return list;
}

}  // namespace

// AlarmManager

AlarmManager::AlarmManager(content::BrowserContext* context)
    : browser_context_(context),
      clock_(base::DefaultClock::GetInstance()),
      delegate_(new DefaultAlarmDelegate(context)) {
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context_));

  StateStore* storage = ExtensionSystem::Get(browser_context_)->state_store();
  if (storage)
    storage->RegisterKey(kRegisteredAlarms);
}

AlarmManager::~AlarmManager() = default;

int AlarmManager::GetCountForExtension(const ExtensionId& extension_id) const {
  auto it = alarms_.find(extension_id);
  return it == alarms_.end() ? 0 : it->second.size();
}

void AlarmManager::AddAlarm(const ExtensionId& extension_id,
                            Alarm alarm,
                            AddAlarmCallback callback) {
  RunWhenReady(extension_id,
               base::BindOnce(&AlarmManager::AddAlarmWhenReady,
                              weak_ptr_factory_.GetWeakPtr(), std::move(alarm),
                              std::move(callback)));
}

void AlarmManager::GetAlarm(const ExtensionId& extension_id,
                            const std::string& name,
                            GetAlarmCallback callback) {
  RunWhenReady(extension_id, base::BindOnce(&AlarmManager::GetAlarmWhenReady,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            name, std::move(callback)));
}

void AlarmManager::GetAllAlarms(const ExtensionId& extension_id,
                                GetAllAlarmsCallback callback) {
  RunWhenReady(
      extension_id,
      base::BindOnce(&AlarmManager::GetAllAlarmsWhenReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AlarmManager::RemoveAlarm(const ExtensionId& extension_id,
                               const std::string& name,
                               RemoveAlarmCallback callback) {
  RunWhenReady(extension_id, base::BindOnce(&AlarmManager::RemoveAlarmWhenReady,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            name, std::move(callback)));
}

void AlarmManager::RemoveAllAlarms(const ExtensionId& extension_id,
                                   RemoveAllAlarmsCallback callback) {
  RunWhenReady(
      extension_id,
      base::BindOnce(&AlarmManager::RemoveAllAlarmsWhenReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AlarmManager::AddAlarmWhenReady(Alarm alarm,
                                     AddAlarmCallback callback,
                                     const ExtensionId& extension_id) {
  AddAlarmImpl(extension_id, std::move(alarm));
  WriteToStorage(extension_id);
  std::move(callback).Run();
}

void AlarmManager::GetAlarmWhenReady(const std::string& name,
                                     GetAlarmCallback callback,
                                     const ExtensionId& extension_id) {
  AlarmIterator it = GetAlarmIterator(extension_id, name);
  std::move(callback).Run(it.first != alarms_.end() ? &*it.second : nullptr);
}

void AlarmManager::GetAllAlarmsWhenReady(GetAllAlarmsCallback callback,
                                         const ExtensionId& extension_id) {
  auto list = alarms_.find(extension_id);
  std::move(callback).Run(list != alarms_.end() ? &list->second : nullptr);
}

void AlarmManager::RemoveAlarmWhenReady(const std::string& name,
                                        RemoveAlarmCallback callback,
                                        const ExtensionId& extension_id) {
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
                                            const ExtensionId& extension_id) {
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
    const ExtensionId& extension_id,
    const std::string& name) {
  auto list = alarms_.find(extension_id);
  if (list == alarms_.end())
    return make_pair(alarms_.end(), AlarmList::iterator());

  for (auto it = list->second.begin(); it != list->second.end(); ++it) {
    if (it->js_alarm->name == name)
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
  Alarm& alarm = *it.second;
  ExtensionId extension_id_copy(it.first->first);
  delegate_->OnAlarm(extension_id_copy, alarm);

  // Update our scheduled time for the next alarm.
  if (alarm.js_alarm->period_in_minutes) {
    // Get the timer's delay in JS time (i.e., convert it from minutes to
    // milliseconds).
    double period_in_js_time = *alarm.js_alarm->period_in_minutes *
                               base::Time::kMicrosecondsPerMinute /
                               base::Time::kMicrosecondsPerMillisecond;
    // Find out how many periods have transpired since the alarm last went off
    // (it's possible that we missed some).
    int transpired_periods = (last_poll_time_.InMillisecondsFSinceUnixEpoch() -
                              alarm.js_alarm->scheduled_time) /
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

void AlarmManager::AddAlarmImpl(const ExtensionId& extension_id, Alarm alarm) {
  // Override any old alarm with the same name.
  AlarmIterator old_alarm =
      GetAlarmIterator(extension_id, alarm.js_alarm->name);
  if (old_alarm.first != alarms_.end())
    RemoveAlarmIterator(old_alarm);

  base::Time alarm_time = base::Time::FromMillisecondsSinceUnixEpoch(
      alarm.js_alarm->scheduled_time);
  alarms_[extension_id].emplace_back(std::move(alarm));
  if (next_poll_time_.is_null() || alarm_time < next_poll_time_)
    SetNextPollTime(alarm_time);
}

void AlarmManager::WriteToStorage(const ExtensionId& extension_id) {
  StateStore* storage = ExtensionSystem::Get(browser_context_)->state_store();
  if (!storage)
    return;

  base::Value alarms;
  auto list = alarms_.find(extension_id);
  if (list != alarms_.end())
    alarms = base::Value(AlarmsToValue(list->second));
  else
    alarms = base::Value(AlarmsToValue(AlarmList()));
  storage->SetExtensionValue(extension_id, kRegisteredAlarms,
                             std::move(alarms));
}

void AlarmManager::ReadFromStorage(const ExtensionId& extension_id,
                                   base::TimeDelta min_delay,
                                   std::optional<base::Value> value) {
  if (value && value->is_list()) {
    AlarmList alarm_states =
        AlarmsFromValue(extension_id, min_delay, value->GetList());
    for (auto& alarm : alarm_states)
      AddAlarmImpl(extension_id, std::move(alarm));
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
  timer_.Start(FROM_HERE, time, this, &AlarmManager::PollAlarms);
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
  base::Time soonest_alarm_time = base::Time::FromMillisecondsSinceUnixEpoch(
      alarms_.begin()->second.begin()->js_alarm->scheduled_time);
  base::TimeDelta min_granularity = kDefaultMinPollPeriod();
  for (AlarmMap::const_iterator m_it = alarms_.begin(), m_end = alarms_.end();
       m_it != m_end; ++m_it) {
    for (auto l_it = m_it->second.cbegin(); l_it != m_it->second.cend();
         ++l_it) {
      base::Time cur_alarm_time = base::Time::FromMillisecondsSinceUnixEpoch(
          l_it->js_alarm->scheduled_time);
      if (cur_alarm_time < soonest_alarm_time)
        soonest_alarm_time = cur_alarm_time;
      if (l_it->granularity < min_granularity)
        min_granularity = l_it->granularity;
      base::TimeDelta cur_alarm_delta = cur_alarm_time - last_poll_time_;
      if (cur_alarm_delta < l_it->minimum_granularity)
        cur_alarm_delta = l_it->minimum_granularity;
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
      if (base::Time::FromMillisecondsSinceUnixEpoch(
              cur_alarm->js_alarm->scheduled_time) <= last_poll_time_) {
        OnAlarm(make_pair(cur_extension, cur_alarm));
      }
    }
  }

  ScheduleNextPoll();
}

static void RemoveAllOnUninstallCallback() {
}

void AlarmManager::RunWhenReady(const ExtensionId& extension_id,
                                ReadyAction action) {
  auto it = ready_actions_.find(extension_id);

  if (it == ready_actions_.end()) {
    std::move(action).Run(extension_id);
  } else {
    it->second.push(std::move(action));
  }
}

void AlarmManager::OnExtensionLoaded(content::BrowserContext* browser_context,
                                     const Extension* extension) {
  StateStore* storage = ExtensionSystem::Get(browser_context_)->state_store();
  if (storage) {
    bool is_unpacked = Manifest::IsUnpackedLocation(extension->location());
    ready_actions_.insert(ReadyMap::value_type(extension->id(), ReadyQueue()));
    base::TimeDelta min_delay = alarms_api_constants::GetMinimumDelay(
        is_unpacked, extension->manifest_version());
    storage->GetExtensionValue(extension->id(), kRegisteredAlarms,
                               base::BindOnce(&AlarmManager::ReadFromStorage,
                                              weak_ptr_factory_.GetWeakPtr(),
                                              extension->id(), min_delay));
  }
}

void AlarmManager::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  RemoveAllAlarms(extension->id(),
                  base::BindOnce(RemoveAllOnUninstallCallback));
}

// AlarmManager::Alarm

Alarm::Alarm() : js_alarm(std::in_place) {}

Alarm::Alarm(const std::string& name,
             const alarms::AlarmCreateInfo& create_info,
             base::TimeDelta min_granularity,
             base::Time now)
    : js_alarm(std::in_place) {
  js_alarm->name = name;
  minimum_granularity = min_granularity;

  if (create_info.when) {
    // Absolute scheduling.
    js_alarm->scheduled_time = *create_info.when;
    granularity =
        base::Time::FromMillisecondsSinceUnixEpoch(js_alarm->scheduled_time) -
        now;
  } else {
    // Relative scheduling.
    CHECK(create_info.delay_in_minutes || create_info.period_in_minutes)
        << "ValidateAlarmCreateInfo in alarms_api.cc should have "
        << "validated \"create_info\".";
    const double delay_in_minutes = create_info.delay_in_minutes
                                        ? *create_info.delay_in_minutes
                                        : *create_info.period_in_minutes;
    base::TimeDelta delay = TimeDeltaFromDelay(delay_in_minutes);
    js_alarm->scheduled_time = (now + delay).InMillisecondsFSinceUnixEpoch();
    granularity = delay;
  }

  if (granularity < min_granularity)
    granularity = min_granularity;

  // Check for repetition.
  js_alarm->period_in_minutes = create_info.period_in_minutes;
}

Alarm::~Alarm() = default;

Alarm::Alarm(Alarm&&) noexcept = default;
Alarm& Alarm::operator=(Alarm&&) noexcept = default;

}  // namespace extensions

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/tracing/background_tracing_rule.h"

#include <limits>
#include <string>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/rand_util.h"
#include "base/strings/safe_sprintf.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "components/tracing/common/tracing_messages.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/tracing/trace_message_filter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

const char kConfigRuleKey[] = "rule";
const char kConfigCategoryKey[] = "category";
const char kConfigRuleTriggerNameKey[] = "trigger_name";
const char kConfigRuleTriggerDelay[] = "trigger_delay";
const char kConfigRuleTriggerChance[] = "trigger_chance";
const char kConfigRuleStopTracingOnRepeatedReactive[] =
    "stop_tracing_on_repeated_reactive";
const char kConfigRuleArgsKey[] = "args";

const char kConfigRuleHistogramNameKey[] = "histogram_name";
const char kConfigRuleHistogramValueOldKey[] = "histogram_value";
const char kConfigRuleHistogramValue1Key[] = "histogram_lower_value";
const char kConfigRuleHistogramValue2Key[] = "histogram_upper_value";
const char kConfigRuleHistogramRepeatKey[] = "histogram_repeat";

const char kConfigRuleRandomIntervalTimeoutMin[] = "timeout_min";
const char kConfigRuleRandomIntervalTimeoutMax[] = "timeout_max";

const char kConfigRuleTypeMonitorNamed[] =
    "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED";

const char kConfigRuleTypeMonitorHistogram[] =
    "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE";

const char kConfigRuleTypeTraceOnNavigationUntilTriggerOrFull[] =
    "TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL";

const char kConfigRuleTypeTraceAtRandomIntervals[] =
    "TRACE_AT_RANDOM_INTERVALS";

const char kTraceAtRandomIntervalsEventName[] =
    "ReactiveTraceAtRandomIntervals";

const int kConfigTypeNavigationTimeout = 30;
const int kReactiveTraceRandomStartTimeMin = 60;
const int kReactiveTraceRandomStartTimeMax = 120;

}  // namespace

namespace content {

BackgroundTracingRule::BackgroundTracingRule()
    : trigger_chance_(1.0),
      trigger_delay_(-1),
      stop_tracing_on_repeated_reactive_(false),
      category_preset_(BackgroundTracingConfigImpl::CATEGORY_PRESET_UNSET) {}

BackgroundTracingRule::BackgroundTracingRule(int trigger_delay)
    : trigger_chance_(1.0),
      trigger_delay_(trigger_delay),
      stop_tracing_on_repeated_reactive_(false),
      category_preset_(BackgroundTracingConfigImpl::CATEGORY_PRESET_UNSET) {}

BackgroundTracingRule::~BackgroundTracingRule() {}

bool BackgroundTracingRule::ShouldTriggerNamedEvent(
    const std::string& named_event) const {
  return false;
}

int BackgroundTracingRule::GetTraceDelay() const {
  return trigger_delay_;
}

void BackgroundTracingRule::IntoDict(base::DictionaryValue* dict) const {
  DCHECK(dict);
  if (trigger_chance_ < 1.0)
    dict->SetDouble(kConfigRuleTriggerChance, trigger_chance_);

  if (trigger_delay_ != -1)
    dict->SetInteger(kConfigRuleTriggerDelay, trigger_delay_);

  if (stop_tracing_on_repeated_reactive_) {
    dict->SetBoolean(kConfigRuleStopTracingOnRepeatedReactive,
                     stop_tracing_on_repeated_reactive_);
  }

  if (category_preset_ != BackgroundTracingConfigImpl::CATEGORY_PRESET_UNSET) {
    dict->SetString(
        kConfigCategoryKey,
        BackgroundTracingConfigImpl::CategoryPresetToString(category_preset_));
  }
}

void BackgroundTracingRule::Setup(const base::DictionaryValue* dict) {
  dict->GetDouble(kConfigRuleTriggerChance, &trigger_chance_);
  dict->GetInteger(kConfigRuleTriggerDelay, &trigger_delay_);
  dict->GetBoolean(kConfigRuleStopTracingOnRepeatedReactive,
                   &stop_tracing_on_repeated_reactive_);
}

namespace {

class NamedTriggerRule : public BackgroundTracingRule {
 private:
  NamedTriggerRule(const std::string& named_event)
      : named_event_(named_event) {}

 public:
  static std::unique_ptr<BackgroundTracingRule> Create(
      const base::DictionaryValue* dict) {
    std::string trigger_name;
    if (!dict->GetString(kConfigRuleTriggerNameKey, &trigger_name))
      return nullptr;

    return std::unique_ptr<BackgroundTracingRule>(
        new NamedTriggerRule(trigger_name));
  }

  void IntoDict(base::DictionaryValue* dict) const override {
    DCHECK(dict);
    BackgroundTracingRule::IntoDict(dict);
    dict->SetString(kConfigRuleKey, kConfigRuleTypeMonitorNamed);
    dict->SetString(kConfigRuleTriggerNameKey, named_event_.c_str());
  }

  bool ShouldTriggerNamedEvent(const std::string& named_event) const override {
    return named_event == named_event_;
  }

 private:
  std::string named_event_;
};

class HistogramRule
    : public BackgroundTracingRule,
      public BackgroundTracingManagerImpl::TraceMessageFilterObserver {
 private:
  HistogramRule(const std::string& histogram_name,
                int histogram_lower_value,
                int histogram_upper_value,
                bool repeat)
      : histogram_name_(histogram_name),
        histogram_lower_value_(histogram_lower_value),
        histogram_upper_value_(histogram_upper_value),
        repeat_(repeat),
        installed_(false) {}

 public:
  static std::unique_ptr<BackgroundTracingRule> Create(
      const base::DictionaryValue* dict) {
    std::string histogram_name;
    if (!dict->GetString(kConfigRuleHistogramNameKey, &histogram_name))
      return nullptr;

    // Optional parameter, so we don't need to check if the key exists.
    bool repeat = true;
    dict->GetBoolean(kConfigRuleHistogramRepeatKey, &repeat);

    int histogram_lower_value;
    int histogram_upper_value = std::numeric_limits<int>::max();

    if (!dict->GetInteger(kConfigRuleHistogramValue1Key,
                          &histogram_lower_value)) {
      // Check for the old naming.
      if (!dict->GetInteger(kConfigRuleHistogramValueOldKey,
                            &histogram_lower_value))
        return nullptr;
    }

    dict->GetInteger(kConfigRuleHistogramValue2Key, &histogram_upper_value);

    if (histogram_lower_value >= histogram_upper_value)
      return nullptr;

    std::unique_ptr<BackgroundTracingRule> rule(new HistogramRule(
        histogram_name, histogram_lower_value, histogram_upper_value, repeat));

    const base::DictionaryValue* args_dict = nullptr;
    if (dict->GetDictionary(kConfigRuleArgsKey, &args_dict))
      rule->SetArgs(*args_dict);
    return rule;
  }

  ~HistogramRule() override {
    base::StatisticsRecorder::ClearCallback(histogram_name_);
    if (installed_) {
      BackgroundTracingManagerImpl::GetInstance()
          ->RemoveTraceMessageFilterObserver(this);
    }
  }

  // BackgroundTracingRule implementation
  void Install() override {
    base::StatisticsRecorder::SetCallback(
        histogram_name_,
        base::Bind(&HistogramRule::OnHistogramChangedCallback,
                   base::Unretained(this), histogram_name_,
                   histogram_lower_value_, histogram_upper_value_, repeat_));

    BackgroundTracingManagerImpl::GetInstance()->AddTraceMessageFilterObserver(
        this);
    installed_ = true;
  }

  void IntoDict(base::DictionaryValue* dict) const override {
    DCHECK(dict);
    BackgroundTracingRule::IntoDict(dict);
    dict->SetString(kConfigRuleKey, kConfigRuleTypeMonitorHistogram);
    dict->SetString(kConfigRuleHistogramNameKey, histogram_name_.c_str());
    dict->SetInteger(kConfigRuleHistogramValue1Key, histogram_lower_value_);
    dict->SetInteger(kConfigRuleHistogramValue2Key, histogram_upper_value_);
    dict->SetBoolean(kConfigRuleHistogramRepeatKey, repeat_);
  }

  void OnHistogramTrigger(const std::string& histogram_name) const override {
    if (histogram_name != histogram_name_)
      return;

    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(
            &BackgroundTracingManagerImpl::OnRuleTriggered,
            base::Unretained(BackgroundTracingManagerImpl::GetInstance()), this,
            BackgroundTracingManager::StartedFinalizingCallback()));
  }

  void AbortTracing() {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(
            &BackgroundTracingManagerImpl::AbortScenario,
            base::Unretained(BackgroundTracingManagerImpl::GetInstance())));
  }

  // BackgroundTracingManagerImpl::TraceMessageFilterObserver implementation
  void OnTraceMessageFilterAdded(TraceMessageFilter* filter) override {
    filter->Send(
        new TracingMsg_SetUMACallback(histogram_name_, histogram_lower_value_,
                                      histogram_upper_value_, repeat_));
  }

  void OnTraceMessageFilterRemoved(TraceMessageFilter* filter) override {
    filter->Send(new TracingMsg_ClearUMACallback(histogram_name_));
  }

  void OnHistogramChangedCallback(const std::string& histogram_name,
                                  base::Histogram::Sample reference_lower_value,
                                  base::Histogram::Sample reference_upper_value,
                                  bool repeat,
                                  base::Histogram::Sample actual_value) {
    if (reference_lower_value > actual_value ||
        reference_upper_value < actual_value) {
      if (!repeat)
        AbortTracing();
      return;
    }

    // Add the histogram name and its corresponding value to the trace.
    TRACE_EVENT_INSTANT2("toplevel",
                         "BackgroundTracingRule::OnHistogramTrigger",
                         TRACE_EVENT_SCOPE_THREAD, "histogram_name",
                         histogram_name, "value", actual_value);

    OnHistogramTrigger(histogram_name);
  }

  bool ShouldTriggerNamedEvent(const std::string& named_event) const override {
    return named_event == histogram_name_;
  }

 private:
  std::string histogram_name_;
  int histogram_lower_value_;
  int histogram_upper_value_;
  bool repeat_;
  bool installed_;
};

class TraceForNSOrTriggerOrFullRule : public BackgroundTracingRule {
 private:
  TraceForNSOrTriggerOrFullRule(const std::string& named_event)
      : BackgroundTracingRule(kConfigTypeNavigationTimeout),
        named_event_(named_event) {}

 public:
  static std::unique_ptr<BackgroundTracingRule> Create(
      const base::DictionaryValue* dict) {
    std::string trigger_name;
    if (!dict->GetString(kConfigRuleTriggerNameKey, &trigger_name))
      return nullptr;

    return std::unique_ptr<BackgroundTracingRule>(
        new TraceForNSOrTriggerOrFullRule(trigger_name));
  }

  // BackgroundTracingRule implementation
  void IntoDict(base::DictionaryValue* dict) const override {
    DCHECK(dict);
    BackgroundTracingRule::IntoDict(dict);
    dict->SetString(kConfigRuleKey,
                    kConfigRuleTypeTraceOnNavigationUntilTriggerOrFull);
    dict->SetString(kConfigRuleTriggerNameKey, named_event_.c_str());
  }

  bool ShouldTriggerNamedEvent(const std::string& named_event) const override {
    return named_event == named_event_;
  }

 private:
  std::string named_event_;
};

class TraceAtRandomIntervalsRule : public BackgroundTracingRule {
 private:
  TraceAtRandomIntervalsRule(int timeout_min, int timeout_max)
      : timeout_min_(timeout_min), timeout_max_(timeout_max) {
    named_event_ = GenerateUniqueName();
  }

 public:
  static std::unique_ptr<BackgroundTracingRule> Create(
      const base::DictionaryValue* dict) {
    int timeout_min;
    if (!dict->GetInteger(kConfigRuleRandomIntervalTimeoutMin, &timeout_min))
      return nullptr;

    int timeout_max;
    if (!dict->GetInteger(kConfigRuleRandomIntervalTimeoutMax, &timeout_max))
      return nullptr;

    if (timeout_min > timeout_max)
      return nullptr;

    return std::unique_ptr<BackgroundTracingRule>(
        new TraceAtRandomIntervalsRule(timeout_min, timeout_max));
  }
  ~TraceAtRandomIntervalsRule() override {}

  void IntoDict(base::DictionaryValue* dict) const override {
    DCHECK(dict);
    BackgroundTracingRule::IntoDict(dict);
    dict->SetString(kConfigRuleKey, kConfigRuleTypeTraceAtRandomIntervals);
    dict->SetInteger(kConfigRuleRandomIntervalTimeoutMin, timeout_min_);
    dict->SetInteger(kConfigRuleRandomIntervalTimeoutMax, timeout_max_);
  }

  void Install() override {
    handle_ = BackgroundTracingManagerImpl::GetInstance()->RegisterTriggerType(
        named_event_.c_str());

    StartTimer();
  }

  void OnStartedFinalizing(bool success) {
    if (!success)
      return;

    StartTimer();
  }

  void OnTriggerTimer() {
    BackgroundTracingManagerImpl::GetInstance()->TriggerNamedEvent(
        handle_, base::Bind(&TraceAtRandomIntervalsRule::OnStartedFinalizing,
                            base::Unretained(this)));
  }

  void StartTimer() {
    int time_to_wait = base::RandInt(kReactiveTraceRandomStartTimeMin,
                                     kReactiveTraceRandomStartTimeMax);
    trigger_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(time_to_wait),
        base::BindOnce(&TraceAtRandomIntervalsRule::OnTriggerTimer,
                       base::Unretained(this)));
  }

  int GetTraceDelay() const override {
    return base::RandInt(timeout_min_, timeout_max_);
  }

  bool ShouldTriggerNamedEvent(const std::string& named_event) const override {
    return named_event == named_event_;
  }

  std::string GenerateUniqueName() const {
    static int ids = 0;
    char work_buffer[256];
    base::strings::SafeSNPrintf(work_buffer, sizeof(work_buffer), "%s_%d",
                                kTraceAtRandomIntervalsEventName, ids++);
    return work_buffer;
  }

 private:
  std::string named_event_;
  base::OneShotTimer trigger_timer_;
  BackgroundTracingManagerImpl::TriggerHandle handle_;
  int timeout_min_;
  int timeout_max_;
};

}  // namespace

std::unique_ptr<BackgroundTracingRule>
BackgroundTracingRule::CreateRuleFromDict(const base::DictionaryValue* dict) {
  DCHECK(dict);

  std::string type;
  if (!dict->GetString(kConfigRuleKey, &type))
    return nullptr;

  std::unique_ptr<BackgroundTracingRule> tracing_rule;
  if (type == kConfigRuleTypeMonitorNamed)
    tracing_rule = NamedTriggerRule::Create(dict);
  else if (type == kConfigRuleTypeMonitorHistogram)
    tracing_rule = HistogramRule::Create(dict);
  else if (type == kConfigRuleTypeTraceOnNavigationUntilTriggerOrFull) {
    tracing_rule = TraceForNSOrTriggerOrFullRule::Create(dict);
  } else if (type == kConfigRuleTypeTraceAtRandomIntervals) {
    tracing_rule = TraceAtRandomIntervalsRule::Create(dict);
  }

  if (tracing_rule)
    tracing_rule->Setup(dict);

  return tracing_rule;
}

}  // namespace content

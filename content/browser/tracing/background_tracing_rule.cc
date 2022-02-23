// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/tracing/background_tracing_rule.h"

#include <limits>
#include <string>
#include <type_traits>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/statistics_recorder.h"
#include "base/rand_util.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/strcat.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/values.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "services/tracing/public/mojom/background_tracing_agent.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_histogram_sample.pbzero.h"

namespace {

const char kConfigRuleKey[] = "rule";
const char kConfigCategoryKey[] = "category";
const char kConfigRuleTriggerNameKey[] = "trigger_name";
const char kConfigRuleTriggerDelay[] = "trigger_delay";
const char kConfigRuleTriggerChance[] = "trigger_chance";
const char kConfigRuleStopTracingOnRepeatedReactive[] =
    "stop_tracing_on_repeated_reactive";
const char kConfigRuleArgsKey[] = "args";
const char kConfigRuleIdKey[] = "rule_id";
const char kConfigIsCrashKey[] = "is_crash";

const char kConfigRuleHistogramNameKey[] = "histogram_name";
const char kConfigRuleHistogramValueOldKey[] = "histogram_value";
const char kConfigRuleHistogramValue1Key[] = "histogram_lower_value";
const char kConfigRuleHistogramValue2Key[] = "histogram_upper_value";
const char kConfigRuleHistogramRepeatKey[] = "histogram_repeat";
const char kConfigRuleHistogramUnitsKey[] = "histogram_units";

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

BackgroundTracingRule::BackgroundTracingRule() = default;
BackgroundTracingRule::BackgroundTracingRule(int trigger_delay)
    : trigger_delay_(trigger_delay) {}

BackgroundTracingRule::~BackgroundTracingRule() = default;

bool BackgroundTracingRule::ShouldTriggerNamedEvent(
    const std::string& named_event) const {
  return false;
}

int BackgroundTracingRule::GetTraceDelay() const {
  return trigger_delay_;
}

std::string BackgroundTracingRule::GetDefaultRuleId() const {
  return "org.chromium.background_tracing.trigger";
}

base::Value BackgroundTracingRule::ToDict() const {
  base::Value dict(base::Value::Type::DICTIONARY);

  if (trigger_chance_ < 1.0)
    dict.SetDoubleKey(kConfigRuleTriggerChance, trigger_chance_);

  if (trigger_delay_ != -1)
    dict.SetIntKey(kConfigRuleTriggerDelay, trigger_delay_);

  if (stop_tracing_on_repeated_reactive_) {
    dict.SetBoolKey(kConfigRuleStopTracingOnRepeatedReactive,
                    stop_tracing_on_repeated_reactive_);
  }
  if (rule_id_ != GetDefaultRuleId()) {
    dict.SetStringKey(kConfigRuleIdKey, rule_id_);
  }

  if (category_preset_ != BackgroundTracingConfigImpl::CATEGORY_PRESET_UNSET) {
    dict.SetStringKey(
        kConfigCategoryKey,
        BackgroundTracingConfigImpl::CategoryPresetToString(category_preset_));
  }

  if (is_crash_) {
    dict.SetBoolKey(kConfigIsCrashKey, is_crash_);
  }

  return dict;
}

void BackgroundTracingRule::GenerateMetadataProto(
    BackgroundTracingRule::MetadataProto* out) const {}

void BackgroundTracingRule::Setup(const base::Value& dict) {
  if (auto trigger_chance = dict.FindDoubleKey(kConfigRuleTriggerChance)) {
    trigger_chance_ = *trigger_chance;
  }
  if (auto trigger_delay = dict.FindIntKey(kConfigRuleTriggerDelay)) {
    trigger_delay_ = *trigger_delay;
  }
  if (auto stop_tracing_on_repeated_reactive =
          dict.FindBoolKey(kConfigRuleStopTracingOnRepeatedReactive)) {
    stop_tracing_on_repeated_reactive_ = *stop_tracing_on_repeated_reactive;
  }
  if (const std::string* rule_id = dict.FindStringKey(kConfigRuleIdKey)) {
    rule_id_ = *rule_id;
  } else {
    rule_id_ = GetDefaultRuleId();
  }
  if (auto is_crash = dict.FindBoolKey(kConfigIsCrashKey)) {
    is_crash_ = *is_crash;
  }
}

namespace {

class NamedTriggerRule : public BackgroundTracingRule {
 private:
  explicit NamedTriggerRule(const std::string& named_event)
      : named_event_(named_event) {}

 public:
  static std::unique_ptr<BackgroundTracingRule> Create(
      const base::Value& dict) {
    if (const std::string* trigger_name =
            dict.FindStringKey(kConfigRuleTriggerNameKey)) {
      return base::WrapUnique<BackgroundTracingRule>(
          new NamedTriggerRule(*trigger_name));
    }
    return nullptr;
  }

  base::Value ToDict() const override {
    base::Value dict = BackgroundTracingRule::ToDict();
    DCHECK(dict.is_dict());
    dict.SetStringKey(kConfigRuleKey, kConfigRuleTypeMonitorNamed);
    dict.SetStringKey(kConfigRuleTriggerNameKey, named_event_.c_str());
    return dict;
  }

  void GenerateMetadataProto(
      BackgroundTracingRule::MetadataProto* out) const override {
    DCHECK(out);
    BackgroundTracingRule::GenerateMetadataProto(out);
    out->set_trigger_type(MetadataProto::MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED);
    auto* named_rule = out->set_named_rule();
    if (named_event_ == "startup-config") {
      named_rule->set_event_type(MetadataProto::NamedRule::STARTUP);
    } else if (named_event_ == "navigation-config") {
      named_rule->set_event_type(MetadataProto::NamedRule::NAVIGATION);
    } else if (named_event_ == "session-restore-config") {
      named_rule->set_event_type(MetadataProto::NamedRule::SESSION_RESTORE);
    } else if (named_event_ == "reached-code-config") {
      named_rule->set_event_type(MetadataProto::NamedRule::REACHED_CODE);
    } else if (named_event_ ==
               BackgroundTracingManager::kContentTriggerConfig) {
      named_rule->set_event_type(MetadataProto::NamedRule::CONTENT_TRIGGER);
      // TODO(chrisha): Set the |content_trigger_name_hash|.
    } else if (named_event_ == "preemptive_test") {
      named_rule->set_event_type(MetadataProto::NamedRule::TEST_RULE);
    }
  }

  bool ShouldTriggerNamedEvent(const std::string& named_event) const override {
    return named_event == named_event_;
  }

 protected:
  std::string GetDefaultRuleId() const override {
    return base::StrCat({"org.chromium.background_tracing.", named_event_});
  }

 private:
  std::string named_event_;
};

class HistogramRule : public BackgroundTracingRule,
                      public BackgroundTracingManagerImpl::AgentObserver {
 private:
  // Units that can be displayed specially in OnHistogramChangedCallback.
  enum class Units : int {
    kUnspecified = 0,
    kMilliseconds,
    kMicroseconds,
  };

  static Units IntToUnits(int units_value) {
    static_assert(std::is_same<std::underlying_type_t<Units>,
                               decltype(units_value)>::value,
                  "not safe to cast units_value to Units");
    Units units = static_cast<Units>(units_value);
    switch (units) {
      case Units::kUnspecified:
      case Units::kMilliseconds:
      case Units::kMicroseconds:
        // Recognized enum value.
        return units;
    }
    // Unrecognized enum value.
    return Units::kUnspecified;
  }

  HistogramRule(const std::string& histogram_name,
                int histogram_lower_value,
                int histogram_upper_value,
                Units units,
                bool repeat)
      : histogram_name_(histogram_name),
        histogram_lower_value_(histogram_lower_value),
        histogram_upper_value_(histogram_upper_value),
        units_(units),
        repeat_(repeat),
        installed_(false) {}

 public:
  static std::unique_ptr<BackgroundTracingRule> Create(
      const base::Value& dict) {
    const std::string* histogram_name =
        dict.FindStringKey(kConfigRuleHistogramNameKey);
    if (!histogram_name)
      return nullptr;

    // Optional parameter, so we don't need to check if the key exists.
    bool repeat =
        dict.FindBoolKey(kConfigRuleHistogramRepeatKey).value_or(true);

    absl::optional<int> histogram_lower_value =
        dict.FindIntKey(kConfigRuleHistogramValue1Key);
    if (!histogram_lower_value) {
      // Check for the old naming.
      histogram_lower_value = dict.FindIntKey(kConfigRuleHistogramValueOldKey);
      if (!histogram_lower_value)
        return nullptr;
    }

    int histogram_upper_value = dict.FindIntKey(kConfigRuleHistogramValue2Key)
                                    .value_or(std::numeric_limits<int>::max());

    if (*histogram_lower_value > histogram_upper_value)
      return nullptr;

    Units units = Units::kUnspecified;
    if (auto units_value = dict.FindIntKey(kConfigRuleHistogramUnitsKey)) {
      units = IntToUnits(*units_value);
    }
    std::unique_ptr<BackgroundTracingRule> rule(
        new HistogramRule(*histogram_name, *histogram_lower_value,
                          histogram_upper_value, units, repeat));

    const base::Value* args_dict = dict.FindDictKey(kConfigRuleArgsKey);
    if (args_dict)
      rule->SetArgs(*args_dict);
    return rule;
  }

  ~HistogramRule() override {
    if (installed_) {
      BackgroundTracingManagerImpl::GetInstance()->RemoveAgentObserver(this);
    }
  }

  // BackgroundTracingRule implementation
  void Install() override {
    histogram_sample_callback_ = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        histogram_name_,
        base::BindRepeating(&HistogramRule::OnHistogramChangedCallback,
                            base::Unretained(this), histogram_lower_value_,
                            histogram_upper_value_, units_, repeat_));
    BackgroundTracingManagerImpl::GetInstance()->AddAgentObserver(this);
    installed_ = true;
  }

  base::Value ToDict() const override {
    base::Value dict = BackgroundTracingRule::ToDict();
    DCHECK(dict.is_dict());
    dict.SetStringKey(kConfigRuleKey, kConfigRuleTypeMonitorHistogram);
    dict.SetStringKey(kConfigRuleHistogramNameKey, histogram_name_.c_str());
    dict.SetIntKey(kConfigRuleHistogramValue1Key, histogram_lower_value_);
    dict.SetIntKey(kConfigRuleHistogramValue2Key, histogram_upper_value_);
    if (units_ != Units::kUnspecified)
      dict.SetIntKey(kConfigRuleHistogramUnitsKey, static_cast<int>(units_));
    dict.SetBoolKey(kConfigRuleHistogramRepeatKey, repeat_);
    return dict;
  }

  void GenerateMetadataProto(
      BackgroundTracingRule::MetadataProto* out) const override {
    DCHECK(out);
    BackgroundTracingRule::GenerateMetadataProto(out);
    out->set_trigger_type(
        MetadataProto::MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE);
    auto* rule = out->set_histogram_rule();
    rule->set_histogram_name_hash(base::HashMetricName(histogram_name_));
    rule->set_histogram_min_trigger(histogram_lower_value_);
    rule->set_histogram_max_trigger(histogram_upper_value_);
  }

  void OnHistogramTrigger(const std::string& histogram_name) const override {
    if (histogram_name != histogram_name_)
      return;

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BackgroundTracingManagerImpl::OnRuleTriggered,
            base::Unretained(BackgroundTracingManagerImpl::GetInstance()), this,
            BackgroundTracingManager::StartedFinalizingCallback()));
  }

  void AbortTracing() {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BackgroundTracingManagerImpl::AbortScenario,
            base::Unretained(BackgroundTracingManagerImpl::GetInstance())));
  }

  // BackgroundTracingManagerImpl::AgentObserver implementation
  void OnAgentAdded(tracing::mojom::BackgroundTracingAgent* agent) override {
    agent->SetUMACallback(histogram_name_, histogram_lower_value_,
                          histogram_upper_value_, repeat_);
  }

  void OnAgentRemoved(tracing::mojom::BackgroundTracingAgent* agent) override {
    agent->ClearUMACallback(histogram_name_);
  }

  void OnHistogramChangedCallback(base::Histogram::Sample reference_lower_value,
                                  base::Histogram::Sample reference_upper_value,
                                  Units units,
                                  bool repeat,
                                  const char* histogram_name,
                                  uint64_t name_hash,
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
    const auto trace_details = [&](perfetto::EventContext ctx) {
      perfetto::protos::pbzero::ChromeHistogramSample* new_sample =
          ctx.event()->set_chrome_histogram_sample();
      new_sample->set_name_hash(base::HashMetricName(histogram_name));
      new_sample->set_sample(actual_value);
    };
    const auto track =
        perfetto::Track::FromPointer(this, perfetto::ProcessTrack::Current());
    const auto now = base::TimeTicks::Now();
    if (units == Units::kUnspecified) {
      TRACE_EVENT_INSTANT("toplevel", "HistogramSampleTrigger", track, now,
                          trace_details);
    } else {
      base::TimeDelta delta;
      switch (units) {
        case Units::kUnspecified:
          NOTREACHED();  // Handled above.
          break;
        case Units::kMilliseconds:
          delta = base::Milliseconds(actual_value);
          break;
        case Units::kMicroseconds:
          delta = base::Microseconds(actual_value);
          break;
      }
      TRACE_EVENT_BEGIN("toplevel", "HistogramSampleTrigger", track,
                        now - delta, trace_details);
      TRACE_EVENT_END("toplevel", track, now);
    }

    OnHistogramTrigger(histogram_name);
  }

  bool ShouldTriggerNamedEvent(const std::string& named_event) const override {
    return named_event == histogram_name_;
  }

 protected:
  std::string GetDefaultRuleId() const override {
    return base::StrCat({"org.chromium.background_tracing.", histogram_name_});
  }

 private:
  std::string histogram_name_;
  int histogram_lower_value_;
  int histogram_upper_value_;
  Units units_;
  bool repeat_;
  bool installed_;
  std::unique_ptr<base::StatisticsRecorder::ScopedHistogramSampleObserver>
      histogram_sample_callback_;
};

class TraceForNSOrTriggerOrFullRule : public BackgroundTracingRule {
 private:
  explicit TraceForNSOrTriggerOrFullRule(const std::string& named_event)
      : BackgroundTracingRule(kConfigTypeNavigationTimeout),
        named_event_(named_event) {}

 public:
  static std::unique_ptr<BackgroundTracingRule> Create(
      const base::Value& dict) {
    if (const std::string* trigger_name =
            dict.FindStringKey(kConfigRuleTriggerNameKey)) {
      return base::WrapUnique<BackgroundTracingRule>(
          new TraceForNSOrTriggerOrFullRule(*trigger_name));
    }
    return nullptr;
  }

  // BackgroundTracingRule implementation
  base::Value ToDict() const override {
    base::Value dict = BackgroundTracingRule::ToDict();
    DCHECK(dict.is_dict());
    dict.SetStringKey(kConfigRuleKey,
                      kConfigRuleTypeTraceOnNavigationUntilTriggerOrFull);
    dict.SetStringKey(kConfigRuleTriggerNameKey, named_event_.c_str());
    return dict;
  }

  void GenerateMetadataProto(
      BackgroundTracingRule::MetadataProto* out) const override {
    DCHECK(out);
    BackgroundTracingRule::GenerateMetadataProto(out);
    out->set_trigger_type(MetadataProto::MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED);
    out->set_named_rule()->set_event_type(MetadataProto::NamedRule::NAVIGATION);
  }

  bool ShouldTriggerNamedEvent(const std::string& named_event) const override {
    return named_event == named_event_;
  }

 protected:
  std::string GetDefaultRuleId() const override {
    return base::StrCat({"org.chromium.background_tracing.", named_event_});
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
      const base::Value& dict) {
    absl::optional<int> timeout_min =
        dict.FindIntKey(kConfigRuleRandomIntervalTimeoutMin);
    if (!timeout_min)
      return nullptr;

    absl::optional<int> timeout_max =
        dict.FindIntKey(kConfigRuleRandomIntervalTimeoutMax);
    if (!timeout_max)
      return nullptr;

    if (*timeout_min > *timeout_max)
      return nullptr;

    return std::unique_ptr<BackgroundTracingRule>(
        new TraceAtRandomIntervalsRule(*timeout_min, *timeout_max));
  }
  ~TraceAtRandomIntervalsRule() override {}

  base::Value ToDict() const override {
    base::Value dict = BackgroundTracingRule::ToDict();
    DCHECK(dict.is_dict());
    dict.SetStringKey(kConfigRuleKey, kConfigRuleTypeTraceAtRandomIntervals);
    dict.SetIntKey(kConfigRuleRandomIntervalTimeoutMin, timeout_min_);
    dict.SetIntKey(kConfigRuleRandomIntervalTimeoutMax, timeout_max_);
    return dict;
  }

  void GenerateMetadataProto(
      BackgroundTracingRule::MetadataProto* out) const override {
    // TODO(ssid): Add config if we enabled this  type of trigger.
    NOTREACHED();
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
        handle_,
        base::BindOnce(&TraceAtRandomIntervalsRule::OnStartedFinalizing,
                       base::Unretained(this)));
  }

  void StartTimer() {
    int time_to_wait = base::RandInt(kReactiveTraceRandomStartTimeMin,
                                     kReactiveTraceRandomStartTimeMax);
    trigger_timer_.Start(
        FROM_HERE, base::Seconds(time_to_wait),
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
BackgroundTracingRule::CreateRuleFromDict(const base::Value& dict) {
  DCHECK(dict.is_dict());

  const std::string* type = dict.FindStringKey(kConfigRuleKey);
  if (!type)
    return nullptr;

  std::unique_ptr<BackgroundTracingRule> tracing_rule;
  if (*type == kConfigRuleTypeMonitorNamed) {
    tracing_rule = NamedTriggerRule::Create(dict);
  } else if (*type == kConfigRuleTypeMonitorHistogram) {
    tracing_rule = HistogramRule::Create(dict);
  } else if (*type == kConfigRuleTypeTraceOnNavigationUntilTriggerOrFull) {
    tracing_rule = TraceForNSOrTriggerOrFullRule::Create(dict);
  } else if (*type == kConfigRuleTypeTraceAtRandomIntervals) {
    tracing_rule = TraceAtRandomIntervalsRule::Create(dict);
  }

  if (tracing_rule)
    tracing_rule->Setup(dict);

  return tracing_rule;
}

}  // namespace content

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/tracing/background_tracing_rule.h"

#include <limits>
#include <string>
#include <type_traits>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/statistics_recorder.h"
#include "base/rand_util.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
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
const char kConfigRuleTriggerNameKey[] = "trigger_name";
const char kConfigRuleTriggerDelay[] = "trigger_delay";
const char kConfigRuleTriggerChance[] = "trigger_chance";
const char kConfigRuleIdKey[] = "rule_id";
const char kConfigIsCrashKey[] = "is_crash";

const char kConfigRuleHistogramNameKey[] = "histogram_name";
const char kConfigRuleHistogramValueOldKey[] = "histogram_value";
const char kConfigRuleHistogramValue1Key[] = "histogram_lower_value";
const char kConfigRuleHistogramValue2Key[] = "histogram_upper_value";

const char kConfigRuleTypeMonitorNamed[] =
    "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED";

const char kConfigRuleTypeMonitorHistogram[] =
    "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE";

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

base::Value::Dict BackgroundTracingRule::ToDict() const {
  base::Value::Dict dict;

  if (trigger_chance_ < 1.0)
    dict.Set(kConfigRuleTriggerChance, trigger_chance_);

  if (trigger_delay_ != -1)
    dict.Set(kConfigRuleTriggerDelay, trigger_delay_);

  if (rule_id_ != GetDefaultRuleId()) {
    dict.Set(kConfigRuleIdKey, rule_id_);
  }

  if (is_crash_) {
    dict.Set(kConfigIsCrashKey, is_crash_);
  }

  return dict;
}

void BackgroundTracingRule::GenerateMetadataProto(
    BackgroundTracingRule::MetadataProto* out) const {}

void BackgroundTracingRule::Setup(const base::Value::Dict& dict) {
  if (auto trigger_chance = dict.FindDouble(kConfigRuleTriggerChance)) {
    trigger_chance_ = *trigger_chance;
  }
  if (auto trigger_delay = dict.FindInt(kConfigRuleTriggerDelay)) {
    trigger_delay_ = *trigger_delay;
  }
  if (const std::string* rule_id = dict.FindString(kConfigRuleIdKey)) {
    rule_id_ = *rule_id;
  } else {
    rule_id_ = GetDefaultRuleId();
  }
  if (auto is_crash = dict.FindBool(kConfigIsCrashKey)) {
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
      const base::Value::Dict& dict) {
    if (const std::string* trigger_name =
            dict.FindString(kConfigRuleTriggerNameKey)) {
      return base::WrapUnique<BackgroundTracingRule>(
          new NamedTriggerRule(*trigger_name));
    }
    return nullptr;
  }

  base::Value::Dict ToDict() const override {
    base::Value::Dict dict = BackgroundTracingRule::ToDict();
    dict.Set(kConfigRuleKey, kConfigRuleTypeMonitorNamed);
    dict.Set(kConfigRuleTriggerNameKey, named_event_.c_str());
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
  HistogramRule(const std::string& histogram_name,
                int histogram_lower_value,
                int histogram_upper_value)
      : histogram_name_(histogram_name),
        histogram_lower_value_(histogram_lower_value),
        histogram_upper_value_(histogram_upper_value),
        installed_(false) {}

 public:
  static std::unique_ptr<BackgroundTracingRule> Create(
      const base::Value::Dict& dict) {
    const std::string* histogram_name =
        dict.FindString(kConfigRuleHistogramNameKey);
    if (!histogram_name)
      return nullptr;

    absl::optional<int> histogram_lower_value =
        dict.FindInt(kConfigRuleHistogramValue1Key);
    if (!histogram_lower_value) {
      // Check for the old naming.
      histogram_lower_value = dict.FindInt(kConfigRuleHistogramValueOldKey);
      if (!histogram_lower_value)
        return nullptr;
    }

    int histogram_upper_value = dict.FindInt(kConfigRuleHistogramValue2Key)
                                    .value_or(std::numeric_limits<int>::max());

    if (*histogram_lower_value > histogram_upper_value)
      return nullptr;

    std::unique_ptr<BackgroundTracingRule> rule(new HistogramRule(
        *histogram_name, *histogram_lower_value, histogram_upper_value));

    return rule;
  }

  ~HistogramRule() override {
    if (installed_) {
      BackgroundTracingManagerImpl::GetInstance().RemoveAgentObserver(this);
    }
  }

  // BackgroundTracingRule implementation
  void Install() override {
    histogram_sample_callback_ = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        histogram_name_,
        base::BindRepeating(&HistogramRule::OnHistogramChangedCallback,
                            base::Unretained(this), histogram_lower_value_,
                            histogram_upper_value_));
    BackgroundTracingManagerImpl::GetInstance().AddAgentObserver(this);
    installed_ = true;
  }

  base::Value::Dict ToDict() const override {
    base::Value::Dict dict = BackgroundTracingRule::ToDict();
    dict.Set(kConfigRuleKey, kConfigRuleTypeMonitorHistogram);
    dict.Set(kConfigRuleHistogramNameKey, histogram_name_.c_str());
    dict.Set(kConfigRuleHistogramValue1Key, histogram_lower_value_);
    dict.Set(kConfigRuleHistogramValue2Key, histogram_upper_value_);
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

  void OnHistogramTrigger(const std::string& histogram_name) const {
    if (histogram_name != histogram_name_)
      return;

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BackgroundTracingManagerImpl::OnRuleTriggered,
            base::Unretained(&BackgroundTracingManagerImpl::GetInstance()),
            this, BackgroundTracingManager::StartedFinalizingCallback()));
  }

  void AbortTracing() {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BackgroundTracingManagerImpl::AbortScenario,
            base::Unretained(&BackgroundTracingManagerImpl::GetInstance())));
  }

  // BackgroundTracingManagerImpl::AgentObserver implementation
  void OnAgentAdded(tracing::mojom::BackgroundTracingAgent* agent) override {
    agent->SetUMACallback(histogram_name_, histogram_lower_value_,
                          histogram_upper_value_);
  }

  void OnAgentRemoved(tracing::mojom::BackgroundTracingAgent* agent) override {
    agent->ClearUMACallback(histogram_name_);
  }

  void OnHistogramChangedCallback(base::Histogram::Sample reference_lower_value,
                                  base::Histogram::Sample reference_upper_value,
                                  const char* histogram_name,
                                  uint64_t name_hash,
                                  base::Histogram::Sample actual_value) {
    if (reference_lower_value > actual_value ||
        reference_upper_value < actual_value) {
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
    TRACE_EVENT_INSTANT("toplevel", "HistogramSampleTrigger", track,
                        base::TimeTicks::Now(), trace_details);
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
  bool installed_;
  std::unique_ptr<base::StatisticsRecorder::ScopedHistogramSampleObserver>
      histogram_sample_callback_;
};

}  // namespace

std::unique_ptr<BackgroundTracingRule>
BackgroundTracingRule::CreateRuleFromDict(const base::Value::Dict& dict) {
  const std::string* type = dict.FindString(kConfigRuleKey);
  if (!type)
    return nullptr;

  std::unique_ptr<BackgroundTracingRule> tracing_rule;
  if (*type == kConfigRuleTypeMonitorNamed) {
    tracing_rule = NamedTriggerRule::Create(dict);
  } else if (*type == kConfigRuleTypeMonitorHistogram) {
    tracing_rule = HistogramRule::Create(dict);
  }
  if (tracing_rule)
    tracing_rule->Setup(dict);

  return tracing_rule;
}

}  // namespace content

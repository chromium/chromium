// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/tracing/background_tracing_rule.h"

#include <limits>
#include <optional>
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
#include "components/variations/hashing.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "services/tracing/public/mojom/background_tracing_agent.mojom.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_histogram_sample.pbzero.h"

namespace content {

BackgroundTracingRule::BackgroundTracingRule() = default;

BackgroundTracingRule::~BackgroundTracingRule() {
  DCHECK(!installed());
}

void BackgroundTracingRule::Install(RuleTriggeredCallback trigger_callback) {
  DCHECK(!installed());
  installed_ = true;
  trigger_callback_ = std::move(trigger_callback);
  if (activation_delay_) {
    activation_timer_.Start(FROM_HERE, *activation_delay_,
                            base::BindOnce(&BackgroundTracingRule::DoInstall,
                                           base::Unretained(this)));
  } else {
    DoInstall();
  }
}

void BackgroundTracingRule::Uninstall() {
  if (!installed()) {
    return;
  }
  installed_ = false;
  trigger_timer_.Stop();
  activation_timer_.Stop();
  trigger_callback_.Reset();
  DoUninstall();
}

bool BackgroundTracingRule::OnRuleTriggered(std::optional<int32_t> value) {
  if (!installed()) {
    return false;
  }
  DCHECK(trigger_callback_);
  if (trigger_chance_ < 1.0 && base::RandDouble() > trigger_chance_) {
    return false;
  }
  triggered_value_ = value;
  if (delay_) {
    trigger_timer_.Start(FROM_HERE, *delay_,
                         base::BindOnce(base::IgnoreResult(trigger_callback_),
                                        base::Unretained(this)));
    return true;
  } else {
    return trigger_callback_.Run(this);
  }
}

base::TimeDelta BackgroundTracingRule::GetTraceDelay() const {
  return trigger_delay_;
}

std::string BackgroundTracingRule::GetDefaultRuleId() const {
  return "org.chromium.background_tracing.trigger";
}

perfetto::protos::gen::TriggerRule BackgroundTracingRule::ToProtoForTesting()
    const {
  perfetto::protos::gen::TriggerRule config;
  if (trigger_chance_ < 1.0) {
    config.set_trigger_chance(trigger_chance_);
  }

  if (delay_) {
    config.set_delay_ms(delay_->InMilliseconds());
  }

  config.set_name(rule_id_);

  return config;
}

void BackgroundTracingRule::GenerateMetadataProto(
    BackgroundTracingRule::MetadataProto* out) const {
  uint32_t name_hash = variations::HashName(rule_id());
  out->set_name_hash(name_hash);
}

void BackgroundTracingRule::Setup(
    const perfetto::protos::gen::TriggerRule& config) {
  if (config.has_trigger_chance()) {
    trigger_chance_ = config.trigger_chance();
  }
  if (config.has_delay_ms()) {
    delay_ = base::Milliseconds(config.delay_ms());
  }
  if (config.has_activation_delay_ms()) {
    activation_delay_ = base::Milliseconds(config.activation_delay_ms());
  }
  if (config.has_name()) {
    rule_id_ = config.name();
  } else {
    rule_id_ = GetDefaultRuleId();
  }
}

namespace {

class NamedTriggerRule : public BackgroundTracingRule {
 private:
  explicit NamedTriggerRule(const std::string& named_event)
      : named_event_(named_event) {}

 public:
  static std::unique_ptr<BackgroundTracingRule> Create(
      const perfetto::protos::gen::TriggerRule& config) {
    if (config.has_manual_trigger_name()) {
      return base::WrapUnique<BackgroundTracingRule>(
          new NamedTriggerRule(config.manual_trigger_name()));
    }
    return nullptr;
  }

  void DoInstall() override {
    BackgroundTracingManagerImpl::GetInstance().AddNamedTriggerObserver(
        named_event_, this);
  }

  void DoUninstall() override {
    BackgroundTracingManagerImpl::GetInstance().RemoveNamedTriggerObserver(
        named_event_, this);
  }

  perfetto::protos::gen::TriggerRule ToProtoForTesting() const override {
    perfetto::protos::gen::TriggerRule config =
        BackgroundTracingRule::ToProtoForTesting();
    config.set_manual_trigger_name(named_event_);
    return config;
  }

  void GenerateMetadataProto(
      BackgroundTracingRule::MetadataProto* out) const override {
    DCHECK(out);
    BackgroundTracingRule::GenerateMetadataProto(out);
    out->set_trigger_type(MetadataProto::MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED);
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
        histogram_upper_value_(histogram_upper_value) {}

 public:
  static std::unique_ptr<BackgroundTracingRule> Create(
      const perfetto::protos::gen::TriggerRule& config) {
    DCHECK(config.has_histogram());

    if (!config.histogram().has_histogram_name() ||
        !config.histogram().has_min_value()) {
      return nullptr;
    }
    int histogram_lower_value = config.histogram().min_value();
    int histogram_upper_value = std::numeric_limits<int>::max();
    if (config.histogram().has_max_value()) {
      histogram_upper_value = config.histogram().max_value();
    }
    if (histogram_lower_value > histogram_upper_value) {
      return nullptr;
    }

    return base::WrapUnique(
        new HistogramRule(config.histogram().histogram_name(),
                          histogram_lower_value, histogram_upper_value));
  }

  ~HistogramRule() override = default;

  // BackgroundTracingRule implementation
  void DoInstall() override {
    histogram_sample_callback_.emplace(
        histogram_name_,
        base::BindRepeating(&HistogramRule::OnHistogramChangedCallback,
                            base::Unretained(this), histogram_lower_value_,
                            histogram_upper_value_));
    BackgroundTracingManagerImpl::GetInstance().AddNamedTriggerObserver(
        rule_id(), this);
    BackgroundTracingManagerImpl::GetInstance().AddAgentObserver(this);
  }

  void DoUninstall() override {
    histogram_sample_callback_.reset();
    BackgroundTracingManagerImpl::GetInstance().RemoveAgentObserver(this);
    BackgroundTracingManagerImpl::GetInstance().RemoveNamedTriggerObserver(
        rule_id(), this);
  }

  perfetto::protos::gen::TriggerRule ToProtoForTesting() const override {
    perfetto::protos::gen::TriggerRule config =
        BackgroundTracingRule::ToProtoForTesting();
    auto* histogram = config.mutable_histogram();
    histogram->set_histogram_name(histogram_name_);
    histogram->set_min_value(histogram_lower_value_);
    histogram->set_max_value(histogram_upper_value_);
    return config;
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

  // BackgroundTracingManagerImpl::AgentObserver implementation
  void OnAgentAdded(tracing::mojom::BackgroundTracingAgent* agent) override {
    agent->SetUMACallback(tracing::mojom::BackgroundTracingRule::New(rule_id()),
                          histogram_name_, histogram_lower_value_,
                          histogram_upper_value_);
  }

  void OnAgentRemoved(tracing::mojom::BackgroundTracingAgent* agent) override {
    agent->ClearUMACallback(
        tracing::mojom::BackgroundTracingRule::New(rule_id()));
  }

  void OnHistogramChangedCallback(base::Histogram::Sample reference_lower_value,
                                  base::Histogram::Sample reference_upper_value,
                                  const char* histogram_name,
                                  uint64_t name_hash,
                                  base::Histogram::Sample actual_value) {
    DCHECK_EQ(histogram_name, histogram_name_);
    if (reference_lower_value > actual_value ||
        reference_upper_value < actual_value) {
      return;
    }

    // Add the histogram name and its corresponding value to the trace.
    const auto trace_details = [&](perfetto::EventContext ctx) {
      perfetto::protos::pbzero::ChromeHistogramSample* new_sample =
          ctx.event()->set_chrome_histogram_sample();
      new_sample->set_name_hash(base::HashMetricName(histogram_name));
      new_sample->set_sample(actual_value);
    };
    TRACE_EVENT_INSTANT(
        "toplevel", "HistogramSampleTrigger",
        perfetto::Track::FromPointer(this, perfetto::ProcessTrack::Current()),
        base::TimeTicks::Now(), trace_details);
    OnRuleTriggered(actual_value);
  }

 protected:
  std::string GetDefaultRuleId() const override {
    return base::StrCat({"org.chromium.background_tracing.", histogram_name_});
  }

 private:
  std::string histogram_name_;
  int histogram_lower_value_;
  int histogram_upper_value_;
  std::optional<base::StatisticsRecorder::ScopedHistogramSampleObserver>
      histogram_sample_callback_;
};

class TimerRule : public BackgroundTracingRule {
 private:
  explicit TimerRule() = default;

 public:
  static std::unique_ptr<BackgroundTracingRule> Create(
      const perfetto::protos::gen::TriggerRule& config) {
    return base::WrapUnique<TimerRule>(new TimerRule());
  }

  void DoInstall() override { OnRuleTriggered(std::nullopt); }
  void DoUninstall() override {}

  void GenerateMetadataProto(
      BackgroundTracingRule::MetadataProto* out) const override {
    DCHECK(out);
    BackgroundTracingRule::GenerateMetadataProto(out);
    out->set_trigger_type(MetadataProto::TRIGGER_UNSPECIFIED);
  }

 protected:
  std::string GetDefaultRuleId() const override {
    return "org.chromium.background_tracing.timer";
  }
};

class RepeatingIntervalRule : public BackgroundTracingRule {
 private:
  RepeatingIntervalRule(base::TimeDelta period, bool randomized)
      : period_(period),
        randomized_(randomized),
        interval_phase_(base::TimeTicks::Now()) {}

 public:
  static std::unique_ptr<BackgroundTracingRule> Create(
      const perfetto::protos::gen::TriggerRule& config) {
    DCHECK(config.has_repeating_interval());

    const auto& interval = config.repeating_interval();
    if (!interval.has_period_ms()) {
      return nullptr;
    }
    return base::WrapUnique<RepeatingIntervalRule>(new RepeatingIntervalRule(
        base::Milliseconds(interval.period_ms()), interval.randomized()));
  }

  void DoInstall() override { ScheduleNextTick(); }
  void DoUninstall() override { timer_.Stop(); }

  perfetto::protos::gen::TriggerRule ToProtoForTesting() const override {
    perfetto::protos::gen::TriggerRule config =
        BackgroundTracingRule::ToProtoForTesting();
    auto* histogram = config.mutable_repeating_interval();
    histogram->set_period_ms(period_.InMilliseconds());
    histogram->set_randomized(randomized_);
    return config;
  }

  void GenerateMetadataProto(
      BackgroundTracingRule::MetadataProto* out) const override {
    DCHECK(out);
    BackgroundTracingRule::GenerateMetadataProto(out);
    out->set_trigger_type(MetadataProto::TRIGGER_UNSPECIFIED);
  }

 protected:
  std::string GetDefaultRuleId() const override {
    return "org.chromium.background_tracing.repeating_interval";
  }

 private:
  base::TimeTicks GetFireTimeForInterval(base::TimeTicks interval_start) const {
    if (randomized_) {
      return interval_start +
             base::Microseconds(base::RandGenerator(period_.InMicroseconds()));
    }
    return interval_start;
  }
  base::TimeTicks GetNextIntervalStart(base::TimeTicks now) const {
    base::TimeTicks next_interval_start =
        now.SnappedToNextTick(interval_phase_, period_);
    if (next_interval_start == now) {
      return next_interval_start + period_;
    }
    return next_interval_start;
  }
  void UpdateNextFireTime(base::TimeTicks now) {
    base::TimeTicks next_interval_start = GetNextIntervalStart(now);
    base::TimeTicks prev_interval_start = next_interval_start - period_;
    // If the previously scheduled fire time is in a past interval, the current
    // interval can still fire. Compute a new fire time for the current
    // interval and keep it if it hasn't passed, otherwise move on to the next
    // interval.
    if (randomized_ && (scheduled_fire_time_ < prev_interval_start)) {
      scheduled_fire_time_ = GetFireTimeForInterval(prev_interval_start);
      if (scheduled_fire_time_ >= now) {
        return;
      }
    }
    scheduled_fire_time_ = GetFireTimeForInterval(next_interval_start);
  }

  void ScheduleNextTick() {
    base::TimeTicks now = base::TimeTicks::Now();
    // Update the next fire time only if it's in the past.
    if (scheduled_fire_time_ <= now) {
      UpdateNextFireTime(now);
    }

    timer_.Start(
        FROM_HERE, scheduled_fire_time_,
        base::BindOnce(&RepeatingIntervalRule::OnTick, base::Unretained(this)),
        base::subtle::DelayPolicy::kFlexibleNoSooner);
  }

  void OnTick() {
    OnRuleTriggered(std::nullopt);
    ScheduleNextTick();
  }

  const base::TimeDelta period_;
  const bool randomized_;
  const base::TimeTicks interval_phase_;
  base::TimeTicks scheduled_fire_time_;
  base::DeadlineTimer timer_;
};

}  // namespace

std::unique_ptr<BackgroundTracingRule> BackgroundTracingRule::Create(
    const perfetto::protos::gen::TriggerRule& config) {
  std::unique_ptr<BackgroundTracingRule> tracing_rule;
  if (config.has_manual_trigger_name()) {
    tracing_rule = NamedTriggerRule::Create(config);
  } else if (config.has_histogram()) {
    tracing_rule = HistogramRule::Create(config);
  } else if (config.has_repeating_interval()) {
    tracing_rule = RepeatingIntervalRule::Create(config);
  } else if (config.has_delay_ms()) {
    tracing_rule = TimerRule::Create(config);
  } else {
    return nullptr;
  }
  if (tracing_rule) {
    tracing_rule->Setup(config);
  }
  return tracing_rule;
}

bool BackgroundTracingRule::Append(
    const std::vector<perfetto::protos::gen::TriggerRule>& configs,
    std::vector<std::unique_ptr<BackgroundTracingRule>>& rules) {
  for (const auto& rule_config : configs) {
    auto rule = BackgroundTracingRule::Create(rule_config);
    if (!rule) {
      return false;
    }
    rules.push_back(std::move(rule));
  }
  return true;
}

}  // namespace content

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_RULE_H_
#define CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_RULE_H_

#include <memory>
#include <optional>

#include "base/observer_list_types.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "content/browser/tracing/background_tracing_config_impl.h"
#include "content/common/content_export.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/scenario_config.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_metadata.pbzero.h"

namespace content {

class CONTENT_EXPORT BackgroundTracingRule : public base::CheckedObserver {
 public:
  using MetadataProto =
      perfetto::protos::pbzero::BackgroundTracingMetadata::TriggerRule;
  // Returns true if the trigger caused a scenario to either begin recording or
  // finalize the trace depending on the config.
  using RuleTriggeredCallback =
      base::RepeatingCallback<bool(const BackgroundTracingRule*)>;

  BackgroundTracingRule();

  BackgroundTracingRule(const BackgroundTracingRule&) = delete;
  BackgroundTracingRule& operator=(const BackgroundTracingRule&) = delete;

  ~BackgroundTracingRule() override;

  virtual void Install(RuleTriggeredCallback);
  virtual void Uninstall();
  virtual base::Value::Dict ToDict() const;
  virtual perfetto::protos::gen::TriggerRule ToProtoForTesting() const;
  virtual void GenerateMetadataProto(MetadataProto* out) const;

  // Seconds from the rule is triggered to finalization should start.
  virtual base::TimeDelta GetTraceDelay() const;

  // Probability that we should allow a tigger to  happen.
  double trigger_chance() const { return trigger_chance_; }
  std::optional<base::TimeDelta> delay() const { return delay_; }

  static std::unique_ptr<BackgroundTracingRule> CreateRuleFromDict(
      const base::Value::Dict& dict);

  static std::unique_ptr<BackgroundTracingRule> Create(
      const perfetto::protos::gen::TriggerRule& config);
  static bool Append(
      const std::vector<perfetto::protos::gen::TriggerRule>& configs,
      std::vector<std::unique_ptr<BackgroundTracingRule>>& rules);

  const std::string& rule_id() const { return rule_id_; }
  std::optional<int32_t> triggered_value() const { return triggered_value_; }

  bool is_crash() const { return is_crash_; }

  bool OnRuleTriggered(std::optional<int32_t> value);

 protected:
  virtual std::string GetDefaultRuleId() const;

  virtual void DoInstall() = 0;
  virtual void DoUninstall() = 0;

  bool installed() const { return installed_; }

 private:
  void Setup(const base::Value::Dict& dict);
  void Setup(const perfetto::protos::gen::TriggerRule& config);

  RuleTriggeredCallback trigger_callback_;
  bool installed_ = false;
  double trigger_chance_ = 1.0;
  base::TimeDelta trigger_delay_;
  std::optional<base::TimeDelta> delay_;
  std::optional<base::TimeDelta> activation_delay_;
  base::OneShotTimer trigger_timer_;
  base::OneShotTimer activation_timer_;
  std::string rule_id_;
  std::optional<int32_t> triggered_value_;
  bool is_crash_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_RULE_H_

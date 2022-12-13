// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_RULE_H_
#define CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_RULE_H_

#include <memory>

#include "base/values.h"
#include "content/browser/tracing/background_tracing_config_impl.h"
#include "content/public/browser/background_tracing_manager.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_metadata.pbzero.h"

namespace content {

class BackgroundTracingRule {
 public:
  using MetadataProto =
      perfetto::protos::pbzero::BackgroundTracingMetadata::TriggerRule;

  BackgroundTracingRule();
  explicit BackgroundTracingRule(int trigger_delay);

  BackgroundTracingRule(const BackgroundTracingRule&) = delete;
  BackgroundTracingRule& operator=(const BackgroundTracingRule&) = delete;

  virtual ~BackgroundTracingRule();

  BackgroundTracingConfigImpl::CategoryPreset category_preset() const {
    return category_preset_;
  }
  void set_category_preset(
      BackgroundTracingConfigImpl::CategoryPreset category_preset) {
    category_preset_ = category_preset;
  }

  virtual void Install() {}
  virtual base::Value::Dict ToDict() const;
  virtual void GenerateMetadataProto(MetadataProto* out) const;
  virtual bool ShouldTriggerNamedEvent(const std::string& named_event) const;

  // Seconds from the rule is triggered to finalization should start.
  virtual int GetTraceDelay() const;

  // Probability that we should allow a tigger to  happen.
  double trigger_chance() const { return trigger_chance_; }

  static std::unique_ptr<BackgroundTracingRule> CreateRuleFromDict(
      const base::Value::Dict& dict);

  const std::string& rule_id() const { return rule_id_; }

  bool is_crash() const { return is_crash_; }

 protected:
  virtual std::string GetDefaultRuleId() const;

 private:
  void Setup(const base::Value::Dict& dict);

  double trigger_chance_ = 1.0;
  int trigger_delay_ = -1;
  std::string rule_id_;
  BackgroundTracingConfigImpl::CategoryPreset category_preset_ =
      BackgroundTracingConfigImpl::CATEGORY_PRESET_UNSET;
  bool is_crash_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_RULE_H_

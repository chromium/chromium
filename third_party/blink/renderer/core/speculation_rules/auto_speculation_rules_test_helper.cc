// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/auto_speculation_rules_test_helper.h"
#include "third_party/blink/renderer/core/speculation_rules/auto_speculation_rules_config.h"

namespace blink::test {

AutoSpeculationRulesConfigOverride::AutoSpeculationRulesConfigOverride(
    const String& config_string) {
  current_override_ =
      std::make_unique<AutoSpeculationRulesConfig>(config_string);
  previous_override_ = AutoSpeculationRulesConfig::OverrideInstanceForTesting(
      current_override_.get());
}

AutoSpeculationRulesConfigOverride::~AutoSpeculationRulesConfigOverride() {
  AutoSpeculationRulesConfig* uninstalled_override =
      AutoSpeculationRulesConfig::OverrideInstanceForTesting(
          previous_override_);
  CHECK_EQ(uninstalled_override, current_override_.get());
}

}  // namespace blink::test

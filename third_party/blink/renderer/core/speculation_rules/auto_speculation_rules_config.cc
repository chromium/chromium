// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/auto_speculation_rules_config.h"
#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/javascript_framework_detection.mojom-shared.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

namespace {

static AutoSpeculationRulesConfig* g_override = nullptr;

}

AutoSpeculationRulesConfig::AutoSpeculationRulesConfig(
    const String& config_string) {
  // Because the JSON comes from a fallible remote configuration, we don't want
  // to crash if it's invalid.

  const std::unique_ptr<const JSONObject> config =
      JSONObject::From(ParseJSON(config_string));
  if (!config) {
    LOG(ERROR) << "Unparseable JSON " << config_string;
    return;
  }

  const JSONObject* framework_to_speculation_rules =
      config->GetJSONObject("framework_to_speculation_rules");
  if (!framework_to_speculation_rules) {
    // Not an error.
    return;
  }

  for (wtf_size_t i = 0; i < framework_to_speculation_rules->size(); ++i) {
    const JSONObject::Entry entry = framework_to_speculation_rules->at(i);

    bool key_is_int = false;
    const int key_as_int = entry.first.ToIntStrict(&key_is_int);
    if (!key_is_int) {
      LOG(ERROR) << "Non-integer key " << entry.first
                 << " inside framework_to_speculation_rules";
      continue;
    }

    const mojom::JavaScriptFramework framework =
        static_cast<mojom::JavaScriptFramework>(key_as_int);
    const bool value_is_known = IsKnownEnumValue(framework);
    if (!value_is_known) {
      LOG(ERROR) << "Unknown integer key " << key_as_int
                 << " inside framework_to_speculation_rules";
      continue;
    }

    String speculation_rules;
    bool value_is_string = entry.second->AsString(&speculation_rules);
    if (!value_is_string) {
      LOG(ERROR) << "Non-string value " << entry.second->ToJSONString()
                 << " inside framework_to_speculation_rules";
      continue;
    }

    framework_to_speculation_rules_.emplace_back(framework, speculation_rules);
  }
}

const AutoSpeculationRulesConfig& AutoSpeculationRulesConfig::GetInstance() {
  CHECK(base::FeatureList::IsEnabled(features::kAutoSpeculationRules));

  const String config_string =
      String::FromUTF8(features::kAutoSpeculationRulesConfig.Get());
  DEFINE_STATIC_LOCAL(AutoSpeculationRulesConfig, instance, (config_string));

  if (g_override) {
    return *g_override;
  }

  return instance;
}

AutoSpeculationRulesConfig*
AutoSpeculationRulesConfig::OverrideInstanceForTesting(
    AutoSpeculationRulesConfig* new_override) {
  AutoSpeculationRulesConfig* old_override = g_override;
  g_override = new_override;
  return old_override;
}

String AutoSpeculationRulesConfig::ForFramework(
    mojom::JavaScriptFramework framework) const {
  for (const auto& entry : framework_to_speculation_rules_) {
    if (entry.first == framework) {
      return entry.second;
    }
  }

  return String();
}

}  // namespace blink

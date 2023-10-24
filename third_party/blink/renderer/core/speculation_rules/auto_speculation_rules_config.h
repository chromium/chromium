// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_AUTO_SPECULATION_RULES_CONFIG_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_AUTO_SPECULATION_RULES_CONFIG_H_

#include "third_party/blink/public/mojom/loader/javascript_framework_detection.mojom-shared.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Parses the auto speculation rules config string and provides methods to query
// the resulting configuration. See https://crbug.com/1472970 for more details
// on auto speculation rules.
class CORE_EXPORT AutoSpeculationRulesConfig {
 public:
  explicit AutoSpeculationRulesConfig(const String& config_string);
  ~AutoSpeculationRulesConfig() = default;

  String ForFramework(mojom::JavaScriptFramework) const;

 private:
  WTF::HashMap<mojom::JavaScriptFramework, String>
      framework_to_speculation_rules_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_AUTO_SPECULATION_RULES_CONFIG_H_

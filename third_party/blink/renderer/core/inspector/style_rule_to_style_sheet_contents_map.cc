// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/style_rule_to_style_sheet_contents_map.h"

#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"

namespace blink {

void StyleRuleToStyleSheetContentsMap::Trace(Visitor* visitor) const {
  visitor->Trace(rule_to_sheet_);
}

void StyleRuleToStyleSheetContentsMap::Add(const StyleRule* rule,
                                           const StyleSheetContents* contents) {
  rule_to_sheet_.Set(rule, contents);
}

const StyleSheetContents* StyleRuleToStyleSheetContentsMap::Lookup(
    const StyleRule* rule) const {
  auto it = rule_to_sheet_.find(rule);
  if (it != rule_to_sheet_.end()) {
    return it->value;
  }
  return nullptr;
}

}  // namespace blink

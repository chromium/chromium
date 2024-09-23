// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_rule_nested_declarations.h"

namespace blink {

void StyleRuleNestedDeclarations::ReplaceSelectorList(
    const CSSSelector* selector_list) {
  HeapVector<CSSSelector> selectors = CSSSelectorList::Copy(selector_list);
  style_rule_ = StyleRule::Create(
      selectors, style_rule_->Properties().ImmutableCopyIfNeeded());
}

}  // namespace blink

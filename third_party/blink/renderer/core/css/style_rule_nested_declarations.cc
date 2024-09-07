// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/css/style_rule_nested_declarations.h"

namespace blink {

void StyleRuleNestedDeclarations::ReplaceSelectorList(
    const CSSSelector* selector_list) {
  HeapVector<CSSSelector> selectors = CSSSelectorList::Copy(selector_list);
  style_rule_ = StyleRule::Create(
      base::span<CSSSelector>{selectors.begin(), selectors.size()},
      style_rule_->Properties().ImmutableCopyIfNeeded());
}

}  // namespace blink

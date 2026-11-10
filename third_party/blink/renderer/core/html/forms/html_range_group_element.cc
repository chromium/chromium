// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_range_group_element.h"

#include "base/check.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

HTMLRangeGroupElement::HTMLRangeGroupElement(Document& document)
    : HTMLFormControlElement(html_names::kRangegroupTag, document) {
  CHECK(RuntimeEnabledFeatures::RangeGroupEnabled(
      document.GetExecutionContext()));
}

mojom::blink::FormControlType HTMLRangeGroupElement::FormControlType() const {
  return mojom::blink::FormControlType::kRangeGroup;
}

const AtomicString& HTMLRangeGroupElement::FormControlTypeAsString() const {
  return html_names::kRangegroupTag.LocalName();
}

FocusableState HTMLRangeGroupElement::SupportsFocus(
    UpdateBehavior update_behavior) const {
  if (IsDisabledFormControl()) {
    return FocusableState::kNotFocusable;
  }
  return HTMLElement::SupportsFocus(update_behavior);
}

}  // namespace blink

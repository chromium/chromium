// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_trigger_attachment_value.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

String CSSTriggerAttachmentValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("trigger(");
  result.Append(trigger_name_->CssText());
  result.Append(", ");

  size_t num_pairs = action_behavior_pairs_.size();
  for (size_t i = 0; i < num_pairs; i++) {
    const auto& pair = action_behavior_pairs_[i];

    result.Append(pair.first->CssText());
    result.Append(" ");
    result.Append(pair.second->CssText());

    if (i < num_pairs - 1) {
      result.Append(", ");
    }
  }

  result.Append(")");
  return result.ReleaseString();
}

bool CSSTriggerAttachmentValue::Equals(
    const CSSTriggerAttachmentValue& other) const {
  return trigger_name_->Equals(*other.TriggerName()) &&
         action_behavior_pairs_ == other.ActionBehaviorPairs();
}

void CSSTriggerAttachmentValue::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  visitor->Trace(trigger_name_);
  visitor->Trace(action_behavior_pairs_);

  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue
}  // namespace blink

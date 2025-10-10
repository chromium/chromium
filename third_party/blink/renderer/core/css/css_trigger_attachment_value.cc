// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_trigger_attachment_value.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

String CSSTriggerAttachmentValue::CustomCSSText() const {
  StringBuilder result;
  result.Append(trigger_name_->CssText());
  result.Append(" ");

  result.Append(enter_behavior_->CssText());
  if (exit_behavior_) {
    result.Append(" ");
    result.Append(exit_behavior_->CssText());
  }

  return result.ReleaseString();
}

bool CSSTriggerAttachmentValue::Equals(
    const CSSTriggerAttachmentValue& other) const {
  return trigger_name_->Equals(*other.TriggerName()) &&
         enter_behavior_ == other.EnterBehavior() &&
         exit_behavior_ == other.ExitBehavior();
}

void CSSTriggerAttachmentValue::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  visitor->Trace(trigger_name_);
  visitor->Trace(enter_behavior_);
  visitor->Trace(exit_behavior_);

  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue
}  // namespace blink

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TRIGGER_ATTACHMENT_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TRIGGER_ATTACHMENT_VALUE_H_

#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {
namespace cssvalue {

// Class to represent a single animation trigger attachment declaration.
// This class corresponds to a single instance of trigger() declared within
// animation-trigger. E.g:
//   animation-trigger: trigger(--mytrigger, myaction mybehavior);
// creates one instance of this class. This instance has a single entry in its
// |action_behavior_pairs_|.
// Another example:
//   animation-trigger:
//           trigger(--mytrigger, myaction1 mybehavior1, myaction2 mybehavior2);
// creates one instance of this class with 2 items in |action_behavior_pairs_|.
// And finally,
// animation-trigger:
//   trigger(--mytrigger1, myaction1 mybehavior1)
//   trigger(--mytrigger2, myaction2 mybehavior2);
// creates 2 instances of this class.
class CSSTriggerAttachmentValue : public CSSValue {
 public:
  explicit CSSTriggerAttachmentValue(
      const CSSCustomIdentValue* trigger_name,
      const HeapVector<std::pair<Member<const CSSCustomIdentValue>,
                                 Member<const CSSCustomIdentValue>>>&
          action_behavior_pairs)
      : CSSValue(kTriggerAttachmentClass),
        trigger_name_(trigger_name),
        action_behavior_pairs_(std::move(action_behavior_pairs)) {}

  const CSSCustomIdentValue* TriggerName() const { return trigger_name_.Get(); }
  const HeapVector<std::pair<Member<const CSSCustomIdentValue>,
                             Member<const CSSCustomIdentValue>>>&
  ActionBehaviorPairs() const {
    return action_behavior_pairs_;
  }

  String CustomCSSText() const;
  bool Equals(const CSSTriggerAttachmentValue&) const;
  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  Member<const CSSCustomIdentValue> trigger_name_;
  HeapVector<std::pair<Member<const CSSCustomIdentValue>,
                       Member<const CSSCustomIdentValue>>>
      action_behavior_pairs_;
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSTriggerAttachmentValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsTriggerAttachmentValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TRIGGER_ATTACHMENT_VALUE_H_

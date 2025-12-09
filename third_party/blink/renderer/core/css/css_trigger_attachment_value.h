// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TRIGGER_ATTACHMENT_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TRIGGER_ATTACHMENT_VALUE_H_

#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSIdentifierValue;

namespace cssvalue {

// Class to represent a single animation trigger attachment declaration.
// This class corresponds to a single instance of an animation-trigger
// attachment declaration in the animation-trigger grammar:
//   <animation-trigger-attachment>: <dashed-ident> <ident> [<ident>];
//
//   animation-trigger: [ [ <animation-trigger-attachment> ]+ ]#
// E.g:
//   animation-trigger: --mytrigger play reset;
// creates one instance of this class with "play" as the enter behavior and
// "reset" as the exit behavior.
class CSSTriggerAttachmentValue : public CSSValue {
 public:
  explicit CSSTriggerAttachmentValue(const CSSCustomIdentValue* trigger_name,
                                     const CSSIdentifierValue* enter_behavior,
                                     const CSSIdentifierValue* exit_behavior)
      : CSSValue(kTriggerAttachmentClass),
        trigger_name_(trigger_name),
        enter_behavior_(enter_behavior),
        exit_behavior_(exit_behavior) {
    needs_tree_scope_population_ = true;
  }

  const CSSCustomIdentValue* TriggerName() const { return trigger_name_.Get(); }
  const CSSIdentifierValue* EnterBehavior() const {
    return enter_behavior_.Get();
  }
  const CSSIdentifierValue* ExitBehavior() const {
    return exit_behavior_.Get();
  }

  String CustomCSSText() const;
  bool Equals(const CSSTriggerAttachmentValue&) const;
  void TraceAfterDispatch(blink::Visitor*) const;

  const CSSTriggerAttachmentValue& PopulateWithTreeScope(
      const TreeScope* tree_scope) const;

 private:
  Member<const CSSCustomIdentValue> trigger_name_;
  Member<const CSSIdentifierValue> enter_behavior_;
  Member<const CSSIdentifierValue> exit_behavior_;
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

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SUBMIT_BUTTON_BEHAVIOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SUBMIT_BUTTON_BEHAVIOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/forms/element_behavior.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class HTMLFormElement;
class LabelsNodeList;

// HTMLSubmitButtonBehavior provides submit button behavior to custom elements.
// When attached via ElementInternals, it allows a custom element to trigger
// form submission on activation (click or Enter/Space key).
class CORE_EXPORT HTMLSubmitButtonBehavior final : public ElementBehavior {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static HTMLSubmitButtonBehavior* Create();

  HTMLSubmitButtonBehavior();
  ~HTMLSubmitButtonBehavior() override;

  // ElementBehavior overrides:
  bool HandleActivation(Event& event) override;
  const AtomicString& DefaultAriaRole() const override;
  const char* BehaviorName() const override;

  // Considers both the behavior's own disabled property and the element's
  // disabled state (including fieldset inheritance). This allows submission to
  // be blocked if either the behavior or the element is disabled.
  bool IsEffectivelyDisabled() const;

  // Read-only properties that delegate to the custom element's internals.
  HTMLFormElement* form(ExceptionState& exception_state) const;
  LabelsNodeList* labels(ExceptionState& exception_state) const;

  bool disabled() const { return disabled_; }
  void setDisabled(bool value) { disabled_ = value; }

  String formAction() const { return form_action_; }
  void setFormAction(const String& value) { form_action_ = value; }

  String formEnctype() const { return form_enctype_; }
  void setFormEnctype(const String& value) { form_enctype_ = value; }

  String formMethod() const { return form_method_; }
  void setFormMethod(const String& value) { form_method_ = value; }

  bool formNoValidate() const { return form_no_validate_; }
  void setFormNoValidate(bool value) { form_no_validate_ = value; }

  String formTarget() const { return form_target_; }
  void setFormTarget(const String& value) { form_target_ = value; }

  String name() const { return name_; }
  void setName(const String& value) { name_ = value; }

  String value() const { return value_; }
  void setValue(const String& value) { value_ = value; }

  void Trace(Visitor* visitor) const override;

 private:
  // Returns the associated ElementInternals, or throws InvalidStateError
  // and returns nullptr if the behavior is not attached to an element.
  ElementInternals* GetInternalsOrThrow(ExceptionState& exception_state) const;

  bool disabled_ = false;
  String form_action_;
  String form_enctype_;
  String form_method_;
  bool form_no_validate_ = false;
  String form_target_;
  String name_;
  String value_;
};

}  // namespace blink

template <>
struct blink::DowncastTraits<blink::HTMLSubmitButtonBehavior> {
  static bool AllowFrom(const blink::ElementBehavior& behavior) {
    return behavior.GetWrapperTypeInfo() ==
           blink::HTMLSubmitButtonBehavior::GetStaticWrapperTypeInfo();
  }
};

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SUBMIT_BUTTON_BEHAVIOR_H_

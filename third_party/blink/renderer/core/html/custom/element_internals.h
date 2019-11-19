// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_ELEMENT_INTERNALS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_ELEMENT_INTERNALS_H_

#include "third_party/blink/renderer/bindings/core/v8/file_or_usv_string_or_form_data.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/html/forms/listed_element.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class DOMTokenList;
class HTMLElement;
class LabelsNodeList;
class ValidityStateFlags;

class CORE_EXPORT ElementInternals : public ScriptWrappable,
                                     public ListedElement {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(ElementInternals);

 public:
  ElementInternals(HTMLElement& target);
  void Trace(Visitor* visitor) override;

  HTMLElement& Target() const { return *target_; }
  void DidUpgrade();

  using ControlValue = FileOrUSVStringOrFormData;
  // IDL attributes/operations
  void setFormValue(const ControlValue& value, ExceptionState& exception_state);
  void setFormValue(const ControlValue& value,
                    const ControlValue& state,
                    ExceptionState& exception_state);
  HTMLFormElement* form(ExceptionState& exception_state) const;
  void setValidity(ValidityStateFlags* flags, ExceptionState& exception_state);
  void setValidity(ValidityStateFlags* flags,
                   const String& message,
                   ExceptionState& exception_state);
  void setValidity(ValidityStateFlags* flags,
                   const String& message,
                   Element* anchor,
                   ExceptionState& exception_state);
  bool willValidate(ExceptionState& exception_state) const;
  ValidityState* validity(ExceptionState& exception_state);
  String ValidationMessageForBinding(ExceptionState& exception_state);
  bool checkValidity(ExceptionState& exception_state);
  bool reportValidity(ExceptionState& exception_state);
  LabelsNodeList* labels(ExceptionState& exception_state);
  DOMTokenList* states();

  bool HasState(const AtomicString& state) const;

  // We need these functions because we are reflecting ARIA attributes.
  // See dom/aria_attributes.idl.
  const AtomicString& FastGetAttribute(const QualifiedName&) const;
  void setAttribute(const QualifiedName& attribute, const AtomicString& value);

  void SetElementAttribute(const QualifiedName& name, Element* element);
  Element* GetElementAttribute(const QualifiedName& name);
  HeapVector<Member<Element>> GetElementArrayAttribute(
      const QualifiedName& name,
      bool is_null);
  void SetElementArrayAttribute(const QualifiedName&,
                                HeapVector<Member<Element>>,
                                bool is_null);
  bool HasAttribute(const QualifiedName& attribute) const;
  const HashMap<QualifiedName, AtomicString>& GetAttributes() const;

 private:
  bool IsTargetFormAssociated() const;

  // ListedElement overrides:
  bool IsFormControlElement() const override;
  bool IsElementInternals() const override;
  bool IsEnumeratable() const override;
  void AppendToFormData(FormData& form_data) override;
  void DidChangeForm() override;
  bool HasBadInput() const override;
  bool PatternMismatch() const override;
  bool RangeOverflow() const override;
  bool RangeUnderflow() const override;
  bool StepMismatch() const override;
  bool TooLong() const override;
  bool TooShort() const override;
  bool TypeMismatch() const override;
  bool ValueMissing() const override;
  bool CustomError() const override;
  String validationMessage() const override;
  String ValidationSubMessage() const override;
  Element& ValidationAnchor() const override;
  void DisabledStateMightBeChanged() override;
  bool ClassSupportsStateRestore() const override;
  bool ShouldSaveAndRestoreFormControlState() const override;
  FormControlState SaveFormControlState() const override;
  void RestoreFormControlState(const FormControlState& state) override;

  Member<HTMLElement> target_;

  ControlValue value_;
  ControlValue state_;
  bool is_disabled_ = false;
  Member<ValidityStateFlags> validity_flags_;
  Member<Element> validation_anchor_;

  Member<DOMTokenList> custom_states_;

  HashMap<QualifiedName, AtomicString> accessibility_semantics_map_;

  // See
  // https://whatpr.org/html/3917/common-dom-interfaces.html#reflecting-content-attributes-in-idl-attributes:element
  HeapHashMap<QualifiedName, Member<HeapVector<Member<Element>>>>
      explicitly_set_attr_elements_map_;

  DISALLOW_COPY_AND_ASSIGN(ElementInternals);
};

template <>
struct DowncastTraits<ElementInternals> {
  static bool AllowFrom(const ListedElement& listed_element) {
    return listed_element.IsElementInternals();
  }
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_ELEMENT_INTERNALS_H_

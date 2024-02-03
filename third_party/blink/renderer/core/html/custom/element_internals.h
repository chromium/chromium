// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_ELEMENT_INTERNALS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_ELEMENT_INTERNALS_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/html/forms/labels_node_list.h"
#include "third_party/blink/renderer/core/html/forms/listed_element.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CustomStateSet;
class HTMLElement;
class ValidityStateFlags;

template <typename IDLType>
class FrozenArray;

class CORE_EXPORT ElementInternals : public ScriptWrappable,
                                     public ListedElement,
                                     public ElementRareDataField {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ElementInternals(HTMLElement& target);
  ElementInternals(const ElementInternals&) = delete;
  ElementInternals& operator=(const ElementInternals&) = delete;
  void Trace(Visitor* visitor) const override;

  HTMLElement& Target() const { return *target_; }
  void DidUpgrade();

  void setFormValue(const V8ControlValue* value,
                    ExceptionState& exception_state);
  void setFormValue(const V8ControlValue* value,
                    const V8ControlValue* state,
                    ExceptionState& exception_state);
  HTMLFormElement* form(ExceptionState& exception_state) const;
  void setValidity(ValidityStateFlags* flags, ExceptionState& exception_state);
  void setValidity(ValidityStateFlags* flags,
                   const String& message,
                   ExceptionState& exception_state);
  void setValidity(ValidityStateFlags* flags,
                   const String& message,
                   HTMLElement* anchor,
                   ExceptionState& exception_state);
  bool willValidate(ExceptionState& exception_state) const;
  ValidityState* validity(ExceptionState& exception_state);
  String ValidationMessageForBinding(ExceptionState& exception_state);
  bool checkValidity(ExceptionState& exception_state);
  bool reportValidity(ExceptionState& exception_state);
  LabelsNodeList* labels(ExceptionState& exception_state);
  CustomStateSet* states();

  bool HasState(const AtomicString& state) const;

  ShadowRoot* shadowRoot() const;

  // We need these functions because we are reflecting ARIA attributes.
  // See dom/aria_attributes.idl.
  const AtomicString& FastGetAttribute(const QualifiedName&) const;
  void setAttribute(const QualifiedName& attribute, const AtomicString& value);

  void SetElementAttribute(const QualifiedName& name, Element* element);
  Element* GetElementAttribute(const QualifiedName& name);
  void SetElementArrayAttribute(
      const QualifiedName& name,
      const HeapVector<Member<Element>>* given_elements);
  const FrozenArray<Element>* GetElementArrayAttribute(
      const QualifiedName& name);

  const FrozenArray<Element>* ariaControlsElements();
  void setAriaControlsElements(HeapVector<Member<Element>>* given_elements);
  const FrozenArray<Element>* ariaDescribedByElements();
  void setAriaDescribedByElements(HeapVector<Member<Element>>* given_elements);
  const FrozenArray<Element>* ariaDetailsElements();
  void setAriaDetailsElements(HeapVector<Member<Element>>* given_elements);
  const FrozenArray<Element>* ariaErrorMessageElements();
  void setAriaErrorMessageElements(HeapVector<Member<Element>>* given_elements);
  const FrozenArray<Element>* ariaFlowToElements();
  void setAriaFlowToElements(HeapVector<Member<Element>>* given_elements);
  const FrozenArray<Element>* ariaLabelledByElements();
  void setAriaLabelledByElements(HeapVector<Member<Element>>* given_elements);
  const FrozenArray<Element>* ariaOwnsElements();
  void setAriaOwnsElements(HeapVector<Member<Element>>* given_elements);

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

  Member<const V8ControlValue> value_;
  Member<const V8ControlValue> state_;
  bool is_disabled_ = false;
  Member<ValidityStateFlags> validity_flags_;
  Member<Element> validation_anchor_;

  Member<CustomStateSet> custom_states_;

  HashMap<QualifiedName, AtomicString> accessibility_semantics_map_;

  // See
  // https://whatpr.org/html/3917/common-dom-interfaces.html#reflecting-content-attributes-in-idl-attributes:element
  HeapHashMap<QualifiedName, Member<FrozenArray<Element>>>
      explicitly_set_attr_elements_map_;
};

template <>
struct DowncastTraits<ElementInternals> {
  static bool AllowFrom(const ListedElement& listed_element) {
    return listed_element.IsElementInternals();
  }
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_ELEMENT_INTERNALS_H_

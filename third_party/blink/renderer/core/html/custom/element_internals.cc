// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/element_internals.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/custom/validity_state_flags.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/validity_state.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

namespace {
bool IsValidityStateFlagsValid(const ValidityStateFlags* flags) {
  if (!flags)
    return true;
  if (flags->badInput() || flags->customError() || flags->patternMismatch() ||
      flags->rangeOverflow() || flags->rangeUnderflow() ||
      flags->stepMismatch() || flags->tooLong() || flags->tooShort() ||
      flags->typeMismatch() || flags->valueMissing())
    return false;
  return true;
}
}  // anonymous namespace

class CustomStatesTokenList : public DOMTokenList {
 public:
  CustomStatesTokenList(Element& element)
      : DOMTokenList(element, g_null_name) {}

  AtomicString value() const override { return TokenSet().SerializeToString(); }

  void setValue(const AtomicString& new_value) override {
    DidUpdateAttributeValue(value(), new_value);
    // Should we have invalidation set for each of state tokens?
    GetElement().PseudoStateChanged(CSSSelector::kPseudoState);
  }
};

ElementInternals::ElementInternals(HTMLElement& target) : target_(target) {
  value_.SetUSVString(String());
}

void ElementInternals::Trace(Visitor* visitor) {
  visitor->Trace(target_);
  visitor->Trace(value_);
  visitor->Trace(state_);
  visitor->Trace(validity_flags_);
  visitor->Trace(validation_anchor_);
  visitor->Trace(custom_states_);
  visitor->Trace(explicitly_set_attr_elements_map_);
  ListedElement::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

void ElementInternals::setFormValue(const ControlValue& value,
                                    ExceptionState& exception_state) {
  setFormValue(value, value, exception_state);
}

void ElementInternals::setFormValue(const ControlValue& value,
                                    const ControlValue& state,
                                    ExceptionState& exception_state) {
  if (!IsTargetFormAssociated()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The target element is not a form-associated custom element.");
    return;
  }

  if (value.IsFormData()) {
    value_ = ControlValue::FromFormData(
        MakeGarbageCollected<FormData>(*value.GetAsFormData()));
  } else {
    value_ = value;
  }

  if (&value == &state) {
    state_ = value_;
  } else if (state.IsFormData()) {
    state_ = ControlValue::FromFormData(
        MakeGarbageCollected<FormData>(*state.GetAsFormData()));
  } else {
    state_ = state;
  }
  NotifyFormStateChanged();
}

HTMLFormElement* ElementInternals::form(ExceptionState& exception_state) const {
  if (!IsTargetFormAssociated()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The target element is not a form-associated custom element.");
    return nullptr;
  }
  return ListedElement::Form();
}

void ElementInternals::setValidity(ValidityStateFlags* flags,
                                   ExceptionState& exception_state) {
  setValidity(flags, String(), nullptr, exception_state);
}

void ElementInternals::setValidity(ValidityStateFlags* flags,
                                   const String& message,
                                   ExceptionState& exception_state) {
  setValidity(flags, message, nullptr, exception_state);
}

void ElementInternals::setValidity(ValidityStateFlags* flags,
                                   const String& message,
                                   Element* anchor,
                                   ExceptionState& exception_state) {
  if (!IsTargetFormAssociated()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The target element is not a form-associated custom element.");
    return;
  }
  // Custom element authors should provide a message. They can omit the message
  // argument only if nothing if | flags| is true.
  if (!IsValidityStateFlagsValid(flags) && message.IsEmpty()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTypeMismatchError,
        "The second argument should not be empty if one or more flags in the "
        "first argument are true.");
    return;
  }
  if (anchor && !Target().IsShadowIncludingAncestorOf(*anchor)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "The Element argument should be a shadow-including descendant of the "
        "target element.");
    return;
  }

  if (validation_anchor_ && validation_anchor_ != anchor) {
    HideVisibleValidationMessage();
  }
  validity_flags_ = flags;
  validation_anchor_ = anchor;
  SetCustomValidationMessage(message);
  SetNeedsValidityCheck();
}

bool ElementInternals::willValidate(ExceptionState& exception_state) const {
  if (!IsTargetFormAssociated()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The target element is not a form-associated custom element.");
    return false;
  }
  return WillValidate();
}

ValidityState* ElementInternals::validity(ExceptionState& exception_state) {
  if (!IsTargetFormAssociated()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The target element is not a form-associated custom element.");
    return nullptr;
  }
  return ListedElement::validity();
}

String ElementInternals::ValidationMessageForBinding(
    ExceptionState& exception_state) {
  if (!IsTargetFormAssociated()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The target element is not a form-associated custom element.");
    return String();
  }
  return validationMessage();
}

String ElementInternals::validationMessage() const {
  if (IsValidityStateFlagsValid(validity_flags_))
    return String();
  return CustomValidationMessage();
}

String ElementInternals::ValidationSubMessage() const {
  if (PatternMismatch())
    return Target().FastGetAttribute(html_names::kTitleAttr).GetString();
  return String();
}

Element& ElementInternals::ValidationAnchor() const {
  return validation_anchor_ ? *validation_anchor_ : Target();
}

bool ElementInternals::checkValidity(ExceptionState& exception_state) {
  if (!IsTargetFormAssociated()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The target element is not a form-associated custom element.");
    return false;
  }
  return ListedElement::checkValidity();
}

bool ElementInternals::reportValidity(ExceptionState& exception_state) {
  if (!IsTargetFormAssociated()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The target element is not a form-associated custom element.");
    return false;
  }
  return ListedElement::reportValidity();
}

LabelsNodeList* ElementInternals::labels(ExceptionState& exception_state) {
  if (!IsTargetFormAssociated()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The target element is not a form-associated custom element.");
    return nullptr;
  }
  return Target().labels();
}

DOMTokenList* ElementInternals::states() {
  if (!custom_states_)
    custom_states_ = MakeGarbageCollected<CustomStatesTokenList>(Target());
  return custom_states_;
}

bool ElementInternals::HasState(const AtomicString& state) const {
  return custom_states_ && custom_states_->contains(state);
}

const AtomicString& ElementInternals::FastGetAttribute(
    const QualifiedName& attribute) const {
  return accessibility_semantics_map_.at(attribute);
}

const HashMap<QualifiedName, AtomicString>& ElementInternals::GetAttributes()
    const {
  return accessibility_semantics_map_;
}

void ElementInternals::setAttribute(const QualifiedName& attribute,
                                    const AtomicString& value) {
  accessibility_semantics_map_.Set(attribute, value);
  if (AXObjectCache* cache = Target().GetDocument().ExistingAXObjectCache())
    cache->HandleAttributeChanged(attribute, &Target());
}

bool ElementInternals::HasAttribute(const QualifiedName& attribute) const {
  return accessibility_semantics_map_.Contains(attribute);
}

void ElementInternals::DidUpgrade() {
  ContainerNode* parent = Target().parentNode();
  if (!parent)
    return;
  InsertedInto(*parent);
  if (auto* owner_form = Form()) {
    if (auto* lists = owner_form->NodeLists())
      lists->InvalidateCaches(nullptr);
  }
  for (ContainerNode* node = parent; node; node = node->parentNode()) {
    if (IsA<HTMLFieldSetElement>(node)) {
      // TODO(tkent): Invalidate only HTMLFormControlsCollections.
      if (auto* lists = node->NodeLists())
        lists->InvalidateCaches(nullptr);
    }
  }
  Target().GetDocument().GetFormController().RestoreControlStateOnUpgrade(
      *this);
}

void ElementInternals::SetElementAttribute(const QualifiedName& name,
                                           Element* element) {
  auto result = explicitly_set_attr_elements_map_.insert(name, nullptr);
  if (result.is_new_entry) {
    result.stored_value->value =
        MakeGarbageCollected<HeapVector<Member<Element>>>();
  } else {
    result.stored_value->value->clear();
  }
  result.stored_value->value->push_back(element);
}

Element* ElementInternals::GetElementAttribute(const QualifiedName& name) {
  HeapVector<Member<Element>>* element_vector =
      explicitly_set_attr_elements_map_.at(name);
  if (!element_vector)
    return nullptr;
  DCHECK_EQ(element_vector->size(), 1u);
  return element_vector->at(0);
}

HeapVector<Member<Element>> ElementInternals::GetElementArrayAttribute(
    const QualifiedName& name,
    bool is_null) {
  is_null = true;
  auto iter = explicitly_set_attr_elements_map_.find(name);
  if (iter != explicitly_set_attr_elements_map_.end()) {
    is_null = false;
    return *(iter->value);
  }
  return HeapVector<Member<Element>>();
}

void ElementInternals::SetElementArrayAttribute(
    const QualifiedName& name,
    HeapVector<Member<Element>> elements,
    bool is_null) {
  if (is_null) {
    explicitly_set_attr_elements_map_.erase(name);
    return;
  }
  explicitly_set_attr_elements_map_.Set(
      name, MakeGarbageCollected<HeapVector<Member<Element>>>(elements));
}

bool ElementInternals::IsTargetFormAssociated() const {
  if (Target().IsFormAssociatedCustomElement())
    return true;
  if (Target().GetCustomElementState() != CustomElementState::kUndefined)
    return false;
  // An element is in "undefined" state in its constructor JavaScript code.
  // ElementInternals needs to handle elements to be form-associated same as
  // form-associated custom elements because web authors want to call
  // form-related operations of ElementInternals in constructors.
  CustomElementRegistry* registry = CustomElement::Registry(Target());
  if (!registry)
    return false;
  auto* definition = registry->DefinitionForName(Target().localName());
  return definition && definition->IsFormAssociated();
}

bool ElementInternals::IsFormControlElement() const {
  return false;
}

bool ElementInternals::IsElementInternals() const {
  return true;
}

bool ElementInternals::IsEnumeratable() const {
  return true;
}

void ElementInternals::AppendToFormData(FormData& form_data) {
  if (Target().IsDisabledFormControl())
    return;
  const AtomicString& name = Target().FastGetAttribute(html_names::kNameAttr);
  if (!value_.IsFormData()) {
    if (name.IsEmpty())
      return;
    if (value_.IsFile())
      form_data.AppendFromElement(name, value_.GetAsFile());
    else if (value_.IsUSVString())
      form_data.AppendFromElement(name, value_.GetAsUSVString());
    // Append nothing for null value.
    return;
  }
  for (const auto& entry : value_.GetAsFormData()->Entries()) {
    if (entry->isFile())
      form_data.append(entry->name(), entry->GetFile());
    else
      form_data.append(entry->name(), entry->Value());
  }
}

void ElementInternals::DidChangeForm() {
  ListedElement::DidChangeForm();
  CustomElement::EnqueueFormAssociatedCallback(Target(), Form());
}

bool ElementInternals::HasBadInput() const {
  return validity_flags_ && validity_flags_->badInput();
}

bool ElementInternals::PatternMismatch() const {
  return validity_flags_ && validity_flags_->patternMismatch();
}

bool ElementInternals::RangeOverflow() const {
  return validity_flags_ && validity_flags_->rangeOverflow();
}

bool ElementInternals::RangeUnderflow() const {
  return validity_flags_ && validity_flags_->rangeUnderflow();
}

bool ElementInternals::StepMismatch() const {
  return validity_flags_ && validity_flags_->stepMismatch();
}

bool ElementInternals::TooLong() const {
  return validity_flags_ && validity_flags_->tooLong();
}

bool ElementInternals::TooShort() const {
  return validity_flags_ && validity_flags_->tooShort();
}

bool ElementInternals::TypeMismatch() const {
  return validity_flags_ && validity_flags_->typeMismatch();
}

bool ElementInternals::ValueMissing() const {
  return validity_flags_ && validity_flags_->valueMissing();
}

bool ElementInternals::CustomError() const {
  return validity_flags_ && validity_flags_->customError();
}

void ElementInternals::DisabledStateMightBeChanged() {
  bool new_disabled = IsActuallyDisabled();
  if (is_disabled_ == new_disabled)
    return;
  is_disabled_ = new_disabled;
  CustomElement::EnqueueFormDisabledCallback(Target(), new_disabled);
}

bool ElementInternals::ClassSupportsStateRestore() const {
  return true;
}

bool ElementInternals::ShouldSaveAndRestoreFormControlState() const {
  // We don't save/restore control state in a form with autocomplete=off.
  return Target().isConnected() && (!Form() || Form()->ShouldAutocomplete());
}

FormControlState ElementInternals::SaveFormControlState() const {
  FormControlState state;
  if (value_.IsUSVString()) {
    state.Append("USVString");
    state.Append(value_.GetAsUSVString());
  } else if (value_.IsFile()) {
    state.Append("File");
    File* file = value_.GetAsFile();
    file->AppendToControlState(state);
  } else if (value_.IsFormData()) {
    state.Append("FormData");
    value_.GetAsFormData()->AppendToControlState(state);
  }
  // Add nothing if value_.IsNull().
  return state;
}

void ElementInternals::RestoreFormControlState(const FormControlState& state) {
  if (state.ValueSize() < 2)
    return;
  if (state[0] == "USVString") {
    value_ = ControlValue::FromUSVString(state[1]);
  } else if (state[0] == "File") {
    wtf_size_t i = 1;
    if (auto* file = File::CreateFromControlState(state, i))
      value_ = ControlValue::FromFile(file);
  } else if (state[0] == "FormData") {
    wtf_size_t i = 1;
    if (auto* form_data = FormData::CreateFromControlState(state, i))
      value_ = ControlValue::FromFormData(form_data);
  }
  if (!value_.IsNull())
    CustomElement::EnqueueFormStateRestoreCallback(Target(), value_, "restore");
}

}  // namespace blink

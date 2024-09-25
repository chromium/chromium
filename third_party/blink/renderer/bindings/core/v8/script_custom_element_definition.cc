// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_custom_element_definition.h"

#include "third_party/blink/renderer/bindings/core/v8/script_custom_element_definition_data.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_custom_element_adopted_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_custom_element_attribute_changed_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_custom_element_constructor.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_custom_element_form_associated_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_custom_element_form_disabled_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_custom_element_form_state_restore_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_form_state_restore_mode.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_void_function.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"

namespace blink {

ScriptCustomElementDefinition::ScriptCustomElementDefinition(
    const ScriptCustomElementDefinitionData& data,
    const CustomElementDescriptor& descriptor)
    : CustomElementDefinition(*data.registry_,
                              descriptor,
                              std::move(data.observed_attributes_),
                              data.disabled_features_,
                              data.is_form_associated_
                                  ? FormAssociationFlag::kYes
                                  : FormAssociationFlag::kNo),
      script_state_(data.script_state_),
      constructor_(data.constructor_),
      connected_callback_(data.connected_callback_),
      disconnected_callback_(data.disconnected_callback_),
      adopted_callback_(data.adopted_callback_),
      attribute_changed_callback_(data.attribute_changed_callback_),
      form_associated_callback_(data.form_associated_callback_),
      form_reset_callback_(data.form_reset_callback_),
      form_disabled_callback_(data.form_disabled_callback_),
      form_state_restore_callback_(data.form_state_restore_callback_) {
  DCHECK(data.registry_);
}

void ScriptCustomElementDefinition::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(constructor_);
  visitor->Trace(connected_callback_);
  visitor->Trace(disconnected_callback_);
  visitor->Trace(adopted_callback_);
  visitor->Trace(attribute_changed_callback_);
  visitor->Trace(form_associated_callback_);
  visitor->Trace(form_reset_callback_);
  visitor->Trace(form_disabled_callback_);
  visitor->Trace(form_state_restore_callback_);
  CustomElementDefinition::Trace(visitor);
}

HTMLElement* ScriptCustomElementDefinition::CreateAutonomousCustomElementSync(
    Document& document,
    const QualifiedName& tag_name) {
  DCHECK(CustomElement::ShouldCreateCustomElement(tag_name)) << tag_name;
  if (!script_state_->ContextIsValid())
    return CustomElement::CreateFailedElement(document, tag_name);
  ScriptState::Scope scope(script_state_);
  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::TryCatch try_catch(isolate);

  // Create an element with the synchronous custom elements flag set.
  // https://dom.spec.whatwg.org/#concept-create-element

  // TODO(dominicc): Implement step 5 which constructs customized
  // built-in elements.

  Element* element = nullptr;
  {
    element = CallConstructor();
    if (try_catch.HasCaught()) {
      // 6.1."If any of these subsubsteps threw an exception".1
      // Report the exception.
      V8ScriptRunner::ReportException(isolate, try_catch.Exception());
      // ... .2 Return HTMLUnknownElement.
      return CustomElement::CreateFailedElement(document, tag_name);
    }
  }

  // 6.1.3. through 6.1.9.
  CheckConstructorResult(element, document, tag_name,
                         PassThroughException(isolate));
  if (try_catch.HasCaught()) {
    // 6.1."If any of these subsubsteps threw an exception".1
    // Report the exception.
    V8ScriptRunner::ReportException(isolate, try_catch.Exception());
    // ... .2 Return HTMLUnknownElement.
    return CustomElement::CreateFailedElement(document, tag_name);
  }
  // 6.1.10. Set result’s namespace prefix to prefix.
  if (element->prefix() != tag_name.Prefix())
    element->SetTagNameForCreateElementNS(tag_name);
  DCHECK_EQ(element->GetCustomElementState(), CustomElementState::kCustom);
  return To<HTMLElement>(element);
}

// https://html.spec.whatwg.org/C/#upgrades
bool ScriptCustomElementDefinition::RunConstructor(Element& element) {
  if (!script_state_->ContextIsValid())
    return false;
  ScriptState::Scope scope(script_state_);
  v8::Isolate* isolate = script_state_->GetIsolate();

  // Step 5 says to rethrow the exception; but there is no one to
  // catch it. The side effect is to report the error.
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  if (DisableShadow() && element.GetShadowRoot()) {
    v8::Local<v8::Value> exception = V8ThrowDOMException::CreateOrEmpty(
        script_state_->GetIsolate(), DOMExceptionCode::kNotSupportedError,
        "The element already has a ShadowRoot though it is disabled by "
        "disabledFeatures static field.");
    if (!exception.IsEmpty())
      V8ScriptRunner::ReportException(isolate, exception);
    return false;
  }

  // 8.1.new: set custom element state to kPreCustomized.
  element.SetCustomElementState(CustomElementState::kPreCustomized);

  Element* result = CallConstructor();

  // To report exception thrown from callConstructor()
  if (try_catch.HasCaught())
    return false;

  // Report a TypeError Exception if the constructor returns a different object.
  if (result != &element) {
    const String& message =
        "custom element constructors must call super() first and must "
        "not return a different object";
    v8::Local<v8::Value> exception =
        V8ThrowException::CreateTypeError(script_state_->GetIsolate(), message);
    if (!exception.IsEmpty())
      V8ScriptRunner::ReportException(isolate, exception);
    return false;
  }

  return true;
}

Element* ScriptCustomElementDefinition::CallConstructor() {
  ScriptValue result;
  if (!constructor_->Construct().To(&result)) {
    return nullptr;
  }

  return V8Element::ToWrappable(constructor_->GetIsolate(), result.V8Value());
}

v8::Local<v8::Object> ScriptCustomElementDefinition::Constructor() const {
  return constructor_->CallbackObject();
}

// CustomElementDefinition
ScriptValue ScriptCustomElementDefinition::GetConstructorForScript() {
  return ScriptValue(script_state_->GetIsolate(), Constructor());
}

bool ScriptCustomElementDefinition::HasConnectedCallback() const {
  return connected_callback_ != nullptr;
}

bool ScriptCustomElementDefinition::HasDisconnectedCallback() const {
  return disconnected_callback_ != nullptr;
}

bool ScriptCustomElementDefinition::HasAdoptedCallback() const {
  return adopted_callback_ != nullptr;
}

bool ScriptCustomElementDefinition::HasFormAssociatedCallback() const {
  return form_associated_callback_ != nullptr;
}

bool ScriptCustomElementDefinition::HasFormResetCallback() const {
  return form_reset_callback_ != nullptr;
}

bool ScriptCustomElementDefinition::HasFormDisabledCallback() const {
  return form_disabled_callback_ != nullptr;
}

bool ScriptCustomElementDefinition::HasFormStateRestoreCallback() const {
  return form_state_restore_callback_ != nullptr;
}

void ScriptCustomElementDefinition::RunConnectedCallback(Element& element) {
  if (!connected_callback_)
    return;

  connected_callback_->InvokeAndReportException(&element);
}

void ScriptCustomElementDefinition::RunDisconnectedCallback(Element& element) {
  if (!disconnected_callback_)
    return;

  disconnected_callback_->InvokeAndReportException(&element);
}

void ScriptCustomElementDefinition::RunAdoptedCallback(Element& element,
                                                       Document& old_owner,
                                                       Document& new_owner) {
  if (!adopted_callback_)
    return;

  adopted_callback_->InvokeAndReportException(&element, &old_owner, &new_owner);
}

void ScriptCustomElementDefinition::RunAttributeChangedCallback(
    Element& element,
    const QualifiedName& name,
    const AtomicString& old_value,
    const AtomicString& new_value) {
  if (!attribute_changed_callback_)
    return;

  attribute_changed_callback_->InvokeAndReportException(
      &element, name.LocalName(), old_value, new_value, name.NamespaceURI());
}

void ScriptCustomElementDefinition::RunFormAssociatedCallback(
    Element& element,
    HTMLFormElement* nullable_form) {
  if (!form_associated_callback_)
    return;
  form_associated_callback_->InvokeAndReportException(&element, nullable_form);
}

void ScriptCustomElementDefinition::RunFormResetCallback(Element& element) {
  if (!form_reset_callback_)
    return;
  form_reset_callback_->InvokeAndReportException(&element);
}

void ScriptCustomElementDefinition::RunFormDisabledCallback(Element& element,
                                                            bool is_disabled) {
  if (!form_disabled_callback_)
    return;
  form_disabled_callback_->InvokeAndReportException(&element, is_disabled);
}

void ScriptCustomElementDefinition::RunFormStateRestoreCallback(
    Element& element,
    const V8ControlValue* value,
    const String& mode) {
  if (!form_state_restore_callback_)
    return;
  form_state_restore_callback_->InvokeAndReportException(
      &element, value, V8FormStateRestoreMode::Create(mode).value());
}

}  // namespace blink

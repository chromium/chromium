// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_custom_element_definition.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_custom_element_constructor.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_custom_element_registry.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "v8/include/v8.h"

namespace blink {

class CSSStyleSheet;

ScriptCustomElementDefinition* ScriptCustomElementDefinition::ForConstructor(
    ScriptState* script_state,
    CustomElementRegistry* registry,
    v8::Local<v8::Value> constructor) {
  V8PerContextData* per_context_data = script_state->PerContextData();
  // TODO(yukishiino): Remove this check when crbug.com/583429 is fixed.
  if (UNLIKELY(!per_context_data))
    return nullptr;
  auto private_id = per_context_data->GetPrivateCustomElementDefinitionId();
  v8::Local<v8::Value> id_value;
  if (!constructor.As<v8::Object>()
           ->GetPrivate(script_state->GetContext(), private_id)
           .ToLocal(&id_value))
    return nullptr;
  if (!id_value->IsUint32())
    return nullptr;
  uint32_t id = id_value.As<v8::Uint32>()->Value();

  // This downcast is safe because only ScriptCustomElementDefinitions
  // have an ID associated with them. This relies on three things:
  //
  // 1. Only ScriptCustomElementDefinition::Create sets the private
  //    property on a constructor.
  //
  // 2. CustomElementRegistry adds ScriptCustomElementDefinitions
  //    assigned an ID to the list of definitions without fail.
  //
  // 3. The relationship between the CustomElementRegistry and its
  //    private property is never mixed up; this is guaranteed by the
  //    bindings system because the registry is associated with its
  //    context.
  //
  // At a meta-level, this downcast is safe because there is
  // currently only one implementation of CustomElementDefinition in
  // product code and that is ScriptCustomElementDefinition. But
  // that may change in the future.
  CustomElementDefinition* definition = registry->DefinitionForId(id);
  CHECK(definition);
  return static_cast<ScriptCustomElementDefinition*>(definition);
}

ScriptCustomElementDefinition* ScriptCustomElementDefinition::Create(
    ScriptState* script_state,
    CustomElementRegistry* registry,
    const CustomElementDescriptor& descriptor,
    CustomElementDefinition::Id id,
    V8CustomElementConstructor* constructor,
    V8Function* connected_callback,
    V8Function* disconnected_callback,
    V8Function* adopted_callback,
    V8Function* attribute_changed_callback,
    HashSet<AtomicString>&& observed_attributes) {
  ScriptCustomElementDefinition* definition = new ScriptCustomElementDefinition(
      script_state, descriptor, constructor, connected_callback,
      disconnected_callback, adopted_callback, attribute_changed_callback,
      std::move(observed_attributes));

  // Tag the JavaScript constructor object with its ID.
  v8::Local<v8::Value> id_value =
      v8::Integer::NewFromUnsigned(script_state->GetIsolate(), id);
  auto private_id =
      script_state->PerContextData()->GetPrivateCustomElementDefinitionId();
  CHECK(constructor->CallbackObject()
            ->SetPrivate(script_state->GetContext(), private_id, id_value)
            .ToChecked());

  return definition;
}

ScriptCustomElementDefinition::ScriptCustomElementDefinition(
    ScriptState* script_state,
    const CustomElementDescriptor& descriptor,
    V8CustomElementConstructor* constructor,
    V8Function* connected_callback,
    V8Function* disconnected_callback,
    V8Function* adopted_callback,
    V8Function* attribute_changed_callback,
    HashSet<AtomicString>&& observed_attributes)
    : CustomElementDefinition(descriptor, std::move(observed_attributes)),
      script_state_(script_state),
      constructor_(constructor),
      connected_callback_(connected_callback),
      disconnected_callback_(disconnected_callback),
      adopted_callback_(adopted_callback),
      attribute_changed_callback_(attribute_changed_callback) {}

void ScriptCustomElementDefinition::Trace(Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(constructor_);
  visitor->Trace(connected_callback_);
  visitor->Trace(disconnected_callback_);
  visitor->Trace(adopted_callback_);
  visitor->Trace(attribute_changed_callback_);
  CustomElementDefinition::Trace(visitor);
}

HTMLElement* ScriptCustomElementDefinition::HandleCreateElementSyncException(
    Document& document,
    const QualifiedName& tag_name,
    v8::Isolate* isolate,
    ExceptionState& exception_state) {
  DCHECK(exception_state.HadException());
  // 6.1."If any of these subsubsteps threw an exception".1
  // Report the exception.
  V8ScriptRunner::ReportException(isolate, exception_state.GetException());
  exception_state.ClearException();
  // ... .2 Return HTMLUnknownElement.
  return CustomElement::CreateFailedElement(document, tag_name);
}

HTMLElement* ScriptCustomElementDefinition::CreateAutonomousCustomElementSync(
    Document& document,
    const QualifiedName& tag_name) {
  if (!script_state_->ContextIsValid())
    return CustomElement::CreateFailedElement(document, tag_name);
  ScriptState::Scope scope(script_state_);
  v8::Isolate* isolate = script_state_->GetIsolate();

  ExceptionState exception_state(isolate, ExceptionState::kConstructionContext,
                                 "CustomElement");

  // Create an element with the synchronous custom elements flag set.
  // https://dom.spec.whatwg.org/#concept-create-element

  // TODO(dominicc): Implement step 5 which constructs customized
  // built-in elements.

  Element* element = nullptr;
  {
    v8::TryCatch try_catch(script_state_->GetIsolate());

    if (document.IsHTMLImport()) {
      // V8HTMLElement::constructorCustom() can only refer to
      // window.document() which is not the import document. Create
      // elements in import documents ahead of time so they end up in
      // the right document. This subtly violates recursive
      // construction semantics, but only in import documents.
      element = CreateElementForConstructor(document);
      DCHECK(!try_catch.HasCaught());

      ConstructionStackScope construction_stack_scope(this, element);
      element = CallConstructor();
    } else {
      element = CallConstructor();
    }

    if (try_catch.HasCaught()) {
      exception_state.RethrowV8Exception(try_catch.Exception());
      return HandleCreateElementSyncException(document, tag_name, isolate,
                                              exception_state);
    }
  }

  // 6.1.3. through 6.1.9.
  CheckConstructorResult(element, document, tag_name, exception_state);
  if (exception_state.HadException()) {
    return HandleCreateElementSyncException(document, tag_name, isolate,
                                            exception_state);
  }
  // 6.1.10. Set result’s namespace prefix to prefix.
  if (element->prefix() != tag_name.Prefix())
    element->SetTagNameForCreateElementNS(tag_name);
  DCHECK_EQ(element->GetCustomElementState(), CustomElementState::kCustom);
  AddDefaultStylesTo(*element);
  return ToHTMLElement(element);
}

// https://html.spec.whatwg.org/multipage/scripting.html#upgrades
bool ScriptCustomElementDefinition::RunConstructor(Element* element) {
  if (!script_state_->ContextIsValid())
    return false;
  ScriptState::Scope scope(script_state_);
  v8::Isolate* isolate = script_state_->GetIsolate();

  // Step 5 says to rethrow the exception; but there is no one to
  // catch it. The side effect is to report the error.
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  Element* result = CallConstructor();

  // To report exception thrown from callConstructor()
  if (try_catch.HasCaught())
    return false;

  // To report InvalidStateError Exception, when the constructor returns some
  // different object
  if (result != element) {
    const String& message =
        "custom element constructors must call super() first and must "
        "not return a different object";
    v8::Local<v8::Value> exception = V8ThrowDOMException::CreateOrEmpty(
        script_state_->GetIsolate(), DOMExceptionCode::kInvalidStateError,
        message);
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

  return V8Element::ToImplWithTypeCheck(constructor_->GetIsolate(),
                                        result.V8Value());
}

v8::Local<v8::Object> ScriptCustomElementDefinition::Constructor() const {
  return constructor_->CallbackObject();
}

// CustomElementDefinition
ScriptValue ScriptCustomElementDefinition::GetConstructorForScript() {
  return ScriptValue(script_state_, Constructor());
}

bool ScriptCustomElementDefinition::HasConnectedCallback() const {
  return connected_callback_;
}

bool ScriptCustomElementDefinition::HasDisconnectedCallback() const {
  return disconnected_callback_;
}

bool ScriptCustomElementDefinition::HasAdoptedCallback() const {
  return adopted_callback_;
}

void ScriptCustomElementDefinition::RunConnectedCallback(Element* element) {
  if (!connected_callback_)
    return;

  connected_callback_->InvokeAndReportException(element, Vector<ScriptValue>());
}

void ScriptCustomElementDefinition::RunDisconnectedCallback(Element* element) {
  if (!disconnected_callback_)
    return;

  disconnected_callback_->InvokeAndReportException(element,
                                                   Vector<ScriptValue>());
}

void ScriptCustomElementDefinition::RunAdoptedCallback(Element* element,
                                                       Document* old_owner,
                                                       Document* new_owner) {
  if (!adopted_callback_)
    return;

  ScriptState* script_state = adopted_callback_->CallbackRelevantScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);
  Vector<ScriptValue> args({
      ScriptValue(script_state, ToV8(old_owner, script_state)),
      ScriptValue(script_state, ToV8(new_owner, script_state)),
  });
  adopted_callback_->InvokeAndReportException(element, args);
}

void ScriptCustomElementDefinition::RunAttributeChangedCallback(
    Element* element,
    const QualifiedName& name,
    const AtomicString& old_value,
    const AtomicString& new_value) {
  if (!attribute_changed_callback_)
    return;

  v8::Isolate* isolate = attribute_changed_callback_->GetIsolate();
  ScriptState* script_state =
      attribute_changed_callback_->CallbackRelevantScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);
  Vector<ScriptValue> args({
      ScriptValue(script_state, V8String(isolate, name.LocalName())),
      ScriptValue(script_state, V8StringOrNull(isolate, old_value)),
      ScriptValue(script_state, V8StringOrNull(isolate, new_value)),
      ScriptValue(script_state, V8StringOrNull(isolate, name.NamespaceURI())),
  });
  attribute_changed_callback_->InvokeAndReportException(element, args);
}

}  // namespace blink

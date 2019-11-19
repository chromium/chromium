// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface_event_target.h"

#include <algorithm>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo v8_test_interface_event_target_wrapper_type_info = {
    gin::kEmbedderBlink,
    V8TestInterfaceEventTarget::DomTemplate,
    nullptr,
    "TestInterfaceEventTarget",
    V8EventTarget::GetWrapperTypeInfo(),
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterfaceEventTarget.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInterfaceEventTarget::wrapper_type_info_ = v8_test_interface_event_target_wrapper_type_info;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestInterfaceEventTarget>::value,
    "TestInterfaceEventTarget inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestInterfaceEventTarget::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInterfaceEventTarget is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_interface_event_target_v8_internal {

}  // namespace test_interface_event_target_v8_internal

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo v8_test_interface_event_target_constructor_wrapper_type_info = {
    gin::kEmbedderBlink,
    V8TestInterfaceEventTargetConstructor::DomTemplate,
    nullptr,
    "TestInterfaceEventTarget",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

static void V8TestInterfaceEventTargetConstructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceEventTarget_ConstructorCallback");

  if (!info.IsConstructCall()) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::ConstructorNotCallableAsFunction("Name"));
    return;
  }

  if (ConstructorMode::Current(info.GetIsolate()) == ConstructorMode::kWrapExistingObject) {
    V8SetReturnValue(info, info.Holder());
    return;
  }

  Document& document = *To<Document>(ToExecutionContext(
      info.NewTarget().As<v8::Object>()->CreationContext()));
  TestInterfaceEventTarget* impl = TestInterfaceEventTarget::CreateForJSConstructor(document);
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), V8TestInterfaceEventTargetConstructor::GetWrapperTypeInfo(), wrapper);
  V8SetReturnValue(info, wrapper);
}

v8::Local<v8::FunctionTemplate> V8TestInterfaceEventTargetConstructor::DomTemplate(
    v8::Isolate* isolate, const DOMWrapperWorld& world) {
  static int dom_template_key; // This address is used for a key to look up the dom template.
  V8PerIsolateData* data = V8PerIsolateData::From(isolate);
  v8::Local<v8::FunctionTemplate> result =
      data->FindInterfaceTemplate(world, &dom_template_key);
  if (!result.IsEmpty())
    return result;

  result = v8::FunctionTemplate::New(isolate, V8TestInterfaceEventTargetConstructorCallback);
  v8::Local<v8::ObjectTemplate> instance_template = result->InstanceTemplate();
  instance_template->SetInternalFieldCount(V8TestInterfaceEventTarget::kInternalFieldCount);
  result->SetClassName(V8AtomicString(isolate, "Name"));
  result->Inherit(V8TestInterfaceEventTarget::DomTemplate(isolate, world));
  data->SetInterfaceTemplate(world, &dom_template_key, result);
  return result;
}

void V8TestInterfaceEventTargetConstructor::NamedConstructorAttributeGetter(
    v8::Local<v8::Name> property_name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Context> creationContext = info.Holder()->CreationContext();
  V8PerContextData* per_context_data = V8PerContextData::From(creationContext);
  if (!per_context_data) {
    // TODO(yukishiino): Return a valid named constructor even after the context is detached
    return;
  }

  v8::Local<v8::Function> named_constructor =
      per_context_data->ConstructorForType(V8TestInterfaceEventTargetConstructor::GetWrapperTypeInfo());

  // Set the prototype of named constructors to the regular constructor.
  static const V8PrivateProperty::SymbolKey kPrivatePropertyInitialized;
  auto private_property =
      V8PrivateProperty::GetSymbol(
          info.GetIsolate(), kPrivatePropertyInitialized);
  v8::Local<v8::Context> current_context = info.GetIsolate()->GetCurrentContext();
  v8::Local<v8::Value> private_value;

  if (!private_property.GetOrUndefined(named_constructor).ToLocal(&private_value) ||
      private_value->IsUndefined()) {
    v8::Local<v8::Function> interface =
        per_context_data->ConstructorForType(V8TestInterfaceEventTarget::GetWrapperTypeInfo());
    v8::Local<v8::Value> interface_prototype =
        interface->Get(current_context, V8AtomicString(info.GetIsolate(), "prototype"))
        .ToLocalChecked();
    // https://heycam.github.io/webidl/#named-constructors
    // 8. Perform ! DefinePropertyOrThrow(F, "prototype",
    //        PropertyDescriptor{[[Value]]: proto, [[Writable]]: false,
    //                           [[Enumerable]]: false,
    //                           [Configurable]]: false}).
    const v8::PropertyAttribute prototype_attributes =
        static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontEnum | v8::DontDelete);
    bool result = named_constructor->DefineOwnProperty(
        current_context, V8AtomicString(info.GetIsolate(), "prototype"),
        interface_prototype, prototype_attributes).ToChecked();
    CHECK(result);
    private_property.Set(named_constructor, v8::True(info.GetIsolate()));
  }

  V8SetReturnValue(info, named_constructor);
}

static void InstallV8TestInterfaceEventTargetTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interface_template) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interface_template, V8TestInterfaceEventTarget::GetWrapperTypeInfo()->interface_name, V8EventTarget::DomTemplate(isolate, world), V8TestInterfaceEventTarget::kInternalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instance_template = interface_template->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instance_template);
  v8::Local<v8::ObjectTemplate> prototype_template = interface_template->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototype_template);

  // Register IDL constants, attributes and operations.

  // Custom signature

  V8TestInterfaceEventTarget::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interface_template);
}

void V8TestInterfaceEventTarget::InstallRuntimeEnabledFeaturesOnTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interface_template) {
  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instance_template = interface_template->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instance_template);
  v8::Local<v8::ObjectTemplate> prototype_template = interface_template->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototype_template);

  // Register IDL constants, attributes and operations.

  // Custom signature
}

v8::Local<v8::FunctionTemplate> V8TestInterfaceEventTarget::DomTemplate(
    v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(
      isolate, world, const_cast<WrapperTypeInfo*>(V8TestInterfaceEventTarget::GetWrapperTypeInfo()),
      InstallV8TestInterfaceEventTargetTemplate);
}

bool V8TestInterfaceEventTarget::HasInstance(v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(V8TestInterfaceEventTarget::GetWrapperTypeInfo(), v8_value);
}

v8::Local<v8::Object> V8TestInterfaceEventTarget::FindInstanceInPrototypeChain(
    v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(
      V8TestInterfaceEventTarget::GetWrapperTypeInfo(), v8_value);
}

TestInterfaceEventTarget* V8TestInterfaceEventTarget::ToImplWithTypeCheck(
    v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return HasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterfaceEventTarget* NativeValueTraits<TestInterfaceEventTarget>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  TestInterfaceEventTarget* native_value = V8TestInterfaceEventTarget::ToImplWithTypeCheck(isolate, value);
  if (!native_value) {
    exception_state.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterfaceEventTarget"));
  }
  return native_value;
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface_node.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_listener_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface_empty.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_processing_stack.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestInterfaceNode::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestInterfaceNode::domTemplate,
    nullptr,
    "TestInterfaceNode",
    &V8Node::wrapperTypeInfo,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kNodeClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterfaceNode.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInterfaceNode::wrapper_type_info_ = V8TestInterfaceNode::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestInterfaceNode>::value,
    "TestInterfaceNode inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestInterfaceNode::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInterfaceNode is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_interface_node_v8_internal {

static void nodeNameAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  V8SetReturnValueString(info, impl->nodeName(), info.GetIsolate());
}

static void nodeNameAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setNodeName(cppValue);
}

static void stringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  V8SetReturnValueString(info, impl->stringAttribute(), info.GetIsolate());
}

static void stringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setStringAttribute(cppValue);
}

static void readonlyTestInterfaceEmptyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->readonlyTestInterfaceEmptyAttribute()), impl);
}

static void eventHandlerAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  EventListener* cppValue(WTF::GetPtr(impl->eventHandlerAttribute()));

  V8SetReturnValue(info, JSBasedEventListener::GetListenerOrNull(info.GetIsolate(), impl, cppValue));
}

static void eventHandlerAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  // Prepare the value to be set.

  impl->setEventHandlerAttribute(V8EventListenerHelper::GetEventHandler(ScriptState::ForRelevantRealm(info), v8Value, JSEventHandler::HandlerType::kEventHandler, kListenerFindOrCreate));
}

static void perWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->perWorldBindingsReadonlyTestInterfaceEmptyAttribute()), impl);
}

static void perWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  V8SetReturnValueForMainWorld(info, WTF::GetPtr(impl->perWorldBindingsReadonlyTestInterfaceEmptyAttribute()));
}

static void reflectStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  V8SetReturnValueString(info, impl->FastGetAttribute(HTMLNames::reflectstringattributeAttr), info.GetIsolate());
}

static void reflectStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(HTMLNames::reflectstringattributeAttr, cppValue);
}

static void reflectUrlStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  V8SetReturnValueString(info, impl->GetURLAttribute(HTMLNames::reflecturlstringattributeAttr), info.GetIsolate());
}

static void reflectUrlStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(HTMLNames::reflecturlstringattributeAttr, cppValue);
}

static void testInterfaceEmptyMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(info.Holder());

  V8SetReturnValueFast(info, impl->testInterfaceEmptyMethod(), impl);
}

static void perWorldBindingsTestInterfaceEmptyMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(info.Holder());

  V8SetReturnValueFast(info, impl->perWorldBindingsTestInterfaceEmptyMethod(), impl);
}

static void perWorldBindingsTestInterfaceEmptyMethodMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(info.Holder());

  V8SetReturnValueForMainWorld(info, impl->perWorldBindingsTestInterfaceEmptyMethod());
}

static void perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceNode", "perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArg");

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(info.Holder());

  bool optionalBooleanArgument;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    V8SetReturnValueFast(info, impl->perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArg(), impl);
    return;
  }
  optionalBooleanArgument = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  V8SetReturnValueFast(info, impl->perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArg(optionalBooleanArgument), impl);
}

static void perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArgMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceNode", "perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArg");

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(info.Holder());

  bool optionalBooleanArgument;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    V8SetReturnValueForMainWorld(info, impl->perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArg());
    return;
  }
  optionalBooleanArgument = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  V8SetReturnValueForMainWorld(info, impl->perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArg(optionalBooleanArgument));
}

}  // namespace test_interface_node_v8_internal

void V8TestInterfaceNode::nodeNameAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_nodeName_Getter");

  test_interface_node_v8_internal::nodeNameAttributeGetter(info);
}

void V8TestInterfaceNode::nodeNameAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_nodeName_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_node_v8_internal::nodeNameAttributeSetter(v8Value, info);
}

void V8TestInterfaceNode::stringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_stringAttribute_Getter");

  test_interface_node_v8_internal::stringAttributeAttributeGetter(info);
}

void V8TestInterfaceNode::stringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_stringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_node_v8_internal::stringAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceNode::readonlyTestInterfaceEmptyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_readonlyTestInterfaceEmptyAttribute_Getter");

  test_interface_node_v8_internal::readonlyTestInterfaceEmptyAttributeAttributeGetter(info);
}

void V8TestInterfaceNode::eventHandlerAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_eventHandlerAttribute_Getter");

  test_interface_node_v8_internal::eventHandlerAttributeAttributeGetter(info);
}

void V8TestInterfaceNode::eventHandlerAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_eventHandlerAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_node_v8_internal::eventHandlerAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceNode::perWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_perWorldBindingsReadonlyTestInterfaceEmptyAttribute_Getter");

  test_interface_node_v8_internal::perWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetter(info);
}

void V8TestInterfaceNode::perWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_perWorldBindingsReadonlyTestInterfaceEmptyAttribute_Getter");

  test_interface_node_v8_internal::perWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterForMainWorld(info);
}

void V8TestInterfaceNode::reflectStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_reflectStringAttribute_Getter");

  test_interface_node_v8_internal::reflectStringAttributeAttributeGetter(info);
}

void V8TestInterfaceNode::reflectStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_reflectStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_node_v8_internal::reflectStringAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceNode::reflectUrlStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_reflectUrlStringAttribute_Getter");

  test_interface_node_v8_internal::reflectUrlStringAttributeAttributeGetter(info);
}

void V8TestInterfaceNode::reflectUrlStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_reflectUrlStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_node_v8_internal::reflectUrlStringAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceNode::testInterfaceEmptyMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_testInterfaceEmptyMethod");

  test_interface_node_v8_internal::testInterfaceEmptyMethodMethod(info);
}

void V8TestInterfaceNode::perWorldBindingsTestInterfaceEmptyMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_perWorldBindingsTestInterfaceEmptyMethod");

  test_interface_node_v8_internal::perWorldBindingsTestInterfaceEmptyMethodMethod(info);
}

void V8TestInterfaceNode::perWorldBindingsTestInterfaceEmptyMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_perWorldBindingsTestInterfaceEmptyMethod");

  test_interface_node_v8_internal::perWorldBindingsTestInterfaceEmptyMethodMethodForMainWorld(info);
}

void V8TestInterfaceNode::perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArg");

  test_interface_node_v8_internal::perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArgMethod(info);
}

void V8TestInterfaceNode::perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArgMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArg");

  test_interface_node_v8_internal::perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArgMethodForMainWorld(info);
}

static const V8DOMConfiguration::AccessorConfiguration V8TestInterfaceNodeAccessors[] = {
    { "nodeName", V8TestInterfaceNode::nodeNameAttributeGetterCallback, V8TestInterfaceNode::nodeNameAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "stringAttribute", V8TestInterfaceNode::stringAttributeAttributeGetterCallback, V8TestInterfaceNode::stringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "readonlyTestInterfaceEmptyAttribute", V8TestInterfaceNode::readonlyTestInterfaceEmptyAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "eventHandlerAttribute", V8TestInterfaceNode::eventHandlerAttributeAttributeGetterCallback, V8TestInterfaceNode::eventHandlerAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "perWorldBindingsReadonlyTestInterfaceEmptyAttribute", V8TestInterfaceNode::perWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterCallbackForMainWorld, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kMainWorld },
    { "perWorldBindingsReadonlyTestInterfaceEmptyAttribute", V8TestInterfaceNode::perWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kNonMainWorlds },
    { "reflectStringAttribute", V8TestInterfaceNode::reflectStringAttributeAttributeGetterCallback, V8TestInterfaceNode::reflectStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "reflectUrlStringAttribute", V8TestInterfaceNode::reflectUrlStringAttributeAttributeGetterCallback, V8TestInterfaceNode::reflectUrlStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
};

static const V8DOMConfiguration::MethodConfiguration V8TestInterfaceNodeMethods[] = {
    {"testInterfaceEmptyMethod", V8TestInterfaceNode::testInterfaceEmptyMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"perWorldBindingsTestInterfaceEmptyMethod", V8TestInterfaceNode::perWorldBindingsTestInterfaceEmptyMethodMethodCallbackForMainWorld, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
    {"perWorldBindingsTestInterfaceEmptyMethod", V8TestInterfaceNode::perWorldBindingsTestInterfaceEmptyMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds},
    {"perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArg", V8TestInterfaceNode::perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArgMethodCallbackForMainWorld, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
    {"perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArg", V8TestInterfaceNode::perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds},
};

static void installV8TestInterfaceNodeTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestInterfaceNode::wrapperTypeInfo.interface_name, V8Node::domTemplate(isolate, world), V8TestInterfaceNode::internalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.
  V8DOMConfiguration::InstallAccessors(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestInterfaceNodeAccessors, base::size(V8TestInterfaceNodeAccessors));
  V8DOMConfiguration::InstallMethods(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestInterfaceNodeMethods, base::size(V8TestInterfaceNodeMethods));

  // Custom signature

  V8TestInterfaceNode::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestInterfaceNode::InstallRuntimeEnabledFeaturesOnTemplate(
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

v8::Local<v8::FunctionTemplate> V8TestInterfaceNode::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestInterfaceNodeTemplate);
}

bool V8TestInterfaceNode::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestInterfaceNode::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestInterfaceNode* V8TestInterfaceNode::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterfaceNode* NativeValueTraits<TestInterfaceNode>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestInterfaceNode* nativeValue = V8TestInterfaceNode::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterfaceNode"));
  }
  return nativeValue;
}

}  // namespace blink

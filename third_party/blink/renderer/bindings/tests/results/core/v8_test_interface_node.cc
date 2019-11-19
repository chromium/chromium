// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface_node.h"

#include <algorithm>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/js_event_handler.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface_empty.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_processing_stack.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo v8_test_interface_node_wrapper_type_info = {
    gin::kEmbedderBlink,
    V8TestInterfaceNode::DomTemplate,
    nullptr,
    "TestInterfaceNode",
    V8Node::GetWrapperTypeInfo(),
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
const WrapperTypeInfo& TestInterfaceNode::wrapper_type_info_ = v8_test_interface_node_wrapper_type_info;

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

static void NodeNameAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  V8SetReturnValueString(info, impl->nodeName(), info.GetIsolate());
}

static void NodeNameAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<> cpp_value = v8_value;
  if (!cpp_value.Prepare())
    return;

  impl->setNodeName(cpp_value);
}

static void StringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  V8SetReturnValueString(info, impl->stringAttribute(), info.GetIsolate());
}

static void StringAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<> cpp_value = v8_value;
  if (!cpp_value.Prepare())
    return;

  impl->setStringAttribute(cpp_value);
}

static void ReadonlyTestInterfaceEmptyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->readonlyTestInterfaceEmptyAttribute()), impl);
}

static void EventHandlerAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  EventListener* cpp_value(WTF::GetPtr(impl->eventHandlerAttribute()));

  V8SetReturnValue(info, JSEventHandler::AsV8Value(info.GetIsolate(), impl, cpp_value));
}

static void EventHandlerAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  // Prepare the value to be set.

  impl->setEventHandlerAttribute(JSEventHandler::CreateOrNull(v8_value, JSEventHandler::HandlerType::kEventHandler));
}

static void PerWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->perWorldBindingsReadonlyTestInterfaceEmptyAttribute()), impl);
}

static void PerWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  V8SetReturnValueForMainWorld(info, WTF::GetPtr(impl->perWorldBindingsReadonlyTestInterfaceEmptyAttribute()));
}

static void ReflectStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  V8SetReturnValueString(info, impl->FastGetAttribute(html_names::kReflectstringattributeAttr), info.GetIsolate());
}

static void ReflectStringAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope delivery_scope;

  // Prepare the value to be set.
  V8StringResource<> cpp_value = v8_value;
  if (!cpp_value.Prepare())
    return;

  impl->setAttribute(html_names::kReflectstringattributeAttr, cpp_value);
}

static void ReflectUrlStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  V8SetReturnValueString(info, impl->GetURLAttribute(html_names::kReflecturlstringattributeAttr), info.GetIsolate());
}

static void ReflectUrlStringAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope delivery_scope;

  // Prepare the value to be set.
  V8StringResource<> cpp_value = v8_value;
  if (!cpp_value.Prepare())
    return;

  impl->setAttribute(html_names::kReflecturlstringattributeAttr, cpp_value);
}

static void TestInterfaceEmptyMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(info.Holder());

  V8SetReturnValueFast(info, impl->testInterfaceEmptyMethod(), impl);
}

static void PerWorldBindingsTestInterfaceEmptyMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(info.Holder());

  V8SetReturnValueFast(info, impl->perWorldBindingsTestInterfaceEmptyMethod(), impl);
}

static void PerWorldBindingsTestInterfaceEmptyMethodMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(info.Holder());

  V8SetReturnValueForMainWorld(info, impl->perWorldBindingsTestInterfaceEmptyMethod());
}

static void PerWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceNode", "perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArg");

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(info.Holder());

  bool optional_boolean_argument;
  int num_args_passed = info.Length();
  while (num_args_passed > 0) {
    if (!info[num_args_passed - 1]->IsUndefined())
      break;
    --num_args_passed;
  }
  if (UNLIKELY(num_args_passed <= 0)) {
    V8SetReturnValueFast(info, impl->perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArg(), impl);
    return;
  }
  optional_boolean_argument = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), info[0], exception_state);
  if (exception_state.HadException())
    return;

  V8SetReturnValueFast(info, impl->perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArg(optional_boolean_argument), impl);
}

static void PerWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArgMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceNode", "perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArg");

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(info.Holder());

  bool optional_boolean_argument;
  int num_args_passed = info.Length();
  while (num_args_passed > 0) {
    if (!info[num_args_passed - 1]->IsUndefined())
      break;
    --num_args_passed;
  }
  if (UNLIKELY(num_args_passed <= 0)) {
    V8SetReturnValueForMainWorld(info, impl->perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArg());
    return;
  }
  optional_boolean_argument = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), info[0], exception_state);
  if (exception_state.HadException())
    return;

  V8SetReturnValueForMainWorld(info, impl->perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArg(optional_boolean_argument));
}

static void TestInterfaceEmptyMethodOverloadWithVariadicArgs1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceNode", "testInterfaceEmptyMethodOverloadWithVariadicArgs");

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(info.Holder());

  bool boolean_arg;
  Node* node_arg;
  Vector<String> dom_string_variadic_args;
  boolean_arg = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), info[0], exception_state);
  if (exception_state.HadException())
    return;

  node_arg = V8Node::ToImplWithTypeCheck(info.GetIsolate(), info[1]);
  if (!node_arg) {
    exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(1, "Node"));
    return;
  }

  dom_string_variadic_args = ToImplArguments<IDLString>(info, 2, exception_state);
  if (exception_state.HadException())
    return;

  V8SetReturnValueFast(info, impl->testInterfaceEmptyMethodOverloadWithVariadicArgs(boolean_arg, node_arg, dom_string_variadic_args), impl);
}

static void TestInterfaceEmptyMethodOverloadWithVariadicArgs2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceNode", "testInterfaceEmptyMethodOverloadWithVariadicArgs");

  TestInterfaceNode* impl = V8TestInterfaceNode::ToImpl(info.Holder());

  bool boolean_arg;
  Vector<String> dom_string_variadic_args;
  boolean_arg = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), info[0], exception_state);
  if (exception_state.HadException())
    return;

  dom_string_variadic_args = ToImplArguments<IDLString>(info, 1, exception_state);
  if (exception_state.HadException())
    return;

  V8SetReturnValueFast(info, impl->testInterfaceEmptyMethodOverloadWithVariadicArgs(boolean_arg, dom_string_variadic_args), impl);
}

static void TestInterfaceEmptyMethodOverloadWithVariadicArgsMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  scheduler::CooperativeSchedulingManager::Instance()->Safepoint();

  bool is_arity_error = false;

  switch (std::min(3, info.Length())) {
    case 1:
      if (true) {
        TestInterfaceEmptyMethodOverloadWithVariadicArgs2Method(info);
        return;
      }
      break;
    case 2:
      if (V8Node::HasInstance(info[1], info.GetIsolate())) {
        TestInterfaceEmptyMethodOverloadWithVariadicArgs1Method(info);
        return;
      }
      if (true) {
        TestInterfaceEmptyMethodOverloadWithVariadicArgs2Method(info);
        return;
      }
      break;
    case 3:
      if (V8Node::HasInstance(info[1], info.GetIsolate())) {
        TestInterfaceEmptyMethodOverloadWithVariadicArgs1Method(info);
        return;
      }
      if (true) {
        TestInterfaceEmptyMethodOverloadWithVariadicArgs2Method(info);
        return;
      }
      break;
    default:
      is_arity_error = true;
  }

  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceNode", "testInterfaceEmptyMethodOverloadWithVariadicArgs");
  if (is_arity_error) {
    if (info.Length() < 1) {
      exception_state.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exception_state.ThrowTypeError("No function was found that matched the signature provided.");
}

}  // namespace test_interface_node_v8_internal

void V8TestInterfaceNode::NodeNameAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_nodeName_Getter");

  test_interface_node_v8_internal::NodeNameAttributeGetter(info);
}

void V8TestInterfaceNode::NodeNameAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_nodeName_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_node_v8_internal::NodeNameAttributeSetter(v8_value, info);
}

void V8TestInterfaceNode::StringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_stringAttribute_Getter");

  test_interface_node_v8_internal::StringAttributeAttributeGetter(info);
}

void V8TestInterfaceNode::StringAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_stringAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_node_v8_internal::StringAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceNode::ReadonlyTestInterfaceEmptyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_readonlyTestInterfaceEmptyAttribute_Getter");

  test_interface_node_v8_internal::ReadonlyTestInterfaceEmptyAttributeAttributeGetter(info);
}

void V8TestInterfaceNode::EventHandlerAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_eventHandlerAttribute_Getter");

  test_interface_node_v8_internal::EventHandlerAttributeAttributeGetter(info);
}

void V8TestInterfaceNode::EventHandlerAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_eventHandlerAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_node_v8_internal::EventHandlerAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceNode::PerWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_perWorldBindingsReadonlyTestInterfaceEmptyAttribute_Getter");

  test_interface_node_v8_internal::PerWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetter(info);
}

void V8TestInterfaceNode::PerWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_perWorldBindingsReadonlyTestInterfaceEmptyAttribute_Getter");

  test_interface_node_v8_internal::PerWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterForMainWorld(info);
}

void V8TestInterfaceNode::ReflectStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_reflectStringAttribute_Getter");

  test_interface_node_v8_internal::ReflectStringAttributeAttributeGetter(info);
}

void V8TestInterfaceNode::ReflectStringAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_reflectStringAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_node_v8_internal::ReflectStringAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceNode::ReflectUrlStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_reflectUrlStringAttribute_Getter");

  test_interface_node_v8_internal::ReflectUrlStringAttributeAttributeGetter(info);
}

void V8TestInterfaceNode::ReflectUrlStringAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_reflectUrlStringAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_node_v8_internal::ReflectUrlStringAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceNode::TestInterfaceEmptyMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_testInterfaceEmptyMethod");

  test_interface_node_v8_internal::TestInterfaceEmptyMethodMethod(info);
}

void V8TestInterfaceNode::PerWorldBindingsTestInterfaceEmptyMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_perWorldBindingsTestInterfaceEmptyMethod");

  test_interface_node_v8_internal::PerWorldBindingsTestInterfaceEmptyMethodMethod(info);
}

void V8TestInterfaceNode::PerWorldBindingsTestInterfaceEmptyMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_perWorldBindingsTestInterfaceEmptyMethod");

  test_interface_node_v8_internal::PerWorldBindingsTestInterfaceEmptyMethodMethodForMainWorld(info);
}

void V8TestInterfaceNode::PerWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArg");

  test_interface_node_v8_internal::PerWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArgMethod(info);
}

void V8TestInterfaceNode::PerWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArgMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArg");

  test_interface_node_v8_internal::PerWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArgMethodForMainWorld(info);
}

void V8TestInterfaceNode::TestInterfaceEmptyMethodOverloadWithVariadicArgsMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNode_testInterfaceEmptyMethodOverloadWithVariadicArgs");

  test_interface_node_v8_internal::TestInterfaceEmptyMethodOverloadWithVariadicArgsMethod(info);
}

static constexpr V8DOMConfiguration::MethodConfiguration kV8TestInterfaceNodeMethods[] = {
    {"testInterfaceEmptyMethod", V8TestInterfaceNode::TestInterfaceEmptyMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"perWorldBindingsTestInterfaceEmptyMethod", V8TestInterfaceNode::PerWorldBindingsTestInterfaceEmptyMethodMethodCallbackForMainWorld, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
    {"perWorldBindingsTestInterfaceEmptyMethod", V8TestInterfaceNode::PerWorldBindingsTestInterfaceEmptyMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds},
    {"perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArg", V8TestInterfaceNode::PerWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArgMethodCallbackForMainWorld, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
    {"perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArg", V8TestInterfaceNode::PerWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds},
    {"testInterfaceEmptyMethodOverloadWithVariadicArgs", V8TestInterfaceNode::TestInterfaceEmptyMethodOverloadWithVariadicArgsMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
};

static void InstallV8TestInterfaceNodeTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interface_template) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interface_template, V8TestInterfaceNode::GetWrapperTypeInfo()->interface_name, V8Node::DomTemplate(isolate, world), V8TestInterfaceNode::kInternalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instance_template = interface_template->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instance_template);
  v8::Local<v8::ObjectTemplate> prototype_template = interface_template->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototype_template);

  // Register IDL constants, attributes and operations.
  static constexpr V8DOMConfiguration::AccessorConfiguration
  kAccessorConfigurations[] = {
      { "nodeName", V8TestInterfaceNode::NodeNameAttributeGetterCallback, V8TestInterfaceNode::NodeNameAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "stringAttribute", V8TestInterfaceNode::StringAttributeAttributeGetterCallback, V8TestInterfaceNode::StringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "readonlyTestInterfaceEmptyAttribute", V8TestInterfaceNode::ReadonlyTestInterfaceEmptyAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "eventHandlerAttribute", V8TestInterfaceNode::EventHandlerAttributeAttributeGetterCallback, V8TestInterfaceNode::EventHandlerAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "perWorldBindingsReadonlyTestInterfaceEmptyAttribute", V8TestInterfaceNode::PerWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterCallbackForMainWorld, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kMainWorld },
      { "perWorldBindingsReadonlyTestInterfaceEmptyAttribute", V8TestInterfaceNode::PerWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kNonMainWorlds },
      { "reflectStringAttribute", V8TestInterfaceNode::ReflectStringAttributeAttributeGetterCallback, V8TestInterfaceNode::ReflectStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "reflectUrlStringAttribute", V8TestInterfaceNode::ReflectUrlStringAttributeAttributeGetterCallback, V8TestInterfaceNode::ReflectUrlStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
  };
  V8DOMConfiguration::InstallAccessors(
      isolate, world, instance_template, prototype_template, interface_template,
      signature, kAccessorConfigurations,
      base::size(kAccessorConfigurations));
  V8DOMConfiguration::InstallMethods(
      isolate, world, instance_template, prototype_template, interface_template,
      signature, kV8TestInterfaceNodeMethods, base::size(kV8TestInterfaceNodeMethods));

  // Custom signature

  V8TestInterfaceNode::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interface_template);
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

v8::Local<v8::FunctionTemplate> V8TestInterfaceNode::DomTemplate(
    v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(
      isolate, world, const_cast<WrapperTypeInfo*>(V8TestInterfaceNode::GetWrapperTypeInfo()),
      InstallV8TestInterfaceNodeTemplate);
}

bool V8TestInterfaceNode::HasInstance(v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(V8TestInterfaceNode::GetWrapperTypeInfo(), v8_value);
}

v8::Local<v8::Object> V8TestInterfaceNode::FindInstanceInPrototypeChain(
    v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(
      V8TestInterfaceNode::GetWrapperTypeInfo(), v8_value);
}

TestInterfaceNode* V8TestInterfaceNode::ToImplWithTypeCheck(
    v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return HasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterfaceNode* NativeValueTraits<TestInterfaceNode>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  TestInterfaceNode* native_value = V8TestInterfaceNode::ToImplWithTypeCheck(isolate, value);
  if (!native_value) {
    exception_state.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterfaceNode"));
  }
  return native_value;
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface_2.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface_empty.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
WrapperTypeInfo V8TestInterface2::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestInterface2::domTemplate,
    nullptr,
    "TestInterface2",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterface2.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInterface2::wrapper_type_info_ = V8TestInterface2::wrapperTypeInfo;

// [ActiveScriptWrappable]
static_assert(
    std::is_base_of<ActiveScriptWrappableBase, TestInterface2>::value,
    "TestInterface2 does not inherit from ActiveScriptWrappable<>, but specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    !std::is_same<decltype(&TestInterface2::HasPendingActivity),
                  decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInterface2 is not overriding hasPendingActivity(), but is specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_interface_2_v8_internal {

static void sizeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterface2* impl = V8TestInterface2::ToImpl(holder);

  V8SetReturnValueUnsigned(info, impl->size());
}

static void itemMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface2", "item");

  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  uint32_t index;
  index = NativeValueTraits<IDLUnsignedLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfaceEmpty* result = impl->item(index, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void setItemMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface2", "setItem");

  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 2)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  uint32_t index;
  TestInterfaceEmpty* value;
  index = NativeValueTraits<IDLUnsignedLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  value = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[1]);
  if (!value) {
    exceptionState.ThrowTypeError("parameter 2 is not of type 'TestInterfaceEmpty'.");
    return;
  }

  TestInterfaceEmpty* result = impl->setItem(index, value, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void deleteItemMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface2", "deleteItem");

  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  uint32_t index;
  index = NativeValueTraits<IDLUnsignedLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  bool result = impl->deleteItem(index, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValueBool(info, result);
}

static void namedItemMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface2", "namedItem");

  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> name;
  name = info[0];
  if (!name.Prepare())
    return;

  TestInterfaceEmpty* result = impl->namedItem(name, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void setNamedItemMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface2", "setNamedItem");

  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 2)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  V8StringResource<> name;
  TestInterfaceEmpty* value;
  name = info[0];
  if (!name.Prepare())
    return;

  value = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[1]);
  if (!value && !IsUndefinedOrNull(info[1])) {
    exceptionState.ThrowTypeError("parameter 2 is not of type 'TestInterfaceEmpty'.");
    return;
  }

  TestInterfaceEmpty* result = impl->setNamedItem(name, value, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void deleteNamedItemMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface2", "deleteNamedItem");

  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> name;
  name = info[0];
  if (!name.Prepare())
    return;

  bool result = impl->deleteNamedItem(name, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValueBool(info, result);
}

static void stringifierMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());

  V8SetReturnValueString(info, impl->stringifierMethod(), info.GetIsolate());
}

static void keysMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface2", "keys");

  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  Iterator* result = impl->keysForBinding(scriptState, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void entriesMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface2", "entries");

  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  Iterator* result = impl->entriesForBinding(scriptState, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void forEachMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface2", "forEach");

  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  ScriptValue callback;
  ScriptValue thisArg;
  if (info[0]->IsFunction()) {
    callback = ScriptValue(ScriptState::Current(info.GetIsolate()), info[0]);
  } else {
    exceptionState.ThrowTypeError("The callback provided as parameter 1 is not a function.");
    return;
  }

  thisArg = ScriptValue(ScriptState::Current(info.GetIsolate()), info[1]);

  impl->forEachForBinding(scriptState, ScriptValue(scriptState, info.Holder()), callback, thisArg, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
}

static void hasMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface2", "has");

  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  TestInterfaceEmpty* value;
  value = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!value) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'TestInterfaceEmpty'.");
    return;
  }

  bool result = impl->hasForBinding(scriptState, value, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValueBool(info, result);
}

static void toStringMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());

  V8SetReturnValueString(info, impl->stringifierMethod(), info.GetIsolate());
}

static void iteratorMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface2", "iterator");

  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  Iterator* result = impl->GetIterator(scriptState, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void constructor(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface2_ConstructorCallback");

  TestInterface2* impl = TestInterface2::Create();
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestInterface2::wrapperTypeInfo, wrapper);
  V8SetReturnValue(info, wrapper);
}

static void namedPropertyGetter(const AtomicString& name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  const CString& nameInUtf8 = name.Utf8();
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kGetterContext, "TestInterface2", nameInUtf8.data());

  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());
  TestInterfaceEmpty* result = impl->namedItem(name, exceptionState);
  if (!result)
    return;
  V8SetReturnValueFast(info, result, impl);
}

static void namedPropertySetter(const AtomicString& name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  const CString& nameInUtf8 = name.Utf8();
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kSetterContext, "TestInterface2", nameInUtf8.data());

  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());
  TestInterfaceEmpty* propertyValue = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), v8Value);
  if (!propertyValue && !IsUndefinedOrNull(v8Value)) {
    exceptionState.ThrowTypeError("The provided value is not of type 'TestInterfaceEmpty'.");
    return;
  }

  bool result = impl->setNamedItem(name, propertyValue, exceptionState);
  if (exceptionState.HadException())
    return;
  if (!result)
    return;
  V8SetReturnValue(info, v8Value);
}

static void namedPropertyDeleter(const AtomicString& name, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  const CString& nameInUtf8 = name.Utf8();
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kDeletionContext, "TestInterface2", nameInUtf8.data());

  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());

  DeleteResult result = impl->deleteNamedItem(name, exceptionState);
  if (exceptionState.HadException())
    return;
  if (result == kDeleteUnknownProperty)
    return;
  V8SetReturnValue(info, result == kDeleteSuccess);
}

static void namedPropertyQuery(const AtomicString& name, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  const CString& nameInUtf8 = name.Utf8();
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kGetterContext, "TestInterface2", nameInUtf8.data());

  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());

  bool result = impl->NamedPropertyQuery(name, exceptionState);
  if (!result)
    return;
  // https://heycam.github.io/webidl/#LegacyPlatformObjectGetOwnProperty
  // 2.7. If |O| implements an interface with a named property setter, then set
  //      desc.[[Writable]] to true, otherwise set it to false.
  // 2.8. If |O| implements an interface with the
  //      [LegacyUnenumerableNamedProperties] extended attribute, then set
  //      desc.[[Enumerable]] to false, otherwise set it to true.
  V8SetReturnValueInt(info, v8::None);
}

static void namedPropertyEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kEnumerationContext, "TestInterface2");

  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());

  Vector<String> names;
  impl->NamedPropertyEnumerator(names, exceptionState);
  if (exceptionState.HadException())
    return;
  V8SetReturnValue(info, ToV8(names, info.Holder(), info.GetIsolate()).As<v8::Array>());
}

static void indexedPropertyGetter(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kIndexedGetterContext, "TestInterface2");

  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());

  // We assume that all the implementations support length() method, although
  // the spec doesn't require that length() must exist.  It's okay that
  // the interface does not have length attribute as long as the
  // implementation supports length() member function.
  if (index >= impl->length())
    return;  // Returns undefined due to out-of-range.

  TestInterfaceEmpty* result = impl->item(index, exceptionState);
  V8SetReturnValueFast(info, result, impl);
}

static void indexedPropertyDescriptor(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  // https://heycam.github.io/webidl/#LegacyPlatformObjectGetOwnProperty
  // Steps 1.1 to 1.2.4 are covered here: we rely on indexedPropertyGetter() to
  // call the getter function and check that |index| is a valid property index,
  // in which case it will have set info.GetReturnValue() to something other
  // than undefined.
  V8TestInterface2::indexedPropertyGetterCallback(index, info);
  v8::Local<v8::Value> getterValue = info.GetReturnValue().Get();
  if (!getterValue->IsUndefined()) {
    // 1.2.5. Let |desc| be a newly created Property Descriptor with no fields.
    // 1.2.6. Set desc.[[Value]] to the result of converting value to an
    //        ECMAScript value.
    // 1.2.7. If O implements an interface with an indexed property setter,
    //        then set desc.[[Writable]] to true, otherwise set it to false.
    v8::PropertyDescriptor desc(getterValue, true);
    // 1.2.8. Set desc.[[Enumerable]] and desc.[[Configurable]] to true.
    desc.set_enumerable(true);
    desc.set_configurable(true);
    // 1.2.9. Return |desc|.
    V8SetReturnValue(info, desc);
  }
}

static void indexedPropertySetter(uint32_t index, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kIndexedSetterContext, "TestInterface2");

  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());
  TestInterfaceEmpty* propertyValue = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), v8Value);
  if (!propertyValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'TestInterfaceEmpty'.");
    return;
  }

  bool result = impl->setItem(index, propertyValue, exceptionState);
  if (exceptionState.HadException())
    return;
  if (!result)
    return;
  V8SetReturnValue(info, v8Value);
}

static void indexedPropertyDeleter(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kIndexedDeletionContext, "TestInterface2");

  TestInterface2* impl = V8TestInterface2::ToImpl(info.Holder());

  DeleteResult result = impl->deleteItem(index, exceptionState);
  if (exceptionState.HadException())
    return;
  if (result == kDeleteUnknownProperty)
    return;
  V8SetReturnValue(info, result == kDeleteSuccess);
}

}  // namespace test_interface_2_v8_internal

void V8TestInterface2::sizeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface2_size_Getter");

  test_interface_2_v8_internal::sizeAttributeGetter(info);
}

void V8TestInterface2::itemMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface2_item");

  test_interface_2_v8_internal::itemMethod(info);
}

void V8TestInterface2::setItemMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface2_setItem");

  test_interface_2_v8_internal::setItemMethod(info);
}

void V8TestInterface2::deleteItemMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface2_deleteItem");

  test_interface_2_v8_internal::deleteItemMethod(info);
}

void V8TestInterface2::namedItemMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface2_namedItem");

  test_interface_2_v8_internal::namedItemMethod(info);
}

void V8TestInterface2::setNamedItemMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface2_setNamedItem");

  test_interface_2_v8_internal::setNamedItemMethod(info);
}

void V8TestInterface2::deleteNamedItemMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface2_deleteNamedItem");

  test_interface_2_v8_internal::deleteNamedItemMethod(info);
}

void V8TestInterface2::stringifierMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface2_stringifierMethod");

  test_interface_2_v8_internal::stringifierMethodMethod(info);
}

void V8TestInterface2::keysMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface2_keys");

  test_interface_2_v8_internal::keysMethod(info);
}

void V8TestInterface2::entriesMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface2_entries");

  test_interface_2_v8_internal::entriesMethod(info);
}

void V8TestInterface2::forEachMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface2_forEach");

  test_interface_2_v8_internal::forEachMethod(info);
}

void V8TestInterface2::hasMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface2_has");

  test_interface_2_v8_internal::hasMethod(info);
}

void V8TestInterface2::toStringMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface2_toString");

  test_interface_2_v8_internal::toStringMethod(info);
}

void V8TestInterface2::iteratorMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface2_iterator");

  test_interface_2_v8_internal::iteratorMethod(info);
}

void V8TestInterface2::namedPropertyGetterCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface2_NamedPropertyGetter");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_interface_2_v8_internal::namedPropertyGetter(propertyName, info);
}

void V8TestInterface2::namedPropertySetterCallback(v8::Local<v8::Name> name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface2_NamedPropertySetter");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_interface_2_v8_internal::namedPropertySetter(propertyName, v8Value, info);
}

void V8TestInterface2::namedPropertyDeleterCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_interface_2_v8_internal::namedPropertyDeleter(propertyName, info);
}

void V8TestInterface2::namedPropertyQueryCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface2_NamedPropertyQuery");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_interface_2_v8_internal::namedPropertyQuery(propertyName, info);
}

void V8TestInterface2::namedPropertyEnumeratorCallback(const v8::PropertyCallbackInfo<v8::Array>& info) {
  test_interface_2_v8_internal::namedPropertyEnumerator(info);
}

void V8TestInterface2::indexedPropertyGetterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface2_IndexedPropertyGetter");

  test_interface_2_v8_internal::indexedPropertyGetter(index, info);
}

void V8TestInterface2::indexedPropertyDescriptorCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  test_interface_2_v8_internal::indexedPropertyDescriptor(index, info);
}

void V8TestInterface2::indexedPropertySetterCallback(uint32_t index, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  test_interface_2_v8_internal::indexedPropertySetter(index, v8Value, info);
}

void V8TestInterface2::indexedPropertyDeleterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  test_interface_2_v8_internal::indexedPropertyDeleter(index, info);
}

void V8TestInterface2::indexedPropertyDefinerCallback(
    uint32_t index,
    const v8::PropertyDescriptor& desc,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  // https://heycam.github.io/webidl/#legacy-platform-object-defineownproperty
  // 3.9.3. [[DefineOwnProperty]]
  // step 1.1. If the result of calling IsDataDescriptor(Desc) is false, then
  //   return false.
  if (desc.has_get() || desc.has_set()) {
    V8SetReturnValue(info, v8::Null(info.GetIsolate()));
    if (info.ShouldThrowOnError()) {
      ExceptionState exceptionState(info.GetIsolate(),
                                    ExceptionState::kIndexedSetterContext,
                                    "TestInterface2");
      exceptionState.ThrowTypeError("Accessor properties are not allowed.");
    }
    return;
  }

  // Return nothing and fall back to indexedPropertySetterCallback.
}

static const V8DOMConfiguration::AccessorConfiguration V8TestInterface2Accessors[] = {
    { "size", V8TestInterface2::sizeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::DontEnum | v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
};

static const V8DOMConfiguration::MethodConfiguration V8TestInterface2Methods[] = {
    {"item", V8TestInterface2::itemMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"setItem", V8TestInterface2::setItemMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"deleteItem", V8TestInterface2::deleteItemMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"namedItem", V8TestInterface2::namedItemMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"setNamedItem", V8TestInterface2::setNamedItemMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"deleteNamedItem", V8TestInterface2::deleteNamedItemMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"stringifierMethod", V8TestInterface2::stringifierMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"keys", V8TestInterface2::keysMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"entries", V8TestInterface2::entriesMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"forEach", V8TestInterface2::forEachMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"has", V8TestInterface2::hasMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"toString", V8TestInterface2::toStringMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
};

void V8TestInterface2::constructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface2_Constructor");

  if (!info.IsConstructCall()) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::ConstructorNotCallableAsFunction("TestInterface2"));
    return;
  }

  if (ConstructorMode::Current(info.GetIsolate()) == ConstructorMode::kWrapExistingObject) {
    V8SetReturnValue(info, info.Holder());
    return;
  }

  test_interface_2_v8_internal::constructor(info);
}

void V8TestInterface2::installV8TestInterface2Template(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestInterface2::wrapperTypeInfo.interface_name, v8::Local<v8::FunctionTemplate>(), V8TestInterface2::internalFieldCount);
  interfaceTemplate->SetCallHandler(V8TestInterface2::constructorCallback);
  interfaceTemplate->SetLength(0);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.
  static_assert(1 == TestInterface2::kConstValue1, "the value of TestInterface2_kConstValue1 does not match with implementation");
  V8DOMConfiguration::InstallAccessors(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestInterface2Accessors, base::size(V8TestInterface2Accessors));
  V8DOMConfiguration::InstallMethods(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestInterface2Methods, base::size(V8TestInterface2Methods));

  // Indexed properties
  v8::IndexedPropertyHandlerConfiguration indexedPropertyHandlerConfig(
      V8TestInterface2::indexedPropertyGetterCallback,
      V8TestInterface2::indexedPropertySetterCallback,
      V8TestInterface2::indexedPropertyDescriptorCallback,
      V8TestInterface2::indexedPropertyDeleterCallback,
      IndexedPropertyEnumerator<TestInterface2>,
      V8TestInterface2::indexedPropertyDefinerCallback,
      v8::Local<v8::Value>(),
      v8::PropertyHandlerFlags::kNone);
  instanceTemplate->SetHandler(indexedPropertyHandlerConfig);
  // Named properties
  v8::NamedPropertyHandlerConfiguration namedPropertyHandlerConfig(V8TestInterface2::namedPropertyGetterCallback, V8TestInterface2::namedPropertySetterCallback, V8TestInterface2::namedPropertyQueryCallback, V8TestInterface2::namedPropertyDeleterCallback, V8TestInterface2::namedPropertyEnumeratorCallback, v8::Local<v8::Value>(), static_cast<v8::PropertyHandlerFlags>(int(v8::PropertyHandlerFlags::kOnlyInterceptStrings) | int(v8::PropertyHandlerFlags::kNonMasking)));
  instanceTemplate->SetHandler(namedPropertyHandlerConfig);

  // Iterator (@@iterator)
  static const V8DOMConfiguration::SymbolKeyedMethodConfiguration
  symbolKeyedIteratorConfiguration = {
      v8::Symbol::GetIterator,
      "values",
      V8TestInterface2::iteratorMethodCallback,
      0,
      v8::DontEnum,
      V8DOMConfiguration::kOnPrototype,
      V8DOMConfiguration::kCheckHolder,
      V8DOMConfiguration::kDoNotCheckAccess,
      V8DOMConfiguration::kHasSideEffect
  };
  V8DOMConfiguration::InstallMethod(isolate, world, prototypeTemplate, signature, symbolKeyedIteratorConfiguration);

  // Custom signature
}

void V8TestInterface2::InstallRuntimeEnabledFeaturesOnTemplate(
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
  if (RuntimeEnabledFeatures::FeatureNameEnabled()) {
    static const V8DOMConfiguration::ConstantConfiguration constant_configurations[] = {
        {"CONST_VALUE_1", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(1)},
    };
    V8DOMConfiguration::InstallConstants(
        isolate, interface_template, prototype_template,
        constant_configurations, base::size(constant_configurations));
  }

  // Custom signature
}

v8::Local<v8::FunctionTemplate> V8TestInterface2::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), V8TestInterface2::installV8TestInterface2TemplateFunction);
}

bool V8TestInterface2::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestInterface2::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestInterface2* V8TestInterface2::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterface2* NativeValueTraits<TestInterface2>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestInterface2* nativeValue = V8TestInterface2::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterface2"));
  }
  return nativeValue;
}

InstallRuntimeEnabledFeaturesOnTemplateFunction
V8TestInterface2::install_runtime_enabled_features_on_template_function_ =
    &V8TestInterface2::InstallRuntimeEnabledFeaturesOnTemplate;

InstallTemplateFunction V8TestInterface2::installV8TestInterface2TemplateFunction =
    &V8TestInterface2::installV8TestInterface2Template;

void V8TestInterface2::UpdateWrapperTypeInfo(
    InstallTemplateFunction install_template_function,
    InstallRuntimeEnabledFeaturesFunction install_runtime_enabled_features_function,
    InstallRuntimeEnabledFeaturesOnTemplateFunction install_runtime_enabled_features_on_template_function,
    InstallConditionalFeaturesFunction install_conditional_features_function) {
  V8TestInterface2::installV8TestInterface2TemplateFunction =
      install_template_function;

  CHECK(install_runtime_enabled_features_on_template_function);
  V8TestInterface2::install_runtime_enabled_features_on_template_function_ =
      install_runtime_enabled_features_on_template_function;

  if (install_conditional_features_function) {
    V8TestInterface2::wrapperTypeInfo.install_conditional_features_function =
        install_conditional_features_function;
  }
}

}  // namespace blink

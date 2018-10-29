// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_object.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_call_stack.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value_factory.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_attr.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_fragment.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_listener_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_target.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_float32_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_collection.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_int32_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node_filter.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shadow_root.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_callback_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface_empty.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_object.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint8_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_xpath_ns_resolver.h"
#include "third_party/blink/renderer/core/dom/class_collection.h"
#include "third_party/blink/renderer/core/dom/tag_collection.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_processing_stack.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_options_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_form_controls_collection.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_table_rows_collection.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/script_arguments.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/flexible_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_activity_logger.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestObject::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestObject::domTemplate,
    V8TestObject::InstallConditionalFeatures,
    "TestObject",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestObject.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestObject::wrapper_type_info_ = V8TestObject::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestObject>::value,
    "TestObject inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestObject::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestObject is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_object_v8_internal {

static void stringifierAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->stringifierAttribute(), info.GetIsolate());
}

static void stringifierAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setStringifierAttribute(cppValue);
}

static void readonlyStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->readonlyStringAttribute(), info.GetIsolate());
}

static void readonlyTestInterfaceEmptyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  TestInterfaceEmpty* cppValue(WTF::GetPtr(impl->readonlyTestInterfaceEmptyAttribute()));

  // Keep the wrapper object for the return value alive as long as |this|
  // object is alive in order to save creation time of the wrapper object.
  if (cppValue && DOMDataStore::SetReturnValue(info.GetReturnValue(), cppValue))
    return;
  v8::Local<v8::Value> v8Value(ToV8(cppValue, holder, info.GetIsolate()));
  V8PrivateProperty::GetSymbol(
      info.GetIsolate(), "KeepAlive#TestObject#readonlyTestInterfaceEmptyAttribute")
      .Set(holder, v8Value);

  V8SetReturnValue(info, v8Value);
}

static void readonlyLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->readonlyLongAttribute());
}

static void dateAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, v8::Date::New(info.GetIsolate()->GetCurrentContext(), impl->dateAttribute()));
}

static void dateAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "dateAttribute");

  // Prepare the value to be set.
  double cppValue = NativeValueTraits<IDLDate>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setDateAttribute(cppValue);
}

static void stringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->stringAttribute(), info.GetIsolate());
}

static void stringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setStringAttribute(cppValue);
}

static void byteStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->byteStringAttribute(), info.GetIsolate());
}

static void byteStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "byteStringAttribute");

  // Prepare the value to be set.
  V8StringResource<> cppValue = NativeValueTraits<IDLByteString>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setByteStringAttribute(cppValue);
}

static void usvStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->usvStringAttribute(), info.GetIsolate());
}

static void usvStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "usvStringAttribute");

  // Prepare the value to be set.
  V8StringResource<> cppValue = NativeValueTraits<IDLUSVString>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUsvStringAttribute(cppValue);
}

static void domTimeStampAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, static_cast<double>(impl->domTimeStampAttribute()));
}

static void domTimeStampAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "domTimeStampAttribute");

  // Prepare the value to be set.
  uint64_t cppValue = NativeValueTraits<IDLUnsignedLongLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setDomTimeStampAttribute(cppValue);
}

static void booleanAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueBool(info, impl->booleanAttribute());
}

static void booleanAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "booleanAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setBooleanAttribute(cppValue);
}

static void byteAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->byteAttribute());
}

static void byteAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "byteAttribute");

  // Prepare the value to be set.
  int8_t cppValue = NativeValueTraits<IDLByte>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setByteAttribute(cppValue);
}

static void doubleAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, impl->doubleAttribute());
}

static void doubleAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "doubleAttribute");

  // Prepare the value to be set.
  double cppValue = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setDoubleAttribute(cppValue);
}

static void floatAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, impl->floatAttribute());
}

static void floatAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "floatAttribute");

  // Prepare the value to be set.
  float cppValue = NativeValueTraits<IDLFloat>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setFloatAttribute(cppValue);
}

static void longAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->longAttribute());
}

static void longAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "longAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setLongAttribute(cppValue);
}

static void longLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, static_cast<double>(impl->longLongAttribute()));
}

static void longLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "longLongAttribute");

  // Prepare the value to be set.
  int64_t cppValue = NativeValueTraits<IDLLongLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setLongLongAttribute(cppValue);
}

static void octetAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueUnsigned(info, impl->octetAttribute());
}

static void octetAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "octetAttribute");

  // Prepare the value to be set.
  uint8_t cppValue = NativeValueTraits<IDLOctet>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setOctetAttribute(cppValue);
}

static void shortAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->shortAttribute());
}

static void shortAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "shortAttribute");

  // Prepare the value to be set.
  int16_t cppValue = NativeValueTraits<IDLShort>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setShortAttribute(cppValue);
}

static void unrestrictedDoubleAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, impl->unrestrictedDoubleAttribute());
}

static void unrestrictedDoubleAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "unrestrictedDoubleAttribute");

  // Prepare the value to be set.
  double cppValue = NativeValueTraits<IDLUnrestrictedDouble>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnrestrictedDoubleAttribute(cppValue);
}

static void unrestrictedFloatAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, impl->unrestrictedFloatAttribute());
}

static void unrestrictedFloatAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "unrestrictedFloatAttribute");

  // Prepare the value to be set.
  float cppValue = NativeValueTraits<IDLUnrestrictedFloat>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnrestrictedFloatAttribute(cppValue);
}

static void unsignedLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueUnsigned(info, impl->unsignedLongAttribute());
}

static void unsignedLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "unsignedLongAttribute");

  // Prepare the value to be set.
  uint32_t cppValue = NativeValueTraits<IDLUnsignedLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnsignedLongAttribute(cppValue);
}

static void unsignedLongLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, static_cast<double>(impl->unsignedLongLongAttribute()));
}

static void unsignedLongLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "unsignedLongLongAttribute");

  // Prepare the value to be set.
  uint64_t cppValue = NativeValueTraits<IDLUnsignedLongLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnsignedLongLongAttribute(cppValue);
}

static void unsignedShortAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueUnsigned(info, impl->unsignedShortAttribute());
}

static void unsignedShortAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "unsignedShortAttribute");

  // Prepare the value to be set.
  uint16_t cppValue = NativeValueTraits<IDLUnsignedShort>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnsignedShortAttribute(cppValue);
}

static void testInterfaceEmptyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->testInterfaceEmptyAttribute()), impl);
}

static void testInterfaceEmptyAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "testInterfaceEmptyAttribute");

  // Prepare the value to be set.
  TestInterfaceEmpty* cppValue = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'TestInterfaceEmpty'.");
    return;
  }

  impl->setTestInterfaceEmptyAttribute(cppValue);
}

static void testObjectAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->testObjectAttribute()), impl);
}

static void testObjectAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "testObjectAttribute");

  // Prepare the value to be set.
  TestObject* cppValue = V8TestObject::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'TestObject'.");
    return;
  }

  impl->setTestObjectAttribute(cppValue);
}

static void cssAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->cssAttribute());
}

static void cssAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "cssAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setCSSAttribute(cppValue);
}

static void imeAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->imeAttribute());
}

static void imeAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "imeAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setIMEAttribute(cppValue);
}

static void svgAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->svgAttribute());
}

static void svgAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "svgAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSVGAttribute(cppValue);
}

static void xmlAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->xmlAttribute());
}

static void xmlAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "xmlAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setXMLAttribute(cppValue);
}

static void nodeFilterAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, ToV8(WTF::GetPtr(impl->nodeFilterAttribute()), info.Holder(), info.GetIsolate()));
}

static void serializedScriptValueAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, V8Deserialize(info.GetIsolate(), WTF::GetPtr(impl->serializedScriptValueAttribute()).get()));
}

static void serializedScriptValueAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "serializedScriptValueAttribute");

  // Prepare the value to be set.
  scoped_refptr<SerializedScriptValue> cppValue = NativeValueTraits<SerializedScriptValue>::NativeValue(info.GetIsolate(), v8Value, SerializedScriptValue::SerializeOptions(SerializedScriptValue::kNotForStorage), exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSerializedScriptValueAttribute(std::move(cppValue));
}

static void anyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, impl->anyAttribute().V8Value());
}

static void anyAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  ScriptValue cppValue = ScriptValue(ScriptState::Current(info.GetIsolate()), v8Value);

  impl->setAnyAttribute(cppValue);
}

static void promiseAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  // This attribute returns a Promise.
  // Per https://heycam.github.io/webidl/#dfn-attribute-getter, all exceptions
  // must be turned into a Promise rejection.
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kGetterContext, "TestObject", "promiseAttribute");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  // Returning a Promise type requires us to disable some of V8's type checks,
  // so we have to manually check that info.Holder() really points to an
  // instance of the type.
  if (!V8TestObject::hasInstance(info.Holder(), info.GetIsolate())) {
    exceptionState.ThrowTypeError("Illegal invocation");
    return;
  }

  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, impl->promiseAttribute().V8Value());
}

static void promiseAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  ScriptPromise cppValue = ScriptPromise::Cast(ScriptState::Current(info.GetIsolate()), v8Value);

  impl->setPromiseAttribute(cppValue);
}

static void windowAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->windowAttribute()), impl);
}

static void windowAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "windowAttribute");

  // Prepare the value to be set.
  DOMWindow* cppValue = ToDOMWindow(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'Window'.");
    return;
  }

  impl->setWindowAttribute(cppValue);
}

static void documentAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->documentAttribute()), impl);
}

static void documentAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "documentAttribute");

  // Prepare the value to be set.
  Document* cppValue = V8Document::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'Document'.");
    return;
  }

  impl->setDocumentAttribute(cppValue);
}

static void documentFragmentAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->documentFragmentAttribute()), impl);
}

static void documentFragmentAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "documentFragmentAttribute");

  // Prepare the value to be set.
  DocumentFragment* cppValue = V8DocumentFragment::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'DocumentFragment'.");
    return;
  }

  impl->setDocumentFragmentAttribute(cppValue);
}

static void documentTypeAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->documentTypeAttribute()), impl);
}

static void documentTypeAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "documentTypeAttribute");

  // Prepare the value to be set.
  DocumentType* cppValue = V8DocumentType::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'DocumentType'.");
    return;
  }

  impl->setDocumentTypeAttribute(cppValue);
}

static void elementAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->elementAttribute()), impl);
}

static void elementAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "elementAttribute");

  // Prepare the value to be set.
  Element* cppValue = V8Element::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'Element'.");
    return;
  }

  impl->setElementAttribute(cppValue);
}

static void nodeAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->nodeAttribute()), impl);
}

static void nodeAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "nodeAttribute");

  // Prepare the value to be set.
  Node* cppValue = V8Node::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'Node'.");
    return;
  }

  impl->setNodeAttribute(cppValue);
}

static void shadowRootAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->shadowRootAttribute()), impl);
}

static void shadowRootAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "shadowRootAttribute");

  // Prepare the value to be set.
  ShadowRoot* cppValue = V8ShadowRoot::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'ShadowRoot'.");
    return;
  }

  impl->setShadowRootAttribute(cppValue);
}

static void arrayBufferAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->arrayBufferAttribute()), impl);
}

static void arrayBufferAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "arrayBufferAttribute");

  // Prepare the value to be set.
  TestArrayBuffer* cppValue = v8Value->IsArrayBuffer() ? V8ArrayBuffer::ToImpl(v8::Local<v8::ArrayBuffer>::Cast(v8Value)) : 0;

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'ArrayBuffer'.");
    return;
  }

  impl->setArrayBufferAttribute(cppValue);
}

static void float32ArrayAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, impl->float32ArrayAttribute(), impl);
}

static void float32ArrayAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "float32ArrayAttribute");

  // Prepare the value to be set.
  NotShared<DOMFloat32Array> cppValue = ToNotShared<NotShared<DOMFloat32Array>>(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'Float32Array'.");
    return;
  }

  impl->setFloat32ArrayAttribute(cppValue);
}

static void uint8ArrayAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, impl->uint8ArrayAttribute(), impl);
}

static void uint8ArrayAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "uint8ArrayAttribute");

  // Prepare the value to be set.
  NotShared<DOMUint8Array> cppValue = ToNotShared<NotShared<DOMUint8Array>>(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'Uint8Array'.");
    return;
  }

  impl->setUint8ArrayAttribute(cppValue);
}

static void selfAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->self()), impl);
}

static void readonlyEventTargetAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->readonlyEventTargetAttribute()), impl);
}

static void readonlyEventTargetOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->readonlyEventTargetOrNullAttribute()), impl);
}

static void readonlyWindowAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->readonlyWindowAttribute()), impl);
}

static void htmlCollectionAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->htmlCollectionAttribute()), impl);
}

static void htmlElementAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->htmlElementAttribute()), impl);
}

static void stringFrozenArrayAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, FreezeV8Object(ToV8(impl->stringFrozenArrayAttribute(), info.Holder(), info.GetIsolate()), info.GetIsolate()));
}

static void stringFrozenArrayAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "stringFrozenArrayAttribute");

  // Prepare the value to be set.
  Vector<String> cppValue = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setStringFrozenArrayAttribute(cppValue);
}

static void testInterfaceEmptyFrozenArrayAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, FreezeV8Object(ToV8(impl->testInterfaceEmptyFrozenArrayAttribute(), info.Holder(), info.GetIsolate()), info.GetIsolate()));
}

static void testInterfaceEmptyFrozenArrayAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "testInterfaceEmptyFrozenArrayAttribute");

  // Prepare the value to be set.
  HeapVector<Member<TestInterfaceEmpty>> cppValue = NativeValueTraits<IDLSequence<TestInterfaceEmpty>>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setTestInterfaceEmptyFrozenArrayAttribute(cppValue);
}

static void booleanOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  bool isNull = false;

  bool cppValue(impl->booleanOrNullAttribute(isNull));

  if (isNull) {
    V8SetReturnValueNull(info);
    return;
  }

  V8SetReturnValueBool(info, cppValue);
}

static void booleanOrNullAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "booleanOrNullAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  bool isNull = IsUndefinedOrNull(v8Value);
  impl->setBooleanOrNullAttribute(cppValue, isNull);
}

static void stringOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueStringOrNull(info, impl->stringOrNullAttribute(), info.GetIsolate());
}

static void stringOrNullAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<kTreatNullAndUndefinedAsNullString> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setStringOrNullAttribute(cppValue);
}

static void longOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  bool isNull = false;

  int32_t cppValue(impl->longOrNullAttribute(isNull));

  if (isNull) {
    V8SetReturnValueNull(info);
    return;
  }

  V8SetReturnValueInt(info, cppValue);
}

static void longOrNullAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "longOrNullAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  bool isNull = IsUndefinedOrNull(v8Value);
  impl->setLongOrNullAttribute(cppValue, isNull);
}

static void testInterfaceOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->testInterfaceOrNullAttribute()), impl);
}

static void testInterfaceOrNullAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "testInterfaceOrNullAttribute");

  // Prepare the value to be set.
  TestInterfaceImplementation* cppValue = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue && !IsUndefinedOrNull(v8Value)) {
    exceptionState.ThrowTypeError("The provided value is not of type 'TestInterface'.");
    return;
  }

  impl->setTestInterfaceOrNullAttribute(cppValue);
}

static void testEnumAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->testEnumAttribute(), info.GetIsolate());
}

static void testEnumAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "testEnumAttribute");

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  // Type check per: http://heycam.github.io/webidl/#dfn-attribute-setter
  // Returns undefined without setting the value if the value is invalid.
  DummyExceptionStateForTesting dummyExceptionState;
  const char* validValues[] = {
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
  if (!IsValidEnum(cppValue, validValues, base::size(validValues), "TestEnum", dummyExceptionState)) {
    ExecutionContext::ForCurrentRealm(info)->AddConsoleMessage(
        ConsoleMessage::Create(kJSMessageSource, kWarningMessageLevel,
                               dummyExceptionState.Message()));
    return;
  }

  impl->setTestEnumAttribute(cppValue);
}

static void testEnumOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueStringOrNull(info, impl->testEnumOrNullAttribute(), info.GetIsolate());
}

static void testEnumOrNullAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "testEnumOrNullAttribute");

  // Prepare the value to be set.
  V8StringResource<kTreatNullAndUndefinedAsNullString> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  // Type check per: http://heycam.github.io/webidl/#dfn-attribute-setter
  // Returns undefined without setting the value if the value is invalid.
  DummyExceptionStateForTesting dummyExceptionState;
  const char* validValues[] = {
      nullptr,
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
  if (!IsValidEnum(cppValue, validValues, base::size(validValues), "TestEnum", dummyExceptionState)) {
    ExecutionContext::ForCurrentRealm(info)->AddConsoleMessage(
        ConsoleMessage::Create(kJSMessageSource, kWarningMessageLevel,
                               dummyExceptionState.Message()));
    return;
  }

  impl->setTestEnumOrNullAttribute(cppValue);
}

static void staticStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueString(info, TestObject::staticStringAttribute(), info.GetIsolate());
}

static void staticStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  TestObject::setStaticStringAttribute(cppValue);
}

static void staticLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueInt(info, TestObject::staticLongAttribute());
}

static void staticLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "staticLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  TestObject::setStaticLongAttribute(cppValue);
}

static void eventHandlerAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  EventListener* cppValue(WTF::GetPtr(impl->eventHandlerAttribute()));

  V8SetReturnValue(info, JSBasedEventListener::GetListenerOrNull(info.GetIsolate(), impl, cppValue));
}

static void eventHandlerAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.

  impl->setEventHandlerAttribute(V8EventListenerHelper::GetEventHandler(ScriptState::ForRelevantRealm(info), v8Value, JSEventHandler::HandlerType::kEventHandler, kListenerFindOrCreate));
}

static void doubleOrStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  DoubleOrString result;
  impl->doubleOrStringAttribute(result);

  V8SetReturnValue(info, result);
}

static void doubleOrStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "doubleOrStringAttribute");

  // Prepare the value to be set.
  DoubleOrString cppValue;
  V8DoubleOrString::ToImpl(info.GetIsolate(), v8Value, cppValue, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setDoubleOrStringAttribute(cppValue);
}

static void doubleOrStringOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  DoubleOrString result;
  impl->doubleOrStringOrNullAttribute(result);

  V8SetReturnValue(info, result);
}

static void doubleOrStringOrNullAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "doubleOrStringOrNullAttribute");

  // Prepare the value to be set.
  DoubleOrString cppValue;
  V8DoubleOrString::ToImpl(info.GetIsolate(), v8Value, cppValue, UnionTypeConversionMode::kNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setDoubleOrStringOrNullAttribute(cppValue);
}

static void doubleOrNullStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  DoubleOrString result;
  impl->doubleOrNullStringAttribute(result);

  V8SetReturnValue(info, result);
}

static void doubleOrNullStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "doubleOrNullStringAttribute");

  // Prepare the value to be set.
  DoubleOrString cppValue;
  V8DoubleOrString::ToImpl(info.GetIsolate(), v8Value, cppValue, UnionTypeConversionMode::kNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setDoubleOrNullStringAttribute(cppValue);
}

static void stringOrStringSequenceAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  StringOrStringSequence result;
  impl->stringOrStringSequenceAttribute(result);

  V8SetReturnValue(info, result);
}

static void stringOrStringSequenceAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "stringOrStringSequenceAttribute");

  // Prepare the value to be set.
  StringOrStringSequence cppValue;
  V8StringOrStringSequence::ToImpl(info.GetIsolate(), v8Value, cppValue, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setStringOrStringSequenceAttribute(cppValue);
}

static void testEnumOrDoubleAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  TestEnumOrDouble result;
  impl->testEnumOrDoubleAttribute(result);

  V8SetReturnValue(info, result);
}

static void testEnumOrDoubleAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "testEnumOrDoubleAttribute");

  // Prepare the value to be set.
  TestEnumOrDouble cppValue;
  V8TestEnumOrDouble::ToImpl(info.GetIsolate(), v8Value, cppValue, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setTestEnumOrDoubleAttribute(cppValue);
}

static void unrestrictedDoubleOrStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  UnrestrictedDoubleOrString result;
  impl->unrestrictedDoubleOrStringAttribute(result);

  V8SetReturnValue(info, result);
}

static void unrestrictedDoubleOrStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "unrestrictedDoubleOrStringAttribute");

  // Prepare the value to be set.
  UnrestrictedDoubleOrString cppValue;
  V8UnrestrictedDoubleOrString::ToImpl(info.GetIsolate(), v8Value, cppValue, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnrestrictedDoubleOrStringAttribute(cppValue);
}

static void nestedUnionAtributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  DoubleOrStringOrDoubleOrStringSequence result;
  impl->nestedUnionAtribute(result);

  V8SetReturnValue(info, result);
}

static void nestedUnionAtributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "nestedUnionAtribute");

  // Prepare the value to be set.
  DoubleOrStringOrDoubleOrStringSequence cppValue;
  V8DoubleOrStringOrDoubleOrStringSequence::ToImpl(info.GetIsolate(), v8Value, cppValue, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setNestedUnionAtribute(cppValue);
}

static void activityLoggingAccessForAllWorldsLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingAccessForAllWorldsLongAttribute());
}

static void activityLoggingAccessForAllWorldsLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingAccessForAllWorldsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingAccessForAllWorldsLongAttribute(cppValue);
}

static void activityLoggingGetterForAllWorldsLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingGetterForAllWorldsLongAttribute());
}

static void activityLoggingGetterForAllWorldsLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingGetterForAllWorldsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingGetterForAllWorldsLongAttribute(cppValue);
}

static void activityLoggingSetterForAllWorldsLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingSetterForAllWorldsLongAttribute());
}

static void activityLoggingSetterForAllWorldsLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingSetterForAllWorldsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingSetterForAllWorldsLongAttribute(cppValue);
}

static void cachedAttributeAnyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  // [CachedAttribute]
  V8PrivateProperty::Symbol propertySymbol =
      V8PrivateProperty::GetSymbol(info.GetIsolate(),
          "TestObject#Cachedattributeanyattribute");
  if (!static_cast<const TestObject*>(impl)->isValueDirty()) {
    v8::Local<v8::Value> v8Value;
    if (propertySymbol.GetOrUndefined(holder).ToLocal(&v8Value) && !v8Value->IsUndefined()) {
      V8SetReturnValue(info, v8Value);
      return;
    }
  }

  ScriptValue cppValue(impl->cachedAttributeAnyAttribute());

  // [CachedAttribute]
  v8::Local<v8::Value> v8Value(cppValue.V8Value());
  propertySymbol.Set(holder, v8Value);

  V8SetReturnValue(info, v8Value);
}

static void cachedAttributeAnyAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  ScriptValue cppValue = ScriptValue(ScriptState::Current(info.GetIsolate()), v8Value);

  impl->setCachedAttributeAnyAttribute(cppValue);

  // [CachedAttribute]
  // Invalidate the cached value.
  V8PrivateProperty::GetSymbol(
      isolate, "TestObject#Cachedattributeanyattribute")
      .DeleteProperty(holder, v8::Undefined(isolate));
}

static void cachedArrayAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  // [CachedAttribute]
  V8PrivateProperty::Symbol propertySymbol =
      V8PrivateProperty::GetSymbol(info.GetIsolate(),
          "TestObject#Cachedarrayattribute");
  if (!static_cast<const TestObject*>(impl)->isFrozenArrayDirty()) {
    v8::Local<v8::Value> v8Value;
    if (propertySymbol.GetOrUndefined(holder).ToLocal(&v8Value) && !v8Value->IsUndefined()) {
      V8SetReturnValue(info, v8Value);
      return;
    }
  }

  Vector<String> cppValue(impl->cachedArrayAttribute());

  // [CachedAttribute]
  v8::Local<v8::Value> v8Value(FreezeV8Object(ToV8(cppValue, holder, info.GetIsolate()), info.GetIsolate()));
  propertySymbol.Set(holder, v8Value);

  V8SetReturnValue(info, v8Value);
}

static void cachedArrayAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "cachedArrayAttribute");

  // Prepare the value to be set.
  Vector<String> cppValue = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setCachedArrayAttribute(cppValue);

  // [CachedAttribute]
  // Invalidate the cached value.
  V8PrivateProperty::GetSymbol(
      isolate, "TestObject#Cachedarrayattribute")
      .DeleteProperty(holder, v8::Undefined(isolate));
}

static void cachedStringOrNoneAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  // [CachedAttribute]
  V8PrivateProperty::Symbol propertySymbol =
      V8PrivateProperty::GetSymbol(info.GetIsolate(),
          "TestObject#Cachedstringornoneattribute");
  if (!static_cast<const TestObject*>(impl)->isStringDirty()) {
    v8::Local<v8::Value> v8Value;
    if (propertySymbol.GetOrUndefined(holder).ToLocal(&v8Value) && !v8Value->IsUndefined()) {
      V8SetReturnValue(info, v8Value);
      return;
    }
  }

  String cppValue(impl->cachedStringOrNoneAttribute());

  // [CachedAttribute]
  v8::Local<v8::Value> v8Value(cppValue.IsNull() ? v8::Local<v8::Value>(v8::Null(info.GetIsolate())) : V8String(info.GetIsolate(), cppValue));
  propertySymbol.Set(holder, v8Value);

  V8SetReturnValue(info, v8Value);
}

static void cachedStringOrNoneAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<kTreatNullAndUndefinedAsNullString> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setCachedStringOrNoneAttribute(cppValue);

  // [CachedAttribute]
  // Invalidate the cached value.
  V8PrivateProperty::GetSymbol(
      isolate, "TestObject#Cachedstringornoneattribute")
      .DeleteProperty(holder, v8::Undefined(isolate));
}

static void callWithExecutionContextAnyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExecutionContext* executionContext = ExecutionContext::ForRelevantRealm(info);

  V8SetReturnValue(info, impl->callWithExecutionContextAnyAttribute(executionContext).V8Value());
}

static void callWithExecutionContextAnyAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  ScriptValue cppValue = ScriptValue(ScriptState::Current(info.GetIsolate()), v8Value);

  ExecutionContext* executionContext = ExecutionContext::ForRelevantRealm(info);

  impl->setCallWithExecutionContextAnyAttribute(executionContext, cppValue);
}

static void callWithScriptStateAnyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  V8SetReturnValue(info, impl->callWithScriptStateAnyAttribute(scriptState).V8Value());
}

static void callWithScriptStateAnyAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  ScriptValue cppValue = ScriptValue(ScriptState::Current(info.GetIsolate()), v8Value);

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  impl->setCallWithScriptStateAnyAttribute(scriptState, cppValue);
}

static void callWithExecutionContextAndScriptStateAnyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExecutionContext* executionContext = ExecutionContext::ForRelevantRealm(info);

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  V8SetReturnValue(info, impl->callWithExecutionContextAndScriptStateAnyAttribute(scriptState, executionContext).V8Value());
}

static void callWithExecutionContextAndScriptStateAnyAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  ScriptValue cppValue = ScriptValue(ScriptState::Current(info.GetIsolate()), v8Value);

  ExecutionContext* executionContext = ExecutionContext::ForRelevantRealm(info);

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  impl->setCallWithExecutionContextAndScriptStateAnyAttribute(scriptState, executionContext, cppValue);
}

static void checkSecurityForNodeReadonlyDocumentAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Perform a security check for the returned object.
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kGetterContext, "TestObject", "checkSecurityForNodeReadonlyDocumentAttribute");
  if (!BindingSecurity::ShouldAllowAccessTo(CurrentDOMWindow(info.GetIsolate()), WTF::GetPtr(impl->checkSecurityForNodeReadonlyDocumentAttribute()), BindingSecurity::ErrorReportOption::kDoNotReport)) {
    UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                      WebFeature::kCrossOriginTestObjectCheckSecurityForNodeReadonlyDocumentAttribute);
    V8SetReturnValueNull(info);
    return;
  }

  V8SetReturnValue(info, ToV8(WTF::GetPtr(impl->checkSecurityForNodeReadonlyDocumentAttribute()), ToV8(impl->contentWindow(), v8::Local<v8::Object>(), info.GetIsolate()).As<v8::Object>(), info.GetIsolate()));
}

static void customGetterLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "customGetterLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setCustomGetterLongAttribute(cppValue);
}

static void customSetterLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->customSetterLongAttribute());
}

static void deprecatedLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->deprecatedLongAttribute());
}

static void deprecatedLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "deprecatedLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setDeprecatedLongAttribute(cppValue);
}

static void enforceRangeLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->enforceRangeLongAttribute());
}

static void enforceRangeLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "enforceRangeLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLongEnforceRange>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setEnforceRangeLongAttribute(cppValue);
}

static void implementedAsLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->implementedAsName());
}

static void implementedAsLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "implementedAsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setImplementedAsName(cppValue);
}

static void customGetterImplementedAsLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "customGetterImplementedAsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setImplementedAsNameWithCustomGetter(cppValue);
}

static void customSetterImplementedAsLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->implementedAsNameWithCustomGetter());
}

static void measureAsLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->measureAsLongAttribute());
}

static void measureAsLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "measureAsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setMeasureAsLongAttribute(cppValue);
}

static void notEnumerableLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->notEnumerableLongAttribute());
}

static void notEnumerableLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "notEnumerableLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setNotEnumerableLongAttribute(cppValue);
}

static void originTrialEnabledLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->originTrialEnabledLongAttribute());
}

static void originTrialEnabledLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "originTrialEnabledLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setOriginTrialEnabledLongAttribute(cppValue);
}

static void perWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  TestInterfaceEmpty* cppValue(WTF::GetPtr(impl->perWorldBindingsReadonlyTestInterfaceEmptyAttribute()));

  // Keep the wrapper object for the return value alive as long as |this|
  // object is alive in order to save creation time of the wrapper object.
  if (cppValue && DOMDataStore::SetReturnValue(info.GetReturnValue(), cppValue))
    return;
  v8::Local<v8::Value> v8Value(ToV8(cppValue, holder, info.GetIsolate()));
  V8PrivateProperty::GetSymbol(
      info.GetIsolate(), "KeepAlive#TestObject#perWorldBindingsReadonlyTestInterfaceEmptyAttribute")
      .Set(holder, v8Value);

  V8SetReturnValue(info, v8Value);
}

static void perWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  TestInterfaceEmpty* cppValue(WTF::GetPtr(impl->perWorldBindingsReadonlyTestInterfaceEmptyAttribute()));

  // Keep the wrapper object for the return value alive as long as |this|
  // object is alive in order to save creation time of the wrapper object.
  if (cppValue && DOMDataStore::SetReturnValueForMainWorld(info.GetReturnValue(), cppValue))
    return;
  v8::Local<v8::Value> v8Value(ToV8(cppValue, holder, info.GetIsolate()));
  V8PrivateProperty::GetSymbol(
      info.GetIsolate(), "KeepAlive#TestObject#perWorldBindingsReadonlyTestInterfaceEmptyAttribute")
      .Set(holder, v8Value);

  V8SetReturnValue(info, v8Value);
}

static void activityLoggingAccessPerWorldBindingsLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingAccessPerWorldBindingsLongAttribute());
}

static void activityLoggingAccessPerWorldBindingsLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingAccessPerWorldBindingsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingAccessPerWorldBindingsLongAttribute(cppValue);
}

static void activityLoggingAccessPerWorldBindingsLongAttributeAttributeGetterForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingAccessPerWorldBindingsLongAttribute());
}

static void activityLoggingAccessPerWorldBindingsLongAttributeAttributeSetterForMainWorld(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingAccessPerWorldBindingsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingAccessPerWorldBindingsLongAttribute(cppValue);
}

static void activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute());
}

static void activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute(cppValue);
}

static void activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute());
}

static void activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterForMainWorld(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute(cppValue);
}

static void activityLoggingGetterPerWorldBindingsLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingGetterPerWorldBindingsLongAttribute());
}

static void activityLoggingGetterPerWorldBindingsLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingGetterPerWorldBindingsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingGetterPerWorldBindingsLongAttribute(cppValue);
}

static void activityLoggingGetterPerWorldBindingsLongAttributeAttributeGetterForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingGetterPerWorldBindingsLongAttribute());
}

static void activityLoggingGetterPerWorldBindingsLongAttributeAttributeSetterForMainWorld(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingGetterPerWorldBindingsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingGetterPerWorldBindingsLongAttribute(cppValue);
}

static void activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute());
}

static void activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute(cppValue);
}

static void activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute());
}

static void activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterForMainWorld(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute(cppValue);
}

static void locationAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->location()), impl);
}

static void locationAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // [PutForwards] => location.href
  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "location");
  v8::Local<v8::Value> target;
  if (!holder->Get(isolate->GetCurrentContext(), V8AtomicString(isolate, "location")).ToLocal(&target))
    return;
  if (!target->IsObject()) {
    exceptionState.ThrowTypeError("The attribute value is not an object");
    return;
  }
  bool result;
  if (!target.As<v8::Object>()->Set(isolate->GetCurrentContext(), V8AtomicString(isolate, "href"), v8Value).To(&result))
    return;
  if (!result)
    return;
}

static void locationWithExceptionAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->locationWithException()), impl);
}

static void locationWithExceptionAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // [PutForwards] => locationWithException.hrefThrows
  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "locationWithException");
  v8::Local<v8::Value> target;
  if (!holder->Get(isolate->GetCurrentContext(), V8AtomicString(isolate, "locationWithException")).ToLocal(&target))
    return;
  if (!target->IsObject()) {
    exceptionState.ThrowTypeError("The attribute value is not an object");
    return;
  }
  bool result;
  if (!target.As<v8::Object>()->Set(isolate->GetCurrentContext(), V8AtomicString(isolate, "hrefThrows"), v8Value).To(&result))
    return;
  if (!result)
    return;
}

static void locationWithCallWithAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->locationWithCallWith()), impl);
}

static void locationWithCallWithAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // [PutForwards] => locationWithCallWith.hrefCallWith
  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "locationWithCallWith");
  v8::Local<v8::Value> target;
  if (!holder->Get(isolate->GetCurrentContext(), V8AtomicString(isolate, "locationWithCallWith")).ToLocal(&target))
    return;
  if (!target->IsObject()) {
    exceptionState.ThrowTypeError("The attribute value is not an object");
    return;
  }
  bool result;
  if (!target.As<v8::Object>()->Set(isolate->GetCurrentContext(), V8AtomicString(isolate, "hrefCallWith"), v8Value).To(&result))
    return;
  if (!result)
    return;
}

static void locationByteStringAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->locationByteString()), impl);
}

static void locationByteStringAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // [PutForwards] => locationByteString.hrefByteString
  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "locationByteString");
  v8::Local<v8::Value> target;
  if (!holder->Get(isolate->GetCurrentContext(), V8AtomicString(isolate, "locationByteString")).ToLocal(&target))
    return;
  if (!target->IsObject()) {
    exceptionState.ThrowTypeError("The attribute value is not an object");
    return;
  }
  bool result;
  if (!target.As<v8::Object>()->Set(isolate->GetCurrentContext(), V8AtomicString(isolate, "hrefByteString"), v8Value).To(&result))
    return;
  if (!result)
    return;
}

static void locationWithPerWorldBindingsAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->locationWithPerWorldBindings()), impl);
}

static void locationWithPerWorldBindingsAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // [PutForwards] => locationWithPerWorldBindings.href
  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "locationWithPerWorldBindings");
  v8::Local<v8::Value> target;
  if (!holder->Get(isolate->GetCurrentContext(), V8AtomicString(isolate, "locationWithPerWorldBindings")).ToLocal(&target))
    return;
  if (!target->IsObject()) {
    exceptionState.ThrowTypeError("The attribute value is not an object");
    return;
  }
  bool result;
  if (!target.As<v8::Object>()->Set(isolate->GetCurrentContext(), V8AtomicString(isolate, "href"), v8Value).To(&result))
    return;
  if (!result)
    return;
}

static void locationWithPerWorldBindingsAttributeGetterForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueForMainWorld(info, WTF::GetPtr(impl->locationWithPerWorldBindings()));
}

static void locationWithPerWorldBindingsAttributeSetterForMainWorld(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // [PutForwards] => locationWithPerWorldBindings.href
  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "locationWithPerWorldBindings");
  v8::Local<v8::Value> target;
  if (!holder->Get(isolate->GetCurrentContext(), V8AtomicString(isolate, "locationWithPerWorldBindings")).ToLocal(&target))
    return;
  if (!target->IsObject()) {
    exceptionState.ThrowTypeError("The attribute value is not an object");
    return;
  }
  bool result;
  if (!target.As<v8::Object>()->Set(isolate->GetCurrentContext(), V8AtomicString(isolate, "href"), v8Value).To(&result))
    return;
  if (!result)
    return;
}

static void locationLegacyInterfaceTypeCheckingAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->locationLegacyInterfaceTypeChecking()), impl);
}

static void locationLegacyInterfaceTypeCheckingAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // [PutForwards] => locationLegacyInterfaceTypeChecking.href
  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "locationLegacyInterfaceTypeChecking");
  v8::Local<v8::Value> target;
  if (!holder->Get(isolate->GetCurrentContext(), V8AtomicString(isolate, "locationLegacyInterfaceTypeChecking")).ToLocal(&target))
    return;
  if (!target->IsObject()) {
    exceptionState.ThrowTypeError("The attribute value is not an object");
    return;
  }
  bool result;
  if (!target.As<v8::Object>()->Set(isolate->GetCurrentContext(), V8AtomicString(isolate, "href"), v8Value).To(&result))
    return;
  if (!result)
    return;
}

static void raisesExceptionLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kGetterContext, "TestObject", "raisesExceptionLongAttribute");

  int32_t cppValue(impl->raisesExceptionLongAttribute(exceptionState));

  if (UNLIKELY(exceptionState.HadException()))
    return;

  V8SetReturnValueInt(info, cppValue);
}

static void raisesExceptionLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "raisesExceptionLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setRaisesExceptionLongAttribute(cppValue, exceptionState);
}

static void raisesExceptionGetterLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kGetterContext, "TestObject", "raisesExceptionGetterLongAttribute");

  int32_t cppValue(impl->raisesExceptionGetterLongAttribute(exceptionState));

  if (UNLIKELY(exceptionState.HadException()))
    return;

  V8SetReturnValueInt(info, cppValue);
}

static void raisesExceptionGetterLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "raisesExceptionGetterLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setRaisesExceptionGetterLongAttribute(cppValue);
}

static void setterRaisesExceptionLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->setterRaisesExceptionLongAttribute());
}

static void setterRaisesExceptionLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "setterRaisesExceptionLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSetterRaisesExceptionLongAttribute(cppValue, exceptionState);
}

static void raisesExceptionTestInterfaceEmptyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kGetterContext, "TestObject", "raisesExceptionTestInterfaceEmptyAttribute");

  TestInterfaceEmpty* cppValue(impl->raisesExceptionTestInterfaceEmptyAttribute(exceptionState));

  if (UNLIKELY(exceptionState.HadException()))
    return;

  V8SetReturnValueFast(info, cppValue, impl);
}

static void raisesExceptionTestInterfaceEmptyAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "raisesExceptionTestInterfaceEmptyAttribute");

  // Prepare the value to be set.
  TestInterfaceEmpty* cppValue = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'TestInterfaceEmpty'.");
    return;
  }

  impl->setRaisesExceptionTestInterfaceEmptyAttribute(cppValue, exceptionState);
}

static void cachedAttributeRaisesExceptionGetterAnyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  // [CachedAttribute]
  V8PrivateProperty::Symbol propertySymbol =
      V8PrivateProperty::GetSymbol(info.GetIsolate(),
          "TestObject#Cachedattributeraisesexceptiongetteranyattribute");
  if (!static_cast<const TestObject*>(impl)->isValueDirty()) {
    v8::Local<v8::Value> v8Value;
    if (propertySymbol.GetOrUndefined(holder).ToLocal(&v8Value) && !v8Value->IsUndefined()) {
      V8SetReturnValue(info, v8Value);
      return;
    }
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kGetterContext, "TestObject", "cachedAttributeRaisesExceptionGetterAnyAttribute");

  ScriptValue cppValue(impl->cachedAttributeRaisesExceptionGetterAnyAttribute(exceptionState));

  if (UNLIKELY(exceptionState.HadException()))
    return;

  // [CachedAttribute]
  v8::Local<v8::Value> v8Value(cppValue.V8Value());
  propertySymbol.Set(holder, v8Value);

  V8SetReturnValue(info, v8Value);
}

static void cachedAttributeRaisesExceptionGetterAnyAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "cachedAttributeRaisesExceptionGetterAnyAttribute");

  // Prepare the value to be set.
  ScriptValue cppValue = ScriptValue(ScriptState::Current(info.GetIsolate()), v8Value);

  impl->setCachedAttributeRaisesExceptionGetterAnyAttribute(cppValue, exceptionState);

  // [CachedAttribute]
  // Invalidate the cached value.
  V8PrivateProperty::GetSymbol(
      isolate, "TestObject#Cachedattributeraisesexceptiongetteranyattribute")
      .DeleteProperty(holder, v8::Undefined(isolate));
}

static void reflectTestInterfaceAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, impl->FastGetAttribute(HTMLNames::reflecttestinterfaceattributeAttr), impl);
}

static void reflectTestInterfaceAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "reflectTestInterfaceAttribute");

  // Prepare the value to be set.
  TestInterfaceImplementation* cppValue = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'TestInterface'.");
    return;
  }

  impl->setAttribute(HTMLNames::reflecttestinterfaceattributeAttr, cppValue);
}

static void reflectReflectedNameAttributeTestAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, impl->FastGetAttribute(HTMLNames::reflectedNameAttributeAttr), impl);
}

static void reflectReflectedNameAttributeTestAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "reflectReflectedNameAttributeTestAttribute");

  // Prepare the value to be set.
  TestInterfaceImplementation* cppValue = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'TestInterface'.");
    return;
  }

  impl->setAttribute(HTMLNames::reflectedNameAttributeAttr, cppValue);
}

static void reflectBooleanAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueBool(info, impl->FastHasAttribute(HTMLNames::reflectbooleanattributeAttr));
}

static void reflectBooleanAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "reflectBooleanAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->SetBooleanAttribute(HTMLNames::reflectbooleanattributeAttr, cppValue);
}

static void reflectLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->GetIntegralAttribute(HTMLNames::reflectlongattributeAttr));
}

static void reflectLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "reflectLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->SetIntegralAttribute(HTMLNames::reflectlongattributeAttr, cppValue);
}

static void reflectUnsignedShortAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueUnsigned(info, std::max(0, static_cast<int>(impl->FastGetAttribute(HTMLNames::reflectunsignedshortattributeAttr))));
}

static void reflectUnsignedShortAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "reflectUnsignedShortAttribute");

  // Prepare the value to be set.
  uint16_t cppValue = NativeValueTraits<IDLUnsignedShort>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setAttribute(HTMLNames::reflectunsignedshortattributeAttr, cppValue);
}

static void reflectUnsignedLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueUnsigned(info, std::max(0, static_cast<int>(impl->GetIntegralAttribute(HTMLNames::reflectunsignedlongattributeAttr))));
}

static void reflectUnsignedLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "reflectUnsignedLongAttribute");

  // Prepare the value to be set.
  uint32_t cppValue = NativeValueTraits<IDLUnsignedLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->SetUnsignedIntegralAttribute(HTMLNames::reflectunsignedlongattributeAttr, cppValue);
}

static void idAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->GetIdAttribute(), info.GetIsolate());
}

static void idAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(HTMLNames::idAttr, cppValue);
}

static void nameAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->GetNameAttribute(), info.GetIsolate());
}

static void nameAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(HTMLNames::nameAttr, cppValue);
}

static void classAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->GetClassAttribute(), info.GetIsolate());
}

static void classAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(HTMLNames::classAttr, cppValue);
}

static void reflectedIdAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->GetIdAttribute(), info.GetIsolate());
}

static void reflectedIdAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(HTMLNames::idAttr, cppValue);
}

static void reflectedNameAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->GetNameAttribute(), info.GetIsolate());
}

static void reflectedNameAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(HTMLNames::nameAttr, cppValue);
}

static void reflectedClassAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->GetClassAttribute(), info.GetIsolate());
}

static void reflectedClassAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(HTMLNames::classAttr, cppValue);
}

static void limitedToOnlyOneAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  String cppValue(impl->FastGetAttribute(HTMLNames::limitedtoonlyoneattributeAttr));

  if (cppValue.IsEmpty()) {
    ;
  } else if (EqualIgnoringASCIICase(cppValue, "unique")) {
    cppValue = "unique";
  } else {
    cppValue = "";
  }

  V8SetReturnValueString(info, cppValue, info.GetIsolate());
}

static void limitedToOnlyOneAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(HTMLNames::limitedtoonlyoneattributeAttr, cppValue);
}

static void limitedToOnlyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  String cppValue(impl->FastGetAttribute(HTMLNames::limitedtoonlyattributeAttr));

  if (cppValue.IsEmpty()) {
    ;
  } else if (EqualIgnoringASCIICase(cppValue, "Per")) {
    cppValue = "Per";
  } else if (EqualIgnoringASCIICase(cppValue, "Paal")) {
    cppValue = "Paal";
  } else if (EqualIgnoringASCIICase(cppValue, "Espen")) {
    cppValue = "Espen";
  } else {
    cppValue = "";
  }

  V8SetReturnValueString(info, cppValue, info.GetIsolate());
}

static void limitedToOnlyAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(HTMLNames::limitedtoonlyattributeAttr, cppValue);
}

static void limitedToOnlyOtherAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  String cppValue(impl->FastGetAttribute(HTMLNames::otherAttr));

  if (cppValue.IsEmpty()) {
    ;
  } else if (EqualIgnoringASCIICase(cppValue, "Value1")) {
    cppValue = "Value1";
  } else if (EqualIgnoringASCIICase(cppValue, "Value2")) {
    cppValue = "Value2";
  } else {
    cppValue = "";
  }

  V8SetReturnValueString(info, cppValue, info.GetIsolate());
}

static void limitedToOnlyOtherAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(HTMLNames::otherAttr, cppValue);
}

static void limitedWithMissingDefaultAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  String cppValue(impl->FastGetAttribute(HTMLNames::limitedwithmissingdefaultattributeAttr));

  if (cppValue.IsEmpty()) {
    cppValue = "rsa";
  } else if (EqualIgnoringASCIICase(cppValue, "rsa")) {
    cppValue = "rsa";
  } else if (EqualIgnoringASCIICase(cppValue, "dsa")) {
    cppValue = "dsa";
  } else {
    cppValue = "";
  }

  V8SetReturnValueString(info, cppValue, info.GetIsolate());
}

static void limitedWithMissingDefaultAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(HTMLNames::limitedwithmissingdefaultattributeAttr, cppValue);
}

static void limitedWithInvalidMissingDefaultAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  String cppValue(impl->FastGetAttribute(HTMLNames::limitedwithinvalidmissingdefaultattributeAttr));

  if (cppValue.IsEmpty()) {
    cppValue = "auto";
  } else if (EqualIgnoringASCIICase(cppValue, "ltr")) {
    cppValue = "ltr";
  } else if (EqualIgnoringASCIICase(cppValue, "rtl")) {
    cppValue = "rtl";
  } else if (EqualIgnoringASCIICase(cppValue, "auto")) {
    cppValue = "auto";
  } else {
    cppValue = "ltr";
  }

  V8SetReturnValueString(info, cppValue, info.GetIsolate());
}

static void limitedWithInvalidMissingDefaultAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(HTMLNames::limitedwithinvalidmissingdefaultattributeAttr, cppValue);
}

static void corsSettingAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  String cppValue(impl->FastGetAttribute(HTMLNames::corssettingattributeAttr));

  if (cppValue.IsNull()) {
    ;
  } else if (cppValue.IsEmpty()) {
    cppValue = "anonymous";
  } else if (EqualIgnoringASCIICase(cppValue, "anonymous")) {
    cppValue = "anonymous";
  } else if (EqualIgnoringASCIICase(cppValue, "use-credentials")) {
    cppValue = "use-credentials";
  } else {
    cppValue = "anonymous";
  }

  V8SetReturnValueString(info, cppValue, info.GetIsolate());
}

static void limitedWithEmptyMissingInvalidAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  String cppValue(impl->FastGetAttribute(HTMLNames::limitedwithemptymissinginvalidattributeAttr));

  if (cppValue.IsNull()) {
    cppValue = "missing";
  } else if (cppValue.IsEmpty()) {
    cppValue = "empty";
  } else if (EqualIgnoringASCIICase(cppValue, "empty")) {
    cppValue = "empty";
  } else if (EqualIgnoringASCIICase(cppValue, "missing")) {
    cppValue = "missing";
  } else if (EqualIgnoringASCIICase(cppValue, "invalid")) {
    cppValue = "invalid";
  } else if (EqualIgnoringASCIICase(cppValue, "a-normal")) {
    cppValue = "a-normal";
  } else {
    cppValue = "invalid";
  }

  V8SetReturnValueString(info, cppValue, info.GetIsolate());
}

static void RuntimeCallStatsCounterAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->runtimeCallStatsCounterAttribute());
}

static void RuntimeCallStatsCounterAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "RuntimeCallStatsCounterAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setRuntimeCallStatsCounterAttribute(cppValue);
}

static void RuntimeCallStatsCounterReadOnlyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, impl->runtimeCallStatsCounterReadOnlyAttribute());
}

static void replaceableReadonlyLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->replaceableReadonlyLongAttribute());
}

static void replaceableReadonlyLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // Prepare the value to be set.

  v8::Local<v8::String> propertyName = V8AtomicString(isolate, "replaceableReadonlyLongAttribute");
  if (info.Holder()->CreateDataProperty(info.GetIsolate()->GetCurrentContext(), propertyName, v8Value).IsNothing())
    return;
}

static void locationPutForwardsAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->locationPutForwards()), impl);
}

static void locationPutForwardsAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // [PutForwards] => locationPutForwards.href
  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "locationPutForwards");
  v8::Local<v8::Value> target;
  if (!holder->Get(isolate->GetCurrentContext(), V8AtomicString(isolate, "locationPutForwards")).ToLocal(&target))
    return;
  if (!target->IsObject()) {
    exceptionState.ThrowTypeError("The attribute value is not an object");
    return;
  }
  bool result;
  if (!target.As<v8::Object>()->Set(isolate->GetCurrentContext(), V8AtomicString(isolate, "href"), v8Value).To(&result))
    return;
  if (!result)
    return;
}

static void runtimeEnabledLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->runtimeEnabledLongAttribute());
}

static void runtimeEnabledLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "runtimeEnabledLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setRuntimeEnabledLongAttribute(cppValue);
}

static void setterCallWithCurrentWindowAndEnteredWindowStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->setterCallWithCurrentWindowAndEnteredWindowStringAttribute(), info.GetIsolate());
}

static void setterCallWithCurrentWindowAndEnteredWindowStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setSetterCallWithCurrentWindowAndEnteredWindowStringAttribute(CurrentDOMWindow(info.GetIsolate()), EnteredDOMWindow(info.GetIsolate()), cppValue);
}

static void setterCallWithExecutionContextStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->setterCallWithExecutionContextStringAttribute(), info.GetIsolate());
}

static void setterCallWithExecutionContextStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  ExecutionContext* executionContext = ExecutionContext::ForRelevantRealm(info);

  impl->setSetterCallWithExecutionContextStringAttribute(executionContext, cppValue);
}

static void treatNullAsEmptyStringStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->treatNullAsEmptyStringStringAttribute(), info.GetIsolate());
}

static void treatNullAsEmptyStringStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<kTreatNullAsEmptyString> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setTreatNullAsEmptyStringStringAttribute(cppValue);
}

static void legacyInterfaceTypeCheckingFloatAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, impl->legacyInterfaceTypeCheckingFloatAttribute());
}

static void legacyInterfaceTypeCheckingFloatAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "legacyInterfaceTypeCheckingFloatAttribute");

  // Prepare the value to be set.
  float cppValue = NativeValueTraits<IDLFloat>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setLegacyInterfaceTypeCheckingFloatAttribute(cppValue);
}

static void legacyInterfaceTypeCheckingTestInterfaceAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->legacyInterfaceTypeCheckingTestInterfaceAttribute()), impl);
}

static void legacyInterfaceTypeCheckingTestInterfaceAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  TestInterfaceImplementation* cppValue = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  impl->setLegacyInterfaceTypeCheckingTestInterfaceAttribute(cppValue);
}

static void legacyInterfaceTypeCheckingTestInterfaceOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->legacyInterfaceTypeCheckingTestInterfaceOrNullAttribute()), impl);
}

static void legacyInterfaceTypeCheckingTestInterfaceOrNullAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  TestInterfaceImplementation* cppValue = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  impl->setLegacyInterfaceTypeCheckingTestInterfaceOrNullAttribute(cppValue);
}

static void urlStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->GetURLAttribute(HTMLNames::urlstringattributeAttr), info.GetIsolate());
}

static void urlStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(HTMLNames::urlstringattributeAttr, cppValue);
}

static void urlStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->GetURLAttribute(HTMLNames::reflectUrlAttributeAttr), info.GetIsolate());
}

static void urlStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(HTMLNames::reflectUrlAttributeAttr, cppValue);
}

static void unforgeableLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->unforgeableLongAttribute());
}

static void unforgeableLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "unforgeableLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnforgeableLongAttribute(cppValue);
}

static void measuredLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->measuredLongAttribute());
}

static void measuredLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "measuredLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setMeasuredLongAttribute(cppValue);
}

static void sameObjectAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  TestInterfaceImplementation* cppValue(WTF::GetPtr(impl->sameObjectAttribute()));

  // Keep the wrapper object for the return value alive as long as |this|
  // object is alive in order to save creation time of the wrapper object.
  if (cppValue && DOMDataStore::SetReturnValue(info.GetReturnValue(), cppValue))
    return;
  v8::Local<v8::Value> v8Value(ToV8(cppValue, holder, info.GetIsolate()));
  V8PrivateProperty::GetSymbol(
      info.GetIsolate(), "KeepAlive#TestObject#sameObjectAttribute")
      .Set(holder, v8Value);

  V8SetReturnValue(info, v8Value);
}

static void saveSameObjectAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  // [SaveSameObject]
  // If you see a compile error that
  //   V8PrivateProperty::GetSameObjectTestObjectSaveSameObjectAttribute
  // is not defined, then you need to register your attribute at
  // V8_PRIVATE_PROPERTY_FOR_EACH defined in V8PrivateProperty.h as
  //   X(SameObject, TestObjectSaveSameObjectAttribute)
  auto privateSameObject = V8PrivateProperty::GetSameObjectTestObjectSaveSameObjectAttribute(info.GetIsolate());
  {
    v8::Local<v8::Value> v8Value;
    if (privateSameObject.GetOrUndefined(holder).ToLocal(&v8Value) && !v8Value->IsUndefined()) {
      V8SetReturnValue(info, v8Value);
      return;
    }
  }

  TestObject* impl = V8TestObject::ToImpl(holder);

  TestInterfaceImplementation* cppValue(WTF::GetPtr(impl->saveSameObjectAttribute()));

  // Keep the wrapper object for the return value alive as long as |this|
  // object is alive in order to save creation time of the wrapper object.
  if (cppValue && DOMDataStore::SetReturnValue(info.GetReturnValue(), cppValue))
    return;
  v8::Local<v8::Value> v8Value(ToV8(cppValue, holder, info.GetIsolate()));
  V8PrivateProperty::GetSymbol(
      info.GetIsolate(), "KeepAlive#TestObject#saveSameObjectAttribute")
      .Set(holder, v8Value);

  V8SetReturnValue(info, v8Value);

  // [SaveSameObject]
  privateSameObject.Set(holder, info.GetReturnValue().Get());
}

static void staticSaveSameObjectAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  // [SaveSameObject]
  // If you see a compile error that
  //   V8PrivateProperty::GetSameObjectTestObjectStaticSaveSameObjectAttribute
  // is not defined, then you need to register your attribute at
  // V8_PRIVATE_PROPERTY_FOR_EACH defined in V8PrivateProperty.h as
  //   X(SameObject, TestObjectStaticSaveSameObjectAttribute)
  auto privateSameObject = V8PrivateProperty::GetSameObjectTestObjectStaticSaveSameObjectAttribute(info.GetIsolate());
  {
    v8::Local<v8::Value> v8Value;
    if (privateSameObject.GetOrUndefined(holder).ToLocal(&v8Value) && !v8Value->IsUndefined()) {
      V8SetReturnValue(info, v8Value);
      return;
    }
  }

  V8SetReturnValue(info, WTF::GetPtr(TestObject::staticSaveSameObjectAttribute()), info.GetIsolate()->GetCurrentContext()->Global());

  // [SaveSameObject]
  privateSameObject.Set(holder, info.GetReturnValue().Get());
}

static void unscopableLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->unscopableLongAttribute());
}

static void unscopableLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "unscopableLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnscopableLongAttribute(cppValue);
}

static void unscopableOriginTrialEnabledLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->unscopableOriginTrialEnabledLongAttribute());
}

static void unscopableOriginTrialEnabledLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "unscopableOriginTrialEnabledLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnscopableOriginTrialEnabledLongAttribute(cppValue);
}

static void unscopableRuntimeEnabledLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->unscopableRuntimeEnabledLongAttribute());
}

static void unscopableRuntimeEnabledLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "unscopableRuntimeEnabledLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnscopableRuntimeEnabledLongAttribute(cppValue);
}

static void testInterfaceAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->testInterfaceAttribute()), impl);
}

static void testInterfaceAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "testInterfaceAttribute");

  // Prepare the value to be set.
  TestInterfaceImplementation* cppValue = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'TestInterface'.");
    return;
  }

  impl->setTestInterfaceAttribute(cppValue);
}

static void sizeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueUnsigned(info, impl->size());
}

static void unscopableVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->unscopableVoidMethod();
}

static void unscopableRuntimeEnabledVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->unscopableRuntimeEnabledVoidMethod();
}

static void voidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->voidMethod();
}

static void staticVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject::staticVoidMethod();
}

static void dateMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, v8::Date::New(info.GetIsolate()->GetCurrentContext(), impl->dateMethod()));
}

static void stringMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueString(info, impl->stringMethod(), info.GetIsolate());
}

static void byteStringMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueString(info, impl->byteStringMethod(), info.GetIsolate());
}

static void usvStringMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueString(info, impl->usvStringMethod(), info.GetIsolate());
}

static void readonlyDOMTimeStampMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, static_cast<double>(impl->readonlyDOMTimeStampMethod()));
}

static void booleanMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueBool(info, impl->booleanMethod());
}

static void byteMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueInt(info, impl->byteMethod());
}

static void doubleMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->doubleMethod());
}

static void floatMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->floatMethod());
}

static void longMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueInt(info, impl->longMethod());
}

static void longLongMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, static_cast<double>(impl->longLongMethod()));
}

static void octetMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueUnsigned(info, impl->octetMethod());
}

static void shortMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueInt(info, impl->shortMethod());
}

static void unsignedLongMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueUnsigned(info, impl->unsignedLongMethod());
}

static void unsignedLongLongMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, static_cast<double>(impl->unsignedLongLongMethod()));
}

static void unsignedShortMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueUnsigned(info, impl->unsignedShortMethod());
}

static void voidMethodDateArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDateArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  double dateArg;
  dateArg = NativeValueTraits<IDLDate>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDateArg(dateArg);
}

static void voidMethodStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodStringArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8StringResource<> stringArg;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  impl->voidMethodStringArg(stringArg);
}

static void voidMethodByteStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodByteStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> stringArg;
  stringArg = NativeValueTraits<IDLByteString>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodByteStringArg(stringArg);
}

static void voidMethodUSVStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodUSVStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> usvStringArg;
  usvStringArg = NativeValueTraits<IDLUSVString>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodUSVStringArg(usvStringArg);
}

static void voidMethodDOMTimeStampArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDOMTimeStampArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  uint64_t domTimeStampArg;
  domTimeStampArg = NativeValueTraits<IDLUnsignedLongLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDOMTimeStampArg(domTimeStampArg);
}

static void voidMethodBooleanArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodBooleanArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  bool booleanArg;
  booleanArg = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodBooleanArg(booleanArg);
}

static void voidMethodByteArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodByteArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int8_t byteArg;
  byteArg = NativeValueTraits<IDLByte>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodByteArg(byteArg);
}

static void voidMethodDoubleArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDoubleArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  double doubleArg;
  doubleArg = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDoubleArg(doubleArg);
}

static void voidMethodFloatArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodFloatArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  float floatArg;
  floatArg = NativeValueTraits<IDLFloat>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodFloatArg(floatArg);
}

static void voidMethodLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodLongArg(longArg);
}

static void voidMethodLongLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodLongLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int64_t longLongArg;
  longLongArg = NativeValueTraits<IDLLongLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodLongLongArg(longLongArg);
}

static void voidMethodOctetArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodOctetArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  uint8_t octetArg;
  octetArg = NativeValueTraits<IDLOctet>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodOctetArg(octetArg);
}

static void voidMethodShortArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodShortArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int16_t shortArg;
  shortArg = NativeValueTraits<IDLShort>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodShortArg(shortArg);
}

static void voidMethodUnsignedLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodUnsignedLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  uint32_t unsignedLongArg;
  unsignedLongArg = NativeValueTraits<IDLUnsignedLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodUnsignedLongArg(unsignedLongArg);
}

static void voidMethodUnsignedLongLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodUnsignedLongLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  uint64_t unsignedLongLongArg;
  unsignedLongLongArg = NativeValueTraits<IDLUnsignedLongLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodUnsignedLongLongArg(unsignedLongLongArg);
}

static void voidMethodUnsignedShortArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodUnsignedShortArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  uint16_t unsignedShortArg;
  unsignedShortArg = NativeValueTraits<IDLUnsignedShort>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodUnsignedShortArg(unsignedShortArg);
}

static void testInterfaceEmptyMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->testInterfaceEmptyMethod());
}

static void voidMethodTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestInterfaceEmptyArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  TestInterfaceEmpty* testInterfaceEmptyArg;
  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceEmptyArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestInterfaceEmptyArg", "TestObject", "parameter 1 is not of type 'TestInterfaceEmpty'."));
    return;
  }

  impl->voidMethodTestInterfaceEmptyArg(testInterfaceEmptyArg);
}

static void voidMethodLongArgTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodLongArgTestInterfaceEmptyArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 2)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  int32_t longArg;
  TestInterfaceEmpty* testInterfaceEmptyArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[1]);
  if (!testInterfaceEmptyArg) {
    exceptionState.ThrowTypeError("parameter 2 is not of type 'TestInterfaceEmpty'.");
    return;
  }

  impl->voidMethodLongArgTestInterfaceEmptyArg(longArg, testInterfaceEmptyArg);
}

static void anyMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->anyMethod().V8Value());
}

static void voidMethodEventTargetArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodEventTargetArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  EventTarget* eventTargetArg;
  eventTargetArg = V8EventTarget::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!eventTargetArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodEventTargetArg", "TestObject", "parameter 1 is not of type 'EventTarget'."));
    return;
  }

  impl->voidMethodEventTargetArg(eventTargetArg);
}

static void voidMethodAnyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodAnyArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  ScriptValue anyArg;
  anyArg = ScriptValue(ScriptState::Current(info.GetIsolate()), info[0]);

  impl->voidMethodAnyArg(anyArg);
}

static void voidMethodAttrArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodAttrArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  Attr* attrArg;
  attrArg = V8Attr::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!attrArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodAttrArg", "TestObject", "parameter 1 is not of type 'Attr'."));
    return;
  }

  impl->voidMethodAttrArg(attrArg);
}

static void voidMethodDocumentArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodDocumentArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  Document* documentArg;
  documentArg = V8Document::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!documentArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodDocumentArg", "TestObject", "parameter 1 is not of type 'Document'."));
    return;
  }

  impl->voidMethodDocumentArg(documentArg);
}

static void voidMethodDocumentTypeArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodDocumentTypeArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  DocumentType* documentTypeArg;
  documentTypeArg = V8DocumentType::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!documentTypeArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodDocumentTypeArg", "TestObject", "parameter 1 is not of type 'DocumentType'."));
    return;
  }

  impl->voidMethodDocumentTypeArg(documentTypeArg);
}

static void voidMethodElementArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodElementArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  Element* elementArg;
  elementArg = V8Element::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!elementArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodElementArg", "TestObject", "parameter 1 is not of type 'Element'."));
    return;
  }

  impl->voidMethodElementArg(elementArg);
}

static void voidMethodNodeArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodNodeArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  Node* nodeArg;
  nodeArg = V8Node::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!nodeArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodNodeArg", "TestObject", "parameter 1 is not of type 'Node'."));
    return;
  }

  impl->voidMethodNodeArg(nodeArg);
}

static void arrayBufferMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->arrayBufferMethod());
}

static void arrayBufferViewMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->arrayBufferViewMethod());
}

static void arrayBufferViewMethodRaisesExceptionMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "arrayBufferViewMethodRaisesException");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  NotShared<TestArrayBufferView> result = impl->arrayBufferViewMethodRaisesException(exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void float32ArrayMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->float32ArrayMethod());
}

static void int32ArrayMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->int32ArrayMethod());
}

static void uint8ArrayMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->uint8ArrayMethod());
}

static void voidMethodArrayBufferArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodArrayBufferArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  TestArrayBuffer* arrayBufferArg;
  arrayBufferArg = info[0]->IsArrayBuffer() ? V8ArrayBuffer::ToImpl(v8::Local<v8::ArrayBuffer>::Cast(info[0])) : 0;
  if (!arrayBufferArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodArrayBufferArg", "TestObject", "parameter 1 is not of type 'ArrayBuffer'."));
    return;
  }

  impl->voidMethodArrayBufferArg(arrayBufferArg);
}

static void voidMethodArrayBufferOrNullArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodArrayBufferOrNullArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  TestArrayBuffer* arrayBufferArg;
  arrayBufferArg = V8ArrayBuffer::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!arrayBufferArg && !IsUndefinedOrNull(info[0])) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodArrayBufferOrNullArg", "TestObject", "parameter 1 is not of type 'ArrayBuffer'."));
    return;
  }

  impl->voidMethodArrayBufferOrNullArg(arrayBufferArg);
}

static void voidMethodArrayBufferViewArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodArrayBufferViewArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  NotShared<TestArrayBufferView> arrayBufferViewArg;
  arrayBufferViewArg = ToNotShared<NotShared<TestArrayBufferView>>(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;
  if (!arrayBufferViewArg) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'ArrayBufferView'.");
    return;
  }

  impl->voidMethodArrayBufferViewArg(arrayBufferViewArg);
}

static void voidMethodFlexibleArrayBufferViewArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodFlexibleArrayBufferViewArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  FlexibleArrayBufferView arrayBufferViewArg;
  ToFlexibleArrayBufferView(info.GetIsolate(), info[0], arrayBufferViewArg, allocateFlexibleArrayBufferViewStorage(info[0]));
  if (!arrayBufferViewArg) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'ArrayBufferView'.");
    return;
  }

  impl->voidMethodFlexibleArrayBufferViewArg(arrayBufferViewArg);
}

static void voidMethodFlexibleArrayBufferViewTypedArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodFlexibleArrayBufferViewTypedArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  FlexibleFloat32ArrayView typedArrayBufferViewArg;
  ToFlexibleArrayBufferView(info.GetIsolate(), info[0], typedArrayBufferViewArg, allocateFlexibleArrayBufferViewStorage(info[0]));
  if (!typedArrayBufferViewArg) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'Float32Array'.");
    return;
  }

  impl->voidMethodFlexibleArrayBufferViewTypedArg(typedArrayBufferViewArg);
}

static void voidMethodFloat32ArrayArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodFloat32ArrayArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  NotShared<DOMFloat32Array> float32ArrayArg;
  float32ArrayArg = ToNotShared<NotShared<DOMFloat32Array>>(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;
  if (!float32ArrayArg) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'Float32Array'.");
    return;
  }

  impl->voidMethodFloat32ArrayArg(float32ArrayArg);
}

static void voidMethodInt32ArrayArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodInt32ArrayArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  NotShared<DOMInt32Array> int32ArrayArg;
  int32ArrayArg = ToNotShared<NotShared<DOMInt32Array>>(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;
  if (!int32ArrayArg) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'Int32Array'.");
    return;
  }

  impl->voidMethodInt32ArrayArg(int32ArrayArg);
}

static void voidMethodUint8ArrayArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodUint8ArrayArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  NotShared<DOMUint8Array> uint8ArrayArg;
  uint8ArrayArg = ToNotShared<NotShared<DOMUint8Array>>(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;
  if (!uint8ArrayArg) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'Uint8Array'.");
    return;
  }

  impl->voidMethodUint8ArrayArg(uint8ArrayArg);
}

static void voidMethodAllowSharedArrayBufferViewArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodAllowSharedArrayBufferViewArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  MaybeShared<TestArrayBufferView> arrayBufferViewArg;
  arrayBufferViewArg = ToMaybeShared<MaybeShared<TestArrayBufferView>>(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;
  if (!arrayBufferViewArg) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'ArrayBufferView'.");
    return;
  }

  impl->voidMethodAllowSharedArrayBufferViewArg(arrayBufferViewArg);
}

static void voidMethodAllowSharedUint8ArrayArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodAllowSharedUint8ArrayArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  MaybeShared<DOMUint8Array> uint8ArrayArg;
  uint8ArrayArg = ToMaybeShared<MaybeShared<DOMUint8Array>>(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;
  if (!uint8ArrayArg) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'Uint8Array'.");
    return;
  }

  impl->voidMethodAllowSharedUint8ArrayArg(uint8ArrayArg);
}

static void longSequenceMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, ToV8(impl->longSequenceMethod(), info.Holder(), info.GetIsolate()));
}

static void stringSequenceMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, ToV8(impl->stringSequenceMethod(), info.Holder(), info.GetIsolate()));
}

static void testInterfaceEmptySequenceMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, ToV8(impl->testInterfaceEmptySequenceMethod(), info.Holder(), info.GetIsolate()));
}

static void voidMethodSequenceLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodSequenceLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  Vector<int32_t> longSequenceArg;
  longSequenceArg = NativeValueTraits<IDLSequence<IDLLong>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodSequenceLongArg(longSequenceArg);
}

static void voidMethodSequenceStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodSequenceStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  Vector<String> stringSequenceArg;
  stringSequenceArg = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodSequenceStringArg(stringSequenceArg);
}

static void voidMethodSequenceTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodSequenceTestInterfaceEmptyArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  HeapVector<Member<TestInterfaceEmpty>> testInterfaceEmptySequenceArg;
  testInterfaceEmptySequenceArg = NativeValueTraits<IDLSequence<TestInterfaceEmpty>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodSequenceTestInterfaceEmptyArg(testInterfaceEmptySequenceArg);
}

static void voidMethodSequenceSequenceDOMStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodSequenceSequenceDOMStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  Vector<Vector<String>> stringSequenceSequenceArg;
  stringSequenceSequenceArg = NativeValueTraits<IDLSequence<IDLSequence<IDLString>>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodSequenceSequenceDOMStringArg(stringSequenceSequenceArg);
}

static void voidMethodNullableSequenceLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodNullableSequenceLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  base::Optional<Vector<int32_t>> longSequenceArg;
  if (!info[0]->IsNullOrUndefined()) {
    longSequenceArg = NativeValueTraits<IDLSequence<IDLLong>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
    if (exceptionState.HadException())
      return;
  }

  impl->voidMethodNullableSequenceLongArg(longSequenceArg);
}

static void longFrozenArrayMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, FreezeV8Object(ToV8(impl->longFrozenArrayMethod(), info.Holder(), info.GetIsolate()), info.GetIsolate()));
}

static void voidMethodStringFrozenArrayMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodStringFrozenArrayMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  Vector<String> stringFrozenArrayArg;
  stringFrozenArrayArg = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodStringFrozenArrayMethod(stringFrozenArrayArg);
}

static void voidMethodTestInterfaceEmptyFrozenArrayMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodTestInterfaceEmptyFrozenArrayMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  HeapVector<Member<TestInterfaceEmpty>> testInterfaceEmptyFrozenArrayArg;
  testInterfaceEmptyFrozenArrayArg = NativeValueTraits<IDLSequence<TestInterfaceEmpty>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodTestInterfaceEmptyFrozenArrayMethod(testInterfaceEmptyFrozenArrayArg);
}

static void nullableLongMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  base::Optional<int32_t> result = impl->nullableLongMethod();
  if (!result)
    V8SetReturnValueNull(info);
  else
    V8SetReturnValueInt(info, result.value());
}

static void nullableStringMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueStringOrNull(info, impl->nullableStringMethod(), info.GetIsolate());
}

static void nullableTestInterfaceMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->nullableTestInterfaceMethod());
}

static void nullableLongSequenceMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  base::Optional<Vector<int32_t>> result = impl->nullableLongSequenceMethod();
  if (!result)
    V8SetReturnValueNull(info);
  else
    V8SetReturnValue(info, ToV8(result.value(), info.Holder(), info.GetIsolate()));
}

static void booleanOrDOMStringOrUnrestrictedDoubleMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  BooleanOrStringOrUnrestrictedDouble result;
  impl->booleanOrDOMStringOrUnrestrictedDoubleMethod(result);
  V8SetReturnValue(info, result);
}

static void testInterfaceOrLongMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceOrLong result;
  impl->testInterfaceOrLongMethod(result);
  V8SetReturnValue(info, result);
}

static void voidMethodDoubleOrDOMStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDoubleOrDOMStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  DoubleOrString arg;
  V8DoubleOrString::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDoubleOrDOMStringArg(arg);
}

static void voidMethodDoubleOrDOMStringOrNullArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDoubleOrDOMStringOrNullArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  DoubleOrString arg;
  V8DoubleOrString::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDoubleOrDOMStringOrNullArg(arg);
}

static void voidMethodDoubleOrNullOrDOMStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDoubleOrNullOrDOMStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  DoubleOrString arg;
  V8DoubleOrString::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDoubleOrNullOrDOMStringArg(arg);
}

static void voidMethodDOMStringOrArrayBufferOrArrayBufferViewArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDOMStringOrArrayBufferOrArrayBufferViewArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  StringOrArrayBufferOrArrayBufferView arg;
  V8StringOrArrayBufferOrArrayBufferView::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDOMStringOrArrayBufferOrArrayBufferViewArg(arg);
}

static void voidMethodArrayBufferOrArrayBufferViewOrDictionaryArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodArrayBufferOrArrayBufferViewOrDictionaryArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  ArrayBufferOrArrayBufferViewOrDictionary arg;
  V8ArrayBufferOrArrayBufferViewOrDictionary::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodArrayBufferOrArrayBufferViewOrDictionaryArg(arg);
}

static void voidMethodBooleanOrElementSequenceArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodBooleanOrElementSequenceArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  BooleanOrElementSequence arg;
  V8BooleanOrElementSequence::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodBooleanOrElementSequenceArg(arg);
}

static void voidMethodDoubleOrLongOrBooleanSequenceArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDoubleOrLongOrBooleanSequenceArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  DoubleOrLongOrBooleanSequence arg;
  V8DoubleOrLongOrBooleanSequence::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDoubleOrLongOrBooleanSequenceArg(arg);
}

static void voidMethodElementSequenceOrByteStringDoubleOrStringRecordMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodElementSequenceOrByteStringDoubleOrStringRecord");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  ElementSequenceOrByteStringDoubleOrStringRecord arg;
  V8ElementSequenceOrByteStringDoubleOrStringRecord::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodElementSequenceOrByteStringDoubleOrStringRecord(arg);
}

static void voidMethodArrayOfDoubleOrDOMStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodArrayOfDoubleOrDOMStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  HeapVector<DoubleOrString> arg;
  arg = ToImplArguments<DoubleOrString>(info, 0, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodArrayOfDoubleOrDOMStringArg(arg);
}

static void voidMethodTestInterfaceEmptyOrNullArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestInterfaceEmptyOrNullArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  TestInterfaceEmpty* nullableTestInterfaceEmptyArg;
  nullableTestInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!nullableTestInterfaceEmptyArg && !IsUndefinedOrNull(info[0])) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestInterfaceEmptyOrNullArg", "TestObject", "parameter 1 is not of type 'TestInterfaceEmpty'."));
    return;
  }

  impl->voidMethodTestInterfaceEmptyOrNullArg(nullableTestInterfaceEmptyArg);
}

static void voidMethodTestCallbackInterfaceArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodTestCallbackInterfaceArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestCallbackInterfaceArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8TestCallbackInterface* testCallbackInterfaceArg;
  if (info[0]->IsObject()) {
    testCallbackInterfaceArg = V8TestCallbackInterface::CreateOrNull(info[0].As<v8::Object>());
    if (!testCallbackInterfaceArg) {
      exceptionState.ThrowSecurityError("The callback provided as parameter 1 is a cross origin object.");
      return;
    }
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestCallbackInterfaceArg", "TestObject", "The callback provided as parameter 1 is not an object."));
    return;
  }

  impl->voidMethodTestCallbackInterfaceArg(testCallbackInterfaceArg);
}

static void voidMethodOptionalTestCallbackInterfaceArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodOptionalTestCallbackInterfaceArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8TestCallbackInterface* optionalTestCallbackInterfaceArg;
  if (info[0]->IsObject()) {
    optionalTestCallbackInterfaceArg = V8TestCallbackInterface::CreateOrNull(info[0].As<v8::Object>());
    if (!optionalTestCallbackInterfaceArg) {
      exceptionState.ThrowSecurityError("The callback provided as parameter 1 is a cross origin object.");
      return;
    }
  } else if (info[0]->IsUndefined()) {
    optionalTestCallbackInterfaceArg = nullptr;
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodOptionalTestCallbackInterfaceArg", "TestObject", "The callback provided as parameter 1 is not an object."));
    return;
  }

  impl->voidMethodOptionalTestCallbackInterfaceArg(optionalTestCallbackInterfaceArg);
}

static void voidMethodTestCallbackInterfaceOrNullArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodTestCallbackInterfaceOrNullArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestCallbackInterfaceOrNullArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8TestCallbackInterface* testCallbackInterfaceArg;
  if (info[0]->IsObject()) {
    testCallbackInterfaceArg = V8TestCallbackInterface::CreateOrNull(info[0].As<v8::Object>());
    if (!testCallbackInterfaceArg) {
      exceptionState.ThrowSecurityError("The callback provided as parameter 1 is a cross origin object.");
      return;
    }
  } else if (info[0]->IsNullOrUndefined()) {
    testCallbackInterfaceArg = nullptr;
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestCallbackInterfaceOrNullArg", "TestObject", "The callback provided as parameter 1 is not an object."));
    return;
  }

  impl->voidMethodTestCallbackInterfaceOrNullArg(testCallbackInterfaceArg);
}

static void testEnumMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueString(info, impl->testEnumMethod(), info.GetIsolate());
}

static void voidMethodTestEnumArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodTestEnumArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> testEnumTypeArg;
  testEnumTypeArg = info[0];
  if (!testEnumTypeArg.Prepare())
    return;
  const char* validTestEnumTypeArgValues[] = {
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
  if (!IsValidEnum(testEnumTypeArg, validTestEnumTypeArgValues, base::size(validTestEnumTypeArgValues), "TestEnum", exceptionState)) {
    return;
  }

  impl->voidMethodTestEnumArg(testEnumTypeArg);
}

static void voidMethodTestMultipleEnumArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodTestMultipleEnumArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 2)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  V8StringResource<> testEnumTypeArg;
  V8StringResource<> testEnumTypeArg2;
  testEnumTypeArg = info[0];
  if (!testEnumTypeArg.Prepare())
    return;
  const char* validTestEnumTypeArgValues[] = {
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
  if (!IsValidEnum(testEnumTypeArg, validTestEnumTypeArgValues, base::size(validTestEnumTypeArgValues), "TestEnum", exceptionState)) {
    return;
  }

  testEnumTypeArg2 = info[1];
  if (!testEnumTypeArg2.Prepare())
    return;
  const char* validTestEnumTypeArg2Values[] = {
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
  if (!IsValidEnum(testEnumTypeArg2, validTestEnumTypeArg2Values, base::size(validTestEnumTypeArg2Values), "TestEnum2", exceptionState)) {
    return;
  }

  impl->voidMethodTestMultipleEnumArg(testEnumTypeArg, testEnumTypeArg2);
}

static void dictionaryMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->dictionaryMethod());
}

static void testDictionaryMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestDictionary result;
  impl->testDictionaryMethod(result);
  V8SetReturnValue(info, result);
}

static void nullableTestDictionaryMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  base::Optional<TestDictionary> result;
  impl->nullableTestDictionaryMethod(result);
  if (!result)
    V8SetReturnValueNull(info);
  else
    V8SetReturnValue(info, result.value());
}

static void staticTestDictionaryMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestDictionary result;
  TestObject::staticTestDictionaryMethod(result);
  V8SetReturnValue(info, result, info.GetIsolate()->GetCurrentContext()->Global());
}

static void staticNullableTestDictionaryMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  base::Optional<TestDictionary> result;
  TestObject::staticNullableTestDictionaryMethod(result);
  if (!result)
    V8SetReturnValueNull(info);
  else
    V8SetReturnValue(info, result.value(), info.GetIsolate()->GetCurrentContext()->Global());
}

static void passPermissiveDictionaryMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "passPermissiveDictionaryMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestDictionary arg;
  V8TestDictionary::ToImpl(info.GetIsolate(), info[0], arg, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->passPermissiveDictionaryMethod(arg);
}

static void nodeFilterMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, ToV8(impl->nodeFilterMethod(), info.Holder(), info.GetIsolate()));
}

static void promiseMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "promiseMethod");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  // V8DOMConfiguration::kDoNotCheckHolder
  // Make sure that info.Holder() really points to an instance of the type.
  if (!V8TestObject::hasInstance(info.Holder(), info.GetIsolate())) {
    exceptionState.ThrowTypeError("Illegal invocation");
    return;
  }
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 3)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(3, info.Length()));
    return;
  }

  int32_t arg1;
  Dictionary arg2;
  V8StringResource<> arg3;
  Vector<String> variadic;
  arg1 = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  if (!info[1]->IsNullOrUndefined() && !info[1]->IsObject()) {
    exceptionState.ThrowTypeError("parameter 2 ('arg2') is not an object.");
    return;
  }
  arg2 = NativeValueTraits<Dictionary>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  arg3 = info[2];
  if (!arg3.Prepare(exceptionState))
    return;

  variadic = ToImplArguments<IDLString>(info, 3, exceptionState);
  if (exceptionState.HadException())
    return;

  V8SetReturnValue(info, impl->promiseMethod(arg1, arg2, arg3, variadic).V8Value());
}

static void promiseMethodWithoutExceptionStateMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "promiseMethodWithoutExceptionState");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  // V8DOMConfiguration::kDoNotCheckHolder
  // Make sure that info.Holder() really points to an instance of the type.
  if (!V8TestObject::hasInstance(info.Holder(), info.GetIsolate())) {
    exceptionState.ThrowTypeError("Illegal invocation");
    return;
  }
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  Dictionary arg1;
  if (!info[0]->IsNullOrUndefined() && !info[0]->IsObject()) {
    exceptionState.ThrowTypeError("parameter 1 ('arg1') is not an object.");
    return;
  }
  arg1 = NativeValueTraits<Dictionary>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  V8SetReturnValue(info, impl->promiseMethodWithoutExceptionState(arg1).V8Value());
}

static void serializedScriptValueMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, V8Deserialize(info.GetIsolate(), impl->serializedScriptValueMethod().get()));
}

static void xPathNSResolverMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->xPathNSResolverMethod());
}

static void voidMethodDictionaryArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDictionaryArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  Dictionary dictionaryArg;
  if (!info[0]->IsNullOrUndefined() && !info[0]->IsObject()) {
    exceptionState.ThrowTypeError("parameter 1 ('dictionaryArg') is not an object.");
    return;
  }
  dictionaryArg = NativeValueTraits<Dictionary>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDictionaryArg(dictionaryArg);
}

static void voidMethodNodeFilterArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodNodeFilterArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodNodeFilterArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8NodeFilter* nodeFilterArg;
  if (info[0]->IsObject()) {
    nodeFilterArg = V8NodeFilter::CreateOrNull(info[0].As<v8::Object>());
    if (!nodeFilterArg) {
      exceptionState.ThrowSecurityError("The callback provided as parameter 1 is a cross origin object.");
      return;
    }
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodNodeFilterArg", "TestObject", "The callback provided as parameter 1 is not an object."));
    return;
  }

  impl->voidMethodNodeFilterArg(nodeFilterArg);
}

static void voidMethodPromiseArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodPromiseArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  ScriptPromise promiseArg;
  promiseArg = ScriptPromise::Cast(ScriptState::Current(info.GetIsolate()), info[0]);
  if (!promiseArg.IsUndefinedOrNull() && !promiseArg.IsObject()) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodPromiseArg", "TestObject", "parameter 1 ('promiseArg') is not an object."));
    return;
  }

  impl->voidMethodPromiseArg(promiseArg);
}

static void voidMethodSerializedScriptValueArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodSerializedScriptValueArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  scoped_refptr<SerializedScriptValue> serializedScriptValueArg;
  serializedScriptValueArg = NativeValueTraits<SerializedScriptValue>::NativeValue(info.GetIsolate(), info[0], SerializedScriptValue::SerializeOptions(SerializedScriptValue::kNotForStorage), exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodSerializedScriptValueArg(std::move(serializedScriptValueArg));
}

static void voidMethodXPathNSResolverArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodXPathNSResolverArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  XPathNSResolver* xPathNSResolverArg;
  xPathNSResolverArg = ToXPathNSResolver(ScriptState::Current(info.GetIsolate()), info[0]);
  if (!xPathNSResolverArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodXPathNSResolverArg", "TestObject", "parameter 1 is not of type 'XPathNSResolver'."));
    return;
  }

  impl->voidMethodXPathNSResolverArg(xPathNSResolverArg);
}

static void voidMethodDictionarySequenceArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDictionarySequenceArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  Vector<Dictionary> dictionarySequenceArg;
  dictionarySequenceArg = NativeValueTraits<IDLSequence<Dictionary>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDictionarySequenceArg(dictionarySequenceArg);
}

static void voidMethodStringArgLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodStringArgLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 2)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  V8StringResource<> stringArg;
  int32_t longArg;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodStringArgLongArg(stringArg, longArg);
}

static void voidMethodByteStringOrNullOptionalUSVStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodByteStringOrNullOptionalUSVStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<kTreatNullAndUndefinedAsNullString> byteStringArg;
  V8StringResource<> usvStringArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  byteStringArg = NativeValueTraits<IDLByteStringOrNull>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  if (UNLIKELY(numArgsPassed <= 1)) {
    impl->voidMethodByteStringOrNullOptionalUSVStringArg(byteStringArg);
    return;
  }
  usvStringArg = NativeValueTraits<IDLUSVString>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodByteStringOrNullOptionalUSVStringArg(byteStringArg, usvStringArg);
}

static void voidMethodOptionalStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> optionalStringArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    impl->voidMethodOptionalStringArg();
    return;
  }
  optionalStringArg = info[0];
  if (!optionalStringArg.Prepare())
    return;

  impl->voidMethodOptionalStringArg(optionalStringArg);
}

static void voidMethodOptionalTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceEmpty* optionalTestInterfaceEmptyArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    impl->voidMethodOptionalTestInterfaceEmptyArg();
    return;
  }
  optionalTestInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!optionalTestInterfaceEmptyArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodOptionalTestInterfaceEmptyArg", "TestObject", "parameter 1 is not of type 'TestInterfaceEmpty'."));
    return;
  }

  impl->voidMethodOptionalTestInterfaceEmptyArg(optionalTestInterfaceEmptyArg);
}

static void voidMethodOptionalLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodOptionalLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t optionalLongArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    impl->voidMethodOptionalLongArg();
    return;
  }
  optionalLongArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodOptionalLongArg(optionalLongArg);
}

static void stringMethodOptionalLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "stringMethodOptionalLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t optionalLongArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    V8SetReturnValueString(info, impl->stringMethodOptionalLongArg(), info.GetIsolate());
    return;
  }
  optionalLongArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  V8SetReturnValueString(info, impl->stringMethodOptionalLongArg(optionalLongArg), info.GetIsolate());
}

static void testInterfaceEmptyMethodOptionalLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "testInterfaceEmptyMethodOptionalLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t optionalLongArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    V8SetReturnValue(info, impl->testInterfaceEmptyMethodOptionalLongArg());
    return;
  }
  optionalLongArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  V8SetReturnValue(info, impl->testInterfaceEmptyMethodOptionalLongArg(optionalLongArg));
}

static void longMethodOptionalLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "longMethodOptionalLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t optionalLongArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    V8SetReturnValueInt(info, impl->longMethodOptionalLongArg());
    return;
  }
  optionalLongArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  V8SetReturnValueInt(info, impl->longMethodOptionalLongArg(optionalLongArg));
}

static void voidMethodLongArgOptionalLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodLongArgOptionalLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int32_t longArg;
  int32_t optionalLongArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  if (UNLIKELY(numArgsPassed <= 1)) {
    impl->voidMethodLongArgOptionalLongArg(longArg);
    return;
  }
  optionalLongArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodLongArgOptionalLongArg(longArg, optionalLongArg);
}

static void voidMethodLongArgOptionalLongArgOptionalLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodLongArgOptionalLongArgOptionalLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int32_t longArg;
  int32_t optionalLongArg1;
  int32_t optionalLongArg2;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  if (UNLIKELY(numArgsPassed <= 1)) {
    impl->voidMethodLongArgOptionalLongArgOptionalLongArg(longArg);
    return;
  }
  optionalLongArg1 = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  if (UNLIKELY(numArgsPassed <= 2)) {
    impl->voidMethodLongArgOptionalLongArgOptionalLongArg(longArg, optionalLongArg1);
    return;
  }
  optionalLongArg2 = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[2], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodLongArgOptionalLongArgOptionalLongArg(longArg, optionalLongArg1, optionalLongArg2);
}

static void voidMethodLongArgOptionalTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodLongArgOptionalTestInterfaceEmptyArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int32_t longArg;
  TestInterfaceEmpty* optionalTestInterfaceEmpty;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  if (UNLIKELY(numArgsPassed <= 1)) {
    impl->voidMethodLongArgOptionalTestInterfaceEmptyArg(longArg);
    return;
  }
  optionalTestInterfaceEmpty = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[1]);
  if (!optionalTestInterfaceEmpty) {
    exceptionState.ThrowTypeError("parameter 2 is not of type 'TestInterfaceEmpty'.");
    return;
  }

  impl->voidMethodLongArgOptionalTestInterfaceEmptyArg(longArg, optionalTestInterfaceEmpty);
}

static void voidMethodTestInterfaceEmptyArgOptionalLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodTestInterfaceEmptyArgOptionalLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  TestInterfaceEmpty* optionalTestInterfaceEmpty;
  int32_t longArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  optionalTestInterfaceEmpty = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!optionalTestInterfaceEmpty) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'TestInterfaceEmpty'.");
    return;
  }

  if (UNLIKELY(numArgsPassed <= 1)) {
    impl->voidMethodTestInterfaceEmptyArgOptionalLongArg(optionalTestInterfaceEmpty);
    return;
  }
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodTestInterfaceEmptyArgOptionalLongArg(optionalTestInterfaceEmpty, longArg);
}

static void voidMethodOptionalDictionaryArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodOptionalDictionaryArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  Dictionary optionalDictionaryArg;
  if (!info[0]->IsNullOrUndefined() && !info[0]->IsObject()) {
    exceptionState.ThrowTypeError("parameter 1 ('optionalDictionaryArg') is not an object.");
    return;
  }
  optionalDictionaryArg = NativeValueTraits<Dictionary>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodOptionalDictionaryArg(optionalDictionaryArg);
}

static void voidMethodDefaultByteStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDefaultByteStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> defaultByteStringArg;
  if (!info[0]->IsUndefined()) {
    defaultByteStringArg = NativeValueTraits<IDLByteString>::NativeValue(info.GetIsolate(), info[0], exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    defaultByteStringArg = "foo";
  }

  impl->voidMethodDefaultByteStringArg(defaultByteStringArg);
}

static void voidMethodDefaultStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> defaultStringArg;
  if (!info[0]->IsUndefined()) {
    defaultStringArg = info[0];
    if (!defaultStringArg.Prepare())
      return;
  } else {
    defaultStringArg = "foo";
  }

  impl->voidMethodDefaultStringArg(defaultStringArg);
}

static void voidMethodDefaultIntegerArgsMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDefaultIntegerArgs");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t defaultLongArg;
  int64_t defaultLongLongArg;
  uint32_t defaultUnsignedArg;
  if (!info[0]->IsUndefined()) {
    defaultLongArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    defaultLongArg = 10;
  }
  if (!info[1]->IsUndefined()) {
    defaultLongLongArg = NativeValueTraits<IDLLongLong>::NativeValue(info.GetIsolate(), info[1], exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    defaultLongLongArg = -10;
  }
  if (!info[2]->IsUndefined()) {
    defaultUnsignedArg = NativeValueTraits<IDLUnsignedLong>::NativeValue(info.GetIsolate(), info[2], exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    defaultUnsignedArg = 4294967295u;
  }

  impl->voidMethodDefaultIntegerArgs(defaultLongArg, defaultLongLongArg, defaultUnsignedArg);
}

static void voidMethodDefaultDoubleArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDefaultDoubleArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  double defaultDoubleArg;
  if (!info[0]->IsUndefined()) {
    defaultDoubleArg = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[0], exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    defaultDoubleArg = 0.5;
  }

  impl->voidMethodDefaultDoubleArg(defaultDoubleArg);
}

static void voidMethodDefaultTrueBooleanArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDefaultTrueBooleanArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  bool defaultBooleanArg;
  if (!info[0]->IsUndefined()) {
    defaultBooleanArg = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), info[0], exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    defaultBooleanArg = true;
  }

  impl->voidMethodDefaultTrueBooleanArg(defaultBooleanArg);
}

static void voidMethodDefaultFalseBooleanArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDefaultFalseBooleanArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  bool defaultBooleanArg;
  if (!info[0]->IsUndefined()) {
    defaultBooleanArg = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), info[0], exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    defaultBooleanArg = false;
  }

  impl->voidMethodDefaultFalseBooleanArg(defaultBooleanArg);
}

static void voidMethodDefaultNullableByteStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDefaultNullableByteStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<kTreatNullAndUndefinedAsNullString> defaultStringArg;
  if (!info[0]->IsUndefined()) {
    defaultStringArg = NativeValueTraits<IDLByteStringOrNull>::NativeValue(info.GetIsolate(), info[0], exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    defaultStringArg = nullptr;
  }

  impl->voidMethodDefaultNullableByteStringArg(defaultStringArg);
}

static void voidMethodDefaultNullableStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<kTreatNullAndUndefinedAsNullString> defaultStringArg;
  if (!info[0]->IsUndefined()) {
    defaultStringArg = info[0];
    if (!defaultStringArg.Prepare())
      return;
  } else {
    defaultStringArg = nullptr;
  }

  impl->voidMethodDefaultNullableStringArg(defaultStringArg);
}

static void voidMethodDefaultNullableTestInterfaceArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceImplementation* defaultTestInterfaceArg;
  if (!info[0]->IsUndefined()) {
    defaultTestInterfaceArg = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
    if (!defaultTestInterfaceArg && !IsUndefinedOrNull(info[0])) {
      V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodDefaultNullableTestInterfaceArg", "TestObject", "parameter 1 is not of type 'TestInterface'."));
      return;
    }
  } else {
    defaultTestInterfaceArg = nullptr;
  }

  impl->voidMethodDefaultNullableTestInterfaceArg(defaultTestInterfaceArg);
}

static void voidMethodDefaultDoubleOrStringArgsMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDefaultDoubleOrStringArgs");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  DoubleOrString defaultLongArg;
  DoubleOrString defaultStringArg;
  DoubleOrString defaultNullArg;
  if (!info[0]->IsUndefined()) {
    V8DoubleOrString::ToImpl(info.GetIsolate(), info[0], defaultLongArg, UnionTypeConversionMode::kNotNullable, exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    defaultLongArg.SetDouble(10);
  }
  if (!info[1]->IsUndefined()) {
    V8DoubleOrString::ToImpl(info.GetIsolate(), info[1], defaultStringArg, UnionTypeConversionMode::kNullable, exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    defaultStringArg.SetString("foo");
  }
  if (!info[2]->IsUndefined()) {
    V8DoubleOrString::ToImpl(info.GetIsolate(), info[2], defaultNullArg, UnionTypeConversionMode::kNullable, exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    /* null default value */;
  }

  impl->voidMethodDefaultDoubleOrStringArgs(defaultLongArg, defaultStringArg, defaultNullArg);
}

static void voidMethodDefaultStringSequenceArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDefaultStringSequenceArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  Vector<String> defaultStringSequenceArg;
  if (!info[0]->IsUndefined()) {
    defaultStringSequenceArg = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    /* Nothing to do */;
  }

  impl->voidMethodDefaultStringSequenceArg(defaultStringSequenceArg);
}

static void voidMethodVariadicStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodVariadicStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  Vector<String> variadicStringArgs;
  variadicStringArgs = ToImplArguments<IDLString>(info, 0, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodVariadicStringArg(variadicStringArgs);
}

static void voidMethodStringArgVariadicStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodStringArgVariadicStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> stringArg;
  Vector<String> variadicStringArgs;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  variadicStringArgs = ToImplArguments<IDLString>(info, 1, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodStringArgVariadicStringArg(stringArg, variadicStringArgs);
}

static void voidMethodVariadicTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodVariadicTestInterfaceEmptyArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  HeapVector<Member<TestInterfaceEmpty>> variadicTestInterfaceEmptyArgs;
  for (int i = 0; i < info.Length(); ++i) {
    if (!V8TestInterfaceEmpty::hasInstance(info[i], info.GetIsolate())) {
      exceptionState.ThrowTypeError("parameter 1 is not of type 'TestInterfaceEmpty'.");
      return;
    }
    variadicTestInterfaceEmptyArgs.push_back(V8TestInterfaceEmpty::ToImpl(v8::Local<v8::Object>::Cast(info[i])));
  }

  impl->voidMethodVariadicTestInterfaceEmptyArg(variadicTestInterfaceEmptyArgs);
}

static void voidMethodTestInterfaceEmptyArgVariadicTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodTestInterfaceEmptyArgVariadicTestInterfaceEmptyArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  TestInterfaceEmpty* testInterfaceEmptyArg;
  HeapVector<Member<TestInterfaceEmpty>> variadicTestInterfaceEmptyArgs;
  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceEmptyArg) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'TestInterfaceEmpty'.");
    return;
  }

  for (int i = 1; i < info.Length(); ++i) {
    if (!V8TestInterfaceEmpty::hasInstance(info[i], info.GetIsolate())) {
      exceptionState.ThrowTypeError("parameter 2 is not of type 'TestInterfaceEmpty'.");
      return;
    }
    variadicTestInterfaceEmptyArgs.push_back(V8TestInterfaceEmpty::ToImpl(v8::Local<v8::Object>::Cast(info[i])));
  }

  impl->voidMethodTestInterfaceEmptyArgVariadicTestInterfaceEmptyArg(testInterfaceEmptyArg, variadicTestInterfaceEmptyArgs);
}

static void overloadedMethodA1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodA");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodA(longArg);
}

static void overloadedMethodA2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodA");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg1;
  int32_t longArg2;
  longArg1 = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  longArg2 = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodA(longArg1, longArg2);
}

static void overloadedMethodAMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(2, info.Length())) {
    case 1:
      if (true) {
        overloadedMethodA1Method(info);
        return;
      }
      break;
    case 2:
      if (true) {
        overloadedMethodA2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodA");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void overloadedMethodB1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodB");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodB(longArg);
}

static void overloadedMethodB2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodB");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> stringArg;
  int32_t longArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  if (UNLIKELY(numArgsPassed <= 1)) {
    impl->overloadedMethodB(stringArg);
    return;
  }
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodB(stringArg, longArg);
}

static void overloadedMethodBMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(2, info.Length())) {
    case 1:
      if (info[0]->IsNumber()) {
        overloadedMethodB1Method(info);
        return;
      }
      if (true) {
        overloadedMethodB2Method(info);
        return;
      }
      if (true) {
        overloadedMethodB1Method(info);
        return;
      }
      break;
    case 2:
      if (true) {
        overloadedMethodB2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodB");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void overloadedMethodC1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodC");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodC(longArg);
}

static void overloadedMethodC2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceEmpty* testInterfaceEmptyArg;
  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceEmptyArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("overloadedMethodC", "TestObject", "parameter 1 is not of type 'TestInterfaceEmpty'."));
    return;
  }

  impl->overloadedMethodC(testInterfaceEmptyArg);
}

static void overloadedMethodCMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (V8TestInterfaceEmpty::hasInstance(info[0], info.GetIsolate())) {
        overloadedMethodC2Method(info);
        return;
      }
      if (info[0]->IsNumber()) {
        overloadedMethodC1Method(info);
        return;
      }
      if (true) {
        overloadedMethodC1Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodC");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void overloadedMethodD1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodD");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodD(longArg);
}

static void overloadedMethodD2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodD");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  Vector<int32_t> longArraySequence;
  longArraySequence = NativeValueTraits<IDLSequence<IDLLong>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodD(longArraySequence);
}

static void overloadedMethodDMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (info[0]->IsArray()) {
        overloadedMethodD2Method(info);
        return;
      }
      {
        ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext,
                                      "TestObject", "overloadedMethodD");
        if (HasCallableIteratorSymbol(info.GetIsolate(), info[0], exceptionState)) {
          overloadedMethodD2Method(info);
          return;
        }
        if (exceptionState.HadException()) {
          exceptionState.RethrowV8Exception(exceptionState.GetException());
          return;
        }
      }
      if (info[0]->IsNumber()) {
        overloadedMethodD1Method(info);
        return;
      }
      if (true) {
        overloadedMethodD1Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodD");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void overloadedMethodE1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodE");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodE(longArg);
}

static void overloadedMethodE2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceEmpty* testInterfaceEmptyOrNullArg;
  testInterfaceEmptyOrNullArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceEmptyOrNullArg && !IsUndefinedOrNull(info[0])) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("overloadedMethodE", "TestObject", "parameter 1 is not of type 'TestInterfaceEmpty'."));
    return;
  }

  impl->overloadedMethodE(testInterfaceEmptyOrNullArg);
}

static void overloadedMethodEMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (IsUndefinedOrNull(info[0])) {
        overloadedMethodE2Method(info);
        return;
      }
      if (V8TestInterfaceEmpty::hasInstance(info[0], info.GetIsolate())) {
        overloadedMethodE2Method(info);
        return;
      }
      if (info[0]->IsNumber()) {
        overloadedMethodE1Method(info);
        return;
      }
      if (true) {
        overloadedMethodE1Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodE");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void overloadedMethodF1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> stringArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    impl->overloadedMethodF();
    return;
  }
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  impl->overloadedMethodF(stringArg);
}

static void overloadedMethodF2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodF");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  double doubleArg;
  doubleArg = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodF(doubleArg);
}

static void overloadedMethodFMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        overloadedMethodF1Method(info);
        return;
      }
      break;
    case 1:
      if (info[0]->IsUndefined()) {
        overloadedMethodF1Method(info);
        return;
      }
      if (info[0]->IsNumber()) {
        overloadedMethodF2Method(info);
        return;
      }
      if (true) {
        overloadedMethodF1Method(info);
        return;
      }
      if (true) {
        overloadedMethodF2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodF");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void overloadedMethodG1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodG");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodG(longArg);
}

static void overloadedMethodG2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceEmpty* testInterfaceEmptyOrNullArg;
  if (!info[0]->IsUndefined()) {
    testInterfaceEmptyOrNullArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
    if (!testInterfaceEmptyOrNullArg && !IsUndefinedOrNull(info[0])) {
      V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("overloadedMethodG", "TestObject", "parameter 1 is not of type 'TestInterfaceEmpty'."));
      return;
    }
  } else {
    testInterfaceEmptyOrNullArg = nullptr;
  }

  impl->overloadedMethodG(testInterfaceEmptyOrNullArg);
}

static void overloadedMethodGMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        overloadedMethodG2Method(info);
        return;
      }
      break;
    case 1:
      if (info[0]->IsUndefined()) {
        overloadedMethodG2Method(info);
        return;
      }
      if (IsUndefinedOrNull(info[0])) {
        overloadedMethodG2Method(info);
        return;
      }
      if (V8TestInterfaceEmpty::hasInstance(info[0], info.GetIsolate())) {
        overloadedMethodG2Method(info);
        return;
      }
      if (info[0]->IsNumber()) {
        overloadedMethodG1Method(info);
        return;
      }
      if (true) {
        overloadedMethodG1Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodG");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void overloadedMethodH1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceImplementation* testInterfaceArg;
  testInterfaceArg = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("overloadedMethodH", "TestObject", "parameter 1 is not of type 'TestInterface'."));
    return;
  }

  impl->overloadedMethodH(testInterfaceArg);
}

static void overloadedMethodH2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceEmpty* testInterfaceEmptyArg;
  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceEmptyArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("overloadedMethodH", "TestObject", "parameter 1 is not of type 'TestInterfaceEmpty'."));
    return;
  }

  impl->overloadedMethodH(testInterfaceEmptyArg);
}

static void overloadedMethodHMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (V8TestInterface::hasInstance(info[0], info.GetIsolate())) {
        overloadedMethodH1Method(info);
        return;
      }
      if (V8TestInterfaceEmpty::hasInstance(info[0], info.GetIsolate())) {
        overloadedMethodH2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodH");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void overloadedMethodI1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> stringArg;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  impl->overloadedMethodI(stringArg);
}

static void overloadedMethodI2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodI");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  double doubleArg;
  doubleArg = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodI(doubleArg);
}

static void overloadedMethodIMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (info[0]->IsNumber()) {
        overloadedMethodI2Method(info);
        return;
      }
      if (true) {
        overloadedMethodI1Method(info);
        return;
      }
      if (true) {
        overloadedMethodI2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodI");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void overloadedMethodJ1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> stringArg;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  impl->overloadedMethodJ(stringArg);
}

static void overloadedMethodJ2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodJ");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestDictionary testDictionaryArg;
  if (!info[0]->IsNullOrUndefined() && !info[0]->IsObject()) {
    exceptionState.ThrowTypeError("parameter 1 ('testDictionaryArg') is not an object.");
    return;
  }
  V8TestDictionary::ToImpl(info.GetIsolate(), info[0], testDictionaryArg, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodJ(testDictionaryArg);
}

static void overloadedMethodJMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (IsUndefinedOrNull(info[0])) {
        overloadedMethodJ2Method(info);
        return;
      }
      if (info[0]->IsObject()) {
        overloadedMethodJ2Method(info);
        return;
      }
      if (true) {
        overloadedMethodJ1Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodJ");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void overloadedMethodK1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptValue functionArg;
  if (info[0]->IsFunction()) {
    functionArg = ScriptValue(ScriptState::Current(info.GetIsolate()), info[0]);
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("overloadedMethodK", "TestObject", "The callback provided as parameter 1 is not a function."));
    return;
  }

  impl->overloadedMethodK(functionArg);
}

static void overloadedMethodK2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> stringArg;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  impl->overloadedMethodK(stringArg);
}

static void overloadedMethodKMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (info[0]->IsFunction()) {
        overloadedMethodK1Method(info);
        return;
      }
      if (true) {
        overloadedMethodK2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodK");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void overloadedMethodL1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodL");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  Vector<ScriptValue> restArgs;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  restArgs = ToImplArguments<ScriptValue>(info, 1, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodL(longArg, restArgs);
}

static void overloadedMethodL2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodL");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> stringArg;
  Vector<ScriptValue> restArgs;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  restArgs = ToImplArguments<ScriptValue>(info, 1, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodL(stringArg, restArgs);
}

static void overloadedMethodLMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(2, info.Length())) {
    case 1:
      if (info[0]->IsNumber()) {
        overloadedMethodL1Method(info);
        return;
      }
      if (true) {
        overloadedMethodL2Method(info);
        return;
      }
      if (true) {
        overloadedMethodL1Method(info);
        return;
      }
      break;
    case 2:
      if (info[0]->IsNumber()) {
        overloadedMethodL1Method(info);
        return;
      }
      if (true) {
        overloadedMethodL2Method(info);
        return;
      }
      if (true) {
        overloadedMethodL1Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodL");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void overloadedMethodN1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceImplementation* testInterfaceArg;
  testInterfaceArg = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("overloadedMethodN", "TestObject", "parameter 1 is not of type 'TestInterface'."));
    return;
  }

  impl->overloadedMethodN(testInterfaceArg);
}

static void overloadedMethodN2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodN");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8TestCallbackInterface* testCallbackInterfaceArg;
  if (info[0]->IsObject()) {
    testCallbackInterfaceArg = V8TestCallbackInterface::CreateOrNull(info[0].As<v8::Object>());
    if (!testCallbackInterfaceArg) {
      exceptionState.ThrowSecurityError("The callback provided as parameter 1 is a cross origin object.");
      return;
    }
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("overloadedMethodN", "TestObject", "The callback provided as parameter 1 is not an object."));
    return;
  }

  impl->overloadedMethodN(testCallbackInterfaceArg);
}

static void overloadedMethodNMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (V8TestInterface::hasInstance(info[0], info.GetIsolate())) {
        overloadedMethodN1Method(info);
        return;
      }
      if (info[0]->IsObject()) {
        overloadedMethodN2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodN");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void promiseOverloadMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "promiseOverloadMethod");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  // V8DOMConfiguration::kDoNotCheckHolder
  // Make sure that info.Holder() really points to an instance of the type.
  if (!V8TestObject::hasInstance(info.Holder(), info.GetIsolate())) {
    exceptionState.ThrowTypeError("Illegal invocation");
    return;
  }
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->promiseOverloadMethod().V8Value());
}

static void promiseOverloadMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "promiseOverloadMethod");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  // V8DOMConfiguration::kDoNotCheckHolder
  // Make sure that info.Holder() really points to an instance of the type.
  if (!V8TestObject::hasInstance(info.Holder(), info.GetIsolate())) {
    exceptionState.ThrowTypeError("Illegal invocation");
    return;
  }
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  DOMWindow* arg1;
  double arg2;
  arg1 = ToDOMWindow(info.GetIsolate(), info[0]);
  if (!arg1) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'Window'.");
    return;
  }

  arg2 = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  V8SetReturnValue(info, impl->promiseOverloadMethod(arg1, arg2).V8Value());
}

static void promiseOverloadMethod3Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "promiseOverloadMethod");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  // V8DOMConfiguration::kDoNotCheckHolder
  // Make sure that info.Holder() really points to an instance of the type.
  if (!V8TestObject::hasInstance(info.Holder(), info.GetIsolate())) {
    exceptionState.ThrowTypeError("Illegal invocation");
    return;
  }
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  Document* arg1;
  double arg2;
  arg1 = V8Document::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!arg1) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'Document'.");
    return;
  }

  arg2 = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  V8SetReturnValue(info, impl->promiseOverloadMethod(arg1, arg2).V8Value());
}

static void promiseOverloadMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(2, info.Length())) {
    case 0:
      if (true) {
        promiseOverloadMethod1Method(info);
        return;
      }
      break;
    case 2:
      if (V8Window::hasInstance(info[0], info.GetIsolate())) {
        promiseOverloadMethod2Method(info);
        return;
      }
      if (V8Document::hasInstance(info[0], info.GetIsolate())) {
        promiseOverloadMethod3Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "promiseOverloadMethod");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);
  if (isArityError) {
    if (info.Length() >= 0) {
      exceptionState.ThrowTypeError(ExceptionMessages::InvalidArity("[0, 2]", info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void overloadedPerWorldBindingsMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->overloadedPerWorldBindingsMethod();
}

static void overloadedPerWorldBindingsMethod1MethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->overloadedPerWorldBindingsMethod();
}

static void overloadedPerWorldBindingsMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedPerWorldBindingsMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedPerWorldBindingsMethod(longArg);
}

static void overloadedPerWorldBindingsMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        overloadedPerWorldBindingsMethod1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        overloadedPerWorldBindingsMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedPerWorldBindingsMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void overloadedPerWorldBindingsMethod2MethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedPerWorldBindingsMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedPerWorldBindingsMethod(longArg);
}

static void overloadedPerWorldBindingsMethodMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        overloadedPerWorldBindingsMethod1MethodForMainWorld(info);
        return;
      }
      break;
    case 1:
      if (true) {
        overloadedPerWorldBindingsMethod2MethodForMainWorld(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedPerWorldBindingsMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void overloadedStaticMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedStaticMethod");

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  TestObject::overloadedStaticMethod(longArg);
}

static void overloadedStaticMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedStaticMethod");

  int32_t longArg1;
  int32_t longArg2;
  longArg1 = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  longArg2 = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  TestObject::overloadedStaticMethod(longArg1, longArg2);
}

static void overloadedStaticMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(2, info.Length())) {
    case 1:
      if (true) {
        overloadedStaticMethod1Method(info);
        return;
      }
      break;
    case 2:
      if (true) {
        overloadedStaticMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedStaticMethod");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void itemMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "item");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  uint32_t index;
  index = NativeValueTraits<IDLUnsignedLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  ScriptValue result = impl->item(scriptState, index);
  V8SetReturnValue(info, result.V8Value());
}

static void setItemMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "setItem");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  if (UNLIKELY(info.Length() < 2)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  uint32_t index;
  V8StringResource<> value;
  index = NativeValueTraits<IDLUnsignedLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  value = info[1];
  if (!value.Prepare())
    return;

  String result = impl->setItem(scriptState, index, value);
  V8SetReturnValueString(info, result, info.GetIsolate());
}

static void voidMethodClampUnsignedShortArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodClampUnsignedShortArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  uint16_t clampUnsignedShortArg;
  clampUnsignedShortArg = NativeValueTraits<IDLUnsignedShortClamp>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodClampUnsignedShortArg(clampUnsignedShortArg);
}

static void voidMethodClampUnsignedLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodClampUnsignedLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  uint32_t clampUnsignedLongArg;
  clampUnsignedLongArg = NativeValueTraits<IDLUnsignedLongClamp>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodClampUnsignedLongArg(clampUnsignedLongArg);
}

static void voidMethodDefaultUndefinedTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceEmpty* defaultUndefinedTestInterfaceEmptyArg;
  defaultUndefinedTestInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!defaultUndefinedTestInterfaceEmptyArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodDefaultUndefinedTestInterfaceEmptyArg", "TestObject", "parameter 1 is not of type 'TestInterfaceEmpty'."));
    return;
  }

  impl->voidMethodDefaultUndefinedTestInterfaceEmptyArg(defaultUndefinedTestInterfaceEmptyArg);
}

static void voidMethodDefaultUndefinedLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDefaultUndefinedLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t defaultUndefinedLongArg;
  defaultUndefinedLongArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDefaultUndefinedLongArg(defaultUndefinedLongArg);
}

static void voidMethodDefaultUndefinedStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> defaultUndefinedStringArg;
  defaultUndefinedStringArg = info[0];
  if (!defaultUndefinedStringArg.Prepare())
    return;

  impl->voidMethodDefaultUndefinedStringArg(defaultUndefinedStringArg);
}

static void voidMethodEnforceRangeLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodEnforceRangeLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int32_t enforceRangeLongArg;
  enforceRangeLongArg = NativeValueTraits<IDLLongEnforceRange>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodEnforceRangeLongArg(enforceRangeLongArg);
}

static void voidMethodTreatNullAsEmptyStringStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTreatNullAsEmptyStringStringArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8StringResource<kTreatNullAsEmptyString> treatNullAsEmptyStringStringArg;
  treatNullAsEmptyStringStringArg = info[0];
  if (!treatNullAsEmptyStringStringArg.Prepare())
    return;

  impl->voidMethodTreatNullAsEmptyStringStringArg(treatNullAsEmptyStringStringArg);
}

static void activityLoggingAccessForAllWorldsMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->activityLoggingAccessForAllWorldsMethod();
}

static void callWithExecutionContextVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ExecutionContext* executionContext = ExecutionContext::ForRelevantRealm(info);
  impl->callWithExecutionContextVoidMethod(executionContext);
}

static void callWithScriptStateVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  impl->callWithScriptStateVoidMethod(scriptState);
}

static void callWithScriptStateLongMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  int32_t result = impl->callWithScriptStateLongMethod(scriptState);
  V8SetReturnValueInt(info, result);
}

static void callWithScriptStateExecutionContextVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  ExecutionContext* executionContext = ExecutionContext::ForRelevantRealm(info);
  impl->callWithScriptStateExecutionContextVoidMethod(scriptState, executionContext);
}

static void callWithScriptStateScriptArgumentsVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  ScriptArguments* scriptArguments(ScriptArguments::Create(scriptState, info, 0));
  impl->callWithScriptStateScriptArgumentsVoidMethod(scriptState, scriptArguments);
}

static void callWithScriptStateScriptArgumentsVoidMethodOptionalBooleanArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "callWithScriptStateScriptArgumentsVoidMethodOptionalBooleanArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  bool optionalBooleanArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    ScriptArguments* scriptArguments(ScriptArguments::Create(scriptState, info, 1));
    impl->callWithScriptStateScriptArgumentsVoidMethodOptionalBooleanArg(scriptState, scriptArguments);
    return;
  }
  optionalBooleanArg = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  ScriptArguments* scriptArguments(ScriptArguments::Create(scriptState, info, 1));
  impl->callWithScriptStateScriptArgumentsVoidMethodOptionalBooleanArg(scriptState, scriptArguments, optionalBooleanArg);
}

static void callWithCurrentWindowMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->callWithCurrentWindow(CurrentDOMWindow(info.GetIsolate()));
}

static void callWithCurrentWindowScriptWindowMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->callWithCurrentWindowScriptWindow(CurrentDOMWindow(info.GetIsolate()), EnteredDOMWindow(info.GetIsolate()));
}

static void callWithThisValueMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  impl->callWithThisValue(ScriptValue(scriptState, info.Holder()));
}

static void checkSecurityForNodeVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "checkSecurityForNodeVoidMethod");
  if (!BindingSecurity::ShouldAllowAccessTo(CurrentDOMWindow(info.GetIsolate()), impl->checkSecurityForNodeVoidMethod(), BindingSecurity::ErrorReportOption::kDoNotReport)) {
    UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                      WebFeature::kCrossOriginTestObjectCheckSecurityForNodeVoidMethod);
    V8SetReturnValueNull(info);
    return;
  }

  impl->checkSecurityForNodeVoidMethod();
}

static void customCallPrologueVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8TestObject::customCallPrologueVoidMethodMethodPrologueCustom(info, impl);
  impl->customCallPrologueVoidMethod();
}

static void customCallEpilogueVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->customCallEpilogueVoidMethod();
  V8TestObject::customCallEpilogueVoidMethodMethodEpilogueCustom(info, impl);
}

static void deprecatedVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->deprecatedVoidMethod();
}

static void implementedAsVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->implementedAsMethodName();
}

static void measureAsVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->measureAsVoidMethod();
}

static void measureMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->measureMethod();
}

static void measureOverloadedMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->measureOverloadedMethod();
}

static void measureOverloadedMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "measureOverloadedMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t arg;
  arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->measureOverloadedMethod(arg);
}

static void measureOverloadedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kV8TestObject_MeasureOverloadedMethod_Method);
        measureOverloadedMethod1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kV8TestObject_MeasureOverloadedMethod_Method);
        measureOverloadedMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "measureOverloadedMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void DeprecateAsOverloadedMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->DeprecateAsOverloadedMethod();
}

static void DeprecateAsOverloadedMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "DeprecateAsOverloadedMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t arg;
  arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->DeprecateAsOverloadedMethod(arg);
}

static void DeprecateAsOverloadedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureA);
        DeprecateAsOverloadedMethod1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureB);
        DeprecateAsOverloadedMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "DeprecateAsOverloadedMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void DeprecateAsSameValueOverloadedMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->DeprecateAsSameValueOverloadedMethod();
}

static void DeprecateAsSameValueOverloadedMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "DeprecateAsSameValueOverloadedMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t arg;
  arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->DeprecateAsSameValueOverloadedMethod(arg);
}

static void DeprecateAsSameValueOverloadedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeature);

  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        DeprecateAsSameValueOverloadedMethod1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        DeprecateAsSameValueOverloadedMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "DeprecateAsSameValueOverloadedMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void measureAsOverloadedMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->measureAsOverloadedMethod();
}

static void measureAsOverloadedMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "measureAsOverloadedMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t arg;
  arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->measureAsOverloadedMethod(arg);
}

static void measureAsOverloadedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureA);
        measureAsOverloadedMethod1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureB);
        measureAsOverloadedMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "measureAsOverloadedMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void measureAsSameValueOverloadedMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->measureAsSameValueOverloadedMethod();
}

static void measureAsSameValueOverloadedMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "measureAsSameValueOverloadedMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t arg;
  arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->measureAsSameValueOverloadedMethod(arg);
}

static void measureAsSameValueOverloadedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeature);
        measureAsSameValueOverloadedMethod1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeature);
        measureAsSameValueOverloadedMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "measureAsSameValueOverloadedMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void ceReactionsNotOverloadedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "ceReactionsNotOverloadedMethod");
  CEReactionsScope ce_reactions_scope;

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  bool arg;
  arg = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->ceReactionsNotOverloadedMethod(arg);
}

static void ceReactionsOverloadedMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->ceReactionsOverloadedMethod();
}

static void ceReactionsOverloadedMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "ceReactionsOverloadedMethod");
  CEReactionsScope ce_reactions_scope;

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t arg;
  arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->ceReactionsOverloadedMethod(arg);
}

static void ceReactionsOverloadedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        ceReactionsOverloadedMethod1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        ceReactionsOverloadedMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "ceReactionsOverloadedMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void deprecateAsMeasureAsSameValueOverloadedMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->deprecateAsMeasureAsSameValueOverloadedMethod();
}

static void deprecateAsMeasureAsSameValueOverloadedMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "deprecateAsMeasureAsSameValueOverloadedMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t arg;
  arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->deprecateAsMeasureAsSameValueOverloadedMethod(arg);
}

static void deprecateAsMeasureAsSameValueOverloadedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeature);
        Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureA);
        deprecateAsMeasureAsSameValueOverloadedMethod1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeature);
        Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureB);
        deprecateAsMeasureAsSameValueOverloadedMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "deprecateAsMeasureAsSameValueOverloadedMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void deprecateAsSameValueMeasureAsOverloadedMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->deprecateAsSameValueMeasureAsOverloadedMethod();
}

static void deprecateAsSameValueMeasureAsOverloadedMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "deprecateAsSameValueMeasureAsOverloadedMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t arg;
  arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->deprecateAsSameValueMeasureAsOverloadedMethod(arg);
}

static void deprecateAsSameValueMeasureAsOverloadedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeature);

  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureA);
        deprecateAsSameValueMeasureAsOverloadedMethod1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureB);
        deprecateAsSameValueMeasureAsOverloadedMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "deprecateAsSameValueMeasureAsOverloadedMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void deprecateAsSameValueMeasureAsSameValueOverloadedMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->deprecateAsSameValueMeasureAsSameValueOverloadedMethod();
}

static void deprecateAsSameValueMeasureAsSameValueOverloadedMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "deprecateAsSameValueMeasureAsSameValueOverloadedMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t arg;
  arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->deprecateAsSameValueMeasureAsSameValueOverloadedMethod(arg);
}

static void deprecateAsSameValueMeasureAsSameValueOverloadedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureA);

  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureB);
        deprecateAsSameValueMeasureAsSameValueOverloadedMethod1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureB);
        deprecateAsSameValueMeasureAsSameValueOverloadedMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "deprecateAsSameValueMeasureAsSameValueOverloadedMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void notEnumerableVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->notEnumerableVoidMethod();
}

static void originTrialEnabledVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->originTrialEnabledVoidMethod();
}

static void perWorldBindingsOriginTrialEnabledVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->perWorldBindingsOriginTrialEnabledVoidMethod();
}

static void perWorldBindingsOriginTrialEnabledVoidMethodMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->perWorldBindingsOriginTrialEnabledVoidMethod();
}

static void perWorldBindingsVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->perWorldBindingsVoidMethod();
}

static void perWorldBindingsVoidMethodMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->perWorldBindingsVoidMethod();
}

static void perWorldBindingsVoidMethodTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("perWorldBindingsVoidMethodTestInterfaceEmptyArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  TestInterfaceEmpty* testInterfaceEmptyArg;
  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceEmptyArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("perWorldBindingsVoidMethodTestInterfaceEmptyArg", "TestObject", "parameter 1 is not of type 'TestInterfaceEmpty'."));
    return;
  }

  impl->perWorldBindingsVoidMethodTestInterfaceEmptyArg(testInterfaceEmptyArg);
}

static void perWorldBindingsVoidMethodTestInterfaceEmptyArgMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("perWorldBindingsVoidMethodTestInterfaceEmptyArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  TestInterfaceEmpty* testInterfaceEmptyArg;
  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceEmptyArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("perWorldBindingsVoidMethodTestInterfaceEmptyArg", "TestObject", "parameter 1 is not of type 'TestInterfaceEmpty'."));
    return;
  }

  impl->perWorldBindingsVoidMethodTestInterfaceEmptyArg(testInterfaceEmptyArg);
}

static void activityLoggingForAllWorldsPerWorldBindingsVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->activityLoggingForAllWorldsPerWorldBindingsVoidMethod();
}

static void activityLoggingForAllWorldsPerWorldBindingsVoidMethodMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->activityLoggingForAllWorldsPerWorldBindingsVoidMethod();
}

static void activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethod();
}

static void activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethodMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethod();
}

static void raisesExceptionVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "raisesExceptionVoidMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->raisesExceptionVoidMethod(exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
}

static void raisesExceptionStringMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "raisesExceptionStringMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  String result = impl->raisesExceptionStringMethod(exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValueString(info, result, info.GetIsolate());
}

static void raisesExceptionVoidMethodOptionalLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "raisesExceptionVoidMethodOptionalLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t optionalLongArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    impl->raisesExceptionVoidMethodOptionalLongArg(exceptionState);
    if (exceptionState.HadException()) {
      return;
    }
    return;
  }
  optionalLongArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->raisesExceptionVoidMethodOptionalLongArg(optionalLongArg, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
}

static void raisesExceptionVoidMethodTestCallbackInterfaceArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "raisesExceptionVoidMethodTestCallbackInterfaceArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8TestCallbackInterface* testCallbackInterfaceArg;
  if (info[0]->IsObject()) {
    testCallbackInterfaceArg = V8TestCallbackInterface::CreateOrNull(info[0].As<v8::Object>());
    if (!testCallbackInterfaceArg) {
      exceptionState.ThrowSecurityError("The callback provided as parameter 1 is a cross origin object.");
      return;
    }
  } else {
    exceptionState.ThrowTypeError("The callback provided as parameter 1 is not an object.");
    return;
  }

  impl->raisesExceptionVoidMethodTestCallbackInterfaceArg(testCallbackInterfaceArg, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
}

static void raisesExceptionVoidMethodOptionalTestCallbackInterfaceArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "raisesExceptionVoidMethodOptionalTestCallbackInterfaceArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8TestCallbackInterface* optionalTestCallbackInterfaceArg;
  if (info[0]->IsObject()) {
    optionalTestCallbackInterfaceArg = V8TestCallbackInterface::CreateOrNull(info[0].As<v8::Object>());
    if (!optionalTestCallbackInterfaceArg) {
      exceptionState.ThrowSecurityError("The callback provided as parameter 1 is a cross origin object.");
      return;
    }
  } else if (info[0]->IsUndefined()) {
    optionalTestCallbackInterfaceArg = nullptr;
  } else {
    exceptionState.ThrowTypeError("The callback provided as parameter 1 is not an object.");
    return;
  }

  impl->raisesExceptionVoidMethodOptionalTestCallbackInterfaceArg(optionalTestCallbackInterfaceArg, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
}

static void raisesExceptionTestInterfaceEmptyVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "raisesExceptionTestInterfaceEmptyVoidMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceEmpty* result = impl->raisesExceptionTestInterfaceEmptyVoidMethod(exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void raisesExceptionXPathNSResolverVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "raisesExceptionXPathNSResolverVoidMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  XPathNSResolver* result = impl->raisesExceptionXPathNSResolverVoidMethod(exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void callWithExecutionContextRaisesExceptionVoidMethodLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "callWithExecutionContextRaisesExceptionVoidMethodLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  ExecutionContext* executionContext = ExecutionContext::ForRelevantRealm(info);
  impl->callWithExecutionContextRaisesExceptionVoidMethodLongArg(executionContext, longArg, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
}

static void runtimeEnabledVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->runtimeEnabledVoidMethod();
}

static void perWorldBindingsRuntimeEnabledVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->perWorldBindingsRuntimeEnabledVoidMethod();
}

static void perWorldBindingsRuntimeEnabledVoidMethodMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->perWorldBindingsRuntimeEnabledVoidMethod();
}

static void runtimeEnabledOverloadedVoidMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> stringArg;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  impl->runtimeEnabledOverloadedVoidMethod(stringArg);
}

static void runtimeEnabledOverloadedVoidMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "runtimeEnabledOverloadedVoidMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->runtimeEnabledOverloadedVoidMethod(longArg);
}

static void runtimeEnabledOverloadedVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (info[0]->IsNumber()) {
        runtimeEnabledOverloadedVoidMethod2Method(info);
        return;
      }
      if (true) {
        runtimeEnabledOverloadedVoidMethod1Method(info);
        return;
      }
      if (true) {
        runtimeEnabledOverloadedVoidMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "runtimeEnabledOverloadedVoidMethod");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void partiallyRuntimeEnabledOverloadedVoidMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> stringArg;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  impl->partiallyRuntimeEnabledOverloadedVoidMethod(stringArg);
}

static void partiallyRuntimeEnabledOverloadedVoidMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceImplementation* testInterfaceArg;
  testInterfaceArg = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("partiallyRuntimeEnabledOverloadedVoidMethod", "TestObject", "parameter 1 is not of type 'TestInterface'."));
    return;
  }

  impl->partiallyRuntimeEnabledOverloadedVoidMethod(testInterfaceArg);
}

static void partiallyRuntimeEnabledOverloadedVoidMethod3Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "partiallyRuntimeEnabledOverloadedVoidMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  V8StringResource<> stringArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  stringArg = info[1];
  if (!stringArg.Prepare())
    return;

  impl->partiallyRuntimeEnabledOverloadedVoidMethod(longArg, stringArg);
}

static void partiallyRuntimeEnabledOverloadedVoidMethod4Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "partiallyRuntimeEnabledOverloadedVoidMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  V8StringResource<> stringArg;
  TestInterfaceImplementation* testInterfaceArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  stringArg = info[1];
  if (!stringArg.Prepare())
    return;

  testInterfaceArg = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), info[2]);
  if (!testInterfaceArg) {
    exceptionState.ThrowTypeError("parameter 3 is not of type 'TestInterface'.");
    return;
  }

  impl->partiallyRuntimeEnabledOverloadedVoidMethod(longArg, stringArg, testInterfaceArg);
}

static int partiallyRuntimeEnabledOverloadedVoidMethodMethodLength() {
  if (RuntimeEnabledFeatures::FeatureName1Enabled()) {
    return 1;
  }
  if (RuntimeEnabledFeatures::FeatureName2Enabled()) {
    return 1;
  }
  return 2;
}

static int partiallyRuntimeEnabledOverloadedVoidMethodMethodMaxArg() {
  if (RuntimeEnabledFeatures::FeatureName3Enabled()) {
    return 3;
  }
  return 2;
}

static void partiallyRuntimeEnabledOverloadedVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(test_object_v8_internal::partiallyRuntimeEnabledOverloadedVoidMethodMethodMaxArg(), info.Length())) {
    case 1:
      if (RuntimeEnabledFeatures::FeatureName2Enabled()) {
        if (V8TestInterface::hasInstance(info[0], info.GetIsolate())) {
          partiallyRuntimeEnabledOverloadedVoidMethod2Method(info);
          return;
        }
      }
      if (RuntimeEnabledFeatures::FeatureName1Enabled()) {
        if (true) {
          partiallyRuntimeEnabledOverloadedVoidMethod1Method(info);
          return;
        }
      }
      break;
    case 2:
      if (true) {
        partiallyRuntimeEnabledOverloadedVoidMethod3Method(info);
        return;
      }
      break;
    case 3:
      if (RuntimeEnabledFeatures::FeatureName3Enabled()) {
        if (true) {
          partiallyRuntimeEnabledOverloadedVoidMethod4Method(info);
          return;
        }
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "partiallyRuntimeEnabledOverloadedVoidMethod");
  if (isArityError) {
    if (info.Length() < test_object_v8_internal::partiallyRuntimeEnabledOverloadedVoidMethodMethodLength()) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(test_object_v8_internal::partiallyRuntimeEnabledOverloadedVoidMethodMethodLength(), info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  TestInterfaceEmpty* testInterfaceEmptyArg;
  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);

  impl->legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyArg(testInterfaceEmptyArg);
}

static void legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyVariadicArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyVariadicArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  HeapVector<Member<TestInterfaceEmpty>> testInterfaceEmptyArg;
  for (int i = 0; i < info.Length(); ++i) {
    if (!V8TestInterfaceEmpty::hasInstance(info[i], info.GetIsolate())) {
      exceptionState.ThrowTypeError("parameter 1 is not of type 'TestInterfaceEmpty'.");
      return;
    }
    testInterfaceEmptyArg.push_back(V8TestInterfaceEmpty::ToImpl(v8::Local<v8::Object>::Cast(info[i])));
  }

  impl->legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyVariadicArg(testInterfaceEmptyArg);
}

static void useToImpl4ArgumentsCheckingIfPossibleWithOptionalArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("useToImpl4ArgumentsCheckingIfPossibleWithOptionalArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  Node* node1;
  Node* node2;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  node1 = V8Node::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!node1) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("useToImpl4ArgumentsCheckingIfPossibleWithOptionalArg", "TestObject", "parameter 1 is not of type 'Node'."));
    return;
  }

  if (UNLIKELY(numArgsPassed <= 1)) {
    impl->useToImpl4ArgumentsCheckingIfPossibleWithOptionalArg(node1);
    return;
  }
  node2 = V8Node::ToImplWithTypeCheck(info.GetIsolate(), info[1]);
  if (!node2) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("useToImpl4ArgumentsCheckingIfPossibleWithOptionalArg", "TestObject", "parameter 2 is not of type 'Node'."));
    return;
  }

  impl->useToImpl4ArgumentsCheckingIfPossibleWithOptionalArg(node1, node2);
}

static void useToImpl4ArgumentsCheckingIfPossibleWithNullableArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 2)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("useToImpl4ArgumentsCheckingIfPossibleWithNullableArg", "TestObject", ExceptionMessages::NotEnoughArguments(2, info.Length())));
    return;
  }

  Node* node1;
  Node* node2;
  node1 = V8Node::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!node1) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("useToImpl4ArgumentsCheckingIfPossibleWithNullableArg", "TestObject", "parameter 1 is not of type 'Node'."));
    return;
  }

  node2 = V8Node::ToImplWithTypeCheck(info.GetIsolate(), info[1]);
  if (!node2 && !IsUndefinedOrNull(info[1])) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("useToImpl4ArgumentsCheckingIfPossibleWithNullableArg", "TestObject", "parameter 2 is not of type 'Node'."));
    return;
  }

  impl->useToImpl4ArgumentsCheckingIfPossibleWithNullableArg(node1, node2);
}

static void useToImpl4ArgumentsCheckingIfPossibleWithUndefinedArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("useToImpl4ArgumentsCheckingIfPossibleWithUndefinedArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  Node* node1;
  Node* node2;
  node1 = V8Node::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!node1) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("useToImpl4ArgumentsCheckingIfPossibleWithUndefinedArg", "TestObject", "parameter 1 is not of type 'Node'."));
    return;
  }

  node2 = V8Node::ToImplWithTypeCheck(info.GetIsolate(), info[1]);
  if (!node2) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("useToImpl4ArgumentsCheckingIfPossibleWithUndefinedArg", "TestObject", "parameter 2 is not of type 'Node'."));
    return;
  }

  impl->useToImpl4ArgumentsCheckingIfPossibleWithUndefinedArg(node1, node2);
}

static void unforgeableVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->unforgeableVoidMethod();
}

static void newObjectTestInterfaceMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceImplementation* result = impl->newObjectTestInterfaceMethod();
  // [NewObject] must always create a new wrapper.  Check that a wrapper
  // does not exist yet.
  DCHECK(!result || DOMDataStore::GetWrapper(result, info.GetIsolate()).IsEmpty());
  V8SetReturnValue(info, result);
}

static void newObjectTestInterfacePromiseMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "newObjectTestInterfacePromiseMethod");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  // V8DOMConfiguration::kDoNotCheckHolder
  // Make sure that info.Holder() really points to an instance of the type.
  if (!V8TestObject::hasInstance(info.Holder(), info.GetIsolate())) {
    exceptionState.ThrowTypeError("Illegal invocation");
    return;
  }
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptPromise result = impl->newObjectTestInterfacePromiseMethod();
  V8SetReturnValue(info, result.V8Value());
}

static void RuntimeCallStatsCounterMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->RuntimeCallStatsCounterMethod();
}

static void clearMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "clear");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  bool result = impl->myMaplikeClear(scriptState, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValueBool(info, result);
}

static void keysMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "keys");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  Iterator* result = impl->keysForBinding(scriptState, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void valuesMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "values");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  Iterator* result = impl->valuesForBinding(scriptState, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void forEachMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "forEach");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

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
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "has");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int32_t key;
  key = NativeValueTraits<IDLLongEnforceRange>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  bool result = impl->hasForBinding(scriptState, key, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValueBool(info, result);
}

static void getMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "get");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int32_t key;
  key = NativeValueTraits<IDLLongEnforceRange>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  ScriptValue result = impl->getForBinding(scriptState, key, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result.V8Value());
}

static void deleteMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "delete");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int32_t key;
  key = NativeValueTraits<IDLLongEnforceRange>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  bool result = impl->deleteForBinding(scriptState, key, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValueBool(info, result);
}

static void setMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "set");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  if (UNLIKELY(info.Length() < 2)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  int32_t key;
  StringOrDouble value;
  key = NativeValueTraits<IDLLongEnforceRange>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  V8StringOrDouble::ToImpl(info.GetIsolate(), info[1], value, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  TestObject* result = impl->setForBinding(scriptState, key, value, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void toJSONMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  ScriptValue result = impl->toJSONForBinding(scriptState);
  V8SetReturnValue(info, result.V8Value());
}

static void toStringMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueString(info, impl->stringifierAttribute(), info.GetIsolate());
}

static void iteratorMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "iterator");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  Iterator* result = impl->GetIterator(scriptState, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void namedPropertyGetter(const AtomicString& name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  TestObject* impl = V8TestObject::ToImpl(info.Holder());
  ScriptValue result = impl->AnonymousNamedGetter(scriptState, name);
  if (result.IsEmpty())
    return;
  V8SetReturnValue(info, result.V8Value());
}

static void namedPropertySetter(const AtomicString& name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  TestObject* impl = V8TestObject::ToImpl(info.Holder());
  V8StringResource<> propertyValue = v8Value;
  if (!propertyValue.Prepare())
    return;

  bool result = impl->AnonymousNamedSetter(scriptState, name, propertyValue);
  if (!result)
    return;
  V8SetReturnValue(info, v8Value);
}

static void namedPropertyDeleter(const AtomicString& name, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  DeleteResult result = impl->AnonymousNamedDeleter(scriptState, name);
  if (result == kDeleteUnknownProperty)
    return;
  V8SetReturnValue(info, result == kDeleteSuccess);
}

static void namedPropertyQuery(const AtomicString& name, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  const CString& nameInUtf8 = name.Utf8();
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kGetterContext, "TestObject", nameInUtf8.data());
  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  bool result = impl->NamedPropertyQuery(scriptState, name, exceptionState);
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
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kEnumerationContext, "TestObject");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  Vector<String> names;
  impl->NamedPropertyEnumerator(names, exceptionState);
  if (exceptionState.HadException())
    return;
  V8SetReturnValue(info, ToV8(names, info.Holder(), info.GetIsolate()).As<v8::Array>());
}

static void indexedPropertyGetter(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  // We assume that all the implementations support length() method, although
  // the spec doesn't require that length() must exist.  It's okay that
  // the interface does not have length attribute as long as the
  // implementation supports length() member function.
  if (index >= impl->length())
    return;  // Returns undefined due to out-of-range.

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  ScriptValue result = impl->item(scriptState, index);
  V8SetReturnValue(info, result.V8Value());
}

static void indexedPropertyDescriptor(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  // https://heycam.github.io/webidl/#LegacyPlatformObjectGetOwnProperty
  // Steps 1.1 to 1.2.4 are covered here: we rely on indexedPropertyGetter() to
  // call the getter function and check that |index| is a valid property index,
  // in which case it will have set info.GetReturnValue() to something other
  // than undefined.
  V8TestObject::indexedPropertyGetterCallback(index, info);
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
  TestObject* impl = V8TestObject::ToImpl(info.Holder());
  V8StringResource<> propertyValue = v8Value;
  if (!propertyValue.Prepare())
    return;

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  bool result = impl->setItem(scriptState, index, propertyValue);
  if (!result)
    return;
  V8SetReturnValue(info, v8Value);
}

static void indexedPropertyDeleter(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kIndexedDeletionContext, "TestObject");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  DeleteResult result = impl->AnonymousIndexedDeleter(scriptState, index, exceptionState);
  if (exceptionState.HadException())
    return;
  if (result == kDeleteUnknownProperty)
    return;
  V8SetReturnValue(info, result == kDeleteSuccess);
}

}  // namespace test_object_v8_internal

void V8TestObject::stringifierAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringifierAttribute_Getter");

  test_object_v8_internal::stringifierAttributeAttributeGetter(info);
}

void V8TestObject::stringifierAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringifierAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::stringifierAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::readonlyStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_readonlyStringAttribute_Getter");

  test_object_v8_internal::readonlyStringAttributeAttributeGetter(info);
}

void V8TestObject::readonlyTestInterfaceEmptyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_readonlyTestInterfaceEmptyAttribute_Getter");

  test_object_v8_internal::readonlyTestInterfaceEmptyAttributeAttributeGetter(info);
}

void V8TestObject::readonlyLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_readonlyLongAttribute_Getter");

  test_object_v8_internal::readonlyLongAttributeAttributeGetter(info);
}

void V8TestObject::dateAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_dateAttribute_Getter");

  test_object_v8_internal::dateAttributeAttributeGetter(info);
}

void V8TestObject::dateAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_dateAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::dateAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::stringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringAttribute_Getter");

  test_object_v8_internal::stringAttributeAttributeGetter(info);
}

void V8TestObject::stringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::stringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::byteStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_byteStringAttribute_Getter");

  test_object_v8_internal::byteStringAttributeAttributeGetter(info);
}

void V8TestObject::byteStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_byteStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::byteStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::usvStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_usvStringAttribute_Getter");

  test_object_v8_internal::usvStringAttributeAttributeGetter(info);
}

void V8TestObject::usvStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_usvStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::usvStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::domTimeStampAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_domTimeStampAttribute_Getter");

  test_object_v8_internal::domTimeStampAttributeAttributeGetter(info);
}

void V8TestObject::domTimeStampAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_domTimeStampAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::domTimeStampAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::booleanAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_booleanAttribute_Getter");

  test_object_v8_internal::booleanAttributeAttributeGetter(info);
}

void V8TestObject::booleanAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_booleanAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::booleanAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::byteAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_byteAttribute_Getter");

  test_object_v8_internal::byteAttributeAttributeGetter(info);
}

void V8TestObject::byteAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_byteAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::byteAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::doubleAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_doubleAttribute_Getter");

  test_object_v8_internal::doubleAttributeAttributeGetter(info);
}

void V8TestObject::doubleAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_doubleAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::doubleAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::floatAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_floatAttribute_Getter");

  test_object_v8_internal::floatAttributeAttributeGetter(info);
}

void V8TestObject::floatAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_floatAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::floatAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::longAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longAttribute_Getter");

  test_object_v8_internal::longAttributeAttributeGetter(info);
}

void V8TestObject::longAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::longAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::longLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longLongAttribute_Getter");

  test_object_v8_internal::longLongAttributeAttributeGetter(info);
}

void V8TestObject::longLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::longLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::octetAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_octetAttribute_Getter");

  test_object_v8_internal::octetAttributeAttributeGetter(info);
}

void V8TestObject::octetAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_octetAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::octetAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::shortAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_shortAttribute_Getter");

  test_object_v8_internal::shortAttributeAttributeGetter(info);
}

void V8TestObject::shortAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_shortAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::shortAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::unrestrictedDoubleAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unrestrictedDoubleAttribute_Getter");

  test_object_v8_internal::unrestrictedDoubleAttributeAttributeGetter(info);
}

void V8TestObject::unrestrictedDoubleAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unrestrictedDoubleAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::unrestrictedDoubleAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::unrestrictedFloatAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unrestrictedFloatAttribute_Getter");

  test_object_v8_internal::unrestrictedFloatAttributeAttributeGetter(info);
}

void V8TestObject::unrestrictedFloatAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unrestrictedFloatAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::unrestrictedFloatAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::unsignedLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unsignedLongAttribute_Getter");

  test_object_v8_internal::unsignedLongAttributeAttributeGetter(info);
}

void V8TestObject::unsignedLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unsignedLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::unsignedLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::unsignedLongLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unsignedLongLongAttribute_Getter");

  test_object_v8_internal::unsignedLongLongAttributeAttributeGetter(info);
}

void V8TestObject::unsignedLongLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unsignedLongLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::unsignedLongLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::unsignedShortAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unsignedShortAttribute_Getter");

  test_object_v8_internal::unsignedShortAttributeAttributeGetter(info);
}

void V8TestObject::unsignedShortAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unsignedShortAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::unsignedShortAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::testInterfaceEmptyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceEmptyAttribute_Getter");

  test_object_v8_internal::testInterfaceEmptyAttributeAttributeGetter(info);
}

void V8TestObject::testInterfaceEmptyAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceEmptyAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::testInterfaceEmptyAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::testObjectAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testObjectAttribute_Getter");

  test_object_v8_internal::testObjectAttributeAttributeGetter(info);
}

void V8TestObject::testObjectAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testObjectAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::testObjectAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::cssAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_cssAttribute_Getter");

  test_object_v8_internal::cssAttributeAttributeGetter(info);
}

void V8TestObject::cssAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_cssAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::cssAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::imeAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_imeAttribute_Getter");

  test_object_v8_internal::imeAttributeAttributeGetter(info);
}

void V8TestObject::imeAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_imeAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::imeAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::svgAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_svgAttribute_Getter");

  test_object_v8_internal::svgAttributeAttributeGetter(info);
}

void V8TestObject::svgAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_svgAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::svgAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::xmlAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_xmlAttribute_Getter");

  test_object_v8_internal::xmlAttributeAttributeGetter(info);
}

void V8TestObject::xmlAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_xmlAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::xmlAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::nodeFilterAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nodeFilterAttribute_Getter");

  test_object_v8_internal::nodeFilterAttributeAttributeGetter(info);
}

void V8TestObject::serializedScriptValueAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_serializedScriptValueAttribute_Getter");

  test_object_v8_internal::serializedScriptValueAttributeAttributeGetter(info);
}

void V8TestObject::serializedScriptValueAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_serializedScriptValueAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::serializedScriptValueAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::anyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_anyAttribute_Getter");

  test_object_v8_internal::anyAttributeAttributeGetter(info);
}

void V8TestObject::anyAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_anyAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::anyAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::promiseAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_promiseAttribute_Getter");

  test_object_v8_internal::promiseAttributeAttributeGetter(info);
}

void V8TestObject::promiseAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_promiseAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::promiseAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::windowAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_windowAttribute_Getter");

  test_object_v8_internal::windowAttributeAttributeGetter(info);
}

void V8TestObject::windowAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_windowAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::windowAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::documentAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_documentAttribute_Getter");

  test_object_v8_internal::documentAttributeAttributeGetter(info);
}

void V8TestObject::documentAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_documentAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::documentAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::documentFragmentAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_documentFragmentAttribute_Getter");

  test_object_v8_internal::documentFragmentAttributeAttributeGetter(info);
}

void V8TestObject::documentFragmentAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_documentFragmentAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::documentFragmentAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::documentTypeAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_documentTypeAttribute_Getter");

  test_object_v8_internal::documentTypeAttributeAttributeGetter(info);
}

void V8TestObject::documentTypeAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_documentTypeAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::documentTypeAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::elementAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_elementAttribute_Getter");

  test_object_v8_internal::elementAttributeAttributeGetter(info);
}

void V8TestObject::elementAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_elementAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::elementAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::nodeAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nodeAttribute_Getter");

  test_object_v8_internal::nodeAttributeAttributeGetter(info);
}

void V8TestObject::nodeAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nodeAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::nodeAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::shadowRootAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_shadowRootAttribute_Getter");

  test_object_v8_internal::shadowRootAttributeAttributeGetter(info);
}

void V8TestObject::shadowRootAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_shadowRootAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::shadowRootAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::arrayBufferAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_arrayBufferAttribute_Getter");

  test_object_v8_internal::arrayBufferAttributeAttributeGetter(info);
}

void V8TestObject::arrayBufferAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_arrayBufferAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::arrayBufferAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::float32ArrayAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_float32ArrayAttribute_Getter");

  test_object_v8_internal::float32ArrayAttributeAttributeGetter(info);
}

void V8TestObject::float32ArrayAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_float32ArrayAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::float32ArrayAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::uint8ArrayAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_uint8ArrayAttribute_Getter");

  test_object_v8_internal::uint8ArrayAttributeAttributeGetter(info);
}

void V8TestObject::uint8ArrayAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_uint8ArrayAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::uint8ArrayAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::selfAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_self_Getter");

  test_object_v8_internal::selfAttributeGetter(info);
}

void V8TestObject::readonlyEventTargetAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_readonlyEventTargetAttribute_Getter");

  test_object_v8_internal::readonlyEventTargetAttributeAttributeGetter(info);
}

void V8TestObject::readonlyEventTargetOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_readonlyEventTargetOrNullAttribute_Getter");

  test_object_v8_internal::readonlyEventTargetOrNullAttributeAttributeGetter(info);
}

void V8TestObject::readonlyWindowAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_readonlyWindowAttribute_Getter");

  test_object_v8_internal::readonlyWindowAttributeAttributeGetter(info);
}

void V8TestObject::htmlCollectionAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_htmlCollectionAttribute_Getter");

  test_object_v8_internal::htmlCollectionAttributeAttributeGetter(info);
}

void V8TestObject::htmlElementAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_htmlElementAttribute_Getter");

  test_object_v8_internal::htmlElementAttributeAttributeGetter(info);
}

void V8TestObject::stringFrozenArrayAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringFrozenArrayAttribute_Getter");

  test_object_v8_internal::stringFrozenArrayAttributeAttributeGetter(info);
}

void V8TestObject::stringFrozenArrayAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringFrozenArrayAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::stringFrozenArrayAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::testInterfaceEmptyFrozenArrayAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceEmptyFrozenArrayAttribute_Getter");

  test_object_v8_internal::testInterfaceEmptyFrozenArrayAttributeAttributeGetter(info);
}

void V8TestObject::testInterfaceEmptyFrozenArrayAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceEmptyFrozenArrayAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::testInterfaceEmptyFrozenArrayAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::booleanOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_booleanOrNullAttribute_Getter");

  test_object_v8_internal::booleanOrNullAttributeAttributeGetter(info);
}

void V8TestObject::booleanOrNullAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_booleanOrNullAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::booleanOrNullAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::stringOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringOrNullAttribute_Getter");

  test_object_v8_internal::stringOrNullAttributeAttributeGetter(info);
}

void V8TestObject::stringOrNullAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringOrNullAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::stringOrNullAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::longOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longOrNullAttribute_Getter");

  test_object_v8_internal::longOrNullAttributeAttributeGetter(info);
}

void V8TestObject::longOrNullAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longOrNullAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::longOrNullAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::testInterfaceOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceOrNullAttribute_Getter");

  test_object_v8_internal::testInterfaceOrNullAttributeAttributeGetter(info);
}

void V8TestObject::testInterfaceOrNullAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceOrNullAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::testInterfaceOrNullAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::testEnumAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testEnumAttribute_Getter");

  test_object_v8_internal::testEnumAttributeAttributeGetter(info);
}

void V8TestObject::testEnumAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testEnumAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::testEnumAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::testEnumOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testEnumOrNullAttribute_Getter");

  test_object_v8_internal::testEnumOrNullAttributeAttributeGetter(info);
}

void V8TestObject::testEnumOrNullAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testEnumOrNullAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::testEnumOrNullAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::staticStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_staticStringAttribute_Getter");

  test_object_v8_internal::staticStringAttributeAttributeGetter(info);
}

void V8TestObject::staticStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_staticStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::staticStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::staticLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_staticLongAttribute_Getter");

  test_object_v8_internal::staticLongAttributeAttributeGetter(info);
}

void V8TestObject::staticLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_staticLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::staticLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::eventHandlerAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_eventHandlerAttribute_Getter");

  test_object_v8_internal::eventHandlerAttributeAttributeGetter(info);
}

void V8TestObject::eventHandlerAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_eventHandlerAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::eventHandlerAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::doubleOrStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_doubleOrStringAttribute_Getter");

  test_object_v8_internal::doubleOrStringAttributeAttributeGetter(info);
}

void V8TestObject::doubleOrStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_doubleOrStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::doubleOrStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::doubleOrStringOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_doubleOrStringOrNullAttribute_Getter");

  test_object_v8_internal::doubleOrStringOrNullAttributeAttributeGetter(info);
}

void V8TestObject::doubleOrStringOrNullAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_doubleOrStringOrNullAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::doubleOrStringOrNullAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::doubleOrNullStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_doubleOrNullStringAttribute_Getter");

  test_object_v8_internal::doubleOrNullStringAttributeAttributeGetter(info);
}

void V8TestObject::doubleOrNullStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_doubleOrNullStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::doubleOrNullStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::stringOrStringSequenceAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringOrStringSequenceAttribute_Getter");

  test_object_v8_internal::stringOrStringSequenceAttributeAttributeGetter(info);
}

void V8TestObject::stringOrStringSequenceAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringOrStringSequenceAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::stringOrStringSequenceAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::testEnumOrDoubleAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testEnumOrDoubleAttribute_Getter");

  test_object_v8_internal::testEnumOrDoubleAttributeAttributeGetter(info);
}

void V8TestObject::testEnumOrDoubleAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testEnumOrDoubleAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::testEnumOrDoubleAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::unrestrictedDoubleOrStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unrestrictedDoubleOrStringAttribute_Getter");

  test_object_v8_internal::unrestrictedDoubleOrStringAttributeAttributeGetter(info);
}

void V8TestObject::unrestrictedDoubleOrStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unrestrictedDoubleOrStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::unrestrictedDoubleOrStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::nestedUnionAtributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nestedUnionAtribute_Getter");

  test_object_v8_internal::nestedUnionAtributeAttributeGetter(info);
}

void V8TestObject::nestedUnionAtributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nestedUnionAtribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::nestedUnionAtributeAttributeSetter(v8Value, info);
}

void V8TestObject::activityLoggingAccessForAllWorldsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessForAllWorldsLongAttribute_Getter");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogGetter("TestObject.activityLoggingAccessForAllWorldsLongAttribute");
  }

  test_object_v8_internal::activityLoggingAccessForAllWorldsLongAttributeAttributeGetter(info);
}

void V8TestObject::activityLoggingAccessForAllWorldsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessForAllWorldsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogSetter("TestObject.activityLoggingAccessForAllWorldsLongAttribute", v8Value);
  }

  test_object_v8_internal::activityLoggingAccessForAllWorldsLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::activityLoggingGetterForAllWorldsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingGetterForAllWorldsLongAttribute_Getter");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogGetter("TestObject.activityLoggingGetterForAllWorldsLongAttribute");
  }

  test_object_v8_internal::activityLoggingGetterForAllWorldsLongAttributeAttributeGetter(info);
}

void V8TestObject::activityLoggingGetterForAllWorldsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingGetterForAllWorldsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::activityLoggingGetterForAllWorldsLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::activityLoggingSetterForAllWorldsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingSetterForAllWorldsLongAttribute_Getter");

  test_object_v8_internal::activityLoggingSetterForAllWorldsLongAttributeAttributeGetter(info);
}

void V8TestObject::activityLoggingSetterForAllWorldsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingSetterForAllWorldsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogSetter("TestObject.activityLoggingSetterForAllWorldsLongAttribute", v8Value);
  }

  test_object_v8_internal::activityLoggingSetterForAllWorldsLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::cachedAttributeAnyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_cachedAttributeAnyAttribute_Getter");

  test_object_v8_internal::cachedAttributeAnyAttributeAttributeGetter(info);
}

void V8TestObject::cachedAttributeAnyAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_cachedAttributeAnyAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::cachedAttributeAnyAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::cachedArrayAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_cachedArrayAttribute_Getter");

  test_object_v8_internal::cachedArrayAttributeAttributeGetter(info);
}

void V8TestObject::cachedArrayAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_cachedArrayAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::cachedArrayAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::cachedStringOrNoneAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_cachedStringOrNoneAttribute_Getter");

  test_object_v8_internal::cachedStringOrNoneAttributeAttributeGetter(info);
}

void V8TestObject::cachedStringOrNoneAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_cachedStringOrNoneAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::cachedStringOrNoneAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::callWithExecutionContextAnyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithExecutionContextAnyAttribute_Getter");

  test_object_v8_internal::callWithExecutionContextAnyAttributeAttributeGetter(info);
}

void V8TestObject::callWithExecutionContextAnyAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithExecutionContextAnyAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::callWithExecutionContextAnyAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::callWithScriptStateAnyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithScriptStateAnyAttribute_Getter");

  test_object_v8_internal::callWithScriptStateAnyAttributeAttributeGetter(info);
}

void V8TestObject::callWithScriptStateAnyAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithScriptStateAnyAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::callWithScriptStateAnyAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::callWithExecutionContextAndScriptStateAnyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithExecutionContextAndScriptStateAnyAttribute_Getter");

  test_object_v8_internal::callWithExecutionContextAndScriptStateAnyAttributeAttributeGetter(info);
}

void V8TestObject::callWithExecutionContextAndScriptStateAnyAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithExecutionContextAndScriptStateAnyAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::callWithExecutionContextAndScriptStateAnyAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::checkSecurityForNodeReadonlyDocumentAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_checkSecurityForNodeReadonlyDocumentAttribute_Getter");

  test_object_v8_internal::checkSecurityForNodeReadonlyDocumentAttributeAttributeGetter(info);
}

void V8TestObject::testInterfaceEmptyConstructorAttributeConstructorGetterCallback(v8::Local<v8::Name> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceEmptyConstructorAttribute_ConstructorGetterCallback");

  V8ConstructorAttributeGetter(property, info, &V8TestInterfaceEmpty::wrapperTypeInfo);
}

void V8TestObject::testInterfaceEmptyConstructorAttributeConstructorGetterCallback(v8::Local<v8::Name> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceEmptyConstructorAttribute_ConstructorGetterCallback");

  Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kdeprecatedTestInterfaceEmptyConstructorAttribute);

  V8ConstructorAttributeGetter(property, info, &V8TestInterfaceEmpty::wrapperTypeInfo);
}

void V8TestObject::measureAsFeatureNameTestInterfaceEmptyConstructorAttributeConstructorGetterCallback(v8::Local<v8::Name> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_measureAsFeatureNameTestInterfaceEmptyConstructorAttribute_ConstructorGetterCallback");

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kFeatureName);

  V8ConstructorAttributeGetter(property, info, &V8TestInterfaceEmpty::wrapperTypeInfo);
}

void V8TestObject::customObjectAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customObjectAttribute_Getter");

  V8TestObject::customObjectAttributeAttributeGetterCustom(info);
}

void V8TestObject::customObjectAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customObjectAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  V8TestObject::customObjectAttributeAttributeSetterCustom(v8Value, info);
}

void V8TestObject::customGetterLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customGetterLongAttribute_Getter");

  V8TestObject::customGetterLongAttributeAttributeGetterCustom(info);
}

void V8TestObject::customGetterLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customGetterLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::customGetterLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::customGetterReadonlyObjectAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customGetterReadonlyObjectAttribute_Getter");

  V8TestObject::customGetterReadonlyObjectAttributeAttributeGetterCustom(info);
}

void V8TestObject::customSetterLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customSetterLongAttribute_Getter");

  test_object_v8_internal::customSetterLongAttributeAttributeGetter(info);
}

void V8TestObject::customSetterLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customSetterLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  V8TestObject::customSetterLongAttributeAttributeSetterCustom(v8Value, info);
}

void V8TestObject::deprecatedLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_deprecatedLongAttribute_Getter");

  Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kLongAttribute);

  test_object_v8_internal::deprecatedLongAttributeAttributeGetter(info);
}

void V8TestObject::deprecatedLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_deprecatedLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kLongAttribute);

  test_object_v8_internal::deprecatedLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::enforceRangeLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_enforceRangeLongAttribute_Getter");

  test_object_v8_internal::enforceRangeLongAttributeAttributeGetter(info);
}

void V8TestObject::enforceRangeLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_enforceRangeLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::enforceRangeLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::implementedAsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_implementedAsLongAttribute_Getter");

  test_object_v8_internal::implementedAsLongAttributeAttributeGetter(info);
}

void V8TestObject::implementedAsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_implementedAsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::implementedAsLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::customImplementedAsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customImplementedAsLongAttribute_Getter");

  V8TestObject::customImplementedAsLongAttributeAttributeGetterCustom(info);
}

void V8TestObject::customImplementedAsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customImplementedAsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  V8TestObject::customImplementedAsLongAttributeAttributeSetterCustom(v8Value, info);
}

void V8TestObject::customGetterImplementedAsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customGetterImplementedAsLongAttribute_Getter");

  V8TestObject::customGetterImplementedAsLongAttributeAttributeGetterCustom(info);
}

void V8TestObject::customGetterImplementedAsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customGetterImplementedAsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::customGetterImplementedAsLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::customSetterImplementedAsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customSetterImplementedAsLongAttribute_Getter");

  test_object_v8_internal::customSetterImplementedAsLongAttributeAttributeGetter(info);
}

void V8TestObject::customSetterImplementedAsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customSetterImplementedAsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  V8TestObject::customSetterImplementedAsLongAttributeAttributeSetterCustom(v8Value, info);
}

void V8TestObject::measureAsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_measureAsLongAttribute_Getter");

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeature);

  test_object_v8_internal::measureAsLongAttributeAttributeGetter(info);
}

void V8TestObject::measureAsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_measureAsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeature);

  test_object_v8_internal::measureAsLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::notEnumerableLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_notEnumerableLongAttribute_Getter");

  test_object_v8_internal::notEnumerableLongAttributeAttributeGetter(info);
}

void V8TestObject::notEnumerableLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_notEnumerableLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::notEnumerableLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::originTrialEnabledLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_originTrialEnabledLongAttribute_Getter");

  test_object_v8_internal::originTrialEnabledLongAttributeAttributeGetter(info);
}

void V8TestObject::originTrialEnabledLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_originTrialEnabledLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::originTrialEnabledLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::perWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_perWorldBindingsReadonlyTestInterfaceEmptyAttribute_Getter");

  test_object_v8_internal::perWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetter(info);
}

void V8TestObject::perWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_perWorldBindingsReadonlyTestInterfaceEmptyAttribute_Getter");

  test_object_v8_internal::perWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterForMainWorld(info);
}

void V8TestObject::activityLoggingAccessPerWorldBindingsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessPerWorldBindingsLongAttribute_Getter");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogGetter("TestObject.activityLoggingAccessPerWorldBindingsLongAttribute");
  }

  test_object_v8_internal::activityLoggingAccessPerWorldBindingsLongAttributeAttributeGetter(info);
}

void V8TestObject::activityLoggingAccessPerWorldBindingsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessPerWorldBindingsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogSetter("TestObject.activityLoggingAccessPerWorldBindingsLongAttribute", v8Value);
  }

  test_object_v8_internal::activityLoggingAccessPerWorldBindingsLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::activityLoggingAccessPerWorldBindingsLongAttributeAttributeGetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessPerWorldBindingsLongAttribute_Getter");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogGetter("TestObject.activityLoggingAccessPerWorldBindingsLongAttribute");
  }

  test_object_v8_internal::activityLoggingAccessPerWorldBindingsLongAttributeAttributeGetterForMainWorld(info);
}

void V8TestObject::activityLoggingAccessPerWorldBindingsLongAttributeAttributeSetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessPerWorldBindingsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogSetter("TestObject.activityLoggingAccessPerWorldBindingsLongAttribute", v8Value);
  }

  test_object_v8_internal::activityLoggingAccessPerWorldBindingsLongAttributeAttributeSetterForMainWorld(v8Value, info);
}

void V8TestObject::activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute_Getter");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogGetter("TestObject.activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute");
  }

  test_object_v8_internal::activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetter(info);
}

void V8TestObject::activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogSetter("TestObject.activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute", v8Value);
  }

  test_object_v8_internal::activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute_Getter");

  test_object_v8_internal::activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterForMainWorld(info);
}

void V8TestObject::activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterForMainWorld(v8Value, info);
}

void V8TestObject::activityLoggingGetterPerWorldBindingsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingGetterPerWorldBindingsLongAttribute_Getter");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogGetter("TestObject.activityLoggingGetterPerWorldBindingsLongAttribute");
  }

  test_object_v8_internal::activityLoggingGetterPerWorldBindingsLongAttributeAttributeGetter(info);
}

void V8TestObject::activityLoggingGetterPerWorldBindingsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingGetterPerWorldBindingsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::activityLoggingGetterPerWorldBindingsLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::activityLoggingGetterPerWorldBindingsLongAttributeAttributeGetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingGetterPerWorldBindingsLongAttribute_Getter");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogGetter("TestObject.activityLoggingGetterPerWorldBindingsLongAttribute");
  }

  test_object_v8_internal::activityLoggingGetterPerWorldBindingsLongAttributeAttributeGetterForMainWorld(info);
}

void V8TestObject::activityLoggingGetterPerWorldBindingsLongAttributeAttributeSetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingGetterPerWorldBindingsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::activityLoggingGetterPerWorldBindingsLongAttributeAttributeSetterForMainWorld(v8Value, info);
}

void V8TestObject::activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute_Getter");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogGetter("TestObject.activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute");
  }

  test_object_v8_internal::activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetter(info);
}

void V8TestObject::activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute_Getter");

  test_object_v8_internal::activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterForMainWorld(info);
}

void V8TestObject::activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterForMainWorld(v8Value, info);
}

void V8TestObject::locationAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_location_Getter");

  test_object_v8_internal::locationAttributeGetter(info);
}

void V8TestObject::locationAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_location_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::locationAttributeSetter(v8Value, info);
}

void V8TestObject::locationWithExceptionAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationWithException_Getter");

  test_object_v8_internal::locationWithExceptionAttributeGetter(info);
}

void V8TestObject::locationWithExceptionAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationWithException_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::locationWithExceptionAttributeSetter(v8Value, info);
}

void V8TestObject::locationWithCallWithAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationWithCallWith_Getter");

  test_object_v8_internal::locationWithCallWithAttributeGetter(info);
}

void V8TestObject::locationWithCallWithAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationWithCallWith_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::locationWithCallWithAttributeSetter(v8Value, info);
}

void V8TestObject::locationByteStringAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationByteString_Getter");

  test_object_v8_internal::locationByteStringAttributeGetter(info);
}

void V8TestObject::locationByteStringAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationByteString_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::locationByteStringAttributeSetter(v8Value, info);
}

void V8TestObject::locationWithPerWorldBindingsAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationWithPerWorldBindings_Getter");

  test_object_v8_internal::locationWithPerWorldBindingsAttributeGetter(info);
}

void V8TestObject::locationWithPerWorldBindingsAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationWithPerWorldBindings_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::locationWithPerWorldBindingsAttributeSetter(v8Value, info);
}

void V8TestObject::locationWithPerWorldBindingsAttributeGetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationWithPerWorldBindings_Getter");

  test_object_v8_internal::locationWithPerWorldBindingsAttributeGetterForMainWorld(info);
}

void V8TestObject::locationWithPerWorldBindingsAttributeSetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationWithPerWorldBindings_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::locationWithPerWorldBindingsAttributeSetterForMainWorld(v8Value, info);
}

void V8TestObject::locationLegacyInterfaceTypeCheckingAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationLegacyInterfaceTypeChecking_Getter");

  test_object_v8_internal::locationLegacyInterfaceTypeCheckingAttributeGetter(info);
}

void V8TestObject::locationLegacyInterfaceTypeCheckingAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationLegacyInterfaceTypeChecking_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::locationLegacyInterfaceTypeCheckingAttributeSetter(v8Value, info);
}

void V8TestObject::raisesExceptionLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionLongAttribute_Getter");

  test_object_v8_internal::raisesExceptionLongAttributeAttributeGetter(info);
}

void V8TestObject::raisesExceptionLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::raisesExceptionLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::raisesExceptionGetterLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionGetterLongAttribute_Getter");

  test_object_v8_internal::raisesExceptionGetterLongAttributeAttributeGetter(info);
}

void V8TestObject::raisesExceptionGetterLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionGetterLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::raisesExceptionGetterLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::setterRaisesExceptionLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_setterRaisesExceptionLongAttribute_Getter");

  test_object_v8_internal::setterRaisesExceptionLongAttributeAttributeGetter(info);
}

void V8TestObject::setterRaisesExceptionLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_setterRaisesExceptionLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::setterRaisesExceptionLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::raisesExceptionTestInterfaceEmptyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionTestInterfaceEmptyAttribute_Getter");

  test_object_v8_internal::raisesExceptionTestInterfaceEmptyAttributeAttributeGetter(info);
}

void V8TestObject::raisesExceptionTestInterfaceEmptyAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionTestInterfaceEmptyAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::raisesExceptionTestInterfaceEmptyAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::cachedAttributeRaisesExceptionGetterAnyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_cachedAttributeRaisesExceptionGetterAnyAttribute_Getter");

  test_object_v8_internal::cachedAttributeRaisesExceptionGetterAnyAttributeAttributeGetter(info);
}

void V8TestObject::cachedAttributeRaisesExceptionGetterAnyAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_cachedAttributeRaisesExceptionGetterAnyAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::cachedAttributeRaisesExceptionGetterAnyAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::reflectTestInterfaceAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectTestInterfaceAttribute_Getter");

  test_object_v8_internal::reflectTestInterfaceAttributeAttributeGetter(info);
}

void V8TestObject::reflectTestInterfaceAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectTestInterfaceAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::reflectTestInterfaceAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::reflectReflectedNameAttributeTestAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectReflectedNameAttributeTestAttribute_Getter");

  test_object_v8_internal::reflectReflectedNameAttributeTestAttributeAttributeGetter(info);
}

void V8TestObject::reflectReflectedNameAttributeTestAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectReflectedNameAttributeTestAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::reflectReflectedNameAttributeTestAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::reflectBooleanAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectBooleanAttribute_Getter");

  test_object_v8_internal::reflectBooleanAttributeAttributeGetter(info);
}

void V8TestObject::reflectBooleanAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectBooleanAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::reflectBooleanAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::reflectLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectLongAttribute_Getter");

  test_object_v8_internal::reflectLongAttributeAttributeGetter(info);
}

void V8TestObject::reflectLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::reflectLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::reflectUnsignedShortAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectUnsignedShortAttribute_Getter");

  test_object_v8_internal::reflectUnsignedShortAttributeAttributeGetter(info);
}

void V8TestObject::reflectUnsignedShortAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectUnsignedShortAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::reflectUnsignedShortAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::reflectUnsignedLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectUnsignedLongAttribute_Getter");

  test_object_v8_internal::reflectUnsignedLongAttributeAttributeGetter(info);
}

void V8TestObject::reflectUnsignedLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectUnsignedLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::reflectUnsignedLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::idAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_id_Getter");

  test_object_v8_internal::idAttributeGetter(info);
}

void V8TestObject::idAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_id_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::idAttributeSetter(v8Value, info);
}

void V8TestObject::nameAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_name_Getter");

  test_object_v8_internal::nameAttributeGetter(info);
}

void V8TestObject::nameAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_name_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::nameAttributeSetter(v8Value, info);
}

void V8TestObject::classAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_class_Getter");

  test_object_v8_internal::classAttributeGetter(info);
}

void V8TestObject::classAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_class_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::classAttributeSetter(v8Value, info);
}

void V8TestObject::reflectedIdAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectedId_Getter");

  test_object_v8_internal::reflectedIdAttributeGetter(info);
}

void V8TestObject::reflectedIdAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectedId_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::reflectedIdAttributeSetter(v8Value, info);
}

void V8TestObject::reflectedNameAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectedName_Getter");

  test_object_v8_internal::reflectedNameAttributeGetter(info);
}

void V8TestObject::reflectedNameAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectedName_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::reflectedNameAttributeSetter(v8Value, info);
}

void V8TestObject::reflectedClassAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectedClass_Getter");

  test_object_v8_internal::reflectedClassAttributeGetter(info);
}

void V8TestObject::reflectedClassAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectedClass_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::reflectedClassAttributeSetter(v8Value, info);
}

void V8TestObject::limitedToOnlyOneAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedToOnlyOneAttribute_Getter");

  test_object_v8_internal::limitedToOnlyOneAttributeAttributeGetter(info);
}

void V8TestObject::limitedToOnlyOneAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedToOnlyOneAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::limitedToOnlyOneAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::limitedToOnlyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedToOnlyAttribute_Getter");

  test_object_v8_internal::limitedToOnlyAttributeAttributeGetter(info);
}

void V8TestObject::limitedToOnlyAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedToOnlyAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::limitedToOnlyAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::limitedToOnlyOtherAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedToOnlyOtherAttribute_Getter");

  test_object_v8_internal::limitedToOnlyOtherAttributeAttributeGetter(info);
}

void V8TestObject::limitedToOnlyOtherAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedToOnlyOtherAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::limitedToOnlyOtherAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::limitedWithMissingDefaultAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedWithMissingDefaultAttribute_Getter");

  test_object_v8_internal::limitedWithMissingDefaultAttributeAttributeGetter(info);
}

void V8TestObject::limitedWithMissingDefaultAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedWithMissingDefaultAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::limitedWithMissingDefaultAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::limitedWithInvalidMissingDefaultAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedWithInvalidMissingDefaultAttribute_Getter");

  test_object_v8_internal::limitedWithInvalidMissingDefaultAttributeAttributeGetter(info);
}

void V8TestObject::limitedWithInvalidMissingDefaultAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedWithInvalidMissingDefaultAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::limitedWithInvalidMissingDefaultAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::corsSettingAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_corsSettingAttribute_Getter");

  test_object_v8_internal::corsSettingAttributeAttributeGetter(info);
}

void V8TestObject::limitedWithEmptyMissingInvalidAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedWithEmptyMissingInvalidAttribute_Getter");

  test_object_v8_internal::limitedWithEmptyMissingInvalidAttributeAttributeGetter(info);
}

void V8TestObject::RuntimeCallStatsCounterAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE(info.GetIsolate(), RuntimeCallStats::CounterId::kRuntimeCallStatsCounterAttribute_Getter);

  test_object_v8_internal::RuntimeCallStatsCounterAttributeAttributeGetter(info);
}

void V8TestObject::RuntimeCallStatsCounterAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE(info.GetIsolate(), RuntimeCallStats::CounterId::kRuntimeCallStatsCounterAttribute_Setter);

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::RuntimeCallStatsCounterAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::RuntimeCallStatsCounterReadOnlyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE(info.GetIsolate(), RuntimeCallStats::CounterId::kRuntimeCallStatsCounterReadOnlyAttribute_Getter);

  test_object_v8_internal::RuntimeCallStatsCounterReadOnlyAttributeAttributeGetter(info);
}

void V8TestObject::replaceableReadonlyLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_replaceableReadonlyLongAttribute_Getter");

  test_object_v8_internal::replaceableReadonlyLongAttributeAttributeGetter(info);
}

void V8TestObject::replaceableReadonlyLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_replaceableReadonlyLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::replaceableReadonlyLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::locationPutForwardsAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationPutForwards_Getter");

  test_object_v8_internal::locationPutForwardsAttributeGetter(info);
}

void V8TestObject::locationPutForwardsAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationPutForwards_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::locationPutForwardsAttributeSetter(v8Value, info);
}

void V8TestObject::runtimeEnabledLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_runtimeEnabledLongAttribute_Getter");

  test_object_v8_internal::runtimeEnabledLongAttributeAttributeGetter(info);
}

void V8TestObject::runtimeEnabledLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_runtimeEnabledLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::runtimeEnabledLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::setterCallWithCurrentWindowAndEnteredWindowStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_setterCallWithCurrentWindowAndEnteredWindowStringAttribute_Getter");

  test_object_v8_internal::setterCallWithCurrentWindowAndEnteredWindowStringAttributeAttributeGetter(info);
}

void V8TestObject::setterCallWithCurrentWindowAndEnteredWindowStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_setterCallWithCurrentWindowAndEnteredWindowStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::setterCallWithCurrentWindowAndEnteredWindowStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::setterCallWithExecutionContextStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_setterCallWithExecutionContextStringAttribute_Getter");

  test_object_v8_internal::setterCallWithExecutionContextStringAttributeAttributeGetter(info);
}

void V8TestObject::setterCallWithExecutionContextStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_setterCallWithExecutionContextStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::setterCallWithExecutionContextStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::treatNullAsEmptyStringStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_treatNullAsEmptyStringStringAttribute_Getter");

  test_object_v8_internal::treatNullAsEmptyStringStringAttributeAttributeGetter(info);
}

void V8TestObject::treatNullAsEmptyStringStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_treatNullAsEmptyStringStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::treatNullAsEmptyStringStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::legacyInterfaceTypeCheckingFloatAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_legacyInterfaceTypeCheckingFloatAttribute_Getter");

  test_object_v8_internal::legacyInterfaceTypeCheckingFloatAttributeAttributeGetter(info);
}

void V8TestObject::legacyInterfaceTypeCheckingFloatAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_legacyInterfaceTypeCheckingFloatAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::legacyInterfaceTypeCheckingFloatAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::legacyInterfaceTypeCheckingTestInterfaceAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_legacyInterfaceTypeCheckingTestInterfaceAttribute_Getter");

  test_object_v8_internal::legacyInterfaceTypeCheckingTestInterfaceAttributeAttributeGetter(info);
}

void V8TestObject::legacyInterfaceTypeCheckingTestInterfaceAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_legacyInterfaceTypeCheckingTestInterfaceAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::legacyInterfaceTypeCheckingTestInterfaceAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::legacyInterfaceTypeCheckingTestInterfaceOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_legacyInterfaceTypeCheckingTestInterfaceOrNullAttribute_Getter");

  test_object_v8_internal::legacyInterfaceTypeCheckingTestInterfaceOrNullAttributeAttributeGetter(info);
}

void V8TestObject::legacyInterfaceTypeCheckingTestInterfaceOrNullAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_legacyInterfaceTypeCheckingTestInterfaceOrNullAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::legacyInterfaceTypeCheckingTestInterfaceOrNullAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::urlStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_urlStringAttribute_Getter");

  test_object_v8_internal::urlStringAttributeAttributeGetter(info);
}

void V8TestObject::urlStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_urlStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::urlStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::urlStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_urlStringAttribute_Getter");

  test_object_v8_internal::urlStringAttributeAttributeGetter(info);
}

void V8TestObject::urlStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_urlStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::urlStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::unforgeableLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unforgeableLongAttribute_Getter");

  test_object_v8_internal::unforgeableLongAttributeAttributeGetter(info);
}

void V8TestObject::unforgeableLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unforgeableLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::unforgeableLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::measuredLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_measuredLongAttribute_Getter");

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kV8TestObject_MeasuredLongAttribute_AttributeGetter);

  test_object_v8_internal::measuredLongAttributeAttributeGetter(info);
}

void V8TestObject::measuredLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_measuredLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kV8TestObject_MeasuredLongAttribute_AttributeSetter);

  test_object_v8_internal::measuredLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::sameObjectAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_sameObjectAttribute_Getter");

  test_object_v8_internal::sameObjectAttributeAttributeGetter(info);
}

void V8TestObject::saveSameObjectAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_saveSameObjectAttribute_Getter");

  test_object_v8_internal::saveSameObjectAttributeAttributeGetter(info);
}

void V8TestObject::staticSaveSameObjectAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_staticSaveSameObjectAttribute_Getter");

  test_object_v8_internal::staticSaveSameObjectAttributeAttributeGetter(info);
}

void V8TestObject::unscopableLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unscopableLongAttribute_Getter");

  test_object_v8_internal::unscopableLongAttributeAttributeGetter(info);
}

void V8TestObject::unscopableLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unscopableLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::unscopableLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::unscopableOriginTrialEnabledLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unscopableOriginTrialEnabledLongAttribute_Getter");

  test_object_v8_internal::unscopableOriginTrialEnabledLongAttributeAttributeGetter(info);
}

void V8TestObject::unscopableOriginTrialEnabledLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unscopableOriginTrialEnabledLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::unscopableOriginTrialEnabledLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::unscopableRuntimeEnabledLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unscopableRuntimeEnabledLongAttribute_Getter");

  test_object_v8_internal::unscopableRuntimeEnabledLongAttributeAttributeGetter(info);
}

void V8TestObject::unscopableRuntimeEnabledLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unscopableRuntimeEnabledLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::unscopableRuntimeEnabledLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::testInterfaceAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceAttribute_Getter");

  test_object_v8_internal::testInterfaceAttributeAttributeGetter(info);
}

void V8TestObject::testInterfaceAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::testInterfaceAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::sizeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_size_Getter");

  test_object_v8_internal::sizeAttributeGetter(info);
}

void V8TestObject::unscopableVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unscopableVoidMethod");

  test_object_v8_internal::unscopableVoidMethodMethod(info);
}

void V8TestObject::unscopableRuntimeEnabledVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unscopableRuntimeEnabledVoidMethod");

  test_object_v8_internal::unscopableRuntimeEnabledVoidMethodMethod(info);
}

void V8TestObject::voidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethod");

  test_object_v8_internal::voidMethodMethod(info);
}

void V8TestObject::staticVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_staticVoidMethod");

  test_object_v8_internal::staticVoidMethodMethod(info);
}

void V8TestObject::dateMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_dateMethod");

  test_object_v8_internal::dateMethodMethod(info);
}

void V8TestObject::stringMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringMethod");

  test_object_v8_internal::stringMethodMethod(info);
}

void V8TestObject::byteStringMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_byteStringMethod");

  test_object_v8_internal::byteStringMethodMethod(info);
}

void V8TestObject::usvStringMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_usvStringMethod");

  test_object_v8_internal::usvStringMethodMethod(info);
}

void V8TestObject::readonlyDOMTimeStampMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_readonlyDOMTimeStampMethod");

  test_object_v8_internal::readonlyDOMTimeStampMethodMethod(info);
}

void V8TestObject::booleanMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_booleanMethod");

  test_object_v8_internal::booleanMethodMethod(info);
}

void V8TestObject::byteMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_byteMethod");

  test_object_v8_internal::byteMethodMethod(info);
}

void V8TestObject::doubleMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_doubleMethod");

  test_object_v8_internal::doubleMethodMethod(info);
}

void V8TestObject::floatMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_floatMethod");

  test_object_v8_internal::floatMethodMethod(info);
}

void V8TestObject::longMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longMethod");

  test_object_v8_internal::longMethodMethod(info);
}

void V8TestObject::longLongMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longLongMethod");

  test_object_v8_internal::longLongMethodMethod(info);
}

void V8TestObject::octetMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_octetMethod");

  test_object_v8_internal::octetMethodMethod(info);
}

void V8TestObject::shortMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_shortMethod");

  test_object_v8_internal::shortMethodMethod(info);
}

void V8TestObject::unsignedLongMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unsignedLongMethod");

  test_object_v8_internal::unsignedLongMethodMethod(info);
}

void V8TestObject::unsignedLongLongMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unsignedLongLongMethod");

  test_object_v8_internal::unsignedLongLongMethodMethod(info);
}

void V8TestObject::unsignedShortMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unsignedShortMethod");

  test_object_v8_internal::unsignedShortMethodMethod(info);
}

void V8TestObject::voidMethodDateArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDateArg");

  test_object_v8_internal::voidMethodDateArgMethod(info);
}

void V8TestObject::voidMethodStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodStringArg");

  test_object_v8_internal::voidMethodStringArgMethod(info);
}

void V8TestObject::voidMethodByteStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodByteStringArg");

  test_object_v8_internal::voidMethodByteStringArgMethod(info);
}

void V8TestObject::voidMethodUSVStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodUSVStringArg");

  test_object_v8_internal::voidMethodUSVStringArgMethod(info);
}

void V8TestObject::voidMethodDOMTimeStampArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDOMTimeStampArg");

  test_object_v8_internal::voidMethodDOMTimeStampArgMethod(info);
}

void V8TestObject::voidMethodBooleanArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodBooleanArg");

  test_object_v8_internal::voidMethodBooleanArgMethod(info);
}

void V8TestObject::voidMethodByteArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodByteArg");

  test_object_v8_internal::voidMethodByteArgMethod(info);
}

void V8TestObject::voidMethodDoubleArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDoubleArg");

  test_object_v8_internal::voidMethodDoubleArgMethod(info);
}

void V8TestObject::voidMethodFloatArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodFloatArg");

  test_object_v8_internal::voidMethodFloatArgMethod(info);
}

void V8TestObject::voidMethodLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodLongArg");

  test_object_v8_internal::voidMethodLongArgMethod(info);
}

void V8TestObject::voidMethodLongLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodLongLongArg");

  test_object_v8_internal::voidMethodLongLongArgMethod(info);
}

void V8TestObject::voidMethodOctetArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodOctetArg");

  test_object_v8_internal::voidMethodOctetArgMethod(info);
}

void V8TestObject::voidMethodShortArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodShortArg");

  test_object_v8_internal::voidMethodShortArgMethod(info);
}

void V8TestObject::voidMethodUnsignedLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodUnsignedLongArg");

  test_object_v8_internal::voidMethodUnsignedLongArgMethod(info);
}

void V8TestObject::voidMethodUnsignedLongLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodUnsignedLongLongArg");

  test_object_v8_internal::voidMethodUnsignedLongLongArgMethod(info);
}

void V8TestObject::voidMethodUnsignedShortArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodUnsignedShortArg");

  test_object_v8_internal::voidMethodUnsignedShortArgMethod(info);
}

void V8TestObject::testInterfaceEmptyMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceEmptyMethod");

  test_object_v8_internal::testInterfaceEmptyMethodMethod(info);
}

void V8TestObject::voidMethodTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodTestInterfaceEmptyArg");

  test_object_v8_internal::voidMethodTestInterfaceEmptyArgMethod(info);
}

void V8TestObject::voidMethodLongArgTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodLongArgTestInterfaceEmptyArg");

  test_object_v8_internal::voidMethodLongArgTestInterfaceEmptyArgMethod(info);
}

void V8TestObject::anyMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_anyMethod");

  test_object_v8_internal::anyMethodMethod(info);
}

void V8TestObject::voidMethodEventTargetArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodEventTargetArg");

  test_object_v8_internal::voidMethodEventTargetArgMethod(info);
}

void V8TestObject::voidMethodAnyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodAnyArg");

  test_object_v8_internal::voidMethodAnyArgMethod(info);
}

void V8TestObject::voidMethodAttrArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodAttrArg");

  test_object_v8_internal::voidMethodAttrArgMethod(info);
}

void V8TestObject::voidMethodDocumentArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDocumentArg");

  test_object_v8_internal::voidMethodDocumentArgMethod(info);
}

void V8TestObject::voidMethodDocumentTypeArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDocumentTypeArg");

  test_object_v8_internal::voidMethodDocumentTypeArgMethod(info);
}

void V8TestObject::voidMethodElementArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodElementArg");

  test_object_v8_internal::voidMethodElementArgMethod(info);
}

void V8TestObject::voidMethodNodeArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodNodeArg");

  test_object_v8_internal::voidMethodNodeArgMethod(info);
}

void V8TestObject::arrayBufferMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_arrayBufferMethod");

  test_object_v8_internal::arrayBufferMethodMethod(info);
}

void V8TestObject::arrayBufferViewMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_arrayBufferViewMethod");

  test_object_v8_internal::arrayBufferViewMethodMethod(info);
}

void V8TestObject::arrayBufferViewMethodRaisesExceptionMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_arrayBufferViewMethodRaisesException");

  test_object_v8_internal::arrayBufferViewMethodRaisesExceptionMethod(info);
}

void V8TestObject::float32ArrayMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_float32ArrayMethod");

  test_object_v8_internal::float32ArrayMethodMethod(info);
}

void V8TestObject::int32ArrayMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_int32ArrayMethod");

  test_object_v8_internal::int32ArrayMethodMethod(info);
}

void V8TestObject::uint8ArrayMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_uint8ArrayMethod");

  test_object_v8_internal::uint8ArrayMethodMethod(info);
}

void V8TestObject::voidMethodArrayBufferArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodArrayBufferArg");

  test_object_v8_internal::voidMethodArrayBufferArgMethod(info);
}

void V8TestObject::voidMethodArrayBufferOrNullArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodArrayBufferOrNullArg");

  test_object_v8_internal::voidMethodArrayBufferOrNullArgMethod(info);
}

void V8TestObject::voidMethodArrayBufferViewArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodArrayBufferViewArg");

  test_object_v8_internal::voidMethodArrayBufferViewArgMethod(info);
}

void V8TestObject::voidMethodFlexibleArrayBufferViewArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodFlexibleArrayBufferViewArg");

  test_object_v8_internal::voidMethodFlexibleArrayBufferViewArgMethod(info);
}

void V8TestObject::voidMethodFlexibleArrayBufferViewTypedArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodFlexibleArrayBufferViewTypedArg");

  test_object_v8_internal::voidMethodFlexibleArrayBufferViewTypedArgMethod(info);
}

void V8TestObject::voidMethodFloat32ArrayArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodFloat32ArrayArg");

  test_object_v8_internal::voidMethodFloat32ArrayArgMethod(info);
}

void V8TestObject::voidMethodInt32ArrayArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodInt32ArrayArg");

  test_object_v8_internal::voidMethodInt32ArrayArgMethod(info);
}

void V8TestObject::voidMethodUint8ArrayArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodUint8ArrayArg");

  test_object_v8_internal::voidMethodUint8ArrayArgMethod(info);
}

void V8TestObject::voidMethodAllowSharedArrayBufferViewArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodAllowSharedArrayBufferViewArg");

  test_object_v8_internal::voidMethodAllowSharedArrayBufferViewArgMethod(info);
}

void V8TestObject::voidMethodAllowSharedUint8ArrayArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodAllowSharedUint8ArrayArg");

  test_object_v8_internal::voidMethodAllowSharedUint8ArrayArgMethod(info);
}

void V8TestObject::longSequenceMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longSequenceMethod");

  test_object_v8_internal::longSequenceMethodMethod(info);
}

void V8TestObject::stringSequenceMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringSequenceMethod");

  test_object_v8_internal::stringSequenceMethodMethod(info);
}

void V8TestObject::testInterfaceEmptySequenceMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceEmptySequenceMethod");

  test_object_v8_internal::testInterfaceEmptySequenceMethodMethod(info);
}

void V8TestObject::voidMethodSequenceLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodSequenceLongArg");

  test_object_v8_internal::voidMethodSequenceLongArgMethod(info);
}

void V8TestObject::voidMethodSequenceStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodSequenceStringArg");

  test_object_v8_internal::voidMethodSequenceStringArgMethod(info);
}

void V8TestObject::voidMethodSequenceTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodSequenceTestInterfaceEmptyArg");

  test_object_v8_internal::voidMethodSequenceTestInterfaceEmptyArgMethod(info);
}

void V8TestObject::voidMethodSequenceSequenceDOMStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodSequenceSequenceDOMStringArg");

  test_object_v8_internal::voidMethodSequenceSequenceDOMStringArgMethod(info);
}

void V8TestObject::voidMethodNullableSequenceLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodNullableSequenceLongArg");

  test_object_v8_internal::voidMethodNullableSequenceLongArgMethod(info);
}

void V8TestObject::longFrozenArrayMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longFrozenArrayMethod");

  test_object_v8_internal::longFrozenArrayMethodMethod(info);
}

void V8TestObject::voidMethodStringFrozenArrayMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodStringFrozenArrayMethod");

  test_object_v8_internal::voidMethodStringFrozenArrayMethodMethod(info);
}

void V8TestObject::voidMethodTestInterfaceEmptyFrozenArrayMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodTestInterfaceEmptyFrozenArrayMethod");

  test_object_v8_internal::voidMethodTestInterfaceEmptyFrozenArrayMethodMethod(info);
}

void V8TestObject::nullableLongMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nullableLongMethod");

  test_object_v8_internal::nullableLongMethodMethod(info);
}

void V8TestObject::nullableStringMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nullableStringMethod");

  test_object_v8_internal::nullableStringMethodMethod(info);
}

void V8TestObject::nullableTestInterfaceMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nullableTestInterfaceMethod");

  test_object_v8_internal::nullableTestInterfaceMethodMethod(info);
}

void V8TestObject::nullableLongSequenceMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nullableLongSequenceMethod");

  test_object_v8_internal::nullableLongSequenceMethodMethod(info);
}

void V8TestObject::booleanOrDOMStringOrUnrestrictedDoubleMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_booleanOrDOMStringOrUnrestrictedDoubleMethod");

  test_object_v8_internal::booleanOrDOMStringOrUnrestrictedDoubleMethodMethod(info);
}

void V8TestObject::testInterfaceOrLongMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceOrLongMethod");

  test_object_v8_internal::testInterfaceOrLongMethodMethod(info);
}

void V8TestObject::voidMethodDoubleOrDOMStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDoubleOrDOMStringArg");

  test_object_v8_internal::voidMethodDoubleOrDOMStringArgMethod(info);
}

void V8TestObject::voidMethodDoubleOrDOMStringOrNullArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDoubleOrDOMStringOrNullArg");

  test_object_v8_internal::voidMethodDoubleOrDOMStringOrNullArgMethod(info);
}

void V8TestObject::voidMethodDoubleOrNullOrDOMStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDoubleOrNullOrDOMStringArg");

  test_object_v8_internal::voidMethodDoubleOrNullOrDOMStringArgMethod(info);
}

void V8TestObject::voidMethodDOMStringOrArrayBufferOrArrayBufferViewArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDOMStringOrArrayBufferOrArrayBufferViewArg");

  test_object_v8_internal::voidMethodDOMStringOrArrayBufferOrArrayBufferViewArgMethod(info);
}

void V8TestObject::voidMethodArrayBufferOrArrayBufferViewOrDictionaryArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodArrayBufferOrArrayBufferViewOrDictionaryArg");

  test_object_v8_internal::voidMethodArrayBufferOrArrayBufferViewOrDictionaryArgMethod(info);
}

void V8TestObject::voidMethodBooleanOrElementSequenceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodBooleanOrElementSequenceArg");

  test_object_v8_internal::voidMethodBooleanOrElementSequenceArgMethod(info);
}

void V8TestObject::voidMethodDoubleOrLongOrBooleanSequenceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDoubleOrLongOrBooleanSequenceArg");

  test_object_v8_internal::voidMethodDoubleOrLongOrBooleanSequenceArgMethod(info);
}

void V8TestObject::voidMethodElementSequenceOrByteStringDoubleOrStringRecordMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodElementSequenceOrByteStringDoubleOrStringRecord");

  test_object_v8_internal::voidMethodElementSequenceOrByteStringDoubleOrStringRecordMethod(info);
}

void V8TestObject::voidMethodArrayOfDoubleOrDOMStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodArrayOfDoubleOrDOMStringArg");

  test_object_v8_internal::voidMethodArrayOfDoubleOrDOMStringArgMethod(info);
}

void V8TestObject::voidMethodTestInterfaceEmptyOrNullArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodTestInterfaceEmptyOrNullArg");

  test_object_v8_internal::voidMethodTestInterfaceEmptyOrNullArgMethod(info);
}

void V8TestObject::voidMethodTestCallbackInterfaceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodTestCallbackInterfaceArg");

  test_object_v8_internal::voidMethodTestCallbackInterfaceArgMethod(info);
}

void V8TestObject::voidMethodOptionalTestCallbackInterfaceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodOptionalTestCallbackInterfaceArg");

  test_object_v8_internal::voidMethodOptionalTestCallbackInterfaceArgMethod(info);
}

void V8TestObject::voidMethodTestCallbackInterfaceOrNullArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodTestCallbackInterfaceOrNullArg");

  test_object_v8_internal::voidMethodTestCallbackInterfaceOrNullArgMethod(info);
}

void V8TestObject::testEnumMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testEnumMethod");

  test_object_v8_internal::testEnumMethodMethod(info);
}

void V8TestObject::voidMethodTestEnumArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodTestEnumArg");

  test_object_v8_internal::voidMethodTestEnumArgMethod(info);
}

void V8TestObject::voidMethodTestMultipleEnumArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodTestMultipleEnumArg");

  test_object_v8_internal::voidMethodTestMultipleEnumArgMethod(info);
}

void V8TestObject::dictionaryMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_dictionaryMethod");

  test_object_v8_internal::dictionaryMethodMethod(info);
}

void V8TestObject::testDictionaryMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testDictionaryMethod");

  test_object_v8_internal::testDictionaryMethodMethod(info);
}

void V8TestObject::nullableTestDictionaryMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nullableTestDictionaryMethod");

  test_object_v8_internal::nullableTestDictionaryMethodMethod(info);
}

void V8TestObject::staticTestDictionaryMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_staticTestDictionaryMethod");

  test_object_v8_internal::staticTestDictionaryMethodMethod(info);
}

void V8TestObject::staticNullableTestDictionaryMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_staticNullableTestDictionaryMethod");

  test_object_v8_internal::staticNullableTestDictionaryMethodMethod(info);
}

void V8TestObject::passPermissiveDictionaryMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_passPermissiveDictionaryMethod");

  test_object_v8_internal::passPermissiveDictionaryMethodMethod(info);
}

void V8TestObject::nodeFilterMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nodeFilterMethod");

  test_object_v8_internal::nodeFilterMethodMethod(info);
}

void V8TestObject::promiseMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_promiseMethod");

  test_object_v8_internal::promiseMethodMethod(info);
}

void V8TestObject::promiseMethodWithoutExceptionStateMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_promiseMethodWithoutExceptionState");

  test_object_v8_internal::promiseMethodWithoutExceptionStateMethod(info);
}

void V8TestObject::serializedScriptValueMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_serializedScriptValueMethod");

  test_object_v8_internal::serializedScriptValueMethodMethod(info);
}

void V8TestObject::xPathNSResolverMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_xPathNSResolverMethod");

  test_object_v8_internal::xPathNSResolverMethodMethod(info);
}

void V8TestObject::voidMethodDictionaryArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDictionaryArg");

  test_object_v8_internal::voidMethodDictionaryArgMethod(info);
}

void V8TestObject::voidMethodNodeFilterArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodNodeFilterArg");

  test_object_v8_internal::voidMethodNodeFilterArgMethod(info);
}

void V8TestObject::voidMethodPromiseArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodPromiseArg");

  test_object_v8_internal::voidMethodPromiseArgMethod(info);
}

void V8TestObject::voidMethodSerializedScriptValueArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodSerializedScriptValueArg");

  test_object_v8_internal::voidMethodSerializedScriptValueArgMethod(info);
}

void V8TestObject::voidMethodXPathNSResolverArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodXPathNSResolverArg");

  test_object_v8_internal::voidMethodXPathNSResolverArgMethod(info);
}

void V8TestObject::voidMethodDictionarySequenceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDictionarySequenceArg");

  test_object_v8_internal::voidMethodDictionarySequenceArgMethod(info);
}

void V8TestObject::voidMethodStringArgLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodStringArgLongArg");

  test_object_v8_internal::voidMethodStringArgLongArgMethod(info);
}

void V8TestObject::voidMethodByteStringOrNullOptionalUSVStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodByteStringOrNullOptionalUSVStringArg");

  test_object_v8_internal::voidMethodByteStringOrNullOptionalUSVStringArgMethod(info);
}

void V8TestObject::voidMethodOptionalStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodOptionalStringArg");

  test_object_v8_internal::voidMethodOptionalStringArgMethod(info);
}

void V8TestObject::voidMethodOptionalTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodOptionalTestInterfaceEmptyArg");

  test_object_v8_internal::voidMethodOptionalTestInterfaceEmptyArgMethod(info);
}

void V8TestObject::voidMethodOptionalLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodOptionalLongArg");

  test_object_v8_internal::voidMethodOptionalLongArgMethod(info);
}

void V8TestObject::stringMethodOptionalLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringMethodOptionalLongArg");

  test_object_v8_internal::stringMethodOptionalLongArgMethod(info);
}

void V8TestObject::testInterfaceEmptyMethodOptionalLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceEmptyMethodOptionalLongArg");

  test_object_v8_internal::testInterfaceEmptyMethodOptionalLongArgMethod(info);
}

void V8TestObject::longMethodOptionalLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longMethodOptionalLongArg");

  test_object_v8_internal::longMethodOptionalLongArgMethod(info);
}

void V8TestObject::voidMethodLongArgOptionalLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodLongArgOptionalLongArg");

  test_object_v8_internal::voidMethodLongArgOptionalLongArgMethod(info);
}

void V8TestObject::voidMethodLongArgOptionalLongArgOptionalLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodLongArgOptionalLongArgOptionalLongArg");

  test_object_v8_internal::voidMethodLongArgOptionalLongArgOptionalLongArgMethod(info);
}

void V8TestObject::voidMethodLongArgOptionalTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodLongArgOptionalTestInterfaceEmptyArg");

  test_object_v8_internal::voidMethodLongArgOptionalTestInterfaceEmptyArgMethod(info);
}

void V8TestObject::voidMethodTestInterfaceEmptyArgOptionalLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodTestInterfaceEmptyArgOptionalLongArg");

  test_object_v8_internal::voidMethodTestInterfaceEmptyArgOptionalLongArgMethod(info);
}

void V8TestObject::voidMethodOptionalDictionaryArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodOptionalDictionaryArg");

  test_object_v8_internal::voidMethodOptionalDictionaryArgMethod(info);
}

void V8TestObject::voidMethodDefaultByteStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultByteStringArg");

  test_object_v8_internal::voidMethodDefaultByteStringArgMethod(info);
}

void V8TestObject::voidMethodDefaultStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultStringArg");

  test_object_v8_internal::voidMethodDefaultStringArgMethod(info);
}

void V8TestObject::voidMethodDefaultIntegerArgsMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultIntegerArgs");

  test_object_v8_internal::voidMethodDefaultIntegerArgsMethod(info);
}

void V8TestObject::voidMethodDefaultDoubleArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultDoubleArg");

  test_object_v8_internal::voidMethodDefaultDoubleArgMethod(info);
}

void V8TestObject::voidMethodDefaultTrueBooleanArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultTrueBooleanArg");

  test_object_v8_internal::voidMethodDefaultTrueBooleanArgMethod(info);
}

void V8TestObject::voidMethodDefaultFalseBooleanArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultFalseBooleanArg");

  test_object_v8_internal::voidMethodDefaultFalseBooleanArgMethod(info);
}

void V8TestObject::voidMethodDefaultNullableByteStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultNullableByteStringArg");

  test_object_v8_internal::voidMethodDefaultNullableByteStringArgMethod(info);
}

void V8TestObject::voidMethodDefaultNullableStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultNullableStringArg");

  test_object_v8_internal::voidMethodDefaultNullableStringArgMethod(info);
}

void V8TestObject::voidMethodDefaultNullableTestInterfaceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultNullableTestInterfaceArg");

  test_object_v8_internal::voidMethodDefaultNullableTestInterfaceArgMethod(info);
}

void V8TestObject::voidMethodDefaultDoubleOrStringArgsMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultDoubleOrStringArgs");

  test_object_v8_internal::voidMethodDefaultDoubleOrStringArgsMethod(info);
}

void V8TestObject::voidMethodDefaultStringSequenceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultStringSequenceArg");

  test_object_v8_internal::voidMethodDefaultStringSequenceArgMethod(info);
}

void V8TestObject::voidMethodVariadicStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodVariadicStringArg");

  test_object_v8_internal::voidMethodVariadicStringArgMethod(info);
}

void V8TestObject::voidMethodStringArgVariadicStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodStringArgVariadicStringArg");

  test_object_v8_internal::voidMethodStringArgVariadicStringArgMethod(info);
}

void V8TestObject::voidMethodVariadicTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodVariadicTestInterfaceEmptyArg");

  test_object_v8_internal::voidMethodVariadicTestInterfaceEmptyArgMethod(info);
}

void V8TestObject::voidMethodTestInterfaceEmptyArgVariadicTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodTestInterfaceEmptyArgVariadicTestInterfaceEmptyArg");

  test_object_v8_internal::voidMethodTestInterfaceEmptyArgVariadicTestInterfaceEmptyArgMethod(info);
}

void V8TestObject::overloadedMethodAMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodA");

  test_object_v8_internal::overloadedMethodAMethod(info);
}

void V8TestObject::overloadedMethodBMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodB");

  test_object_v8_internal::overloadedMethodBMethod(info);
}

void V8TestObject::overloadedMethodCMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodC");

  test_object_v8_internal::overloadedMethodCMethod(info);
}

void V8TestObject::overloadedMethodDMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodD");

  test_object_v8_internal::overloadedMethodDMethod(info);
}

void V8TestObject::overloadedMethodEMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodE");

  test_object_v8_internal::overloadedMethodEMethod(info);
}

void V8TestObject::overloadedMethodFMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodF");

  test_object_v8_internal::overloadedMethodFMethod(info);
}

void V8TestObject::overloadedMethodGMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodG");

  test_object_v8_internal::overloadedMethodGMethod(info);
}

void V8TestObject::overloadedMethodHMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodH");

  test_object_v8_internal::overloadedMethodHMethod(info);
}

void V8TestObject::overloadedMethodIMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodI");

  test_object_v8_internal::overloadedMethodIMethod(info);
}

void V8TestObject::overloadedMethodJMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodJ");

  test_object_v8_internal::overloadedMethodJMethod(info);
}

void V8TestObject::overloadedMethodKMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodK");

  test_object_v8_internal::overloadedMethodKMethod(info);
}

void V8TestObject::overloadedMethodLMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodL");

  test_object_v8_internal::overloadedMethodLMethod(info);
}

void V8TestObject::overloadedMethodNMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodN");

  test_object_v8_internal::overloadedMethodNMethod(info);
}

void V8TestObject::promiseOverloadMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_promiseOverloadMethod");

  test_object_v8_internal::promiseOverloadMethodMethod(info);
}

void V8TestObject::overloadedPerWorldBindingsMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedPerWorldBindingsMethod");

  test_object_v8_internal::overloadedPerWorldBindingsMethodMethod(info);
}

void V8TestObject::overloadedPerWorldBindingsMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedPerWorldBindingsMethod");

  test_object_v8_internal::overloadedPerWorldBindingsMethodMethodForMainWorld(info);
}

void V8TestObject::overloadedStaticMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedStaticMethod");

  test_object_v8_internal::overloadedStaticMethodMethod(info);
}

void V8TestObject::itemMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_item");

  test_object_v8_internal::itemMethod(info);
}

void V8TestObject::setItemMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_setItem");

  test_object_v8_internal::setItemMethod(info);
}

void V8TestObject::voidMethodClampUnsignedShortArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodClampUnsignedShortArg");

  test_object_v8_internal::voidMethodClampUnsignedShortArgMethod(info);
}

void V8TestObject::voidMethodClampUnsignedLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodClampUnsignedLongArg");

  test_object_v8_internal::voidMethodClampUnsignedLongArgMethod(info);
}

void V8TestObject::voidMethodDefaultUndefinedTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultUndefinedTestInterfaceEmptyArg");

  test_object_v8_internal::voidMethodDefaultUndefinedTestInterfaceEmptyArgMethod(info);
}

void V8TestObject::voidMethodDefaultUndefinedLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultUndefinedLongArg");

  test_object_v8_internal::voidMethodDefaultUndefinedLongArgMethod(info);
}

void V8TestObject::voidMethodDefaultUndefinedStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultUndefinedStringArg");

  test_object_v8_internal::voidMethodDefaultUndefinedStringArgMethod(info);
}

void V8TestObject::voidMethodEnforceRangeLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodEnforceRangeLongArg");

  test_object_v8_internal::voidMethodEnforceRangeLongArgMethod(info);
}

void V8TestObject::voidMethodTreatNullAsEmptyStringStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodTreatNullAsEmptyStringStringArg");

  test_object_v8_internal::voidMethodTreatNullAsEmptyStringStringArgMethod(info);
}

void V8TestObject::activityLoggingAccessForAllWorldsMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessForAllWorldsMethod");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogMethod("TestObject.activityLoggingAccessForAllWorldsMethod", info);
  }
  test_object_v8_internal::activityLoggingAccessForAllWorldsMethodMethod(info);
}

void V8TestObject::callWithExecutionContextVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithExecutionContextVoidMethod");

  test_object_v8_internal::callWithExecutionContextVoidMethodMethod(info);
}

void V8TestObject::callWithScriptStateVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithScriptStateVoidMethod");

  test_object_v8_internal::callWithScriptStateVoidMethodMethod(info);
}

void V8TestObject::callWithScriptStateLongMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithScriptStateLongMethod");

  test_object_v8_internal::callWithScriptStateLongMethodMethod(info);
}

void V8TestObject::callWithScriptStateExecutionContextVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithScriptStateExecutionContextVoidMethod");

  test_object_v8_internal::callWithScriptStateExecutionContextVoidMethodMethod(info);
}

void V8TestObject::callWithScriptStateScriptArgumentsVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithScriptStateScriptArgumentsVoidMethod");

  test_object_v8_internal::callWithScriptStateScriptArgumentsVoidMethodMethod(info);
}

void V8TestObject::callWithScriptStateScriptArgumentsVoidMethodOptionalBooleanArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithScriptStateScriptArgumentsVoidMethodOptionalBooleanArg");

  test_object_v8_internal::callWithScriptStateScriptArgumentsVoidMethodOptionalBooleanArgMethod(info);
}

void V8TestObject::callWithCurrentWindowMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithCurrentWindow");

  test_object_v8_internal::callWithCurrentWindowMethod(info);
}

void V8TestObject::callWithCurrentWindowScriptWindowMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithCurrentWindowScriptWindow");

  test_object_v8_internal::callWithCurrentWindowScriptWindowMethod(info);
}

void V8TestObject::callWithThisValueMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithThisValue");

  test_object_v8_internal::callWithThisValueMethod(info);
}

void V8TestObject::checkSecurityForNodeVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_checkSecurityForNodeVoidMethod");

  test_object_v8_internal::checkSecurityForNodeVoidMethodMethod(info);
}

void V8TestObject::customVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customVoidMethod");

  V8TestObject::customVoidMethodMethodCustom(info);
}

void V8TestObject::customCallPrologueVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customCallPrologueVoidMethod");

  test_object_v8_internal::customCallPrologueVoidMethodMethod(info);
}

void V8TestObject::customCallEpilogueVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customCallEpilogueVoidMethod");

  test_object_v8_internal::customCallEpilogueVoidMethodMethod(info);
}

void V8TestObject::deprecatedVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_deprecatedVoidMethod");

  Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kvoidMethod);
  test_object_v8_internal::deprecatedVoidMethodMethod(info);
}

void V8TestObject::implementedAsVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_implementedAsVoidMethod");

  test_object_v8_internal::implementedAsVoidMethodMethod(info);
}

void V8TestObject::measureAsVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_measureAsVoidMethod");

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeature);
  test_object_v8_internal::measureAsVoidMethodMethod(info);
}

void V8TestObject::measureMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_measureMethod");

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kV8TestObject_MeasureMethod_Method);
  test_object_v8_internal::measureMethodMethod(info);
}

void V8TestObject::measureOverloadedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_measureOverloadedMethod");

  test_object_v8_internal::measureOverloadedMethodMethod(info);
}

void V8TestObject::DeprecateAsOverloadedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_DeprecateAsOverloadedMethod");

  test_object_v8_internal::DeprecateAsOverloadedMethodMethod(info);
}

void V8TestObject::DeprecateAsSameValueOverloadedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_DeprecateAsSameValueOverloadedMethod");

  test_object_v8_internal::DeprecateAsSameValueOverloadedMethodMethod(info);
}

void V8TestObject::measureAsOverloadedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_measureAsOverloadedMethod");

  test_object_v8_internal::measureAsOverloadedMethodMethod(info);
}

void V8TestObject::measureAsSameValueOverloadedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_measureAsSameValueOverloadedMethod");

  test_object_v8_internal::measureAsSameValueOverloadedMethodMethod(info);
}

void V8TestObject::ceReactionsNotOverloadedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_ceReactionsNotOverloadedMethod");

  test_object_v8_internal::ceReactionsNotOverloadedMethodMethod(info);
}

void V8TestObject::ceReactionsOverloadedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_ceReactionsOverloadedMethod");

  test_object_v8_internal::ceReactionsOverloadedMethodMethod(info);
}

void V8TestObject::deprecateAsMeasureAsSameValueOverloadedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_deprecateAsMeasureAsSameValueOverloadedMethod");

  test_object_v8_internal::deprecateAsMeasureAsSameValueOverloadedMethodMethod(info);
}

void V8TestObject::deprecateAsSameValueMeasureAsOverloadedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_deprecateAsSameValueMeasureAsOverloadedMethod");

  test_object_v8_internal::deprecateAsSameValueMeasureAsOverloadedMethodMethod(info);
}

void V8TestObject::deprecateAsSameValueMeasureAsSameValueOverloadedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_deprecateAsSameValueMeasureAsSameValueOverloadedMethod");

  test_object_v8_internal::deprecateAsSameValueMeasureAsSameValueOverloadedMethodMethod(info);
}

void V8TestObject::notEnumerableVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_notEnumerableVoidMethod");

  test_object_v8_internal::notEnumerableVoidMethodMethod(info);
}

void V8TestObject::originTrialEnabledVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_originTrialEnabledVoidMethod");

  test_object_v8_internal::originTrialEnabledVoidMethodMethod(info);
}

void V8TestObject::perWorldBindingsOriginTrialEnabledVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_perWorldBindingsOriginTrialEnabledVoidMethod");

  test_object_v8_internal::perWorldBindingsOriginTrialEnabledVoidMethodMethod(info);
}

void V8TestObject::perWorldBindingsOriginTrialEnabledVoidMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_perWorldBindingsOriginTrialEnabledVoidMethod");

  test_object_v8_internal::perWorldBindingsOriginTrialEnabledVoidMethodMethodForMainWorld(info);
}

void V8TestObject::perWorldBindingsVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_perWorldBindingsVoidMethod");

  test_object_v8_internal::perWorldBindingsVoidMethodMethod(info);
}

void V8TestObject::perWorldBindingsVoidMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_perWorldBindingsVoidMethod");

  test_object_v8_internal::perWorldBindingsVoidMethodMethodForMainWorld(info);
}

void V8TestObject::perWorldBindingsVoidMethodTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_perWorldBindingsVoidMethodTestInterfaceEmptyArg");

  test_object_v8_internal::perWorldBindingsVoidMethodTestInterfaceEmptyArgMethod(info);
}

void V8TestObject::perWorldBindingsVoidMethodTestInterfaceEmptyArgMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_perWorldBindingsVoidMethodTestInterfaceEmptyArg");

  test_object_v8_internal::perWorldBindingsVoidMethodTestInterfaceEmptyArgMethodForMainWorld(info);
}

void V8TestObject::activityLoggingForAllWorldsPerWorldBindingsVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingForAllWorldsPerWorldBindingsVoidMethod");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogMethod("TestObject.activityLoggingForAllWorldsPerWorldBindingsVoidMethod", info);
  }
  test_object_v8_internal::activityLoggingForAllWorldsPerWorldBindingsVoidMethodMethod(info);
}

void V8TestObject::activityLoggingForAllWorldsPerWorldBindingsVoidMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingForAllWorldsPerWorldBindingsVoidMethod");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogMethod("TestObject.activityLoggingForAllWorldsPerWorldBindingsVoidMethod", info);
  }
  test_object_v8_internal::activityLoggingForAllWorldsPerWorldBindingsVoidMethodMethodForMainWorld(info);
}

void V8TestObject::activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethod");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogMethod("TestObject.activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethod", info);
  }
  test_object_v8_internal::activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethodMethod(info);
}

void V8TestObject::activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethod");

  test_object_v8_internal::activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethodMethodForMainWorld(info);
}

void V8TestObject::raisesExceptionVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionVoidMethod");

  test_object_v8_internal::raisesExceptionVoidMethodMethod(info);
}

void V8TestObject::raisesExceptionStringMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionStringMethod");

  test_object_v8_internal::raisesExceptionStringMethodMethod(info);
}

void V8TestObject::raisesExceptionVoidMethodOptionalLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionVoidMethodOptionalLongArg");

  test_object_v8_internal::raisesExceptionVoidMethodOptionalLongArgMethod(info);
}

void V8TestObject::raisesExceptionVoidMethodTestCallbackInterfaceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionVoidMethodTestCallbackInterfaceArg");

  test_object_v8_internal::raisesExceptionVoidMethodTestCallbackInterfaceArgMethod(info);
}

void V8TestObject::raisesExceptionVoidMethodOptionalTestCallbackInterfaceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionVoidMethodOptionalTestCallbackInterfaceArg");

  test_object_v8_internal::raisesExceptionVoidMethodOptionalTestCallbackInterfaceArgMethod(info);
}

void V8TestObject::raisesExceptionTestInterfaceEmptyVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionTestInterfaceEmptyVoidMethod");

  test_object_v8_internal::raisesExceptionTestInterfaceEmptyVoidMethodMethod(info);
}

void V8TestObject::raisesExceptionXPathNSResolverVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionXPathNSResolverVoidMethod");

  test_object_v8_internal::raisesExceptionXPathNSResolverVoidMethodMethod(info);
}

void V8TestObject::callWithExecutionContextRaisesExceptionVoidMethodLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithExecutionContextRaisesExceptionVoidMethodLongArg");

  test_object_v8_internal::callWithExecutionContextRaisesExceptionVoidMethodLongArgMethod(info);
}

void V8TestObject::runtimeEnabledVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_runtimeEnabledVoidMethod");

  test_object_v8_internal::runtimeEnabledVoidMethodMethod(info);
}

void V8TestObject::perWorldBindingsRuntimeEnabledVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_perWorldBindingsRuntimeEnabledVoidMethod");

  test_object_v8_internal::perWorldBindingsRuntimeEnabledVoidMethodMethod(info);
}

void V8TestObject::perWorldBindingsRuntimeEnabledVoidMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_perWorldBindingsRuntimeEnabledVoidMethod");

  test_object_v8_internal::perWorldBindingsRuntimeEnabledVoidMethodMethodForMainWorld(info);
}

void V8TestObject::runtimeEnabledOverloadedVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_runtimeEnabledOverloadedVoidMethod");

  test_object_v8_internal::runtimeEnabledOverloadedVoidMethodMethod(info);
}

void V8TestObject::partiallyRuntimeEnabledOverloadedVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_partiallyRuntimeEnabledOverloadedVoidMethod");

  test_object_v8_internal::partiallyRuntimeEnabledOverloadedVoidMethodMethod(info);
}

void V8TestObject::legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyArg");

  test_object_v8_internal::legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyArgMethod(info);
}

void V8TestObject::legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyVariadicArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyVariadicArg");

  test_object_v8_internal::legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyVariadicArgMethod(info);
}

void V8TestObject::useToImpl4ArgumentsCheckingIfPossibleWithOptionalArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_useToImpl4ArgumentsCheckingIfPossibleWithOptionalArg");

  test_object_v8_internal::useToImpl4ArgumentsCheckingIfPossibleWithOptionalArgMethod(info);
}

void V8TestObject::useToImpl4ArgumentsCheckingIfPossibleWithNullableArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_useToImpl4ArgumentsCheckingIfPossibleWithNullableArg");

  test_object_v8_internal::useToImpl4ArgumentsCheckingIfPossibleWithNullableArgMethod(info);
}

void V8TestObject::useToImpl4ArgumentsCheckingIfPossibleWithUndefinedArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_useToImpl4ArgumentsCheckingIfPossibleWithUndefinedArg");

  test_object_v8_internal::useToImpl4ArgumentsCheckingIfPossibleWithUndefinedArgMethod(info);
}

void V8TestObject::unforgeableVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unforgeableVoidMethod");

  test_object_v8_internal::unforgeableVoidMethodMethod(info);
}

void V8TestObject::newObjectTestInterfaceMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_newObjectTestInterfaceMethod");

  test_object_v8_internal::newObjectTestInterfaceMethodMethod(info);
}

void V8TestObject::newObjectTestInterfacePromiseMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_newObjectTestInterfacePromiseMethod");

  test_object_v8_internal::newObjectTestInterfacePromiseMethodMethod(info);
}

void V8TestObject::RuntimeCallStatsCounterMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE(info.GetIsolate(), RuntimeCallStats::CounterId::kRuntimeCallStatsCounterMethod);
  test_object_v8_internal::RuntimeCallStatsCounterMethodMethod(info);
}

void V8TestObject::clearMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_clear");

  test_object_v8_internal::clearMethod(info);
}

void V8TestObject::keysMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_keys");

  test_object_v8_internal::keysMethod(info);
}

void V8TestObject::valuesMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_values");

  test_object_v8_internal::valuesMethod(info);
}

void V8TestObject::forEachMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_forEach");

  test_object_v8_internal::forEachMethod(info);
}

void V8TestObject::hasMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_has");

  test_object_v8_internal::hasMethod(info);
}

void V8TestObject::getMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_get");

  test_object_v8_internal::getMethod(info);
}

void V8TestObject::deleteMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_delete");

  test_object_v8_internal::deleteMethod(info);
}

void V8TestObject::setMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_set");

  test_object_v8_internal::setMethod(info);
}

void V8TestObject::toJSONMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_toJSON");

  test_object_v8_internal::toJSONMethod(info);
}

void V8TestObject::toStringMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_toString");

  test_object_v8_internal::toStringMethod(info);
}

void V8TestObject::iteratorMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_iterator");

  test_object_v8_internal::iteratorMethod(info);
}

void V8TestObject::namedPropertyGetterCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_NamedPropertyGetter");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_object_v8_internal::namedPropertyGetter(propertyName, info);
}

void V8TestObject::namedPropertySetterCallback(v8::Local<v8::Name> name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_NamedPropertySetter");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_object_v8_internal::namedPropertySetter(propertyName, v8Value, info);
}

void V8TestObject::namedPropertyDeleterCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_object_v8_internal::namedPropertyDeleter(propertyName, info);
}

void V8TestObject::namedPropertyQueryCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_NamedPropertyQuery");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_object_v8_internal::namedPropertyQuery(propertyName, info);
}

void V8TestObject::namedPropertyEnumeratorCallback(const v8::PropertyCallbackInfo<v8::Array>& info) {
  test_object_v8_internal::namedPropertyEnumerator(info);
}

void V8TestObject::indexedPropertyGetterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_IndexedPropertyGetter");

  test_object_v8_internal::indexedPropertyGetter(index, info);
}

void V8TestObject::indexedPropertyDescriptorCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  test_object_v8_internal::indexedPropertyDescriptor(index, info);
}

void V8TestObject::indexedPropertySetterCallback(uint32_t index, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  test_object_v8_internal::indexedPropertySetter(index, v8Value, info);
}

void V8TestObject::indexedPropertyDeleterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  test_object_v8_internal::indexedPropertyDeleter(index, info);
}

void V8TestObject::indexedPropertyDefinerCallback(
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
                                    "TestObject");
      exceptionState.ThrowTypeError("Accessor properties are not allowed.");
    }
    return;
  }

  // Return nothing and fall back to indexedPropertySetterCallback.
}

// Suppress warning: global constructors, because AttributeConfiguration is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
static const V8DOMConfiguration::AttributeConfiguration V8TestObjectAttributes[] = {
    { "testInterfaceEmptyConstructorAttribute", V8TestObject::testInterfaceEmptyConstructorAttributeConstructorGetterCallback, nullptr, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kReplaceWithDataProperty, V8DOMConfiguration::kAllWorlds },
    { "testInterfaceEmptyConstructorAttribute", V8TestObject::testInterfaceEmptyConstructorAttributeConstructorGetterCallback, nullptr, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "measureAsFeatureNameTestInterfaceEmptyConstructorAttribute", V8TestObject::measureAsFeatureNameTestInterfaceEmptyConstructorAttributeConstructorGetterCallback, nullptr, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

static const V8DOMConfiguration::AccessorConfiguration V8TestObjectAccessors[] = {
    { "stringifierAttribute", V8TestObject::stringifierAttributeAttributeGetterCallback, V8TestObject::stringifierAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "readonlyStringAttribute", V8TestObject::readonlyStringAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "readonlyTestInterfaceEmptyAttribute", V8TestObject::readonlyTestInterfaceEmptyAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "readonlyLongAttribute", V8TestObject::readonlyLongAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "dateAttribute", V8TestObject::dateAttributeAttributeGetterCallback, V8TestObject::dateAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "stringAttribute", V8TestObject::stringAttributeAttributeGetterCallback, V8TestObject::stringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "byteStringAttribute", V8TestObject::byteStringAttributeAttributeGetterCallback, V8TestObject::byteStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "usvStringAttribute", V8TestObject::usvStringAttributeAttributeGetterCallback, V8TestObject::usvStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "domTimeStampAttribute", V8TestObject::domTimeStampAttributeAttributeGetterCallback, V8TestObject::domTimeStampAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "booleanAttribute", V8TestObject::booleanAttributeAttributeGetterCallback, V8TestObject::booleanAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "byteAttribute", V8TestObject::byteAttributeAttributeGetterCallback, V8TestObject::byteAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "doubleAttribute", V8TestObject::doubleAttributeAttributeGetterCallback, V8TestObject::doubleAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "floatAttribute", V8TestObject::floatAttributeAttributeGetterCallback, V8TestObject::floatAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "longAttribute", V8TestObject::longAttributeAttributeGetterCallback, V8TestObject::longAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "longLongAttribute", V8TestObject::longLongAttributeAttributeGetterCallback, V8TestObject::longLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "octetAttribute", V8TestObject::octetAttributeAttributeGetterCallback, V8TestObject::octetAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "shortAttribute", V8TestObject::shortAttributeAttributeGetterCallback, V8TestObject::shortAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "unrestrictedDoubleAttribute", V8TestObject::unrestrictedDoubleAttributeAttributeGetterCallback, V8TestObject::unrestrictedDoubleAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "unrestrictedFloatAttribute", V8TestObject::unrestrictedFloatAttributeAttributeGetterCallback, V8TestObject::unrestrictedFloatAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "unsignedLongAttribute", V8TestObject::unsignedLongAttributeAttributeGetterCallback, V8TestObject::unsignedLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "unsignedLongLongAttribute", V8TestObject::unsignedLongLongAttributeAttributeGetterCallback, V8TestObject::unsignedLongLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "unsignedShortAttribute", V8TestObject::unsignedShortAttributeAttributeGetterCallback, V8TestObject::unsignedShortAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "testInterfaceEmptyAttribute", V8TestObject::testInterfaceEmptyAttributeAttributeGetterCallback, V8TestObject::testInterfaceEmptyAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "testObjectAttribute", V8TestObject::testObjectAttributeAttributeGetterCallback, V8TestObject::testObjectAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "cssAttribute", V8TestObject::cssAttributeAttributeGetterCallback, V8TestObject::cssAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "imeAttribute", V8TestObject::imeAttributeAttributeGetterCallback, V8TestObject::imeAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "svgAttribute", V8TestObject::svgAttributeAttributeGetterCallback, V8TestObject::svgAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "xmlAttribute", V8TestObject::xmlAttributeAttributeGetterCallback, V8TestObject::xmlAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "nodeFilterAttribute", V8TestObject::nodeFilterAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "serializedScriptValueAttribute", V8TestObject::serializedScriptValueAttributeAttributeGetterCallback, V8TestObject::serializedScriptValueAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "anyAttribute", V8TestObject::anyAttributeAttributeGetterCallback, V8TestObject::anyAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "promiseAttribute", V8TestObject::promiseAttributeAttributeGetterCallback, V8TestObject::promiseAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kDoNotCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "windowAttribute", V8TestObject::windowAttributeAttributeGetterCallback, V8TestObject::windowAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "documentAttribute", V8TestObject::documentAttributeAttributeGetterCallback, V8TestObject::documentAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "documentFragmentAttribute", V8TestObject::documentFragmentAttributeAttributeGetterCallback, V8TestObject::documentFragmentAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "documentTypeAttribute", V8TestObject::documentTypeAttributeAttributeGetterCallback, V8TestObject::documentTypeAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "elementAttribute", V8TestObject::elementAttributeAttributeGetterCallback, V8TestObject::elementAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "nodeAttribute", V8TestObject::nodeAttributeAttributeGetterCallback, V8TestObject::nodeAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "shadowRootAttribute", V8TestObject::shadowRootAttributeAttributeGetterCallback, V8TestObject::shadowRootAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "arrayBufferAttribute", V8TestObject::arrayBufferAttributeAttributeGetterCallback, V8TestObject::arrayBufferAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "float32ArrayAttribute", V8TestObject::float32ArrayAttributeAttributeGetterCallback, V8TestObject::float32ArrayAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "uint8ArrayAttribute", V8TestObject::uint8ArrayAttributeAttributeGetterCallback, V8TestObject::uint8ArrayAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "self", V8TestObject::selfAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "readonlyEventTargetAttribute", V8TestObject::readonlyEventTargetAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "readonlyEventTargetOrNullAttribute", V8TestObject::readonlyEventTargetOrNullAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "readonlyWindowAttribute", V8TestObject::readonlyWindowAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "htmlCollectionAttribute", V8TestObject::htmlCollectionAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "htmlElementAttribute", V8TestObject::htmlElementAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "stringFrozenArrayAttribute", V8TestObject::stringFrozenArrayAttributeAttributeGetterCallback, V8TestObject::stringFrozenArrayAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "testInterfaceEmptyFrozenArrayAttribute", V8TestObject::testInterfaceEmptyFrozenArrayAttributeAttributeGetterCallback, V8TestObject::testInterfaceEmptyFrozenArrayAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "booleanOrNullAttribute", V8TestObject::booleanOrNullAttributeAttributeGetterCallback, V8TestObject::booleanOrNullAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "stringOrNullAttribute", V8TestObject::stringOrNullAttributeAttributeGetterCallback, V8TestObject::stringOrNullAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "longOrNullAttribute", V8TestObject::longOrNullAttributeAttributeGetterCallback, V8TestObject::longOrNullAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "testInterfaceOrNullAttribute", V8TestObject::testInterfaceOrNullAttributeAttributeGetterCallback, V8TestObject::testInterfaceOrNullAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "testEnumAttribute", V8TestObject::testEnumAttributeAttributeGetterCallback, V8TestObject::testEnumAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "testEnumOrNullAttribute", V8TestObject::testEnumOrNullAttributeAttributeGetterCallback, V8TestObject::testEnumOrNullAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "staticStringAttribute", V8TestObject::staticStringAttributeAttributeGetterCallback, V8TestObject::staticStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "staticLongAttribute", V8TestObject::staticLongAttributeAttributeGetterCallback, V8TestObject::staticLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "eventHandlerAttribute", V8TestObject::eventHandlerAttributeAttributeGetterCallback, V8TestObject::eventHandlerAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "doubleOrStringAttribute", V8TestObject::doubleOrStringAttributeAttributeGetterCallback, V8TestObject::doubleOrStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "doubleOrStringOrNullAttribute", V8TestObject::doubleOrStringOrNullAttributeAttributeGetterCallback, V8TestObject::doubleOrStringOrNullAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "doubleOrNullStringAttribute", V8TestObject::doubleOrNullStringAttributeAttributeGetterCallback, V8TestObject::doubleOrNullStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "stringOrStringSequenceAttribute", V8TestObject::stringOrStringSequenceAttributeAttributeGetterCallback, V8TestObject::stringOrStringSequenceAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "testEnumOrDoubleAttribute", V8TestObject::testEnumOrDoubleAttributeAttributeGetterCallback, V8TestObject::testEnumOrDoubleAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "unrestrictedDoubleOrStringAttribute", V8TestObject::unrestrictedDoubleOrStringAttributeAttributeGetterCallback, V8TestObject::unrestrictedDoubleOrStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "nestedUnionAtribute", V8TestObject::nestedUnionAtributeAttributeGetterCallback, V8TestObject::nestedUnionAtributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "activityLoggingAccessForAllWorldsLongAttribute", V8TestObject::activityLoggingAccessForAllWorldsLongAttributeAttributeGetterCallback, V8TestObject::activityLoggingAccessForAllWorldsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "activityLoggingGetterForAllWorldsLongAttribute", V8TestObject::activityLoggingGetterForAllWorldsLongAttributeAttributeGetterCallback, V8TestObject::activityLoggingGetterForAllWorldsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "activityLoggingSetterForAllWorldsLongAttribute", V8TestObject::activityLoggingSetterForAllWorldsLongAttributeAttributeGetterCallback, V8TestObject::activityLoggingSetterForAllWorldsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "cachedAttributeAnyAttribute", V8TestObject::cachedAttributeAnyAttributeAttributeGetterCallback, V8TestObject::cachedAttributeAnyAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "cachedArrayAttribute", V8TestObject::cachedArrayAttributeAttributeGetterCallback, V8TestObject::cachedArrayAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "cachedStringOrNoneAttribute", V8TestObject::cachedStringOrNoneAttributeAttributeGetterCallback, V8TestObject::cachedStringOrNoneAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "callWithExecutionContextAnyAttribute", V8TestObject::callWithExecutionContextAnyAttributeAttributeGetterCallback, V8TestObject::callWithExecutionContextAnyAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "callWithScriptStateAnyAttribute", V8TestObject::callWithScriptStateAnyAttributeAttributeGetterCallback, V8TestObject::callWithScriptStateAnyAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "callWithExecutionContextAndScriptStateAnyAttribute", V8TestObject::callWithExecutionContextAndScriptStateAnyAttributeAttributeGetterCallback, V8TestObject::callWithExecutionContextAndScriptStateAnyAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "checkSecurityForNodeReadonlyDocumentAttribute", V8TestObject::checkSecurityForNodeReadonlyDocumentAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "customObjectAttribute", V8TestObject::customObjectAttributeAttributeGetterCallback, V8TestObject::customObjectAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "customGetterLongAttribute", V8TestObject::customGetterLongAttributeAttributeGetterCallback, V8TestObject::customGetterLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "customGetterReadonlyObjectAttribute", V8TestObject::customGetterReadonlyObjectAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "customSetterLongAttribute", V8TestObject::customSetterLongAttributeAttributeGetterCallback, V8TestObject::customSetterLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "deprecatedLongAttribute", V8TestObject::deprecatedLongAttributeAttributeGetterCallback, V8TestObject::deprecatedLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "enforceRangeLongAttribute", V8TestObject::enforceRangeLongAttributeAttributeGetterCallback, V8TestObject::enforceRangeLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "implementedAsLongAttribute", V8TestObject::implementedAsLongAttributeAttributeGetterCallback, V8TestObject::implementedAsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "customImplementedAsLongAttribute", V8TestObject::customImplementedAsLongAttributeAttributeGetterCallback, V8TestObject::customImplementedAsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "customGetterImplementedAsLongAttribute", V8TestObject::customGetterImplementedAsLongAttributeAttributeGetterCallback, V8TestObject::customGetterImplementedAsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "customSetterImplementedAsLongAttribute", V8TestObject::customSetterImplementedAsLongAttributeAttributeGetterCallback, V8TestObject::customSetterImplementedAsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "measureAsLongAttribute", V8TestObject::measureAsLongAttributeAttributeGetterCallback, V8TestObject::measureAsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "notEnumerableLongAttribute", V8TestObject::notEnumerableLongAttributeAttributeGetterCallback, V8TestObject::notEnumerableLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "perWorldBindingsReadonlyTestInterfaceEmptyAttribute", V8TestObject::perWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterCallbackForMainWorld, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kMainWorld },
    { "perWorldBindingsReadonlyTestInterfaceEmptyAttribute", V8TestObject::perWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kNonMainWorlds },
    { "activityLoggingAccessPerWorldBindingsLongAttribute", V8TestObject::activityLoggingAccessPerWorldBindingsLongAttributeAttributeGetterCallbackForMainWorld, V8TestObject::activityLoggingAccessPerWorldBindingsLongAttributeAttributeSetterCallbackForMainWorld, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kMainWorld },
    { "activityLoggingAccessPerWorldBindingsLongAttribute", V8TestObject::activityLoggingAccessPerWorldBindingsLongAttributeAttributeGetterCallback, V8TestObject::activityLoggingAccessPerWorldBindingsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kNonMainWorlds },
    { "activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute", V8TestObject::activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterCallbackForMainWorld, V8TestObject::activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterCallbackForMainWorld, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kMainWorld },
    { "activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute", V8TestObject::activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterCallback, V8TestObject::activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kNonMainWorlds },
    { "activityLoggingGetterPerWorldBindingsLongAttribute", V8TestObject::activityLoggingGetterPerWorldBindingsLongAttributeAttributeGetterCallbackForMainWorld, V8TestObject::activityLoggingGetterPerWorldBindingsLongAttributeAttributeSetterCallbackForMainWorld, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kMainWorld },
    { "activityLoggingGetterPerWorldBindingsLongAttribute", V8TestObject::activityLoggingGetterPerWorldBindingsLongAttributeAttributeGetterCallback, V8TestObject::activityLoggingGetterPerWorldBindingsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kNonMainWorlds },
    { "activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute", V8TestObject::activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterCallbackForMainWorld, V8TestObject::activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterCallbackForMainWorld, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kMainWorld },
    { "activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute", V8TestObject::activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterCallback, V8TestObject::activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kNonMainWorlds },
    { "location", V8TestObject::locationAttributeGetterCallback, V8TestObject::locationAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "locationWithException", V8TestObject::locationWithExceptionAttributeGetterCallback, V8TestObject::locationWithExceptionAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "locationWithCallWith", V8TestObject::locationWithCallWithAttributeGetterCallback, V8TestObject::locationWithCallWithAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "locationByteString", V8TestObject::locationByteStringAttributeGetterCallback, V8TestObject::locationByteStringAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "locationWithPerWorldBindings", V8TestObject::locationWithPerWorldBindingsAttributeGetterCallbackForMainWorld, V8TestObject::locationWithPerWorldBindingsAttributeSetterCallbackForMainWorld, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kMainWorld },
    { "locationWithPerWorldBindings", V8TestObject::locationWithPerWorldBindingsAttributeGetterCallback, V8TestObject::locationWithPerWorldBindingsAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kNonMainWorlds },
    { "locationLegacyInterfaceTypeChecking", V8TestObject::locationLegacyInterfaceTypeCheckingAttributeGetterCallback, V8TestObject::locationLegacyInterfaceTypeCheckingAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "raisesExceptionLongAttribute", V8TestObject::raisesExceptionLongAttributeAttributeGetterCallback, V8TestObject::raisesExceptionLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "raisesExceptionGetterLongAttribute", V8TestObject::raisesExceptionGetterLongAttributeAttributeGetterCallback, V8TestObject::raisesExceptionGetterLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "setterRaisesExceptionLongAttribute", V8TestObject::setterRaisesExceptionLongAttributeAttributeGetterCallback, V8TestObject::setterRaisesExceptionLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "raisesExceptionTestInterfaceEmptyAttribute", V8TestObject::raisesExceptionTestInterfaceEmptyAttributeAttributeGetterCallback, V8TestObject::raisesExceptionTestInterfaceEmptyAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "cachedAttributeRaisesExceptionGetterAnyAttribute", V8TestObject::cachedAttributeRaisesExceptionGetterAnyAttributeAttributeGetterCallback, V8TestObject::cachedAttributeRaisesExceptionGetterAnyAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "reflectTestInterfaceAttribute", V8TestObject::reflectTestInterfaceAttributeAttributeGetterCallback, V8TestObject::reflectTestInterfaceAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "reflectReflectedNameAttributeTestAttribute", V8TestObject::reflectReflectedNameAttributeTestAttributeAttributeGetterCallback, V8TestObject::reflectReflectedNameAttributeTestAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "reflectBooleanAttribute", V8TestObject::reflectBooleanAttributeAttributeGetterCallback, V8TestObject::reflectBooleanAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "reflectLongAttribute", V8TestObject::reflectLongAttributeAttributeGetterCallback, V8TestObject::reflectLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "reflectUnsignedShortAttribute", V8TestObject::reflectUnsignedShortAttributeAttributeGetterCallback, V8TestObject::reflectUnsignedShortAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "reflectUnsignedLongAttribute", V8TestObject::reflectUnsignedLongAttributeAttributeGetterCallback, V8TestObject::reflectUnsignedLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "id", V8TestObject::idAttributeGetterCallback, V8TestObject::idAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "name", V8TestObject::nameAttributeGetterCallback, V8TestObject::nameAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "class", V8TestObject::classAttributeGetterCallback, V8TestObject::classAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "reflectedId", V8TestObject::reflectedIdAttributeGetterCallback, V8TestObject::reflectedIdAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "reflectedName", V8TestObject::reflectedNameAttributeGetterCallback, V8TestObject::reflectedNameAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "reflectedClass", V8TestObject::reflectedClassAttributeGetterCallback, V8TestObject::reflectedClassAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "limitedToOnlyOneAttribute", V8TestObject::limitedToOnlyOneAttributeAttributeGetterCallback, V8TestObject::limitedToOnlyOneAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "limitedToOnlyAttribute", V8TestObject::limitedToOnlyAttributeAttributeGetterCallback, V8TestObject::limitedToOnlyAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "limitedToOnlyOtherAttribute", V8TestObject::limitedToOnlyOtherAttributeAttributeGetterCallback, V8TestObject::limitedToOnlyOtherAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "limitedWithMissingDefaultAttribute", V8TestObject::limitedWithMissingDefaultAttributeAttributeGetterCallback, V8TestObject::limitedWithMissingDefaultAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "limitedWithInvalidMissingDefaultAttribute", V8TestObject::limitedWithInvalidMissingDefaultAttributeAttributeGetterCallback, V8TestObject::limitedWithInvalidMissingDefaultAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "corsSettingAttribute", V8TestObject::corsSettingAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "limitedWithEmptyMissingInvalidAttribute", V8TestObject::limitedWithEmptyMissingInvalidAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "RuntimeCallStatsCounterAttribute", V8TestObject::RuntimeCallStatsCounterAttributeAttributeGetterCallback, V8TestObject::RuntimeCallStatsCounterAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "RuntimeCallStatsCounterReadOnlyAttribute", V8TestObject::RuntimeCallStatsCounterReadOnlyAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "replaceableReadonlyLongAttribute", V8TestObject::replaceableReadonlyLongAttributeAttributeGetterCallback, V8TestObject::replaceableReadonlyLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "locationPutForwards", V8TestObject::locationPutForwardsAttributeGetterCallback, V8TestObject::locationPutForwardsAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "setterCallWithCurrentWindowAndEnteredWindowStringAttribute", V8TestObject::setterCallWithCurrentWindowAndEnteredWindowStringAttributeAttributeGetterCallback, V8TestObject::setterCallWithCurrentWindowAndEnteredWindowStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "setterCallWithExecutionContextStringAttribute", V8TestObject::setterCallWithExecutionContextStringAttributeAttributeGetterCallback, V8TestObject::setterCallWithExecutionContextStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "treatNullAsEmptyStringStringAttribute", V8TestObject::treatNullAsEmptyStringStringAttributeAttributeGetterCallback, V8TestObject::treatNullAsEmptyStringStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "legacyInterfaceTypeCheckingFloatAttribute", V8TestObject::legacyInterfaceTypeCheckingFloatAttributeAttributeGetterCallback, V8TestObject::legacyInterfaceTypeCheckingFloatAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "legacyInterfaceTypeCheckingTestInterfaceAttribute", V8TestObject::legacyInterfaceTypeCheckingTestInterfaceAttributeAttributeGetterCallback, V8TestObject::legacyInterfaceTypeCheckingTestInterfaceAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "legacyInterfaceTypeCheckingTestInterfaceOrNullAttribute", V8TestObject::legacyInterfaceTypeCheckingTestInterfaceOrNullAttributeAttributeGetterCallback, V8TestObject::legacyInterfaceTypeCheckingTestInterfaceOrNullAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "urlStringAttribute", V8TestObject::urlStringAttributeAttributeGetterCallback, V8TestObject::urlStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "urlStringAttribute", V8TestObject::urlStringAttributeAttributeGetterCallback, V8TestObject::urlStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "unforgeableLongAttribute", V8TestObject::unforgeableLongAttributeAttributeGetterCallback, V8TestObject::unforgeableLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::DontDelete), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "measuredLongAttribute", V8TestObject::measuredLongAttributeAttributeGetterCallback, V8TestObject::measuredLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "sameObjectAttribute", V8TestObject::sameObjectAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "saveSameObjectAttribute", V8TestObject::saveSameObjectAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "staticSaveSameObjectAttribute", V8TestObject::staticSaveSameObjectAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "unscopableLongAttribute", V8TestObject::unscopableLongAttributeAttributeGetterCallback, V8TestObject::unscopableLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "testInterfaceAttribute", V8TestObject::testInterfaceAttributeAttributeGetterCallback, V8TestObject::testInterfaceAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "size", V8TestObject::sizeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::DontEnum | v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
};

static const V8DOMConfiguration::MethodConfiguration V8TestObjectMethods[] = {
    {"unscopableVoidMethod", V8TestObject::unscopableVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethod", V8TestObject::voidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"staticVoidMethod", V8TestObject::staticVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"dateMethod", V8TestObject::dateMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"stringMethod", V8TestObject::stringMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"byteStringMethod", V8TestObject::byteStringMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"usvStringMethod", V8TestObject::usvStringMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"readonlyDOMTimeStampMethod", V8TestObject::readonlyDOMTimeStampMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"booleanMethod", V8TestObject::booleanMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"byteMethod", V8TestObject::byteMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"doubleMethod", V8TestObject::doubleMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"floatMethod", V8TestObject::floatMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"longMethod", V8TestObject::longMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"longLongMethod", V8TestObject::longLongMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"octetMethod", V8TestObject::octetMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"shortMethod", V8TestObject::shortMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"unsignedLongMethod", V8TestObject::unsignedLongMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"unsignedLongLongMethod", V8TestObject::unsignedLongLongMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"unsignedShortMethod", V8TestObject::unsignedShortMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDateArg", V8TestObject::voidMethodDateArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodStringArg", V8TestObject::voidMethodStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodByteStringArg", V8TestObject::voidMethodByteStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodUSVStringArg", V8TestObject::voidMethodUSVStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDOMTimeStampArg", V8TestObject::voidMethodDOMTimeStampArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodBooleanArg", V8TestObject::voidMethodBooleanArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodByteArg", V8TestObject::voidMethodByteArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDoubleArg", V8TestObject::voidMethodDoubleArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodFloatArg", V8TestObject::voidMethodFloatArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodLongArg", V8TestObject::voidMethodLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodLongLongArg", V8TestObject::voidMethodLongLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodOctetArg", V8TestObject::voidMethodOctetArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodShortArg", V8TestObject::voidMethodShortArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodUnsignedLongArg", V8TestObject::voidMethodUnsignedLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodUnsignedLongLongArg", V8TestObject::voidMethodUnsignedLongLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodUnsignedShortArg", V8TestObject::voidMethodUnsignedShortArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"testInterfaceEmptyMethod", V8TestObject::testInterfaceEmptyMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestInterfaceEmptyArg", V8TestObject::voidMethodTestInterfaceEmptyArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodLongArgTestInterfaceEmptyArg", V8TestObject::voidMethodLongArgTestInterfaceEmptyArgMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"anyMethod", V8TestObject::anyMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodEventTargetArg", V8TestObject::voidMethodEventTargetArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodAnyArg", V8TestObject::voidMethodAnyArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodAttrArg", V8TestObject::voidMethodAttrArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDocumentArg", V8TestObject::voidMethodDocumentArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDocumentTypeArg", V8TestObject::voidMethodDocumentTypeArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodElementArg", V8TestObject::voidMethodElementArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodNodeArg", V8TestObject::voidMethodNodeArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"arrayBufferMethod", V8TestObject::arrayBufferMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"arrayBufferViewMethod", V8TestObject::arrayBufferViewMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"arrayBufferViewMethodRaisesException", V8TestObject::arrayBufferViewMethodRaisesExceptionMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"float32ArrayMethod", V8TestObject::float32ArrayMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"int32ArrayMethod", V8TestObject::int32ArrayMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"uint8ArrayMethod", V8TestObject::uint8ArrayMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodArrayBufferArg", V8TestObject::voidMethodArrayBufferArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodArrayBufferOrNullArg", V8TestObject::voidMethodArrayBufferOrNullArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodArrayBufferViewArg", V8TestObject::voidMethodArrayBufferViewArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodFlexibleArrayBufferViewArg", V8TestObject::voidMethodFlexibleArrayBufferViewArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodFlexibleArrayBufferViewTypedArg", V8TestObject::voidMethodFlexibleArrayBufferViewTypedArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodFloat32ArrayArg", V8TestObject::voidMethodFloat32ArrayArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodInt32ArrayArg", V8TestObject::voidMethodInt32ArrayArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodUint8ArrayArg", V8TestObject::voidMethodUint8ArrayArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodAllowSharedArrayBufferViewArg", V8TestObject::voidMethodAllowSharedArrayBufferViewArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodAllowSharedUint8ArrayArg", V8TestObject::voidMethodAllowSharedUint8ArrayArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"longSequenceMethod", V8TestObject::longSequenceMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"stringSequenceMethod", V8TestObject::stringSequenceMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"testInterfaceEmptySequenceMethod", V8TestObject::testInterfaceEmptySequenceMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodSequenceLongArg", V8TestObject::voidMethodSequenceLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodSequenceStringArg", V8TestObject::voidMethodSequenceStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodSequenceTestInterfaceEmptyArg", V8TestObject::voidMethodSequenceTestInterfaceEmptyArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodSequenceSequenceDOMStringArg", V8TestObject::voidMethodSequenceSequenceDOMStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodNullableSequenceLongArg", V8TestObject::voidMethodNullableSequenceLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"longFrozenArrayMethod", V8TestObject::longFrozenArrayMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodStringFrozenArrayMethod", V8TestObject::voidMethodStringFrozenArrayMethodMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestInterfaceEmptyFrozenArrayMethod", V8TestObject::voidMethodTestInterfaceEmptyFrozenArrayMethodMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"nullableLongMethod", V8TestObject::nullableLongMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"nullableStringMethod", V8TestObject::nullableStringMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"nullableTestInterfaceMethod", V8TestObject::nullableTestInterfaceMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"nullableLongSequenceMethod", V8TestObject::nullableLongSequenceMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"booleanOrDOMStringOrUnrestrictedDoubleMethod", V8TestObject::booleanOrDOMStringOrUnrestrictedDoubleMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"testInterfaceOrLongMethod", V8TestObject::testInterfaceOrLongMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDoubleOrDOMStringArg", V8TestObject::voidMethodDoubleOrDOMStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDoubleOrDOMStringOrNullArg", V8TestObject::voidMethodDoubleOrDOMStringOrNullArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDoubleOrNullOrDOMStringArg", V8TestObject::voidMethodDoubleOrNullOrDOMStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDOMStringOrArrayBufferOrArrayBufferViewArg", V8TestObject::voidMethodDOMStringOrArrayBufferOrArrayBufferViewArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodArrayBufferOrArrayBufferViewOrDictionaryArg", V8TestObject::voidMethodArrayBufferOrArrayBufferViewOrDictionaryArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodBooleanOrElementSequenceArg", V8TestObject::voidMethodBooleanOrElementSequenceArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDoubleOrLongOrBooleanSequenceArg", V8TestObject::voidMethodDoubleOrLongOrBooleanSequenceArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodElementSequenceOrByteStringDoubleOrStringRecord", V8TestObject::voidMethodElementSequenceOrByteStringDoubleOrStringRecordMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodArrayOfDoubleOrDOMStringArg", V8TestObject::voidMethodArrayOfDoubleOrDOMStringArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestInterfaceEmptyOrNullArg", V8TestObject::voidMethodTestInterfaceEmptyOrNullArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestCallbackInterfaceArg", V8TestObject::voidMethodTestCallbackInterfaceArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodOptionalTestCallbackInterfaceArg", V8TestObject::voidMethodOptionalTestCallbackInterfaceArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestCallbackInterfaceOrNullArg", V8TestObject::voidMethodTestCallbackInterfaceOrNullArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"testEnumMethod", V8TestObject::testEnumMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestEnumArg", V8TestObject::voidMethodTestEnumArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestMultipleEnumArg", V8TestObject::voidMethodTestMultipleEnumArgMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"dictionaryMethod", V8TestObject::dictionaryMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"testDictionaryMethod", V8TestObject::testDictionaryMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"nullableTestDictionaryMethod", V8TestObject::nullableTestDictionaryMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"staticTestDictionaryMethod", V8TestObject::staticTestDictionaryMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"staticNullableTestDictionaryMethod", V8TestObject::staticNullableTestDictionaryMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"passPermissiveDictionaryMethod", V8TestObject::passPermissiveDictionaryMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"nodeFilterMethod", V8TestObject::nodeFilterMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"promiseMethod", V8TestObject::promiseMethodMethodCallback, 3, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kDoNotCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"promiseMethodWithoutExceptionState", V8TestObject::promiseMethodWithoutExceptionStateMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kDoNotCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"serializedScriptValueMethod", V8TestObject::serializedScriptValueMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"xPathNSResolverMethod", V8TestObject::xPathNSResolverMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDictionaryArg", V8TestObject::voidMethodDictionaryArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodNodeFilterArg", V8TestObject::voidMethodNodeFilterArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodPromiseArg", V8TestObject::voidMethodPromiseArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodSerializedScriptValueArg", V8TestObject::voidMethodSerializedScriptValueArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodXPathNSResolverArg", V8TestObject::voidMethodXPathNSResolverArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDictionarySequenceArg", V8TestObject::voidMethodDictionarySequenceArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodStringArgLongArg", V8TestObject::voidMethodStringArgLongArgMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodByteStringOrNullOptionalUSVStringArg", V8TestObject::voidMethodByteStringOrNullOptionalUSVStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodOptionalStringArg", V8TestObject::voidMethodOptionalStringArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodOptionalTestInterfaceEmptyArg", V8TestObject::voidMethodOptionalTestInterfaceEmptyArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodOptionalLongArg", V8TestObject::voidMethodOptionalLongArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"stringMethodOptionalLongArg", V8TestObject::stringMethodOptionalLongArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"testInterfaceEmptyMethodOptionalLongArg", V8TestObject::testInterfaceEmptyMethodOptionalLongArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"longMethodOptionalLongArg", V8TestObject::longMethodOptionalLongArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodLongArgOptionalLongArg", V8TestObject::voidMethodLongArgOptionalLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodLongArgOptionalLongArgOptionalLongArg", V8TestObject::voidMethodLongArgOptionalLongArgOptionalLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodLongArgOptionalTestInterfaceEmptyArg", V8TestObject::voidMethodLongArgOptionalTestInterfaceEmptyArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestInterfaceEmptyArgOptionalLongArg", V8TestObject::voidMethodTestInterfaceEmptyArgOptionalLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodOptionalDictionaryArg", V8TestObject::voidMethodOptionalDictionaryArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultByteStringArg", V8TestObject::voidMethodDefaultByteStringArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultStringArg", V8TestObject::voidMethodDefaultStringArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultIntegerArgs", V8TestObject::voidMethodDefaultIntegerArgsMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultDoubleArg", V8TestObject::voidMethodDefaultDoubleArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultTrueBooleanArg", V8TestObject::voidMethodDefaultTrueBooleanArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultFalseBooleanArg", V8TestObject::voidMethodDefaultFalseBooleanArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultNullableByteStringArg", V8TestObject::voidMethodDefaultNullableByteStringArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultNullableStringArg", V8TestObject::voidMethodDefaultNullableStringArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultNullableTestInterfaceArg", V8TestObject::voidMethodDefaultNullableTestInterfaceArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultDoubleOrStringArgs", V8TestObject::voidMethodDefaultDoubleOrStringArgsMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultStringSequenceArg", V8TestObject::voidMethodDefaultStringSequenceArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodVariadicStringArg", V8TestObject::voidMethodVariadicStringArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodStringArgVariadicStringArg", V8TestObject::voidMethodStringArgVariadicStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodVariadicTestInterfaceEmptyArg", V8TestObject::voidMethodVariadicTestInterfaceEmptyArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestInterfaceEmptyArgVariadicTestInterfaceEmptyArg", V8TestObject::voidMethodTestInterfaceEmptyArgVariadicTestInterfaceEmptyArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodA", V8TestObject::overloadedMethodAMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodB", V8TestObject::overloadedMethodBMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodC", V8TestObject::overloadedMethodCMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodD", V8TestObject::overloadedMethodDMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodE", V8TestObject::overloadedMethodEMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodF", V8TestObject::overloadedMethodFMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodG", V8TestObject::overloadedMethodGMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodH", V8TestObject::overloadedMethodHMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodI", V8TestObject::overloadedMethodIMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodJ", V8TestObject::overloadedMethodJMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodK", V8TestObject::overloadedMethodKMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodL", V8TestObject::overloadedMethodLMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodN", V8TestObject::overloadedMethodNMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"promiseOverloadMethod", V8TestObject::promiseOverloadMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kDoNotCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedPerWorldBindingsMethod", V8TestObject::overloadedPerWorldBindingsMethodMethodCallbackForMainWorld, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
    {"overloadedPerWorldBindingsMethod", V8TestObject::overloadedPerWorldBindingsMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds},
    {"overloadedStaticMethod", V8TestObject::overloadedStaticMethodMethodCallback, 1, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"item", V8TestObject::itemMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"setItem", V8TestObject::setItemMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodClampUnsignedShortArg", V8TestObject::voidMethodClampUnsignedShortArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodClampUnsignedLongArg", V8TestObject::voidMethodClampUnsignedLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultUndefinedTestInterfaceEmptyArg", V8TestObject::voidMethodDefaultUndefinedTestInterfaceEmptyArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultUndefinedLongArg", V8TestObject::voidMethodDefaultUndefinedLongArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultUndefinedStringArg", V8TestObject::voidMethodDefaultUndefinedStringArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodEnforceRangeLongArg", V8TestObject::voidMethodEnforceRangeLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTreatNullAsEmptyStringStringArg", V8TestObject::voidMethodTreatNullAsEmptyStringStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"activityLoggingAccessForAllWorldsMethod", V8TestObject::activityLoggingAccessForAllWorldsMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"callWithExecutionContextVoidMethod", V8TestObject::callWithExecutionContextVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"callWithScriptStateVoidMethod", V8TestObject::callWithScriptStateVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"callWithScriptStateLongMethod", V8TestObject::callWithScriptStateLongMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"callWithScriptStateExecutionContextVoidMethod", V8TestObject::callWithScriptStateExecutionContextVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"callWithScriptStateScriptArgumentsVoidMethod", V8TestObject::callWithScriptStateScriptArgumentsVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"callWithScriptStateScriptArgumentsVoidMethodOptionalBooleanArg", V8TestObject::callWithScriptStateScriptArgumentsVoidMethodOptionalBooleanArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"callWithCurrentWindow", V8TestObject::callWithCurrentWindowMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"callWithCurrentWindowScriptWindow", V8TestObject::callWithCurrentWindowScriptWindowMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"callWithThisValue", V8TestObject::callWithThisValueMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"checkSecurityForNodeVoidMethod", V8TestObject::checkSecurityForNodeVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"customVoidMethod", V8TestObject::customVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"customCallPrologueVoidMethod", V8TestObject::customCallPrologueVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"customCallEpilogueVoidMethod", V8TestObject::customCallEpilogueVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"deprecatedVoidMethod", V8TestObject::deprecatedVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"implementedAsVoidMethod", V8TestObject::implementedAsVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"measureAsVoidMethod", V8TestObject::measureAsVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"measureMethod", V8TestObject::measureMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"measureOverloadedMethod", V8TestObject::measureOverloadedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"DeprecateAsOverloadedMethod", V8TestObject::DeprecateAsOverloadedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"DeprecateAsSameValueOverloadedMethod", V8TestObject::DeprecateAsSameValueOverloadedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"measureAsOverloadedMethod", V8TestObject::measureAsOverloadedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"measureAsSameValueOverloadedMethod", V8TestObject::measureAsSameValueOverloadedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"ceReactionsNotOverloadedMethod", V8TestObject::ceReactionsNotOverloadedMethodMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"ceReactionsOverloadedMethod", V8TestObject::ceReactionsOverloadedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"deprecateAsMeasureAsSameValueOverloadedMethod", V8TestObject::deprecateAsMeasureAsSameValueOverloadedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"deprecateAsSameValueMeasureAsOverloadedMethod", V8TestObject::deprecateAsSameValueMeasureAsOverloadedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"deprecateAsSameValueMeasureAsSameValueOverloadedMethod", V8TestObject::deprecateAsSameValueMeasureAsSameValueOverloadedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"notEnumerableVoidMethod", V8TestObject::notEnumerableVoidMethodMethodCallback, 0, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"perWorldBindingsVoidMethod", V8TestObject::perWorldBindingsVoidMethodMethodCallbackForMainWorld, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
    {"perWorldBindingsVoidMethod", V8TestObject::perWorldBindingsVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds},
    {"perWorldBindingsVoidMethodTestInterfaceEmptyArg", V8TestObject::perWorldBindingsVoidMethodTestInterfaceEmptyArgMethodCallbackForMainWorld, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
    {"perWorldBindingsVoidMethodTestInterfaceEmptyArg", V8TestObject::perWorldBindingsVoidMethodTestInterfaceEmptyArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds},
    {"activityLoggingForAllWorldsPerWorldBindingsVoidMethod", V8TestObject::activityLoggingForAllWorldsPerWorldBindingsVoidMethodMethodCallbackForMainWorld, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
    {"activityLoggingForAllWorldsPerWorldBindingsVoidMethod", V8TestObject::activityLoggingForAllWorldsPerWorldBindingsVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds},
    {"activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethod", V8TestObject::activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethodMethodCallbackForMainWorld, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
    {"activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethod", V8TestObject::activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds},
    {"raisesExceptionVoidMethod", V8TestObject::raisesExceptionVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"raisesExceptionStringMethod", V8TestObject::raisesExceptionStringMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"raisesExceptionVoidMethodOptionalLongArg", V8TestObject::raisesExceptionVoidMethodOptionalLongArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"raisesExceptionVoidMethodTestCallbackInterfaceArg", V8TestObject::raisesExceptionVoidMethodTestCallbackInterfaceArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"raisesExceptionVoidMethodOptionalTestCallbackInterfaceArg", V8TestObject::raisesExceptionVoidMethodOptionalTestCallbackInterfaceArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"raisesExceptionTestInterfaceEmptyVoidMethod", V8TestObject::raisesExceptionTestInterfaceEmptyVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"raisesExceptionXPathNSResolverVoidMethod", V8TestObject::raisesExceptionXPathNSResolverVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"callWithExecutionContextRaisesExceptionVoidMethodLongArg", V8TestObject::callWithExecutionContextRaisesExceptionVoidMethodLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyArg", V8TestObject::legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyVariadicArg", V8TestObject::legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyVariadicArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"useToImpl4ArgumentsCheckingIfPossibleWithOptionalArg", V8TestObject::useToImpl4ArgumentsCheckingIfPossibleWithOptionalArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"useToImpl4ArgumentsCheckingIfPossibleWithNullableArg", V8TestObject::useToImpl4ArgumentsCheckingIfPossibleWithNullableArgMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"useToImpl4ArgumentsCheckingIfPossibleWithUndefinedArg", V8TestObject::useToImpl4ArgumentsCheckingIfPossibleWithUndefinedArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"unforgeableVoidMethod", V8TestObject::unforgeableVoidMethodMethodCallback, 0, static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"newObjectTestInterfaceMethod", V8TestObject::newObjectTestInterfaceMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"newObjectTestInterfacePromiseMethod", V8TestObject::newObjectTestInterfacePromiseMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kDoNotCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"RuntimeCallStatsCounterMethod", V8TestObject::RuntimeCallStatsCounterMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"keys", V8TestObject::keysMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"values", V8TestObject::valuesMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"forEach", V8TestObject::forEachMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"has", V8TestObject::hasMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"get", V8TestObject::getMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"delete", V8TestObject::deleteMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"set", V8TestObject::setMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"toJSON", V8TestObject::toJSONMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"toString", V8TestObject::toStringMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
};

static void installV8TestObjectTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestObject::wrapperTypeInfo.interface_name, v8::Local<v8::FunctionTemplate>(), V8TestObject::internalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.
  V8DOMConfiguration::InstallAttributes(
      isolate, world, instanceTemplate, prototypeTemplate,
      V8TestObjectAttributes, base::size(V8TestObjectAttributes));
  V8DOMConfiguration::InstallAccessors(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestObjectAccessors, base::size(V8TestObjectAccessors));
  V8DOMConfiguration::InstallMethods(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestObjectMethods, base::size(V8TestObjectMethods));

  // Indexed properties
  v8::IndexedPropertyHandlerConfiguration indexedPropertyHandlerConfig(
      V8TestObject::indexedPropertyGetterCallback,
      V8TestObject::indexedPropertySetterCallback,
      V8TestObject::indexedPropertyDescriptorCallback,
      V8TestObject::indexedPropertyDeleterCallback,
      IndexedPropertyEnumerator<TestObject>,
      V8TestObject::indexedPropertyDefinerCallback,
      v8::Local<v8::Value>(),
      v8::PropertyHandlerFlags::kNone);
  instanceTemplate->SetHandler(indexedPropertyHandlerConfig);
  // Named properties
  v8::NamedPropertyHandlerConfiguration namedPropertyHandlerConfig(V8TestObject::namedPropertyGetterCallback, V8TestObject::namedPropertySetterCallback, V8TestObject::namedPropertyQueryCallback, V8TestObject::namedPropertyDeleterCallback, V8TestObject::namedPropertyEnumeratorCallback, v8::Local<v8::Value>(), static_cast<v8::PropertyHandlerFlags>(int(v8::PropertyHandlerFlags::kOnlyInterceptStrings) | int(v8::PropertyHandlerFlags::kNonMasking)));
  instanceTemplate->SetHandler(namedPropertyHandlerConfig);

  // Iterator (@@iterator)
  static const V8DOMConfiguration::SymbolKeyedMethodConfiguration
  symbolKeyedIteratorConfiguration = {
      v8::Symbol::GetIterator,
      "entries",
      V8TestObject::iteratorMethodCallback,
      0,
      v8::DontEnum,
      V8DOMConfiguration::kOnPrototype,
      V8DOMConfiguration::kCheckHolder,
      V8DOMConfiguration::kDoNotCheckAccess,
      V8DOMConfiguration::kHasSideEffect
  };
  V8DOMConfiguration::InstallMethod(isolate, world, prototypeTemplate, signature, symbolKeyedIteratorConfiguration);

  // Custom signature
  const V8DOMConfiguration::MethodConfiguration partiallyRuntimeEnabledOverloadedVoidMethodMethodConfiguration[] = {
    {"partiallyRuntimeEnabledOverloadedVoidMethod", V8TestObject::partiallyRuntimeEnabledOverloadedVoidMethodMethodCallback, test_object_v8_internal::partiallyRuntimeEnabledOverloadedVoidMethodMethodLength(), v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
  };
  for (const auto& methodConfig : partiallyRuntimeEnabledOverloadedVoidMethodMethodConfiguration)
    V8DOMConfiguration::InstallMethod(isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate, signature, methodConfig);

  V8TestObject::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestObject::InstallRuntimeEnabledFeaturesOnTemplate(
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
    static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
        { "runtimeEnabledLongAttribute", V8TestObject::runtimeEnabledLongAttributeAttributeGetterCallback, V8TestObject::runtimeEnabledLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        { "unscopableRuntimeEnabledLongAttribute", V8TestObject::unscopableRuntimeEnabledLongAttributeAttributeGetterCallback, V8TestObject::unscopableRuntimeEnabledLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    };
    V8DOMConfiguration::InstallAccessors(
        isolate, world, instance_template, prototype_template, interface_template,
        signature, accessor_configurations,
        base::size(accessor_configurations));
  }

  // Custom signature
  if (RuntimeEnabledFeatures::FeatureNameEnabled()) {
    const V8DOMConfiguration::MethodConfiguration unscopableRuntimeEnabledVoidMethodMethodConfiguration[] = {
      {"unscopableRuntimeEnabledVoidMethod", V8TestObject::unscopableRuntimeEnabledVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
    };
    for (const auto& methodConfig : unscopableRuntimeEnabledVoidMethodMethodConfiguration)
      V8DOMConfiguration::InstallMethod(isolate, world, instance_template, prototype_template, interface_template, signature, methodConfig);
  }
  if (RuntimeEnabledFeatures::FeatureNameEnabled()) {
    const V8DOMConfiguration::MethodConfiguration runtimeEnabledVoidMethodMethodConfiguration[] = {
      {"runtimeEnabledVoidMethod", V8TestObject::runtimeEnabledVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
    };
    for (const auto& methodConfig : runtimeEnabledVoidMethodMethodConfiguration)
      V8DOMConfiguration::InstallMethod(isolate, world, instance_template, prototype_template, interface_template, signature, methodConfig);
  }
  if (RuntimeEnabledFeatures::FeatureNameEnabled()) {
    const V8DOMConfiguration::MethodConfiguration perWorldBindingsRuntimeEnabledVoidMethodMethodConfiguration[] = {
      {"perWorldBindingsRuntimeEnabledVoidMethod", V8TestObject::perWorldBindingsRuntimeEnabledVoidMethodMethodCallbackForMainWorld, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
      {"perWorldBindingsRuntimeEnabledVoidMethod", V8TestObject::perWorldBindingsRuntimeEnabledVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds}
    };
    for (const auto& methodConfig : perWorldBindingsRuntimeEnabledVoidMethodMethodConfiguration)
      V8DOMConfiguration::InstallMethod(isolate, world, instance_template, prototype_template, interface_template, signature, methodConfig);
  }
  if (RuntimeEnabledFeatures::FeatureNameEnabled()) {
    const V8DOMConfiguration::MethodConfiguration runtimeEnabledOverloadedVoidMethodMethodConfiguration[] = {
      {"runtimeEnabledOverloadedVoidMethod", V8TestObject::runtimeEnabledOverloadedVoidMethodMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
    };
    for (const auto& methodConfig : runtimeEnabledOverloadedVoidMethodMethodConfiguration)
      V8DOMConfiguration::InstallMethod(isolate, world, instance_template, prototype_template, interface_template, signature, methodConfig);
  }
  if (RuntimeEnabledFeatures::FeatureNameEnabled()) {
    const V8DOMConfiguration::MethodConfiguration clearMethodConfiguration[] = {
      {"clear", V8TestObject::clearMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
    };
    for (const auto& methodConfig : clearMethodConfiguration)
      V8DOMConfiguration::InstallMethod(isolate, world, instance_template, prototype_template, interface_template, signature, methodConfig);
  }
}

void V8TestObject::installFeatureName(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::Object> instance, v8::Local<v8::Object> prototype, v8::Local<v8::Function> interface) {
  v8::Local<v8::FunctionTemplate> interfaceTemplate = V8TestObject::wrapperTypeInfo.domTemplate(isolate, world);
  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  static const V8DOMConfiguration::AccessorConfiguration accessororiginTrialEnabledLongAttributeConfiguration[] = {
    { "originTrialEnabledLongAttribute", V8TestObject::originTrialEnabledLongAttributeAttributeGetterCallback, V8TestObject::originTrialEnabledLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds }
  };
  for (const auto& accessorConfig : accessororiginTrialEnabledLongAttributeConfiguration)
    V8DOMConfiguration::InstallAccessor(isolate, world, instance, prototype, interface, signature, accessorConfig);
  static const V8DOMConfiguration::AccessorConfiguration accessorunscopableOriginTrialEnabledLongAttributeConfiguration[] = {
    { "unscopableOriginTrialEnabledLongAttribute", V8TestObject::unscopableOriginTrialEnabledLongAttributeAttributeGetterCallback, V8TestObject::unscopableOriginTrialEnabledLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds }
  };
  for (const auto& accessorConfig : accessorunscopableOriginTrialEnabledLongAttributeConfiguration)
    V8DOMConfiguration::InstallAccessor(isolate, world, instance, prototype, interface, signature, accessorConfig);
  static const V8DOMConfiguration::MethodConfiguration methodOrigintrialenabledvoidmethodConfiguration[] = {
    {"originTrialEnabledVoidMethod", V8TestObject::originTrialEnabledVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
  };
  for (const auto& methodConfig : methodOrigintrialenabledvoidmethodConfiguration)
    V8DOMConfiguration::InstallMethod(isolate, world, instance, prototype, interface, signature, methodConfig);
  static const V8DOMConfiguration::MethodConfiguration methodPerworldbindingsorigintrialenabledvoidmethodConfiguration[] = {
    {"perWorldBindingsOriginTrialEnabledVoidMethod", V8TestObject::perWorldBindingsOriginTrialEnabledVoidMethodMethodCallbackForMainWorld, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
  {"perWorldBindingsOriginTrialEnabledVoidMethod", V8TestObject::perWorldBindingsOriginTrialEnabledVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds}
  };
  for (const auto& methodConfig : methodPerworldbindingsorigintrialenabledvoidmethodConfiguration)
    V8DOMConfiguration::InstallMethod(isolate, world, instance, prototype, interface, signature, methodConfig);
}

void V8TestObject::installFeatureName(ScriptState* scriptState, v8::Local<v8::Object> instance) {
  V8PerContextData* perContextData = scriptState->PerContextData();
  v8::Local<v8::Object> prototype = perContextData->PrototypeForType(&V8TestObject::wrapperTypeInfo);
  v8::Local<v8::Function> interface = perContextData->ConstructorForType(&V8TestObject::wrapperTypeInfo);
  ALLOW_UNUSED_LOCAL(interface);
  installFeatureName(scriptState->GetIsolate(), scriptState->World(), instance, prototype, interface);
}

void V8TestObject::installFeatureName(ScriptState* scriptState) {
  installFeatureName(scriptState, v8::Local<v8::Object>());
}

v8::Local<v8::FunctionTemplate> V8TestObject::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestObjectTemplate);
}

bool V8TestObject::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestObject::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestObject* V8TestObject::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestObject* NativeValueTraits<TestObject>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestObject* nativeValue = V8TestObject::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestObject"));
  }
  return nativeValue;
}

void V8TestObject::InstallConditionalFeatures(
    v8::Local<v8::Context> context,
    const DOMWrapperWorld& world,
    v8::Local<v8::Object> instanceObject,
    v8::Local<v8::Object> prototypeObject,
    v8::Local<v8::Function> interfaceObject,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  CHECK(!interfaceTemplate.IsEmpty());
  DCHECK((!prototypeObject.IsEmpty() && !interfaceObject.IsEmpty()) ||
         !instanceObject.IsEmpty());

  v8::Isolate* isolate = context->GetIsolate();

  if (!prototypeObject.IsEmpty()) {
    v8::Local<v8::Name> unscopablesSymbol(v8::Symbol::GetUnscopables(isolate));
    v8::Local<v8::Object> unscopables;
    bool has_unscopables;
    if (prototypeObject->HasOwnProperty(context, unscopablesSymbol).To(&has_unscopables) && has_unscopables) {
      unscopables = prototypeObject->Get(context, unscopablesSymbol).ToLocalChecked().As<v8::Object>();
    } else {
      // Web IDL 3.6.3. Interface prototype object
      // https://heycam.github.io/webidl/#create-an-interface-prototype-object
      // step 8.1. Let unscopableObject be the result of performing
      //   ! ObjectCreate(null).
      unscopables = v8::Object::New(isolate);
      unscopables->SetPrototype(context, v8::Null(isolate)).ToChecked();
    }
    unscopables->CreateDataProperty(
        context, V8AtomicString(isolate, "unscopableLongAttribute"), v8::True(isolate))
        .FromJust();
    unscopables->CreateDataProperty(
        context, V8AtomicString(isolate, "unscopableOriginTrialEnabledLongAttribute"), v8::True(isolate))
        .FromJust();
    if (RuntimeEnabledFeatures::FeatureNameEnabled()) {
      unscopables->CreateDataProperty(
          context, V8AtomicString(isolate, "unscopableRuntimeEnabledLongAttribute"), v8::True(isolate))
          .FromJust();
    }
    if (RuntimeEnabledFeatures::FeatureNameEnabled()) {
      unscopables->CreateDataProperty(
          context, V8AtomicString(isolate, "unscopableRuntimeEnabledVoidMethod"), v8::True(isolate))
          .FromJust();
    }
    unscopables->CreateDataProperty(
        context, V8AtomicString(isolate, "unscopableVoidMethod"), v8::True(isolate))
        .FromJust();
    prototypeObject->CreateDataProperty(context, unscopablesSymbol, unscopables).FromJust();
  }
}

}  // namespace blink

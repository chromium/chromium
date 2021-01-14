// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/remote_objects/remote_object.h"
#include "gin/converter.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"

namespace blink {

gin::WrapperInfo RemoteObject::kWrapperInfo = {gin::kEmbedderNativeGin};

namespace {
const char kMethodInvocationAsConstructorDisallowed[] =
    "Java bridge method can't be invoked as a constructor";
const char kMethodInvocationNonexistentMethod[] =
    "Java bridge method does not exist for this object";
const char kMethodInvocationOnNonInjectedObjectDisallowed[] =
    "Java bridge method can't be invoked on a non-injected object";
const char kMethodInvocationErrorMessage[] =
    "Java bridge method invocation error";

String RemoteInvocationErrorToString(
    mojom::blink::RemoteInvocationError value) {
  switch (value) {
    case mojom::blink::RemoteInvocationError::METHOD_NOT_FOUND:
      return "method not found";
    case mojom::blink::RemoteInvocationError::OBJECT_GET_CLASS_BLOCKED:
      return "invoking Object.getClass() is not permitted";
    case mojom::blink::RemoteInvocationError::EXCEPTION_THROWN:
      return "an exception was thrown";
    default:
      return String::Format("unknown RemoteInvocationError value: %d", value);
  }
}

v8::Local<v8::Object> GetMethodCache(v8::Isolate* isolate,
                                     v8::Local<v8::Object> object) {
  static const V8PrivateProperty::SymbolKey kMethodCacheKey;
  V8PrivateProperty::Symbol method_cache_symbol =
      V8PrivateProperty::GetSymbol(isolate, kMethodCacheKey);
  v8::Local<v8::Value> result;
  if (!method_cache_symbol.GetOrUndefined(object).ToLocal(&result))
    return v8::Local<v8::Object>();

  if (result->IsUndefined()) {
    result = v8::Object::New(isolate, v8::Null(isolate), nullptr, nullptr, 0);
    ignore_result(method_cache_symbol.Set(object, result));
  }

  DCHECK(result->IsObject());
  return result.As<v8::Object>();
}

mojom::blink::RemoteInvocationArgumentPtr JSValueToMojom(
    const v8::Local<v8::Value>& js_value,
    v8::Isolate* isolate) {
  if (js_value->IsNumber()) {
    return mojom::blink::RemoteInvocationArgument::NewNumberValue(
        js_value->NumberValue(isolate->GetCurrentContext()).ToChecked());
  }

  if (js_value->IsBoolean()) {
    return mojom::blink::RemoteInvocationArgument::NewBooleanValue(
        js_value->BooleanValue(isolate));
  }

  if (js_value->IsString()) {
    return mojom::blink::RemoteInvocationArgument::NewStringValue(
        ToCoreString(js_value.As<v8::String>()));
  }

  if (js_value->IsNull()) {
    return mojom::blink::RemoteInvocationArgument::NewSingletonValue(
        mojom::blink::SingletonJavaScriptValue::kNull);
  }

  if (js_value->IsUndefined()) {
    return mojom::blink::RemoteInvocationArgument::NewSingletonValue(
        mojom::blink::SingletonJavaScriptValue::kUndefined);
  }

  if (js_value->IsArray()) {
    auto array = js_value.As<v8::Array>();
    WTF::Vector<mojom::blink::RemoteInvocationArgumentPtr> nested_arguments;
    for (uint32_t i = 0; i < array->Length(); ++i) {
      v8::Local<v8::Value> element_v8;

      if (!array->Get(isolate->GetCurrentContext(), i).ToLocal(&element_v8))
        return nullptr;

      // The array length might change during iteration. Set the output array
      // elements to null for nonexistent input array elements.
      if (!array->HasRealIndexedProperty(isolate->GetCurrentContext(), i)
               .FromMaybe(false)) {
        nested_arguments.push_back(
            mojom::blink::RemoteInvocationArgument::NewSingletonValue(
                mojom::blink::SingletonJavaScriptValue::kNull));
      } else {
        mojom::blink::RemoteInvocationArgumentPtr nested_argument;

        // This code prevents infinite recursion on the sender side.
        // Null value is sent according to the Java-side conversion rules for
        // expected parameter types:
        // - multi-dimensional and object arrays are not allowed and are
        // converted to nulls;
        // - for primitive arrays, the null value will be converted to primitive
        // zero;
        // - for string arrays, the null value will be converted to a null
        // string. See RemoteObjectImpl.convertArgument() in
        // content/public/android/java/src/org/chromium/content/browser/remoteobjects/RemoteObjectImpl.java
        if (element_v8->IsObject()) {
          nested_argument =
              mojom::blink::RemoteInvocationArgument::NewSingletonValue(
                  mojom::blink::SingletonJavaScriptValue::kNull);
        } else {
          nested_argument = JSValueToMojom(element_v8, isolate);
        }

        if (!nested_argument)
          return nullptr;

        nested_arguments.push_back(std::move(nested_argument));
      }
    }

    return mojom::blink::RemoteInvocationArgument::NewArrayValue(
        std::move(nested_arguments));
  }

  return nullptr;
}

v8::Local<v8::Value> MojomToJSValue(
    const mojom::blink::RemoteInvocationResultValuePtr& result_value,
    v8::Isolate* isolate) {
  if (result_value->is_number_value()) {
    return v8::Number::New(isolate, result_value->get_number_value());
  }

  if (result_value->is_boolean_value()) {
    return v8::Boolean::New(isolate, result_value->get_boolean_value());
  }

  if (result_value->is_string_value()) {
    return V8String(isolate, result_value->get_string_value());
  }

  switch (result_value->get_singleton_value()) {
    case mojom::blink::SingletonJavaScriptValue::kNull:
      return v8::Null(isolate);
    case mojom::blink::SingletonJavaScriptValue::kUndefined:
      return v8::Undefined(isolate);
  }

  return v8::Local<v8::Value>();
}
}  // namespace

RemoteObject::RemoteObject(v8::Isolate* isolate,
                           RemoteObjectGatewayImpl* gateway,
                           int32_t object_id)
    : gin::NamedPropertyInterceptor(isolate, this),
      gateway_(gateway),
      object_id_(object_id) {}

RemoteObject::~RemoteObject() {
  if (gateway_) {
    gateway_->ReleaseObject(object_id_);

    if (object_)
      object_->NotifyReleasedObject();
  }
}

gin::ObjectTemplateBuilder RemoteObject::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<RemoteObject>::GetObjectTemplateBuilder(isolate)
      .AddNamedPropertyInterceptor();
}

void RemoteObject::RemoteObjectInvokeCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  if (info.IsConstructCall()) {
    // This is not a constructor. Throw and return.
    isolate->ThrowException(v8::Exception::Error(
        V8String(isolate, kMethodInvocationAsConstructorDisallowed)));
    return;
  }

  RemoteObject* remote_object;
  if (!gin::ConvertFromV8(isolate, info.Holder(), &remote_object)) {
    // Someone messed with the |this| pointer. Throw and return.
    isolate->ThrowException(v8::Exception::Error(
        V8String(isolate, kMethodInvocationOnNonInjectedObjectDisallowed)));
    return;
  }

  String method_name = ToCoreString(info.Data().As<v8::String>());

  v8::Local<v8::Object> method_cache = GetMethodCache(
      isolate, remote_object->GetWrapper(isolate).ToLocalChecked());
  if (method_cache.IsEmpty())
    return;

  v8::Local<v8::Value> cached_method =
      method_cache
          ->Get(isolate->GetCurrentContext(), info.Data().As<v8::String>())
          .ToLocalChecked();

  if (cached_method->IsUndefined()) {
    isolate->ThrowException(v8::Exception::Error(
        V8String(isolate, kMethodInvocationNonexistentMethod)));
    return;
  }

  WTF::Vector<mojom::blink::RemoteInvocationArgumentPtr> arguments;
  arguments.ReserveInitialCapacity(info.Length());

  for (int i = 0; i < info.Length(); i++) {
    auto argument = JSValueToMojom(info[i], isolate);
    if (!argument)
      return;

    arguments.push_back(std::move(argument));
  }

  remote_object->EnsureRemoteIsBound();
  mojom::blink::RemoteInvocationResultPtr result;
  remote_object->object_->InvokeMethod(method_name, std::move(arguments),
                                       &result);

  if (result->error != mojom::blink::RemoteInvocationError::OK) {
    String message = String::Format("%s : ", kMethodInvocationErrorMessage) +
                     RemoteInvocationErrorToString(result->error);
    isolate->ThrowException(v8::Exception::Error(V8String(isolate, message)));
    return;
  }

  if (!result->value)
    return;

  if (result->value->is_object_id()) {
    // TODO(crbug.com/794320): need to check whether an object with this id has
    // already been injected
    RemoteObject* object_result =
        new RemoteObject(info.GetIsolate(), remote_object->gateway_,
                         result->value->get_object_id());
    gin::Handle<RemoteObject> controller =
        gin::CreateHandle(isolate, object_result);
    if (controller.IsEmpty())
      info.GetReturnValue().SetUndefined();
    else
      info.GetReturnValue().Set(controller.ToV8());
  } else {
    info.GetReturnValue().Set(MojomToJSValue(result->value, isolate));
  }
}

void RemoteObject::EnsureRemoteIsBound() {
  if (!object_.is_bound()) {
    gateway_->BindRemoteObjectReceiver(object_id_,
                                       object_.BindNewPipeAndPassReceiver());
  }
}

v8::Local<v8::Value> RemoteObject::GetNamedProperty(
    v8::Isolate* isolate,
    const std::string& property) {
  auto wtf_property = WTF::String::FromUTF8(property);

  v8::Local<v8::String> v8_property = V8AtomicString(isolate, wtf_property);
  v8::Local<v8::Object> method_cache =
      GetMethodCache(isolate, GetWrapper(isolate).ToLocalChecked());
  if (method_cache.IsEmpty())
    return v8::Local<v8::Value>();

  v8::Local<v8::Value> cached_method =
      method_cache->Get(isolate->GetCurrentContext(), v8_property)
          .ToLocalChecked();

  if (!cached_method->IsUndefined())
    return cached_method;

  // if not in the cache, ask the browser
  EnsureRemoteIsBound();
  bool method_exists = false;
  object_->HasMethod(wtf_property, &method_exists);

  if (!method_exists) {
    return v8::Local<v8::Value>();
  }

  auto function = v8::Function::New(isolate->GetCurrentContext(),
                                    RemoteObjectInvokeCallback, v8_property)
                      .ToLocalChecked();

  ignore_result(method_cache->CreateDataProperty(isolate->GetCurrentContext(),
                                                 v8_property, function));
  return function;
}

std::vector<std::string> RemoteObject::EnumerateNamedProperties(
    v8::Isolate* isolate) {
  EnsureRemoteIsBound();
  WTF::Vector<WTF::String> methods;
  object_->GetMethods(&methods);
  std::vector<std::string> result;
  for (const auto& method : methods)
    result.push_back(method.Utf8());
  return result;
}

}  // namespace blink

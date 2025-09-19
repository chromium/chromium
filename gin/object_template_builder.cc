// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/object_template_builder.h"

#include <stdint.h>

#include <string_view>

#include "gin/interceptor.h"
#include "gin/per_isolate_data.h"
#include "gin/public/wrappable_pointer_tags.h"
#include "gin/public/wrapper_info.h"
#include "gin/wrappable.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-template.h"

namespace gin {

NamedPropertyInterceptor* NamedInterceptorFromV8(v8::Isolate* isolate,
                                                 v8::Local<v8::Value> val,
                                                 WrappablePointerTag tag) {
  if (!val->IsObject()) {
    return nullptr;
  }
  v8::Local<v8::Object> obj = val.As<v8::Object>();
  if (!obj->IsApiWrapper()) {
    return nullptr;
  }
  v8::CppHeapPointerTag cpp_heap_pointer_tag =
      static_cast<v8::CppHeapPointerTag>(tag);
  auto* base = v8::Object::Unwrap<WrappableBase>(
      isolate, obj, {cpp_heap_pointer_tag, cpp_heap_pointer_tag});
  if (!base) {
    return nullptr;
  }
  return base->GetNamedPropertyInterceptor();
}

v8::Intercepted ObjectTemplateBuilder::NamedPropertyGetterImpl(
    WrappablePointerTag tag,
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  NamedPropertyInterceptor* interceptor =
      NamedInterceptorFromV8(isolate, info.HolderV2(), tag);
  if (!interceptor) {
    return v8::Intercepted::kNo;
  }

  std::string name;
  ConvertFromV8(isolate, property, &name);
  v8::Local<v8::Value> result = interceptor->GetNamedProperty(isolate, name);
  if (!result.IsEmpty()) {
    info.GetReturnValue().SetNonEmpty(result);
    return v8::Intercepted::kYes;
  }
  return v8::Intercepted::kNo;
}

v8::Intercepted ObjectTemplateBuilder::NamedPropertySetterImpl(
    WrappablePointerTag tag,
    v8::Local<v8::Name> property,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  NamedPropertyInterceptor* interceptor =
      NamedInterceptorFromV8(isolate, info.HolderV2(), tag);
  if (!interceptor) {
    return v8::Intercepted::kNo;
  }
  std::string name;
  ConvertFromV8(isolate, property, &name);
  if (interceptor->SetNamedProperty(isolate, name, value)) {
    return v8::Intercepted::kYes;
  }
  return v8::Intercepted::kNo;
}

v8::Intercepted ObjectTemplateBuilder::NamedPropertyQueryImpl(
    WrappablePointerTag tag,
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Integer>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  NamedPropertyInterceptor* interceptor =
      NamedInterceptorFromV8(isolate, info.HolderV2(), tag);
  if (!interceptor) {
    return v8::Intercepted::kNo;
  }
  std::string name;
  ConvertFromV8(isolate, property, &name);
  if (!interceptor->GetNamedProperty(isolate, name).IsEmpty()) {
    info.GetReturnValue().Set(v8::None);
    return v8::Intercepted::kYes;
  }
  return v8::Intercepted::kNo;
}

void ObjectTemplateBuilder::NamedPropertyEnumeratorImpl(
    WrappablePointerTag tag,
    const v8::PropertyCallbackInfo<v8::Array>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  NamedPropertyInterceptor* interceptor =
      NamedInterceptorFromV8(isolate, info.HolderV2(), tag);
  if (!interceptor) {
    return;
  }
  v8::Local<v8::Value> properties;
  if (!TryConvertToV8(isolate, interceptor->EnumerateNamedProperties(isolate),
                      &properties)) {
    return;
  }
  info.GetReturnValue().Set(v8::Local<v8::Array>::Cast(properties));
}
namespace {

void Constructor(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  isolate->ThrowException(v8::Exception::Error(info.Data().As<v8::String>()));
}

}  // namespace

ObjectTemplateBuilder::ObjectTemplateBuilder(v8::Isolate* isolate)
    : ObjectTemplateBuilder(isolate, nullptr) {}

ObjectTemplateBuilder::ObjectTemplateBuilder(v8::Isolate* isolate,
                                             const char* type_name)
    : isolate_(isolate),
      type_name_(type_name),
      constructor_template_(v8::FunctionTemplate::New(
          isolate,
          &Constructor,
          StringToV8(
              isolate,
              type_name
                  ? base::StrCat({"Objects of type ", type_name,
                                  " cannot be created using the constructor."})
                  : "Objects of this type cannot be created using the "
                    "constructor"))),
      template_(constructor_template_->InstanceTemplate()) {
  template_->SetInternalFieldCount(kNumberOfInternalFields);
}

ObjectTemplateBuilder::ObjectTemplateBuilder(
    const ObjectTemplateBuilder& other) = default;

ObjectTemplateBuilder::~ObjectTemplateBuilder() = default;

ObjectTemplateBuilder& ObjectTemplateBuilder::SetImpl(
    const std::string_view& name,
    v8::Local<v8::Data> val) {
  template_->Set(StringToSymbol(isolate_, name), val);
  return *this;
}

ObjectTemplateBuilder& ObjectTemplateBuilder::SetImpl(v8::Local<v8::Name> name,
                                                      v8::Local<v8::Data> val) {
  template_->Set(name, val);
  return *this;
}

ObjectTemplateBuilder& ObjectTemplateBuilder::SetPropertyImpl(
    const std::string_view& name,
    v8::Local<v8::FunctionTemplate> getter,
    v8::Local<v8::FunctionTemplate> setter) {
  template_->SetAccessorProperty(StringToSymbol(isolate_, name), getter,
                                 setter);
  return *this;
}

ObjectTemplateBuilder& ObjectTemplateBuilder::SetLazyDataPropertyImpl(
    const std::string_view& name,
    v8::AccessorNameGetterCallback callback,
    v8::Local<v8::Value> data) {
  template_->SetLazyDataProperty(StringToSymbol(isolate_, name), callback,
                                 data);
  return *this;
}

v8::Local<v8::ObjectTemplate> ObjectTemplateBuilder::Build() {
  v8::Local<v8::ObjectTemplate> result = template_;
  template_.Clear();
  constructor_template_.Clear();
  return result;
}

}  // namespace gin

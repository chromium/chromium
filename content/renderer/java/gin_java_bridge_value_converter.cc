// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/java/gin_java_bridge_value_converter.h"

#include <stddef.h>
#include <stdint.h>

#include <cmath>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "content/common/android/gin_java_bridge_value.h"
#include "content/renderer/java/gin_java_bridge_object.h"
#include "gin/array_buffer.h"

namespace content {

GinJavaBridgeValueConverter::GinJavaBridgeValueConverter()
    : converter_(V8ValueConverter::Create()) {
  converter_->SetDateAllowed(false);
  converter_->SetRegExpAllowed(false);
  converter_->SetFunctionAllowed(true);
  converter_->SetStrategy(this);
}

GinJavaBridgeValueConverter::~GinJavaBridgeValueConverter() {
}

v8::Local<v8::Value> GinJavaBridgeValueConverter::ToV8Value(
    const base::Value* value,
    v8::Local<v8::Context> context) const {
  return converter_->ToV8Value(value, context);
}

std::unique_ptr<base::Value> GinJavaBridgeValueConverter::FromV8Value(
    v8::Local<v8::Value> value,
    v8::Local<v8::Context> context) const {
  return converter_->FromV8Value(value, context);
}

bool GinJavaBridgeValueConverter::FromV8Object(
    v8::Local<v8::Object> value,
    std::unique_ptr<base::Value>* out,
    v8::Isolate* isolate) {
  GinJavaBridgeObject* unwrapped;
  if (!gin::ConvertFromV8(isolate, value, &unwrapped)) {
    return false;
  }
  *out = GinJavaBridgeValue::CreateObjectIDValue(unwrapped->object_id());
  return true;
}

namespace {

class TypedArraySerializer {
 public:
  virtual ~TypedArraySerializer() {}
  static std::unique_ptr<TypedArraySerializer> Create(
      v8::Local<v8::TypedArray> typed_array);
  virtual void serializeTo(char* data,
                           size_t data_length,
                           base::ListValue* out) = 0;
 protected:
  TypedArraySerializer() {}
};

template <typename ElementType, typename ListType>
class TypedArraySerializerImpl : public TypedArraySerializer {
 public:
  static std::unique_ptr<TypedArraySerializer> Create(
      v8::Local<v8::TypedArray> typed_array) {
    return base::WrapUnique(
        new TypedArraySerializerImpl<ElementType, ListType>(typed_array));
  }

  void serializeTo(char* data,
                   size_t data_length,
                   base::ListValue* out) override {
    DCHECK_EQ(data_length, typed_array_->Length() * sizeof(ElementType));
    for (ElementType *element = reinterpret_cast<ElementType*>(data),
                     *end = element + typed_array_->Length();
         element != end;
         ++element) {
      out->Append(std::make_unique<base::Value>(ListType(*element)));
    }
  }

 private:
  explicit TypedArraySerializerImpl(v8::Local<v8::TypedArray> typed_array)
      : typed_array_(typed_array) {}

  v8::Local<v8::TypedArray> typed_array_;

  DISALLOW_COPY_AND_ASSIGN(TypedArraySerializerImpl);
};

// static
std::unique_ptr<TypedArraySerializer> TypedArraySerializer::Create(
    v8::Local<v8::TypedArray> typed_array) {
  if (typed_array->IsInt8Array() ||
      typed_array->IsUint8Array() ||
      typed_array->IsUint8ClampedArray()) {
    return TypedArraySerializerImpl<char, int>::Create(typed_array);
  } else if (typed_array->IsInt16Array() || typed_array->IsUint16Array()) {
    return TypedArraySerializerImpl<int16_t, int>::Create(typed_array);
  } else if (typed_array->IsInt32Array() || typed_array->IsUint32Array()) {
    return TypedArraySerializerImpl<int32_t, int>::Create(typed_array);
  } else if (typed_array->IsFloat32Array()) {
    return TypedArraySerializerImpl<float, double>::Create(typed_array);
  } else if (typed_array->IsFloat64Array()) {
    return TypedArraySerializerImpl<double, double>::Create(typed_array);
  }
  NOTREACHED();
  return std::unique_ptr<TypedArraySerializer>();
}

}  // namespace

bool GinJavaBridgeValueConverter::FromV8ArrayBuffer(
    v8::Local<v8::Object> value,
    std::unique_ptr<base::Value>* out,
    v8::Isolate* isolate) {
  if (!value->IsTypedArray()) {
    *out = GinJavaBridgeValue::CreateUndefinedValue();
    return true;
  }

  char* data = NULL;
  size_t data_length = 0;
  gin::ArrayBufferView view;
  if (ConvertFromV8(isolate, value.As<v8::ArrayBufferView>(), &view)) {
    data = reinterpret_cast<char*>(view.bytes());
    data_length = view.num_bytes();
  }
  if (!data) {
    *out = GinJavaBridgeValue::CreateUndefinedValue();
    return true;
  }

  std::unique_ptr<base::ListValue> result(new base::ListValue);
  std::unique_ptr<TypedArraySerializer> serializer(
      TypedArraySerializer::Create(value.As<v8::TypedArray>()));
  serializer->serializeTo(data, data_length, result.get());
  *out = std::move(result);
  return true;
}

bool GinJavaBridgeValueConverter::FromV8Number(
    v8::Local<v8::Number> value,
    std::unique_ptr<base::Value>* out) {
  double double_value = value->Value();
  if (std::isfinite(double_value))
    return false;
  *out = GinJavaBridgeValue::CreateNonFiniteValue(double_value);
  return true;
}

bool GinJavaBridgeValueConverter::FromV8Undefined(
    std::unique_ptr<base::Value>* out) {
  *out = GinJavaBridgeValue::CreateUndefinedValue();
  return true;
}

}  // namespace content

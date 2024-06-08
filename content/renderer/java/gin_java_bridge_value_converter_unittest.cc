// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/renderer/java/gin_java_bridge_value_converter.h"

#include <stddef.h>

#include <cmath>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "content/common/android/gin_java_bridge_value.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-persistent-handle.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-script.h"
#include "v8/include/v8-template.h"

namespace content {

class GinJavaBridgeValueConverterTest : public testing::Test {
 public:
  GinJavaBridgeValueConverterTest()
      : isolate_(v8::Isolate::GetCurrent()) {
  }

 protected:
  void SetUp() override {
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate_);
    context_.Reset(isolate_, v8::Context::New(isolate_, NULL, global));
  }

  void TearDown() override { context_.Reset(); }

  raw_ptr<v8::Isolate> isolate_;

  // Context for the JavaScript in the test.
  v8::Persistent<v8::Context> context_;
};

TEST_F(GinJavaBridgeValueConverterTest, BasicValues) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);

  std::unique_ptr<GinJavaBridgeValueConverter> converter(
      new GinJavaBridgeValueConverter());

  v8::Local<v8::Primitive> v8_undefined(v8::Undefined(isolate_));
  std::unique_ptr<base::Value> undefined(
      converter->FromV8Value(v8_undefined, context));
  ASSERT_TRUE(undefined.get());
  EXPECT_TRUE(GinJavaBridgeValue::ContainsGinJavaBridgeValue(undefined.get()));
  std::unique_ptr<const GinJavaBridgeValue> undefined_value(
      GinJavaBridgeValue::FromValue(undefined.get()));
  ASSERT_TRUE(undefined_value.get());
  EXPECT_TRUE(undefined_value->IsType(GinJavaBridgeValue::TYPE_UNDEFINED));

  v8::Local<v8::Number> v8_infinity(
      v8::Number::New(isolate_, std::numeric_limits<double>::infinity()));
  std::unique_ptr<base::Value> infinity(
      converter->FromV8Value(v8_infinity, context));
  ASSERT_TRUE(infinity.get());
  EXPECT_TRUE(
      GinJavaBridgeValue::ContainsGinJavaBridgeValue(infinity.get()));
  std::unique_ptr<const GinJavaBridgeValue> infinity_value(
      GinJavaBridgeValue::FromValue(infinity.get()));
  ASSERT_TRUE(infinity_value.get());
  float native_float;
  EXPECT_TRUE(
      infinity_value->IsType(GinJavaBridgeValue::TYPE_NONFINITE));
  EXPECT_TRUE(infinity_value->GetAsNonFinite(&native_float));
  EXPECT_TRUE(std::isinf(native_float));
}

TEST_F(GinJavaBridgeValueConverterTest, ArrayBuffer) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);

  std::unique_ptr<GinJavaBridgeValueConverter> converter(
      new GinJavaBridgeValueConverter());

  v8::Local<v8::ArrayBuffer> v8_array_buffer(
      v8::ArrayBuffer::New(isolate_, 0));
  std::unique_ptr<base::Value> undefined(
      converter->FromV8Value(v8_array_buffer, context));
  ASSERT_TRUE(undefined.get());
  EXPECT_TRUE(GinJavaBridgeValue::ContainsGinJavaBridgeValue(undefined.get()));
  std::unique_ptr<const GinJavaBridgeValue> undefined_value(
      GinJavaBridgeValue::FromValue(undefined.get()));
  ASSERT_TRUE(undefined_value.get());
  EXPECT_TRUE(undefined_value->IsType(GinJavaBridgeValue::TYPE_UNDEFINED));
}

TEST_F(GinJavaBridgeValueConverterTest, TypedArrays) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks_scope(
      context, v8::MicrotasksScope::kDoNotRunMicrotasks);

  std::unique_ptr<GinJavaBridgeValueConverter> converter(
      new GinJavaBridgeValueConverter());

  static constexpr char kSourceTemplate[] =
      "(function() {"
      "var array_buffer = new ArrayBuffer(%s);"
      "var array_view = new %s(array_buffer);"
      "array_view[0] = 42;"
      "return array_view;"
      "})();";
  const char* array_types[] = {
    "1", "Int8Array", "1", "Uint8Array", "1", "Uint8ClampedArray",
    "2", "Int16Array", "2", "Uint16Array",
    "4", "Int32Array", "4", "Uint32Array",
    "4", "Float32Array", "8", "Float64Array"
  };
  for (size_t i = 0; i < std::size(array_types); i += 2) {
    const char* typed_array_type = array_types[i + 1];
    v8::Local<v8::Script> script(
        v8::Script::Compile(
            context,
            v8::String::NewFromUtf8(
                isolate_, base::StringPrintf(kSourceTemplate, array_types[i],
                                             typed_array_type)
                              .c_str())
                .ToLocalChecked())
            .ToLocalChecked());
    v8::Local<v8::Value> v8_typed_array = script->Run(context).ToLocalChecked();
    std::unique_ptr<base::Value> list_value(
        converter->FromV8Value(v8_typed_array, context));
    ASSERT_TRUE(list_value.get()) << typed_array_type;
    ASSERT_TRUE(list_value->is_list()) << typed_array_type;
    EXPECT_EQ(1u, list_value->GetList().size()) << typed_array_type;

    const auto value = list_value->GetList().cbegin();
    if (value->type() == base::Value::Type::BINARY) {
      std::unique_ptr<const GinJavaBridgeValue> gin_value(
          GinJavaBridgeValue::FromValue(&*value));
      EXPECT_EQ(gin_value->GetType(), GinJavaBridgeValue::TYPE_UINT32);
      uint32_t first_element = 0;
      ASSERT_TRUE(gin_value->GetAsUInt32(&first_element));
      EXPECT_EQ(42u, first_element) << typed_array_type;

    } else {
      ASSERT_TRUE(value->is_double() || value->is_int()) << typed_array_type;
      EXPECT_EQ(42.0, value->GetDouble()) << typed_array_type;
    }
  }
}

}  // namespace content

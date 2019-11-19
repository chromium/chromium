// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/converter.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "gin/handle.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"
#include "gin/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace gin {

using v8::Array;
using v8::Boolean;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Null;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Undefined;
using v8::Value;

typedef V8Test ConverterTest;

TEST_F(ConverterTest, Bool) {
  HandleScope handle_scope(instance_->isolate());

  EXPECT_TRUE(Converter<bool>::ToV8(instance_->isolate(), true)->StrictEquals(
      Boolean::New(instance_->isolate(), true)));
  EXPECT_TRUE(Converter<bool>::ToV8(instance_->isolate(), false)->StrictEquals(
      Boolean::New(instance_->isolate(), false)));

  struct {
    Local<Value> input;
    bool expected;
  } test_data[] = {
      {Boolean::New(instance_->isolate(), false).As<Value>(), false},
      {Boolean::New(instance_->isolate(), true).As<Value>(), true},
      {Number::New(instance_->isolate(), 0).As<Value>(), false},
      {Number::New(instance_->isolate(), 1).As<Value>(), true},
      {Number::New(instance_->isolate(), -1).As<Value>(), true},
      {Number::New(instance_->isolate(), 0.1).As<Value>(), true},
      {String::NewFromUtf8(instance_->isolate(), "", v8::NewStringType::kNormal)
           .ToLocalChecked()
           .As<Value>(),
       false},
      {String::NewFromUtf8(instance_->isolate(), "foo",
                           v8::NewStringType::kNormal)
           .ToLocalChecked()
           .As<Value>(),
       true},
      {Object::New(instance_->isolate()).As<Value>(), true},
      {Null(instance_->isolate()).As<Value>(), false},
      {Undefined(instance_->isolate()).As<Value>(), false},
  };

  for (size_t i = 0; i < base::size(test_data); ++i) {
    bool result = false;
    EXPECT_TRUE(Converter<bool>::FromV8(instance_->isolate(),
                                        test_data[i].input, &result));
    EXPECT_EQ(test_data[i].expected, result);

    result = true;
    EXPECT_TRUE(Converter<bool>::FromV8(instance_->isolate(),
                                        test_data[i].input, &result));
    EXPECT_EQ(test_data[i].expected, result);
  }
}

TEST_F(ConverterTest, String16) {
  v8::Isolate* isolate = instance_->isolate();

  HandleScope handle_scope(isolate);

  EXPECT_TRUE(Converter<base::string16>::ToV8(isolate, base::ASCIIToUTF16(""))
                  ->StrictEquals(StringToV8(isolate, "")));
  EXPECT_TRUE(
      Converter<base::string16>::ToV8(isolate, base::ASCIIToUTF16("hello"))
          ->StrictEquals(StringToV8(isolate, "hello")));

  base::string16 result;

  ASSERT_FALSE(
      Converter<base::string16>::FromV8(isolate, v8::False(isolate), &result));
  ASSERT_FALSE(
      Converter<base::string16>::FromV8(isolate, v8::True(isolate), &result));
  ASSERT_TRUE(Converter<base::string16>::FromV8(
      isolate, v8::String::Empty(isolate), &result));
  EXPECT_EQ(result, base::string16());
  ASSERT_TRUE(Converter<base::string16>::FromV8(
      isolate, StringToV8(isolate, "hello"), &result));
  EXPECT_EQ(result, base::ASCIIToUTF16("hello"));
}

TEST_F(ConverterTest, Int32) {
  HandleScope handle_scope(instance_->isolate());

  int test_data_to[] = {-1, 0, 1};
  for (size_t i = 0; i < base::size(test_data_to); ++i) {
    EXPECT_TRUE(Converter<int32_t>::ToV8(instance_->isolate(), test_data_to[i])
                    ->StrictEquals(
                          Integer::New(instance_->isolate(), test_data_to[i])));
  }

  struct {
    v8::Local<v8::Value> input;
    bool expect_success;
    int expected_result;
  } test_data_from[] = {
      {Boolean::New(instance_->isolate(), false).As<Value>(), false, 0},
      {Boolean::New(instance_->isolate(), true).As<Value>(), false, 0},
      {Integer::New(instance_->isolate(), -1).As<Value>(), true, -1},
      {Integer::New(instance_->isolate(), 0).As<Value>(), true, 0},
      {Integer::New(instance_->isolate(), 1).As<Value>(), true, 1},
      {Number::New(instance_->isolate(), -1).As<Value>(), true, -1},
      {Number::New(instance_->isolate(), 1.1).As<Value>(), false, 0},
      {String::NewFromUtf8(instance_->isolate(), "42",
                           v8::NewStringType::kNormal)
           .ToLocalChecked()
           .As<Value>(),
       false, 0},
      {String::NewFromUtf8(instance_->isolate(), "foo",
                           v8::NewStringType::kNormal)
           .ToLocalChecked()
           .As<Value>(),
       false, 0},
      {Object::New(instance_->isolate()).As<Value>(), false, 0},
      {Array::New(instance_->isolate()).As<Value>(), false, 0},
      {v8::Null(instance_->isolate()).As<Value>(), false, 0},
      {v8::Undefined(instance_->isolate()).As<Value>(), false, 0},
  };

  for (size_t i = 0; i < base::size(test_data_from); ++i) {
    int32_t result = std::numeric_limits<int32_t>::min();
    bool success = Converter<int32_t>::FromV8(instance_->isolate(),
                                              test_data_from[i].input, &result);
    EXPECT_EQ(test_data_from[i].expect_success, success) << i;
    if (success)
      EXPECT_EQ(test_data_from[i].expected_result, result) << i;
  }
}

TEST_F(ConverterTest, Vector) {
  HandleScope handle_scope(instance_->isolate());

  std::vector<int> expected;
  expected.push_back(-1);
  expected.push_back(0);
  expected.push_back(1);

  auto js_array =
      Converter<std::vector<int>>::ToV8(instance_->isolate(), expected)
          .As<Array>();
  EXPECT_EQ(3u, js_array->Length());
  v8::Local<v8::Context> context = instance_->isolate()->GetCurrentContext();
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_TRUE(
        Integer::New(instance_->isolate(), expected[i])
            ->StrictEquals(
                js_array->Get(context, static_cast<int>(i)).ToLocalChecked()));
  }
}

TEST_F(ConverterTest, VectorOfVectors) {
  HandleScope handle_scope(instance_->isolate());

  std::vector<std::vector<int>> vector_of_vectors = {
      {1, 2, 3}, {4, 5, 6},
  };

  v8::Local<v8::Value> v8_value =
      ConvertToV8(instance_->isolate(), vector_of_vectors);
  std::vector<std::vector<int>> out_value;
  ASSERT_TRUE(ConvertFromV8(instance_->isolate(), v8_value, &out_value));
  EXPECT_THAT(out_value, testing::ContainerEq(vector_of_vectors));
}

namespace {

class MyObject : public Wrappable<MyObject> {
 public:
  static WrapperInfo kWrapperInfo;

  static gin::Handle<MyObject> Create(v8::Isolate* isolate) {
    return CreateHandle(isolate, new MyObject());
  }
};

WrapperInfo MyObject::kWrapperInfo = {kEmbedderNativeGin};

}  // namespace

TEST_F(ConverterTest, VectorOfWrappables) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  Handle<MyObject> obj = MyObject::Create(isolate);
  std::vector<MyObject*> vector = {obj.get()};
  v8::MaybeLocal<v8::Value> maybe = ConvertToV8(isolate, vector);
  v8::Local<v8::Value> array;
  ASSERT_TRUE(maybe.ToLocal(&array));
  v8::Local<v8::Value> array2;
  ASSERT_TRUE(TryConvertToV8(isolate, vector, &array2));

  std::vector<MyObject*> out_value;
  ASSERT_TRUE(ConvertFromV8(isolate, array, &out_value));
  EXPECT_THAT(out_value, testing::ContainerEq(vector));
  std::vector<MyObject*> out_value2;
  ASSERT_TRUE(ConvertFromV8(isolate, array2, &out_value2));
  EXPECT_THAT(out_value2, testing::ContainerEq(vector));
}

}  // namespace gin

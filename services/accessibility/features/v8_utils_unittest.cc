// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/v8_utils.h"

#include <memory>

#include "base/logging.h"
#include "base/test/task_environment.h"
#include "gin/array_buffer.h"
#include "gin/public/isolate_holder.h"
#include "services/accessibility/features/bindings_isolate_holder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8-array-buffer.h"
#include "v8-container.h"
#include "v8-isolate.h"
#include "v8-local-handle.h"
#include "v8-object.h"
#include "v8-primitive.h"
#include "v8-template.h"

namespace ax {

class V8ValueConverterTest : public testing::Test,
                             public BindingsIsolateHolder {
 public:
  V8ValueConverterTest() {
    BindingsIsolateHolder::InitializeV8();

    isolate_holder_ = std::make_unique<gin::IsolateHolder>(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        gin::IsolateHolder::kSingleThread,
        gin::IsolateHolder::IsolateType::kTest);
    isolate_ = isolate_holder_->isolate();
  }

  // BindingsIsolateHolder:
  v8::Isolate* GetIsolate() const override { return isolate_; }
  v8::Local<v8::Context> GetContext() const override {
    v8::EscapableHandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context =
        v8::Local<v8::Context>::New(isolate_, context_);
    return handle_scope.Escape(context);
  }
  void HandleError(const std::string& message) override {
    LOG(ERROR) << message;
  }

 protected:
  void SetUp() override {
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate_);
    context_.Reset(isolate_, v8::Context::New(isolate_, nullptr, global));
  }

  void TearDown() override { context_.Reset(); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<gin::IsolateHolder> isolate_holder_;
  raw_ptr<v8::Isolate> isolate_ = nullptr;

  // Context for the JavaScript in the test.
  v8::Persistent<v8::Context> context_;
};

// This test covers all data types that accessibility uses to dispatch events.
// They are all added to a dictionary (which is also a type used), and makes
// sure the final conversion has the right types.
TEST_F(V8ValueConverterTest, DictWithBasicTypes) {
  base::Value::Dict dict;
  dict.Set("boolean", true);
  dict.Set("integer", 2);
  dict.Set("double", 2.0f);
  dict.Set("string", "hello world");

  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = GetContext();
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> v8_object = V8ValueConverter::GetInstance()
                                        ->ConvertToV8Value(dict, context)
                                        .As<v8::Object>();
  ASSERT_FALSE(v8_object.IsEmpty());

  EXPECT_EQ(dict.size(),
            v8_object->GetPropertyNames(context).ToLocalChecked()->Length());

  EXPECT_TRUE(v8_object
                  ->Get(context, v8::String::NewFromUtf8(
                                     isolate_, "boolean",
                                     v8::NewStringType::kInternalized)
                                     .ToLocalChecked())
                  .ToLocalChecked()
                  ->IsTrue());
  EXPECT_TRUE(v8_object
                  ->Get(context, v8::String::NewFromUtf8(
                                     isolate_, "integer",
                                     v8::NewStringType::kInternalized)
                                     .ToLocalChecked())
                  .ToLocalChecked()
                  ->IsInt32());
  EXPECT_TRUE(v8_object
                  ->Get(context, v8::String::NewFromUtf8(
                                     isolate_, "double",
                                     v8::NewStringType::kInternalized)
                                     .ToLocalChecked())
                  .ToLocalChecked()
                  ->IsNumber());
  EXPECT_TRUE(v8_object
                  ->Get(context, v8::String::NewFromUtf8(
                                     isolate_, "string",
                                     v8::NewStringType::kInternalized)
                                     .ToLocalChecked())
                  .ToLocalChecked()
                  ->IsString());
}

// Tests the conversion of the list type, used as the arguments list to function
// calls.
TEST_F(V8ValueConverterTest, ConvertsList) {
  base::Value::List list;
  list.Append(true);
  list.Append(2);

  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = GetContext();
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Array> v8_array = V8ValueConverter::GetInstance()
                                      ->ConvertToV8Value(list, context)
                                      .As<v8::Array>();

  EXPECT_EQ(list.size(), v8_array->Length());
  EXPECT_TRUE(v8_array->Get(context, v8::Integer::New(isolate_, 0))
                  .ToLocalChecked()
                  ->IsTrue());
  EXPECT_TRUE(v8_array->Get(context, v8::Integer::New(isolate_, 1))
                  .ToLocalChecked()
                  ->IsInt32());
}

TEST_F(V8ValueConverterTest, ConvertsBlogStorage) {
  base::Value::BlobStorage storage({1u, 2u});

  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = GetContext();
  v8::Context::Scope context_scope(context);

  v8::Local<v8::ArrayBuffer> v8_array = V8ValueConverter::GetInstance()
                                            ->ConvertToV8Value(storage, context)
                                            .As<v8::ArrayBuffer>();

  EXPECT_EQ(storage.size(), v8_array->ByteLength());
}

}  // namespace ax

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/activity_log_converter_strategy.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace extensions {

class ActivityLogConverterStrategyTest : public testing::Test {
 public:
  ActivityLogConverterStrategyTest()
      : isolate_(v8::Isolate::GetCurrent()),
        handle_scope_(isolate_),
        context_(isolate_, v8::Context::New(isolate_)),
        context_scope_(context()) {}

 protected:
  void SetUp() override {
    converter_ = content::V8ValueConverter::Create();
    strategy_ = std::make_unique<ActivityLogConverterStrategy>();
    converter_->SetFunctionAllowed(true);
    converter_->SetStrategy(strategy_.get());
  }

  testing::AssertionResult VerifyNull(v8::Local<v8::Value> v8_value) {
    std::unique_ptr<base::Value> value(
        converter_->FromV8Value(v8_value, context()));
    if (value->is_none())
      return testing::AssertionSuccess();
    return testing::AssertionFailure();
  }

  testing::AssertionResult VerifyBoolean(v8::Local<v8::Value> v8_value,
                                         bool expected) {
    std::unique_ptr<base::Value> value(
        converter_->FromV8Value(v8_value, context()));
    if (value->is_bool() && value->GetBool() == expected)
      return testing::AssertionSuccess();
    return testing::AssertionFailure();
  }

  testing::AssertionResult VerifyInteger(v8::Local<v8::Value> v8_value,
                                         int expected) {
    std::unique_ptr<base::Value> value(
        converter_->FromV8Value(v8_value, context()));
    if (value->is_int() && value->GetInt() == expected)
      return testing::AssertionSuccess();
    return testing::AssertionFailure();
  }

  testing::AssertionResult VerifyDouble(v8::Local<v8::Value> v8_value,
                                        double expected) {
    std::unique_ptr<base::Value> value(
        converter_->FromV8Value(v8_value, context()));
    if (value->is_double() && value->GetDouble() == expected)
      return testing::AssertionSuccess();
    return testing::AssertionFailure();
  }

  testing::AssertionResult VerifyString(v8::Local<v8::Value> v8_value,
                                        const std::string& expected) {
    std::unique_ptr<base::Value> value(
        converter_->FromV8Value(v8_value, context()));
    if (value->is_string() && value->GetString() == expected)
      return testing::AssertionSuccess();
    return testing::AssertionFailure();
  }

  v8::Local<v8::Context> context() const {
    return v8::Local<v8::Context>::New(isolate_, context_);
  }

  raw_ptr<v8::Isolate> isolate_;
  v8::HandleScope handle_scope_;
  v8::Global<v8::Context> context_;
  v8::Context::Scope context_scope_;
  std::unique_ptr<content::V8ValueConverter> converter_;
  std::unique_ptr<ActivityLogConverterStrategy> strategy_;
};

TEST_F(ActivityLogConverterStrategyTest, ConversionTest) {
  const char* source = "(function() {"
      "function foo() {}"
      "return {"
        "null: null,"
        "true: true,"
        "false: false,"
        "positive_int: 42,"
        "negative_int: -42,"
        "zero: 0,"
        "double: 88.8,"
        "big_integral_double: 9007199254740992.0,"  // 2.0^53
        "string: \"foobar\","
        "empty_string: \"\","
        "dictionary: {"
          "foo: \"bar\","
          "hot: \"dog\","
        "},"
        "empty_dictionary: {},"
        "list: [ \"bar\", \"foo\" ],"
        "empty_list: [],"
        "function: (0, function() {}),"  // ensure function is anonymous
        "named_function: foo"
      "};"
      "})();";

  v8::Local<v8::Context> context = context_.Get(isolate_);
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::Script> script(
      v8::Script::Compile(
          context, v8::String::NewFromUtf8(isolate_, source,
                                           v8::NewStringType::kInternalized)
                       .ToLocalChecked())
          .ToLocalChecked());
  v8::Local<v8::Object> v8_object =
      script->Run(context).ToLocalChecked().As<v8::Object>();

  EXPECT_TRUE(VerifyString(v8_object, "[Object]"));
  EXPECT_TRUE(VerifyNull(
      v8_object
          ->Get(context, v8::String::NewFromUtf8(
                             isolate_, "null", v8::NewStringType::kInternalized)
                             .ToLocalChecked())
          .ToLocalChecked()));
  EXPECT_TRUE(VerifyBoolean(
      v8_object
          ->Get(context, v8::String::NewFromUtf8(
                             isolate_, "true", v8::NewStringType::kInternalized)
                             .ToLocalChecked())
          .ToLocalChecked(),
      true));
  EXPECT_TRUE(VerifyBoolean(
      v8_object
          ->Get(context,
                v8::String::NewFromUtf8(isolate_, "false",
                                        v8::NewStringType::kInternalized)
                    .ToLocalChecked())
          .ToLocalChecked(),
      false));
  EXPECT_TRUE(VerifyInteger(
      v8_object
          ->Get(context,
                v8::String::NewFromUtf8(isolate_, "positive_int",
                                        v8::NewStringType::kInternalized)
                    .ToLocalChecked())
          .ToLocalChecked(),
      42));
  EXPECT_TRUE(VerifyInteger(
      v8_object
          ->Get(context,
                v8::String::NewFromUtf8(isolate_, "negative_int",
                                        v8::NewStringType::kInternalized)
                    .ToLocalChecked())
          .ToLocalChecked(),
      -42));
  EXPECT_TRUE(VerifyInteger(
      v8_object
          ->Get(context, v8::String::NewFromUtf8(
                             isolate_, "zero", v8::NewStringType::kInternalized)
                             .ToLocalChecked())
          .ToLocalChecked(),
      0));
  EXPECT_TRUE(VerifyDouble(
      v8_object
          ->Get(context,
                v8::String::NewFromUtf8(isolate_, "double",
                                        v8::NewStringType::kInternalized)
                    .ToLocalChecked())
          .ToLocalChecked(),
      88.8));
  EXPECT_TRUE(VerifyDouble(
      v8_object
          ->Get(context,
                v8::String::NewFromUtf8(isolate_, "big_integral_double",
                                        v8::NewStringType::kInternalized)
                    .ToLocalChecked())
          .ToLocalChecked(),
      9007199254740992.0));
  EXPECT_TRUE(VerifyString(
      v8_object
          ->Get(context,
                v8::String::NewFromUtf8(isolate_, "string",
                                        v8::NewStringType::kInternalized)
                    .ToLocalChecked())
          .ToLocalChecked(),
      "foobar"));
  EXPECT_TRUE(VerifyString(
      v8_object
          ->Get(context,
                v8::String::NewFromUtf8(isolate_, "empty_string",
                                        v8::NewStringType::kInternalized)
                    .ToLocalChecked())
          .ToLocalChecked(),
      ""));
  EXPECT_TRUE(VerifyString(
      v8_object
          ->Get(context,
                v8::String::NewFromUtf8(isolate_, "dictionary",
                                        v8::NewStringType::kInternalized)
                    .ToLocalChecked())
          .ToLocalChecked(),
      "[Object]"));
  EXPECT_TRUE(VerifyString(
      v8_object
          ->Get(context,
                v8::String::NewFromUtf8(isolate_, "empty_dictionary",
                                        v8::NewStringType::kInternalized)
                    .ToLocalChecked())
          .ToLocalChecked(),
      "[Object]"));
  EXPECT_TRUE(VerifyString(
      v8_object
          ->Get(context, v8::String::NewFromUtf8(
                             isolate_, "list", v8::NewStringType::kInternalized)
                             .ToLocalChecked())
          .ToLocalChecked(),
      "[Array]"));
  EXPECT_TRUE(VerifyString(
      v8_object
          ->Get(context,
                v8::String::NewFromUtf8(isolate_, "empty_list",
                                        v8::NewStringType::kInternalized)
                    .ToLocalChecked())
          .ToLocalChecked(),
      "[Array]"));
  EXPECT_TRUE(VerifyString(
      v8_object
          ->Get(context,
                v8::String::NewFromUtf8(isolate_, "function",
                                        v8::NewStringType::kInternalized)
                    .ToLocalChecked())
          .ToLocalChecked(),
      "[Function]"));
  EXPECT_TRUE(VerifyString(
      v8_object
          ->Get(context,
                v8::String::NewFromUtf8(isolate_, "named_function",
                                        v8::NewStringType::kInternalized)
                    .ToLocalChecked())
          .ToLocalChecked(),
      "[Function foo()]"));
}

}  // namespace extensions

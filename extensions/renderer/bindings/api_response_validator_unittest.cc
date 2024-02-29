// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_response_validator.h"

#include <optional>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/api_invocation_errors.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/bindings/api_type_reference_map.h"
#include "extensions/renderer/bindings/argument_spec.h"
#include "extensions/renderer/bindings/argument_spec_builder.h"
#include "extensions/renderer/bindings/returns_async_builder.h"
#include "gin/converter.h"
#include "v8/include/v8.h"

namespace extensions {
namespace {

constexpr char kMethodName[] = "oneString";

std::unique_ptr<APISignature> OneStringCallbackSignature() {
  std::vector<std::unique_ptr<ArgumentSpec>> empty_specs;
  std::vector<std::unique_ptr<ArgumentSpec>> async_specs;
  async_specs.push_back(
      ArgumentSpecBuilder(ArgumentType::STRING, "str").Build());
  return std::make_unique<APISignature>(
      std::move(empty_specs),
      ReturnsAsyncBuilder(std::move(async_specs)).Build(), nullptr);
}

v8::LocalVector<v8::Value> StringToV8Vector(v8::Local<v8::Context> context,
                                            const char* args) {
  v8::Local<v8::Value> v8_args = V8ValueFromScriptSource(context, args);
  if (v8_args.IsEmpty()) {
    ADD_FAILURE() << "Could not convert args: " << args;
    return v8::LocalVector<v8::Value>(context->GetIsolate());
  }
  EXPECT_TRUE(v8_args->IsArray());
  v8::LocalVector<v8::Value> vector_args(context->GetIsolate());
  EXPECT_TRUE(gin::ConvertFromV8(context->GetIsolate(), v8_args, &vector_args));
  return vector_args;
}

}  // namespace

class APIResponseValidatorTest : public APIBindingTest {
 public:
  APIResponseValidatorTest()
      : type_refs_(APITypeReferenceMap::InitializeTypeCallback()),
        test_handler_(
            base::BindRepeating(&APIResponseValidatorTest::OnValidationFailure,
                                base::Unretained(this))),
        validator_(&type_refs_) {}

  APIResponseValidatorTest(const APIResponseValidatorTest&) = delete;
  APIResponseValidatorTest& operator=(const APIResponseValidatorTest&) = delete;

  ~APIResponseValidatorTest() override = default;

  void SetUp() override {
    APIBindingTest::SetUp();
    response_validation_override_ =
        binding::SetResponseValidationEnabledForTesting(true);
    type_refs_.AddAPIMethodSignature(kMethodName, OneStringCallbackSignature());
  }

  void TearDown() override {
    response_validation_override_.reset();
    APIBindingTest::TearDown();
  }

  APIResponseValidator* validator() { return &validator_; }
  const std::optional<std::string>& failure_method() const {
    return failure_method_;
  }
  const std::optional<std::string>& failure_error() const {
    return failure_error_;
  }

  void reset() {
    failure_method_ = std::nullopt;
    failure_error_ = std::nullopt;
  }

 private:
  void OnValidationFailure(const std::string& method,
                           const std::string& error) {
    failure_method_ = method;
    failure_error_ = error;
  }

  std::unique_ptr<base::AutoReset<bool>> response_validation_override_;

  APITypeReferenceMap type_refs_;
  APIResponseValidator::TestHandler test_handler_;
  APIResponseValidator validator_;

  std::optional<std::string> failure_method_;
  std::optional<std::string> failure_error_;
};

TEST_F(APIResponseValidatorTest, TestValidation) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  validator()->ValidateResponse(
      context, kMethodName, StringToV8Vector(context, "['hi']"), std::string(),
      APIResponseValidator::CallbackType::kCallerProvided);
  EXPECT_FALSE(failure_method());
  EXPECT_FALSE(failure_error());

  validator()->ValidateResponse(
      context, kMethodName, StringToV8Vector(context, "[1]"), std::string(),
      APIResponseValidator::CallbackType::kCallerProvided);
  EXPECT_EQ(kMethodName, failure_method().value_or("no value"));
  EXPECT_EQ(api_errors::ArgumentError(
                "str", api_errors::InvalidType(api_errors::kTypeString,
                                               api_errors::kTypeInteger)),
            failure_error().value_or("no value"));
}

TEST_F(APIResponseValidatorTest, TestDoesNotValidateWithAPIProvidedCallback) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  validator()->ValidateResponse(
      context, kMethodName, StringToV8Vector(context, "['hi']"), std::string(),
      APIResponseValidator::CallbackType::kCallerProvided);
  EXPECT_FALSE(failure_method());
  EXPECT_FALSE(failure_error());

  validator()->ValidateResponse(
      context, kMethodName, StringToV8Vector(context, "[1]"), std::string(),
      APIResponseValidator::CallbackType::kAPIProvided);
  EXPECT_FALSE(failure_method());
  EXPECT_FALSE(failure_error());
}

TEST_F(APIResponseValidatorTest, TestDoesNotValidateWhenAPIErrorPresent) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  validator()->ValidateResponse(
      context, kMethodName, v8::LocalVector<v8::Value>(isolate()),
      "Some API Error", APIResponseValidator::CallbackType::kCallerProvided);
  EXPECT_FALSE(failure_method());
  EXPECT_FALSE(failure_error());
}

TEST_F(APIResponseValidatorTest, TestDoesNotValidateWithUnknownSignature) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  validator()->ValidateResponse(
      context, "differentMethodName", StringToV8Vector(context, "[true]"),
      std::string(), APIResponseValidator::CallbackType::kCallerProvided);
  EXPECT_FALSE(failure_method());
  EXPECT_FALSE(failure_error());
}

}  // namespace extensions

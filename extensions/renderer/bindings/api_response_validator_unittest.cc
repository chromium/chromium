// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_response_validator.h"

#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/optional.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/api_invocation_errors.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/bindings/api_type_reference_map.h"
#include "extensions/renderer/bindings/argument_spec.h"
#include "extensions/renderer/bindings/argument_spec_builder.h"
#include "gin/converter.h"
#include "v8/include/v8.h"

namespace extensions {
namespace {

constexpr char kMethodName[] = "oneString";

std::unique_ptr<APISignature> OneStringSignature() {
  std::vector<std::unique_ptr<ArgumentSpec>> specs;
  specs.push_back(ArgumentSpecBuilder(ArgumentType::STRING, "str").Build());
  return std::make_unique<APISignature>(std::move(specs));
}

std::vector<v8::Local<v8::Value>> StringToV8Vector(
    v8::Local<v8::Context> context,
    const char* args) {
  v8::Local<v8::Value> v8_args = V8ValueFromScriptSource(context, args);
  if (v8_args.IsEmpty()) {
    ADD_FAILURE() << "Could not convert args: " << args;
    return std::vector<v8::Local<v8::Value>>();
  }
  EXPECT_TRUE(v8_args->IsArray());
  std::vector<v8::Local<v8::Value>> vector_args;
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
  ~APIResponseValidatorTest() override = default;

  void SetUp() override {
    APIBindingTest::SetUp();
    response_validation_override_ =
        binding::SetResponseValidationEnabledForTesting(true);
    type_refs_.AddCallbackSignature(kMethodName, OneStringSignature());
  }

  void TearDown() override {
    response_validation_override_.reset();
    APIBindingTest::TearDown();
  }

  APIResponseValidator* validator() { return &validator_; }
  const base::Optional<std::string>& failure_method() const {
    return failure_method_;
  }
  const base::Optional<std::string>& failure_error() const {
    return failure_error_;
  }

  void reset() {
    failure_method_ = base::nullopt;
    failure_error_ = base::nullopt;
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

  base::Optional<std::string> failure_method_;
  base::Optional<std::string> failure_error_;

  DISALLOW_COPY_AND_ASSIGN(APIResponseValidatorTest);
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
      context, kMethodName, {}, "Some API Error",
      APIResponseValidator::CallbackType::kCallerProvided);
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

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_request_handler.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/exception_handler.h"
#include "extensions/renderer/bindings/test_interaction_provider.h"
#include "extensions/renderer/bindings/test_js_runner.h"
#include "extensions/renderer/v8_helpers.h"
#include "gin/converter.h"
#include "gin/function_template.h"
#include "gin/public/context_holder.h"
#include "gin/public/isolate_holder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

namespace {

const char kEchoArgs[] =
    "(function() { this.result = Array.from(arguments); })";

const char kMethod[] = "method";

// TODO(devlin): We should probably hoist this up to e.g. api_binding_types.h.
using ArgumentList = v8::LocalVector<v8::Value>;

// TODO(devlin): Should we move some parts of api_binding_unittest.cc to here?

}  // namespace

class APIRequestHandlerTest : public APIBindingTest {
 public:
  APIRequestHandlerTest(const APIRequestHandlerTest&) = delete;
  APIRequestHandlerTest& operator=(const APIRequestHandlerTest&) = delete;

  std::unique_ptr<APIRequestHandler> CreateRequestHandler() {
    return std::make_unique<APIRequestHandler>(
        base::DoNothing(),
        APILastError(APILastError::GetParent(), binding::AddConsoleError()),
        exception_handler(), interaction_provider());
  }

  void SaveUserActivationState(v8::Local<v8::Context> context,
                               std::optional<bool>* ran_with_user_gesture) {
    *ran_with_user_gesture =
        interaction_provider()->HasActiveInteraction(context);
  }

 protected:
  APIRequestHandlerTest() {}
  ~APIRequestHandlerTest() override {}

  std::unique_ptr<TestJSRunner::Scope> CreateTestJSRunner() override {
    return std::make_unique<TestJSRunner::Scope>(
        std::make_unique<TestJSRunner>(base::BindRepeating(
            &APIRequestHandlerTest::SetDidRunJS, base::Unretained(this))));
  }

  InteractionProvider* interaction_provider() {
    if (!interaction_provider_)
      interaction_provider_ = std::make_unique<TestInteractionProvider>();
    return interaction_provider_.get();
  }

  ExceptionHandler* exception_handler() {
    if (!exception_handler_) {
      exception_handler_ =
          std::make_unique<ExceptionHandler>(binding::AddConsoleError());
    }
    return exception_handler_.get();
  }

  bool did_run_js() const { return did_run_js_; }

 private:
  void SetDidRunJS() { did_run_js_ = true; }

  bool did_run_js_ = false;
  std::unique_ptr<TestInteractionProvider> interaction_provider_;
  std::unique_ptr<ExceptionHandler> exception_handler_;
};

// Tests adding a request to the request handler, and then triggering the
// response.
TEST_F(APIRequestHandlerTest, AddRequestAndCompleteRequestTest) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  std::unique_ptr<APIRequestHandler> request_handler = CreateRequestHandler();

  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());

  v8::Local<v8::Function> function = FunctionFromString(context, kEchoArgs);
  ASSERT_FALSE(function.IsEmpty());

  request_handler->StartRequest(context, kMethod, base::Value::List(),
                                binding::AsyncResponseType::kCallback, function,
                                v8::Local<v8::Function>(),
                                binding::ResultModifierFunction());
  int request_id = request_handler->last_sent_request_id();
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  const char kArguments[] = "['foo',1,{'prop1':'bar'}]";
  request_handler->CompleteRequest(request_id, ListValueFromString(kArguments),
                                   std::string());

  EXPECT_TRUE(did_run_js());
  EXPECT_EQ(ReplaceSingleQuotes(kArguments),
            GetStringPropertyFromObject(context->Global(), context, "result"));

  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());

  request_handler->StartRequest(
      context, kMethod, base::Value::List(), binding::AsyncResponseType::kNone,
      v8::Local<v8::Function>(), v8::Local<v8::Function>(),
      binding::ResultModifierFunction());
  request_id = request_handler->last_sent_request_id();
  EXPECT_NE(-1, request_id);
  request_handler->CompleteRequest(request_id, base::Value::List(),
                                   std::string());
}

// Tests that trying to run non-existent or invalided requests is a no-op.
TEST_F(APIRequestHandlerTest, InvalidRequestsTest) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  std::unique_ptr<APIRequestHandler> request_handler = CreateRequestHandler();

  v8::Local<v8::Function> function = FunctionFromString(context, kEchoArgs);
  ASSERT_FALSE(function.IsEmpty());

  request_handler->StartRequest(context, kMethod, base::Value::List(),
                                binding::AsyncResponseType::kCallback, function,
                                v8::Local<v8::Function>(),
                                binding::ResultModifierFunction());
  int request_id = request_handler->last_sent_request_id();
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  // Try running with a non-existent request id.
  int fake_request_id = 42;
  request_handler->CompleteRequest(
      fake_request_id, ListValueFromString("['foo']"), std::string());
  EXPECT_FALSE(did_run_js());

  // Try running with a request from an invalidated context.
  request_handler->InvalidateContext(context);
  request_handler->CompleteRequest(request_id, ListValueFromString("['foo']"),
                                   std::string());
  EXPECT_FALSE(did_run_js());
}

TEST_F(APIRequestHandlerTest, MultipleRequestsAndContexts) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context_a = MainContext();
  v8::Local<v8::Context> context_b = AddContext();

  std::unique_ptr<APIRequestHandler> request_handler = CreateRequestHandler();

  // By having both different arguments and different behaviors in the
  // callbacks, we can easily verify that the right function is called in the
  // right context.
  v8::Local<v8::Function> function_a = FunctionFromString(
      context_a, "(function(res) { this.result = res + 'alpha'; })");
  v8::Local<v8::Function> function_b = FunctionFromString(
      context_b, "(function(res) { this.result = res + 'beta'; })");

  request_handler->StartRequest(context_a, kMethod, base::Value::List(),
                                binding::AsyncResponseType::kCallback,
                                function_a, v8::Local<v8::Function>(),
                                binding::ResultModifierFunction());
  int request_a = request_handler->last_sent_request_id();
  request_handler->StartRequest(context_b, kMethod, base::Value::List(),
                                binding::AsyncResponseType::kCallback,
                                function_b, v8::Local<v8::Function>(),
                                binding::ResultModifierFunction());
  int request_b = request_handler->last_sent_request_id();

  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_a, request_b));

  request_handler->CompleteRequest(
      request_a, ListValueFromString("['response_a:']"), std::string());
  EXPECT_TRUE(did_run_js());
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_b));

  EXPECT_EQ(
      ReplaceSingleQuotes("'response_a:alpha'"),
      GetStringPropertyFromObject(context_a->Global(), context_a, "result"));

  request_handler->CompleteRequest(
      request_b, ListValueFromString("['response_b:']"), std::string());
  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());

  EXPECT_EQ(
      ReplaceSingleQuotes("'response_b:beta'"),
      GetStringPropertyFromObject(context_b->Global(), context_b, "result"));
}

TEST_F(APIRequestHandlerTest, CustomCallbackArguments) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  std::unique_ptr<APIRequestHandler> request_handler = CreateRequestHandler();

  v8::Local<v8::Function> custom_callback =
      FunctionFromString(context, kEchoArgs);
  v8::Local<v8::Function> callback = FunctionFromString(
      context, "(function(arg) {this.callbackCalled = arg})");
  ASSERT_FALSE(callback.IsEmpty());
  ASSERT_FALSE(custom_callback.IsEmpty());

  request_handler->StartRequest(context, "method", base::Value::List(),
                                binding::AsyncResponseType::kCallback, callback,
                                custom_callback,
                                binding::ResultModifierFunction());
  int request_id = request_handler->last_sent_request_id();
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  request_handler->CompleteRequest(
      request_id, ListValueFromString("['response', 'arguments']"),
      std::string());

  EXPECT_TRUE(did_run_js());
  v8::Local<v8::Array> result;
  ASSERT_TRUE(
      GetPropertyFromObjectAs(context->Global(), context, "result", &result));
  ArgumentList args(isolate());
  ASSERT_TRUE(gin::Converter<ArgumentList>::FromV8(isolate(), result, &args));
  ASSERT_EQ(3u, args.size());
  EXPECT_TRUE(args[0]->IsFunction());
  EXPECT_EQ(R"("response")", V8ToString(args[1], context));
  EXPECT_EQ(R"("arguments")", V8ToString(args[2], context));

  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());

  // The function passed to the custom callback isn't actually the same callback
  // that was passed in when calling the API, but invoking it below should still
  // result in the original callback being run.
  EXPECT_TRUE(
      GetPropertyFromObject(context->Global(), context, "callbackCalled")
          ->IsUndefined());
  v8::Local<v8::Value> callback_args[] = {gin::StringToV8(isolate(), "foo")};
  RunFunctionOnGlobal(args[0].As<v8::Function>(), context, 1, callback_args);

  EXPECT_EQ(R"("foo")", GetStringPropertyFromObject(context->Global(), context,
                                                    "callbackCalled"));
}

TEST_F(APIRequestHandlerTest, CustomCallbackWithErrorInExtensionCallback) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  auto add_console_error = [](std::optional<std::string>* error_out,
                              v8::Local<v8::Context> context,
                              const std::string& error) { *error_out = error; };

  std::optional<std::string> logged_error;
  ExceptionHandler exception_handler(
      base::BindRepeating(add_console_error, &logged_error));

  APIRequestHandler request_handler(
      base::DoNothing(),
      APILastError(APILastError::GetParent(), binding::AddConsoleError()),
      &exception_handler, interaction_provider());

  constexpr char kExtensionCallback[] =
      R"((function() {
           this.callbackCalled = true;
           throw new Error('hello');
         }))";

  v8::Local<v8::Function> callback_throwing_error =
      FunctionFromString(context, kExtensionCallback);
  constexpr char kCustomCallback[] =
      R"((function(callback) {
           this.customCallbackCalled = true;
           callback();
         }))";
  v8::Local<v8::Function> custom_callback =
      FunctionFromString(context, kCustomCallback);
  ASSERT_FALSE(callback_throwing_error.IsEmpty());
  ASSERT_FALSE(custom_callback.IsEmpty());

  request_handler.StartRequest(context, "method", base::Value::List(),
                               binding::AsyncResponseType::kCallback,
                               callback_throwing_error, custom_callback,
                               binding::ResultModifierFunction());
  int request_id = request_handler.last_sent_request_id();
  EXPECT_THAT(request_handler.GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  v8::TryCatch try_catch(isolate());
  {
    TestJSRunner::AllowErrors allow_errors;
    request_handler.CompleteRequest(request_id, base::Value::List(),
                                    std::string());
  }

  EXPECT_TRUE(did_run_js());
  EXPECT_TRUE(request_handler.GetPendingRequestIdsForTesting().empty());

  EXPECT_EQ("true", GetStringPropertyFromObject(context->Global(), context,
                                                "customCallbackCalled"));
  EXPECT_EQ("true", GetStringPropertyFromObject(context->Global(), context,
                                                "callbackCalled"));

  // The `try_catch` should not have caught an error. This is important to not
  // disrupt our bindings code (or other running JS) when asynchronously
  // returning from an API call. Instead, the error should be caught and handled
  // by the exception handler.
  EXPECT_FALSE(try_catch.HasCaught());
  ASSERT_TRUE(logged_error);
  EXPECT_THAT(*logged_error,
              testing::StartsWith("Error handling response: Error: hello"));
}

TEST_F(APIRequestHandlerTest, CustomCallbackPromiseBased) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  std::unique_ptr<APIRequestHandler> request_handler = CreateRequestHandler();

  v8::Local<v8::Function> custom_callback =
      FunctionFromString(context, kEchoArgs);
  ASSERT_FALSE(custom_callback.IsEmpty());

  v8::Local<v8::Promise> promise = request_handler->StartRequest(
      context, "method", base::Value::List(),
      binding::AsyncResponseType::kPromise, v8::Local<v8::Function>(),
      custom_callback, binding::ResultModifierFunction());
  ASSERT_FALSE(promise.IsEmpty());

  int request_id = request_handler->last_sent_request_id();
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  request_handler->CompleteRequest(
      request_id, ListValueFromString("['response', 'arguments']"),
      std::string());

  EXPECT_TRUE(did_run_js());
  v8::Local<v8::Array> result;
  ASSERT_TRUE(
      GetPropertyFromObjectAs(context->Global(), context, "result", &result));
  ArgumentList args(isolate());
  ASSERT_TRUE(gin::Converter<ArgumentList>::FromV8(isolate(), result, &args));
  ASSERT_EQ(3u, args.size());
  // Even though this is a promise based request the custom callbacks expect a
  // function argument to be passed to them, hence why we get a function here.
  // Invoking the callback however, should still result in the promise being
  // resolved.
  EXPECT_TRUE(args[0]->IsFunction());
  EXPECT_EQ(R"("response")", V8ToString(args[1], context));
  EXPECT_EQ(R"("arguments")", V8ToString(args[2], context));

  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());

  EXPECT_EQ(v8::Promise::kPending, promise->State());
  v8::Local<v8::Value> callback_args[] = {gin::StringToV8(isolate(), "foo")};
  RunFunctionOnGlobal(args[0].As<v8::Function>(), context, 1, callback_args);
  EXPECT_EQ(v8::Promise::kFulfilled, promise->State());
  EXPECT_EQ(R"("foo")", V8ToString(promise->Result(), context));
}

// Test that having a custom callback without an extension-provided callback
// doesn't crash.
TEST_F(APIRequestHandlerTest, CustomCallbackArgumentsWithEmptyCallback) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  std::unique_ptr<APIRequestHandler> request_handler = CreateRequestHandler();

  v8::Local<v8::Function> custom_callback =
      FunctionFromString(context, kEchoArgs);
  ASSERT_FALSE(custom_callback.IsEmpty());

  v8::Local<v8::Function> empty_callback;
  request_handler->StartRequest(
      context, "method", base::Value::List(), binding::AsyncResponseType::kNone,
      empty_callback, custom_callback, binding::ResultModifierFunction());
  int request_id = request_handler->last_sent_request_id();
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  request_handler->CompleteRequest(request_id, base::Value::List(),
                                   std::string());

  EXPECT_TRUE(did_run_js());
  v8::Local<v8::Array> result;
  ASSERT_TRUE(
      GetPropertyFromObjectAs(context->Global(), context, "result", &result));
  ArgumentList args(isolate());
  ASSERT_TRUE(gin::Converter<ArgumentList>::FromV8(isolate(), result, &args));
  ASSERT_EQ(1u, args.size());
  EXPECT_TRUE(args[0]->IsUndefined());

  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());
}

TEST_F(APIRequestHandlerTest, ResultModifier) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  binding::ResultModifierFunction result_modifier =
      base::BindOnce([](const v8::LocalVector<v8::Value>& result_args,
                        v8::Local<v8::Context> context,
                        binding::AsyncResponseType async_type) {
        EXPECT_EQ(1u, result_args.size());
        EXPECT_TRUE(result_args[0]->IsObject());
        v8::Local<v8::Object> result_obj = result_args[0].As<v8::Object>();

        v8::Local<v8::Value> prop_1;
        bool success =
            v8_helpers::GetProperty(context, result_obj, "prop1", &prop_1);
        DCHECK(success);
        v8::Local<v8::Value> prop_2;
        success =
            v8_helpers::GetProperty(context, result_obj, "prop2", &prop_2);
        DCHECK(success);

        v8::LocalVector<v8::Value> new_args(context->GetIsolate(),
                                            {prop_1, prop_2});
        return new_args;
      });

  std::unique_ptr<APIRequestHandler> request_handler = CreateRequestHandler();

  v8::Local<v8::Function> callback = FunctionFromString(
      context, "(function(arg1, arg2) {this.arg1 = arg1; this.arg2 = arg2});");
  ASSERT_FALSE(callback.IsEmpty());

  request_handler->StartRequest(context, "method", base::Value::List(),
                                binding::AsyncResponseType::kCallback, callback,
                                v8::Local<v8::Function>(),
                                std::move(result_modifier));
  int request_id = request_handler->last_sent_request_id();
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  request_handler->CompleteRequest(
      request_id, ListValueFromString("[{'prop1':'foo', 'prop2':'bar'}]"),
      std::string());
  EXPECT_TRUE(did_run_js());

  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());

  EXPECT_EQ(R"("foo")",
            GetStringPropertyFromObject(context->Global(), context, "arg1"));
  EXPECT_EQ(R"("bar")",
            GetStringPropertyFromObject(context->Global(), context, "arg2"));
}

// Test user gestures being curried around for API requests.
TEST_F(APIRequestHandlerTest, UserGestureTest) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  std::unique_ptr<APIRequestHandler> request_handler = CreateRequestHandler();

  // Set up a callback to be used with the request so we can check if a user
  // gesture was active.
  std::optional<bool> ran_with_user_gesture;
  v8::Local<v8::FunctionTemplate> function_template =
      gin::CreateFunctionTemplate(
          isolate(),
          base::BindRepeating(&APIRequestHandlerTest::SaveUserActivationState,
                              base::Unretained(this), context,
                              &ran_with_user_gesture));
  v8::Local<v8::Function> v8_callback =
      function_template->GetFunction(context).ToLocalChecked();

  // Try first without a user gesture.
  request_handler->StartRequest(context, kMethod, base::Value::List(),
                                binding::AsyncResponseType::kCallback,
                                v8_callback, v8::Local<v8::Function>(),
                                binding::ResultModifierFunction());
  int request_id = request_handler->last_sent_request_id();
  request_handler->CompleteRequest(request_id, ListValueFromString("[]"),
                                   std::string());

  ASSERT_TRUE(ran_with_user_gesture);
  EXPECT_FALSE(*ran_with_user_gesture);
  ran_with_user_gesture.reset();

  // Next try calling with a user gesture. Since a gesture will be active at the
  // time of the call, it should also be active during the callback.

  ScopedTestUserActivation test_user_activation;
  // TODO(devlin): This isn't quite right with UAv1/UAv2.  V1 should properly
  // activate a new user gesture on the stack, and v2 should rely on the gesture
  // being persisted (or generated from the browser). We should clean this up.

  EXPECT_TRUE(interaction_provider()->HasActiveInteraction(context));

  request_handler->StartRequest(context, kMethod, base::Value::List(),
                                binding::AsyncResponseType::kCallback,
                                v8_callback, v8::Local<v8::Function>(),
                                binding::ResultModifierFunction());
  request_id = request_handler->last_sent_request_id();
  request_handler->CompleteRequest(request_id, ListValueFromString("[]"),
                                   std::string());
  ASSERT_TRUE(ran_with_user_gesture);
  EXPECT_TRUE(*ran_with_user_gesture);

  // Sanity check: the callback doesn't change the state
  EXPECT_TRUE(interaction_provider()->HasActiveInteraction(context));
}

TEST_F(APIRequestHandlerTest, SettingLastError) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  std::optional<std::string> logged_error;
  auto get_parent = [](v8::Local<v8::Context> context,
                       v8::Local<v8::Object>* secondary_parent) {
    return context->Global();
  };

  auto log_error = [](std::optional<std::string>* logged_error,
                      v8::Local<v8::Context> context,
                      const std::string& error) { *logged_error = error; };

  APIRequestHandler request_handler(
      base::DoNothing(),
      APILastError(base::BindRepeating(get_parent),
                   base::BindRepeating(log_error, &logged_error)),
      exception_handler(), interaction_provider());

  const char kReportExposedLastError[] =
      "(function() {\n"
      "  if (this.lastError)\n"
      "    this.seenLastError = this.lastError.message;\n"
      "})";
  auto get_exposed_error = [context]() {
    return GetStringPropertyFromObject(context->Global(), context,
                                       "seenLastError");
  };

  {
    // Test a successful function call. No last error should be emitted to the
    // console or exposed to the callback.
    v8::Local<v8::Function> callback =
        FunctionFromString(context, kReportExposedLastError);
    request_handler.StartRequest(context, kMethod, base::Value::List(),
                                 binding::AsyncResponseType::kCallback,
                                 callback, v8::Local<v8::Function>(),
                                 binding::ResultModifierFunction());
    int request_id = request_handler.last_sent_request_id();
    request_handler.CompleteRequest(request_id, base::Value::List(),
                                    std::string());
    EXPECT_FALSE(logged_error);
    EXPECT_EQ("undefined", get_exposed_error());
    logged_error.reset();
  }

  {
    // Test a function call resulting in an error. Since the callback checks the
    // last error, no error should be logged to the console (but it should be
    // exposed to the callback).
    v8::Local<v8::Function> callback =
        FunctionFromString(context, kReportExposedLastError);
    request_handler.StartRequest(context, kMethod, base::Value::List(),
                                 binding::AsyncResponseType::kCallback,
                                 callback, v8::Local<v8::Function>(),
                                 binding::ResultModifierFunction());
    int request_id = request_handler.last_sent_request_id();
    request_handler.CompleteRequest(request_id, base::Value::List(),
                                    "some error");
    EXPECT_FALSE(logged_error);
    EXPECT_EQ("\"some error\"", get_exposed_error());
    logged_error.reset();
  }

  {
    // Test a function call resulting in an error that goes unchecked by the
    // callback. The error should be logged.
    v8::Local<v8::Function> callback =
        FunctionFromString(context, "(function() {})");
    request_handler.StartRequest(context, kMethod, base::Value::List(),
                                 binding::AsyncResponseType::kCallback,
                                 callback, v8::Local<v8::Function>(),
                                 binding::ResultModifierFunction());
    int request_id = request_handler.last_sent_request_id();
    request_handler.CompleteRequest(request_id, base::Value::List(),
                                    "some error");
    ASSERT_TRUE(logged_error);
    EXPECT_EQ("Unchecked runtime.lastError: some error", *logged_error);
    logged_error.reset();
  }

  {
    // Test a function call resulting in an error with only a custom callback,
    // and no author-script-provided callback. The error should be logged.
    v8::Local<v8::Function> custom_callback =
        FunctionFromString(context, "(function() {})");
    request_handler.StartRequest(context, kMethod, base::Value::List(),
                                 binding::AsyncResponseType::kNone,
                                 v8::Local<v8::Function>(), custom_callback,
                                 binding::ResultModifierFunction());
    int request_id = request_handler.last_sent_request_id();
    request_handler.CompleteRequest(request_id, base::Value::List(),
                                    "some error");
    ASSERT_TRUE(logged_error);
    EXPECT_EQ("Unchecked runtime.lastError: some error", *logged_error);
    logged_error.reset();
  }

  {
    // Test a function call resulting in an error that does not have an
    // associated callback callback. The error should be logged.
    request_handler.StartRequest(
        context, kMethod, base::Value::List(),
        binding::AsyncResponseType::kNone, v8::Local<v8::Function>(),
        v8::Local<v8::Function>(), binding::ResultModifierFunction());
    int request_id = request_handler.last_sent_request_id();
    request_handler.CompleteRequest(request_id, base::Value::List(),
                                    "some error");
    ASSERT_TRUE(logged_error);
    EXPECT_EQ("Unchecked runtime.lastError: some error", *logged_error);
    logged_error.reset();
  }

  {
    // Test a function call resulting in an error for a request handler that has
    // an associated result modifier. The result modifier should never be called
    // and since the callback checks last error no error should be logged to the
    // console.
    bool result_modifier_called = false;
    auto result_modifier = [&result_modifier_called](
                               const v8::LocalVector<v8::Value>& result_args,
                               v8::Local<v8::Context> context,
                               binding::AsyncResponseType async_type) {
      result_modifier_called = true;
      return result_args;
    };
    v8::Local<v8::Function> callback =
        FunctionFromString(context, kReportExposedLastError);
    request_handler.StartRequest(context, kMethod, base::Value::List(),
                                 binding::AsyncResponseType::kCallback,
                                 callback, v8::Local<v8::Function>(),
                                 base::BindLambdaForTesting(result_modifier));
    int request_id = request_handler.last_sent_request_id();
    request_handler.CompleteRequest(request_id, base::Value::List(),
                                    "some error");
    EXPECT_FALSE(logged_error);
    EXPECT_EQ("\"some error\"", get_exposed_error());
    EXPECT_FALSE(result_modifier_called);
    logged_error.reset();
  }
}

TEST_F(APIRequestHandlerTest, AddPendingRequestCallback) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  bool dispatched_request = false;
  auto handle_request = [](bool* dispatched_request,
                           std::unique_ptr<APIRequestHandler::Request> request,
                           v8::Local<v8::Context> context) {
    *dispatched_request = true;
  };

  APIRequestHandler request_handler(
      base::BindRepeating(handle_request, &dispatched_request),
      APILastError(APILastError::GetParent(), binding::AddConsoleError()),
      exception_handler(), interaction_provider());

  EXPECT_TRUE(request_handler.GetPendingRequestIdsForTesting().empty());
  v8::Local<v8::Function> function = FunctionFromString(context, kEchoArgs);
  ASSERT_FALSE(function.IsEmpty());

  auto details = request_handler.AddPendingRequest(
      context, binding::AsyncResponseType::kCallback, function,
      binding::ResultModifierFunction());
  int request_id = details.request_id;
  EXPECT_TRUE(details.promise.IsEmpty());
  EXPECT_THAT(request_handler.GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));
  // Even though we add a pending request, we shouldn't have dispatched anything
  // because AddPendingRequest() is intended for renderer-side implementations.
  EXPECT_FALSE(dispatched_request);

  const char kArguments[] = "['foo',1,{'prop1':'bar'}]";
  request_handler.CompleteRequest(request_id, ListValueFromString(kArguments),
                                  std::string());

  EXPECT_EQ(ReplaceSingleQuotes(kArguments),
            GetStringPropertyFromObject(context->Global(), context, "result"));

  EXPECT_TRUE(request_handler.GetPendingRequestIdsForTesting().empty());
  EXPECT_FALSE(dispatched_request);
}

TEST_F(APIRequestHandlerTest, AddPendingRequestPromise) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  bool dispatched_request = false;
  auto handle_request = [](bool* dispatched_request,
                           std::unique_ptr<APIRequestHandler::Request> request,
                           v8::Local<v8::Context> context) {
    *dispatched_request = true;
  };

  APIRequestHandler request_handler(
      base::BindRepeating(handle_request, &dispatched_request),
      APILastError(APILastError::GetParent(), binding::AddConsoleError()),
      exception_handler(), interaction_provider());

  EXPECT_TRUE(request_handler.GetPendingRequestIdsForTesting().empty());

  auto details = request_handler.AddPendingRequest(
      context, binding::AsyncResponseType::kPromise, v8::Local<v8::Function>(),
      binding::ResultModifierFunction());
  int request_id = details.request_id;
  v8::Local<v8::Promise> promise = details.promise;
  EXPECT_THAT(request_handler.GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));
  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_EQ(v8::Promise::kPending, promise->State());

  // Even though we add a pending request, we shouldn't have dispatched anything
  // because AddPendingRequest() is intended for renderer-side implementations.
  EXPECT_FALSE(dispatched_request);

  request_handler.CompleteRequest(
      request_id, ListValueFromString("[{'foo': 'bar'}]"), std::string());

  ASSERT_EQ(v8::Promise::kFulfilled, promise->State());
  EXPECT_EQ(R"({"foo":"bar"})", V8ToString(promise->Result(), context));

  EXPECT_TRUE(request_handler.GetPendingRequestIdsForTesting().empty());
  EXPECT_FALSE(dispatched_request);
}

TEST_F(APIRequestHandlerTest, AddPendingRequestWithResultModifier) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  binding::ResultModifierFunction result_modifier =
      base::BindOnce([](const v8::LocalVector<v8::Value>& result_args,
                        v8::Local<v8::Context> context,
                        binding::AsyncResponseType async_type) {
        DCHECK_EQ(1u, result_args.size());
        DCHECK(result_args[0]->IsObject());
        v8::Local<v8::Object> result_obj = result_args[0].As<v8::Object>();

        v8::Local<v8::Value> prop_1;
        bool success =
            v8_helpers::GetProperty(context, result_obj, "prop1", &prop_1);
        DCHECK(success);
        v8::Local<v8::Value> prop_2;
        success =
            v8_helpers::GetProperty(context, result_obj, "prop2", &prop_2);
        DCHECK(success);

        v8::LocalVector<v8::Value> new_args(context->GetIsolate(),
                                            {prop_1, prop_2});
        return new_args;
      });

  std::unique_ptr<APIRequestHandler> request_handler = CreateRequestHandler();

  v8::Local<v8::Function> function = FunctionFromString(context, kEchoArgs);
  ASSERT_FALSE(function.IsEmpty());

  auto details = request_handler->AddPendingRequest(
      context, binding::AsyncResponseType::kCallback, function,
      std::move(result_modifier));
  int request_id = details.request_id;
  EXPECT_TRUE(details.promise.IsEmpty());

  const char kArguments[] = "[{'prop1':'bar', 'prop2':'baz'}]";
  request_handler->CompleteRequest(request_id, ListValueFromString(kArguments),
                                   std::string());
  EXPECT_EQ(R"(["bar","baz"])",
            GetStringPropertyFromObject(context->Global(), context, "result"));
}

// Tests that throwing an exception in a callback is properly handled.
TEST_F(APIRequestHandlerTest, ThrowExceptionInCallback) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  auto add_console_error = [](std::optional<std::string>* error_out,
                              v8::Local<v8::Context> context,
                              const std::string& error) { *error_out = error; };

  std::optional<std::string> logged_error;
  ExceptionHandler exception_handler(
      base::BindRepeating(add_console_error, &logged_error));

  APIRequestHandler request_handler(
      base::DoNothing(),
      APILastError(APILastError::GetParent(), binding::AddConsoleError()),
      &exception_handler, interaction_provider());

  v8::TryCatch outer_try_catch(isolate());
  v8::Local<v8::Function> callback_throwing_error =
      FunctionFromString(context, "(function() { throw new Error('hello'); })");
  auto details = request_handler.AddPendingRequest(
      context, binding::AsyncResponseType::kCallback, callback_throwing_error,
      binding::ResultModifierFunction());
  int request_id = details.request_id;
  EXPECT_TRUE(details.promise.IsEmpty());

  {
    TestJSRunner::AllowErrors allow_errors;
    request_handler.CompleteRequest(request_id, base::Value::List(),
                                    std::string());
  }
  // |outer_try_catch| should not have caught an error. This is important to not
  // disrupt our bindings code (or other running JS) when asynchronously
  // returning from an API call. Instead, the error should be caught and handled
  // by the exception handler.
  EXPECT_FALSE(outer_try_catch.HasCaught());
  ASSERT_TRUE(logged_error);
  EXPECT_THAT(*logged_error,
              testing::StartsWith("Error handling response: Error: hello"));
}

// Tests promise-based requests with the promise being fulfilled.
TEST_F(APIRequestHandlerTest, PromiseBasedRequests_Fulfilled) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  std::unique_ptr<APIRequestHandler> request_handler = CreateRequestHandler();
  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());

  v8::Local<v8::Promise> promise = request_handler->StartRequest(
      context, kMethod, base::Value::List(),
      binding::AsyncResponseType::kPromise, v8::Local<v8::Function>(),
      v8::Local<v8::Function>(), binding::ResultModifierFunction());
  ASSERT_FALSE(promise.IsEmpty());

  int request_id = request_handler->last_sent_request_id();
  EXPECT_NE(-1, request_id);
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  EXPECT_EQ(v8::Promise::kPending, promise->State());

  request_handler->CompleteRequest(request_id, ListValueFromString("['foo']"),
                                   std::string());

  ASSERT_EQ(v8::Promise::kFulfilled, promise->State());
  EXPECT_EQ(R"("foo")", V8ToString(promise->Result(), context));

  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());
}

// Tests promise-based requests with the promise being rejected.
TEST_F(APIRequestHandlerTest, PromiseBasedRequests_Rejected) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  std::unique_ptr<APIRequestHandler> request_handler = CreateRequestHandler();
  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());

  v8::Local<v8::Promise> promise = request_handler->StartRequest(
      context, kMethod, base::Value::List(),
      binding::AsyncResponseType::kPromise, v8::Local<v8::Function>(),
      v8::Local<v8::Function>(), binding::ResultModifierFunction());
  ASSERT_FALSE(promise.IsEmpty());

  int request_id = request_handler->last_sent_request_id();
  EXPECT_NE(-1, request_id);
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  EXPECT_EQ(v8::Promise::kPending, promise->State());

  constexpr char kError[] = "Something went wrong!";
  request_handler->CompleteRequest(request_id, base::Value::List(), kError);

  ASSERT_EQ(v8::Promise::kRejected, promise->State());
  v8::Local<v8::Value> result = promise->Result();
  ASSERT_FALSE(result.IsEmpty());
  EXPECT_EQ(
      base::StrCat({"Error: ", kError}),
      gin::V8ToString(isolate(), result->ToString(context).ToLocalChecked()));

  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());
}

}  // namespace extensions

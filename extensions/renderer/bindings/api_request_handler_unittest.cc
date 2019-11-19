// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_request_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/exception_handler.h"
#include "extensions/renderer/bindings/test_interaction_provider.h"
#include "extensions/renderer/bindings/test_js_runner.h"
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
using ArgumentList = std::vector<v8::Local<v8::Value>>;

// TODO(devlin): Should we move some parts of api_binding_unittest.cc to here?

}  // namespace

class APIRequestHandlerTest : public APIBindingTest {
 public:
  std::unique_ptr<APIRequestHandler> CreateRequestHandler() {
    return std::make_unique<APIRequestHandler>(
        base::DoNothing(),
        APILastError(APILastError::GetParent(), binding::AddConsoleError()),
        nullptr, interaction_provider());
  }

  void SaveUserActivationState(v8::Local<v8::Context> context,
                               base::Optional<bool>* ran_with_user_gesture) {
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

  bool did_run_js() const { return did_run_js_; }

 private:
  void SetDidRunJS() { did_run_js_ = true; }

  bool did_run_js_ = false;
  std::unique_ptr<TestInteractionProvider> interaction_provider_;

  DISALLOW_COPY_AND_ASSIGN(APIRequestHandlerTest);
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

  int request_id = request_handler->StartRequest(
      context, kMethod, std::make_unique<base::ListValue>(), function,
      v8::Local<v8::Function>());
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  const char kArguments[] = "['foo',1,{'prop1':'bar'}]";
  std::unique_ptr<base::ListValue> response_arguments =
      ListValueFromString(kArguments);
  ASSERT_TRUE(response_arguments);
  request_handler->CompleteRequest(request_id, *response_arguments,
                                   std::string());

  EXPECT_TRUE(did_run_js());
  EXPECT_EQ(ReplaceSingleQuotes(kArguments),
            GetStringPropertyFromObject(context->Global(), context, "result"));

  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());

  request_id = request_handler->StartRequest(
      context, kMethod, std::make_unique<base::ListValue>(),
      v8::Local<v8::Function>(), v8::Local<v8::Function>());
  EXPECT_NE(-1, request_id);
  request_handler->CompleteRequest(request_id, base::ListValue(),
                                   std::string());
}

// Tests that trying to run non-existent or invalided requests is a no-op.
TEST_F(APIRequestHandlerTest, InvalidRequestsTest) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  std::unique_ptr<APIRequestHandler> request_handler = CreateRequestHandler();

  v8::Local<v8::Function> function = FunctionFromString(context, kEchoArgs);
  ASSERT_FALSE(function.IsEmpty());

  int request_id = request_handler->StartRequest(
      context, kMethod, std::make_unique<base::ListValue>(), function,
      v8::Local<v8::Function>());
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  std::unique_ptr<base::ListValue> response_arguments =
      ListValueFromString("['foo']");
  ASSERT_TRUE(response_arguments);

  // Try running with a non-existent request id.
  int fake_request_id = 42;
  request_handler->CompleteRequest(fake_request_id, *response_arguments,
                                   std::string());
  EXPECT_FALSE(did_run_js());

  // Try running with a request from an invalidated context.
  request_handler->InvalidateContext(context);
  request_handler->CompleteRequest(request_id, *response_arguments,
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

  int request_a = request_handler->StartRequest(
      context_a, kMethod, std::make_unique<base::ListValue>(), function_a,
      v8::Local<v8::Function>());
  int request_b = request_handler->StartRequest(
      context_b, kMethod, std::make_unique<base::ListValue>(), function_b,
      v8::Local<v8::Function>());

  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_a, request_b));

  std::unique_ptr<base::ListValue> response_a =
      ListValueFromString("['response_a:']");
  ASSERT_TRUE(response_a);

  request_handler->CompleteRequest(request_a, *response_a, std::string());
  EXPECT_TRUE(did_run_js());
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_b));

  EXPECT_EQ(
      ReplaceSingleQuotes("'response_a:alpha'"),
      GetStringPropertyFromObject(context_a->Global(), context_a, "result"));

  std::unique_ptr<base::ListValue> response_b =
      ListValueFromString("['response_b:']");
  ASSERT_TRUE(response_b);

  request_handler->CompleteRequest(request_b, *response_b, std::string());
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
  v8::Local<v8::Function> callback =
      FunctionFromString(context, "(function() {})");
  ASSERT_FALSE(callback.IsEmpty());
  ASSERT_FALSE(custom_callback.IsEmpty());

  int request_id = request_handler->StartRequest(
      context, "method", std::make_unique<base::ListValue>(), callback,
      custom_callback);
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  std::unique_ptr<base::ListValue> response_arguments =
      ListValueFromString("['response', 'arguments']");
  ASSERT_TRUE(response_arguments);
  request_handler->CompleteRequest(request_id, *response_arguments,
                                   std::string());

  EXPECT_TRUE(did_run_js());
  v8::Local<v8::Value> result =
      GetPropertyFromObject(context->Global(), context, "result");
  ASSERT_FALSE(result.IsEmpty());
  ASSERT_TRUE(result->IsArray());
  ArgumentList args;
  ASSERT_TRUE(gin::Converter<ArgumentList>::FromV8(isolate(), result, &args));
  ASSERT_EQ(5u, args.size());
  EXPECT_EQ("\"method\"", V8ToString(args[0], context));
  EXPECT_EQ(base::StringPrintf("{\"id\":%d}", request_id),
            V8ToString(args[1], context));
  EXPECT_EQ(callback, args[2]);
  EXPECT_EQ("\"response\"", V8ToString(args[3], context));
  EXPECT_EQ("\"arguments\"", V8ToString(args[4], context));

  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());
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
  int request_id = request_handler->StartRequest(
      context, "method", std::make_unique<base::ListValue>(), empty_callback,
      custom_callback);
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  request_handler->CompleteRequest(request_id, base::ListValue(),
                                   std::string());

  EXPECT_TRUE(did_run_js());
  v8::Local<v8::Value> result =
      GetPropertyFromObject(context->Global(), context, "result");
  ASSERT_FALSE(result.IsEmpty());
  ASSERT_TRUE(result->IsArray());
  ArgumentList args;
  ASSERT_TRUE(gin::Converter<ArgumentList>::FromV8(isolate(), result, &args));
  ASSERT_EQ(3u, args.size());
  EXPECT_EQ("\"method\"", V8ToString(args[0], context));
  EXPECT_EQ(base::StringPrintf("{\"id\":%d}", request_id),
            V8ToString(args[1], context));
  EXPECT_TRUE(args[2]->IsUndefined());

  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());
}

// Test user gestures being curried around for API requests.
TEST_F(APIRequestHandlerTest, UserGestureTest) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  std::unique_ptr<APIRequestHandler> request_handler = CreateRequestHandler();

  // Set up a callback to be used with the request so we can check if a user
  // gesture was active.
  base::Optional<bool> ran_with_user_gesture;
  v8::Local<v8::FunctionTemplate> function_template =
      gin::CreateFunctionTemplate(
          isolate(),
          base::BindRepeating(&APIRequestHandlerTest::SaveUserActivationState,
                              base::Unretained(this), context,
                              &ran_with_user_gesture));
  v8::Local<v8::Function> v8_callback =
      function_template->GetFunction(context).ToLocalChecked();

  // Try first without a user gesture.
  int request_id = request_handler->StartRequest(
      context, kMethod, std::make_unique<base::ListValue>(), v8_callback,
      v8::Local<v8::Function>());
  request_handler->CompleteRequest(request_id, *ListValueFromString("[]"),
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

  request_id = request_handler->StartRequest(
      context, kMethod, std::make_unique<base::ListValue>(), v8_callback,
      v8::Local<v8::Function>());
  request_handler->CompleteRequest(request_id, *ListValueFromString("[]"),
                                   std::string());
  ASSERT_TRUE(ran_with_user_gesture);
  EXPECT_TRUE(*ran_with_user_gesture);

  // Sanity check: the callback doesn't change the state
  EXPECT_TRUE(interaction_provider()->HasActiveInteraction(context));
}

TEST_F(APIRequestHandlerTest, SettingLastError) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  base::Optional<std::string> logged_error;
  auto get_parent = [](v8::Local<v8::Context> context,
                       v8::Local<v8::Object>* secondary_parent) {
    return context->Global();
  };

  auto log_error = [](base::Optional<std::string>* logged_error,
                      v8::Local<v8::Context> context,
                      const std::string& error) { *logged_error = error; };

  APIRequestHandler request_handler(
      base::DoNothing(),
      APILastError(base::BindRepeating(get_parent),
                   base::BindRepeating(log_error, &logged_error)),
      nullptr, interaction_provider());

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
    int request_id = request_handler.StartRequest(
        context, kMethod, std::make_unique<base::ListValue>(), callback,
        v8::Local<v8::Function>());
    request_handler.CompleteRequest(request_id, base::ListValue(),
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
    int request_id = request_handler.StartRequest(
        context, kMethod, std::make_unique<base::ListValue>(), callback,
        v8::Local<v8::Function>());
    request_handler.CompleteRequest(request_id, base::ListValue(),
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
    int request_id = request_handler.StartRequest(
        context, kMethod, std::make_unique<base::ListValue>(), callback,
        v8::Local<v8::Function>());
    request_handler.CompleteRequest(request_id, base::ListValue(),
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
    int request_id = request_handler.StartRequest(
        context, kMethod, std::make_unique<base::ListValue>(),
        v8::Local<v8::Function>(), custom_callback);
    request_handler.CompleteRequest(request_id, base::ListValue(),
                                    "some error");
    ASSERT_TRUE(logged_error);
    EXPECT_EQ("Unchecked runtime.lastError: some error", *logged_error);
    logged_error.reset();
  }

  {
    // Test a function call resulting in an error that does not have an
    // associated callback callback. The error should be logged.
    int request_id = request_handler.StartRequest(
        context, kMethod, std::make_unique<base::ListValue>(),
        v8::Local<v8::Function>(), v8::Local<v8::Function>());
    request_handler.CompleteRequest(request_id, base::ListValue(),
                                    "some error");
    ASSERT_TRUE(logged_error);
    EXPECT_EQ("Unchecked runtime.lastError: some error", *logged_error);
    logged_error.reset();
  }
}

TEST_F(APIRequestHandlerTest, AddPendingRequest) {
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
      nullptr, interaction_provider());

  EXPECT_TRUE(request_handler.GetPendingRequestIdsForTesting().empty());
  v8::Local<v8::Function> function = FunctionFromString(context, kEchoArgs);
  ASSERT_FALSE(function.IsEmpty());

  int request_id = request_handler.AddPendingRequest(context, function);
  EXPECT_THAT(request_handler.GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));
  // Even though we add a pending request, we shouldn't have dispatched anything
  // because AddPendingRequest() is intended for renderer-side implementations.
  EXPECT_FALSE(dispatched_request);

  const char kArguments[] = "['foo',1,{'prop1':'bar'}]";
  std::unique_ptr<base::ListValue> response_arguments =
      ListValueFromString(kArguments);
  ASSERT_TRUE(response_arguments);
  request_handler.CompleteRequest(request_id, *response_arguments,
                                  std::string());

  EXPECT_EQ(ReplaceSingleQuotes(kArguments),
            GetStringPropertyFromObject(context->Global(), context, "result"));

  EXPECT_TRUE(request_handler.GetPendingRequestIdsForTesting().empty());
  EXPECT_FALSE(dispatched_request);
}

// Tests that throwing an exception in a callback is properly handled.
TEST_F(APIRequestHandlerTest, ThrowExceptionInCallback) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  auto add_console_error = [](base::Optional<std::string>* error_out,
                              v8::Local<v8::Context> context,
                              const std::string& error) { *error_out = error; };

  base::Optional<std::string> logged_error;
  ExceptionHandler exception_handler(
      base::BindRepeating(add_console_error, &logged_error));

  APIRequestHandler request_handler(
      base::DoNothing(),
      APILastError(APILastError::GetParent(), binding::AddConsoleError()),
      &exception_handler, interaction_provider());

  v8::TryCatch outer_try_catch(isolate());
  v8::Local<v8::Function> callback_throwing_error =
      FunctionFromString(context, "(function() { throw new Error('hello'); })");
  int request_id =
      request_handler.AddPendingRequest(context, callback_throwing_error);

  {
    TestJSRunner::AllowErrors allow_errors;
    request_handler.CompleteRequest(request_id, base::ListValue(),
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

  v8::Local<v8::Promise> promise;
  int request_id = -1;
  std::tie(request_id, promise) = request_handler->StartPromiseBasedRequest(
      context, kMethod, std::make_unique<base::ListValue>());

  EXPECT_NE(-1, request_id);
  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  EXPECT_EQ(v8::Promise::kPending, promise->State());

  request_handler->CompleteRequest(request_id, *ListValueFromString("['foo']"),
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

  v8::Local<v8::Promise> promise;
  int request_id = -1;
  std::tie(request_id, promise) = request_handler->StartPromiseBasedRequest(
      context, kMethod, std::make_unique<base::ListValue>());

  EXPECT_NE(-1, request_id);
  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  EXPECT_EQ(v8::Promise::kPending, promise->State());

  constexpr char kError[] = "Something went wrong!";
  request_handler->CompleteRequest(request_id, base::ListValue(), kError);

  ASSERT_EQ(v8::Promise::kRejected, promise->State());
  v8::Local<v8::Value> result = promise->Result();
  ASSERT_FALSE(result.IsEmpty());
  EXPECT_EQ(
      base::StrCat({"Error: ", kError}),
      gin::V8ToString(isolate(), result->ToString(context).ToLocalChecked()));

  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());
}

}  // namespace extensions

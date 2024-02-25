// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_bindings_system_unittest.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/renderer/bindings/api_binding.h"
#include "extensions/renderer/bindings/api_binding_hooks.h"
#include "extensions/renderer/bindings/api_binding_hooks_test_delegate.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/api_bindings_system.h"
#include "extensions/renderer/bindings/api_invocation_errors.h"
#include "extensions/renderer/bindings/test_interaction_provider.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/try_catch.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

namespace {

// Fake API for testing.
const char kAlphaAPIName[] = "alpha";
const char kAlphaAPISpec[] = R"(
    {
      "types": [{
        "id": "alpha.objRef",
        "type": "object",
        "properties": {
          "prop1": {"type": "string"},
          "prop2": {"type": "integer", "optional": true}
        }
      }, {
        "id": "alpha.enumRef",
        "type": "string",
        "enum": ["cat", "dog"]
      }],
      "functions": [{
        "name": "functionWithCallback",
        "parameters": [{
          "name": "str",
          "type": "string"
        }],
        "returns_async": {
          "name": "callback",
          "parameters": [{"name": "strResult", "type": "string"}]
        }
      }, {
        "name": "functionWithRefAndCallback",
        "parameters": [{
          "name": "ref",
          "$ref": "alpha.objRef"
        }],
        "returns_async": {
          "name": "callback",
          "parameters": []
        }
      }, {
        "name": "functionWithEnum",
        "parameters": [{"name": "e", "$ref": "alpha.enumRef"}]
      }],
      "events": [{
        "name": "alphaEvent",
        "parameters": [{
          "name": "eventArg",
          "type": "object",
          "properties": { "key": {"type": "integer"} }
        }]
      }, {
        "name": "alphaOtherEvent"
      }]
    })";

// Another fake API for testing.
const char kBetaAPIName[] = "beta";
const char kBetaAPISpec[] = R"(
    {
      "functions": [{
        "name": "simpleFunc",
        "parameters": [{"name": "int", "type": "integer"}]
      }]
    })";

const char kGammaAPIName[] = "gamma";
const char kGammaAPISpec[] = R"(
    {
      "functions": [{
        "name": "functionWithExternalRef",
        "parameters": [{ "name": "someRef", "$ref": "alpha.objRef" }]
      }]
    })";

// JS strings used for registering custom callbacks for testing.
const char kCustomCallbackHook[] = R"(
    (function(hooks) {
      hooks.setCustomCallback(
          'functionWithCallback', (originalCallback,
                                   firstResult, secondResult) => {
        this.results = [firstResult, secondResult];
        originalCallback(secondResult);
      });
    }))";
const char kCustomCallbackThrowHook[] = R"(
    (function(hooks) {
      hooks.setCustomCallback(
          'functionWithCallback', (name, originalCallback,
                                   firstResult, secondResult) => {
        throw new Error('Custom callback threw');
      });
    }))";

bool AllowAllAPIs(v8::Local<v8::Context> context, const std::string& name) {
  return true;
}

bool AllowPromises(v8::Local<v8::Context> context) {
  return true;
}

}  // namespace

APIBindingsSystemTest::APIBindingsSystemTest() = default;
APIBindingsSystemTest::~APIBindingsSystemTest() = default;

void APIBindingsSystemTest::SetUp() {
  APIBindingTest::SetUp();

  // Create the fake API schemas.
  for (const auto& api : GetAPIs()) {
    base::Value::Dict api_schema = DictValueFromString(api.spec);
    api_schemas_[api.name] = std::move(api_schema);
  }

  binding::AddConsoleError add_console_error(base::BindRepeating(
      &APIBindingsSystemTest::AddConsoleError, base::Unretained(this)));
  auto get_context_owner = [](v8::Local<v8::Context>) {
    return std::string("context");
  };
  bindings_system_ = std::make_unique<APIBindingsSystem>(
      base::BindRepeating(&APIBindingsSystemTest::GetAPISchema,
                          base::Unretained(this)),
      base::BindRepeating(&AllowAllAPIs), base::BindRepeating(&AllowPromises),
      base::BindRepeating(&APIBindingsSystemTest::OnAPIRequest,
                          base::Unretained(this)),
      std::make_unique<TestInteractionProvider>(),
      base::BindRepeating(&APIBindingsSystemTest::OnEventListenersChanged,
                          base::Unretained(this)),
      base::BindRepeating(get_context_owner), base::DoNothing(),
      add_console_error,
      APILastError(
          base::BindRepeating(&APIBindingsSystemTest::GetLastErrorParent,
                              base::Unretained(this)),
          add_console_error));
}

void APIBindingsSystemTest::TearDown() {
  // Dispose all contexts now so that we call WillReleaseContext().
  DisposeAllContexts();
  bindings_system_.reset();
  APIBindingTest::TearDown();
}

void APIBindingsSystemTest::OnWillDisposeContext(
    v8::Local<v8::Context> context) {
  bindings_system_->WillReleaseContext(context);
}

std::vector<APIBindingsSystemTest::FakeSpec> APIBindingsSystemTest::GetAPIs() {
  return {
      {kAlphaAPIName, kAlphaAPISpec},
      {kBetaAPIName, kBetaAPISpec},
      {kGammaAPIName, kGammaAPISpec},
  };
}

v8::Local<v8::Object> APIBindingsSystemTest::GetLastErrorParent(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object>* secondary_parent) {
  return v8::Local<v8::Object>();
}

void APIBindingsSystemTest::AddConsoleError(v8::Local<v8::Context> context,
                                            const std::string& error) {
  console_errors_.push_back(error);
}

const base::Value::Dict& APIBindingsSystemTest::GetAPISchema(
    const std::string& api_name) {
  EXPECT_TRUE(base::Contains(api_schemas_, api_name));
  return api_schemas_[api_name];
}

void APIBindingsSystemTest::OnAPIRequest(
    std::unique_ptr<APIRequestHandler::Request> request,
    v8::Local<v8::Context> context) {
  ASSERT_FALSE(last_request_);
  last_request_ = std::move(request);
}

void APIBindingsSystemTest::OnEventListenersChanged(
    const std::string& event_name,
    binding::EventListenersChanged changed,
    const base::Value::Dict* filter,
    bool was_manual,
    v8::Local<v8::Context> context) {}

void APIBindingsSystemTest::ValidateLastRequest(
    const std::string& expected_name,
    const std::string& expected_arguments) {
  ASSERT_TRUE(last_request());
  EXPECT_EQ(expected_name, last_request()->method_name);
  EXPECT_EQ(ReplaceSingleQuotes(expected_arguments),
            ValueToString(last_request()->arguments_list));
}

v8::Local<v8::Value> APIBindingsSystemTest::CallFunctionOnObject(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> object,
    const std::string& script_source) {
  std::string wrapped_script_source =
      base::StringPrintf("(function(obj) { %s })", script_source.c_str());

  v8::Local<v8::Function> func =
      FunctionFromString(context, wrapped_script_source);
  // Use ADD_FAILURE() to avoid messing up the return type with ASSERT.
  if (func.IsEmpty()) {
    ADD_FAILURE() << script_source;
    return v8::Local<v8::Value>();
  }

  v8::Local<v8::Value> argv[] = {object};
  return RunFunction(func, context, 1, argv);
}

// Tests API object initialization, calling a method on the supplied APIs, and
// triggering the callback for the request.
TEST_F(APIBindingsSystemTest, TestInitializationAndCallbacks) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> alpha_api =
      bindings_system()->CreateAPIInstance(kAlphaAPIName, context, nullptr);
  ASSERT_FALSE(alpha_api.IsEmpty());
  v8::Local<v8::Object> beta_api =
      bindings_system()->CreateAPIInstance(kBetaAPIName, context, nullptr);
  ASSERT_FALSE(beta_api.IsEmpty());

  {
    // Test a simple call -> response.
    const char kTestCall[] = R"(
        obj.functionWithCallback('foo', function() {
          this.callbackArguments = Array.from(arguments);
        });)";
    CallFunctionOnObject(context, alpha_api, kTestCall);

    ValidateLastRequest("alpha.functionWithCallback", "['foo']");

    const char kResponseArgsJson[] = R"(["response"])";
    bindings_system()->CompleteRequest(last_request()->request_id,
                                       ListValueFromString(kResponseArgsJson),
                                       std::string());

    EXPECT_EQ(kResponseArgsJson,
              GetStringPropertyFromObject(context->Global(), context,
                                          "callbackArguments"));
    reset_last_request();
  }

  {
    // Test a call with references -> response.
    const char kTestCall[] = R"(
        obj.functionWithRefAndCallback({prop1: 'alpha', prop2: 42},
                                       function() {
          this.callbackArguments = Array.from(arguments);
        });)";

    CallFunctionOnObject(context, alpha_api, kTestCall);

    ValidateLastRequest("alpha.functionWithRefAndCallback",
                        "[{'prop1':'alpha','prop2':42}]");

    bindings_system()->CompleteRequest(last_request()->request_id,
                                       base::Value::List(), std::string());

    EXPECT_EQ("[]", GetStringPropertyFromObject(context->Global(), context,
                                                "callbackArguments"));
    reset_last_request();
  }

  {
    // Test an invalid invocation -> throwing error.
    const char kTestCall[] =
        "(function(obj) { obj.functionWithEnum('mouse') })";
    v8::Local<v8::Function> function = FunctionFromString(context, kTestCall);
    v8::Local<v8::Value> args[] = {alpha_api};
    RunFunctionAndExpectError(
        function, context, std::size(args), args,
        "Uncaught TypeError: " +
            api_errors::InvocationError(
                "alpha.functionWithEnum", "alpha.enumRef e",
                api_errors::ArgumentError(
                    "e", api_errors::InvalidEnumValue({"cat", "dog"}))));
    EXPECT_FALSE(last_request());
    reset_last_request();  // Just to not pollute future results.
  }

  {
    // Test an event registration -> event occurrence.
    const char kTestCall[] = R"(
        obj.alphaEvent.addListener(function() {
          this.eventArguments = Array.from(arguments);
        });)";
    CallFunctionOnObject(context, alpha_api, kTestCall);

    const char kResponseArgsJson[] = R"([{"key":42}])";
    base::Value::List expected_args = ListValueFromString(kResponseArgsJson);
    bindings_system()->FireEventInContext("alpha.alphaEvent", context,
                                          expected_args, nullptr);

    EXPECT_EQ(kResponseArgsJson,
              GetStringPropertyFromObject(context->Global(), context,
                                          "eventArguments"));
  }

  {
    // Test a call -> response on the second API.
    const char kTestCall[] = "obj.simpleFunc(2)";
    CallFunctionOnObject(context, beta_api, kTestCall);
    ValidateLastRequest("beta.simpleFunc", "[2]");
    reset_last_request();
  }
}

// Tests adding a custom hook to an API.
TEST_F(APIBindingsSystemTest, TestCustomHooks) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  bool did_call = false;
  auto hook = [](bool* did_call, const APISignature* signature,
                 v8::Local<v8::Context> context,
                 v8::LocalVector<v8::Value>* arguments,
                 const APITypeReferenceMap& type_refs) {
    *did_call = true;
    APIBindingHooks::RequestResult result(
        APIBindingHooks::RequestResult::HANDLED);
    if (arguments->size() != 2) {  // ASSERT* messes with the return type.
      EXPECT_EQ(2u, arguments->size());
      return result;
    }
    std::string argument;
    EXPECT_EQ("foo", gin::V8ToString(context->GetIsolate(), arguments->at(0)));
    if (!arguments->at(1)->IsFunction()) {
      EXPECT_TRUE(arguments->at(1)->IsFunction());
      return result;
    }
    v8::Local<v8::String> response =
        gin::StringToV8(context->GetIsolate(), "bar");
    v8::Local<v8::Value> response_args[] = {response};
    RunFunctionOnGlobal(arguments->at(1).As<v8::Function>(),
                        context, 1, response_args);
    return result;
  };

  auto test_hooks = std::make_unique<APIBindingHooksTestDelegate>();
  test_hooks->AddHandler("alpha.functionWithCallback",
                         base::BindRepeating(hook, &did_call));
  bindings_system()->RegisterHooksDelegate(kAlphaAPIName,
                                           std::move(test_hooks));

  v8::Local<v8::Object> alpha_api =
      bindings_system()->CreateAPIInstance(kAlphaAPIName, context, nullptr);
  ASSERT_FALSE(alpha_api.IsEmpty());

  {
    // Test a simple call -> response.
    const char kTestCall[] = R"(
        obj.functionWithCallback('foo', function() {
          this.callbackArguments = Array.from(arguments);
        });)";
    CallFunctionOnObject(context, alpha_api, kTestCall);
    EXPECT_TRUE(did_call);

    EXPECT_EQ(R"(["bar"])",
              GetStringPropertyFromObject(context->Global(), context,
                                          "callbackArguments"));
  }
}

// Tests a call with a callback into an API using a setCustomCallback hook
// works as expected.
TEST_F(APIBindingsSystemTest, TestSetCustomCallback_SuccessWithCallback) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  APIBindingHooks* hooks = nullptr;
  v8::Local<v8::Object> alpha_api =
      bindings_system()->CreateAPIInstance(kAlphaAPIName, context, &hooks);
  ASSERT_FALSE(alpha_api.IsEmpty());
  ASSERT_TRUE(hooks);
  v8::Local<v8::Object> js_hooks = hooks->GetJSHookInterface(context);
  v8::Local<v8::Function> function =
      FunctionFromString(context, kCustomCallbackHook);
  v8::Local<v8::Value> args[] = {js_hooks};
  RunFunctionOnGlobal(function, context, std::size(args), args);

  const char kTestCall[] = R"(
      obj.functionWithCallback('foo', function() {
        this.callbackArguments = Array.from(arguments);
      });)";
  CallFunctionOnObject(context, alpha_api, kTestCall);

  ValidateLastRequest("alpha.functionWithCallback", "['foo']");

  // Although this response would violate the return on the spec, since this
  // method has a custom callback defined it skips response validation. We
  // expect the custom callback will transform the return to the correct form
  // when calling the original callback, but this is not currently enforced or
  // validated.
  // TODO(tjudkins): Now that we use the CustomCallbackAdaptor, we could
  // potentially send the response validator to the custom callback adaptor
  // and validate the result returned from the custom callback before sending
  // it on to the original callback.
  bindings_system()->CompleteRequest(last_request()->request_id,
                                     ListValueFromString(R"(["alpha","beta"])"),
                                     std::string());

  EXPECT_EQ(R"(["alpha","beta"])",
            GetStringPropertyFromObject(context->Global(), context, "results"));
  EXPECT_EQ(R"(["beta"])",
            GetStringPropertyFromObject(context->Global(), context,
                                        "callbackArguments"));
}

// Tests a call with a promise into an API using a setCustomCallback hook works
// as expected.
TEST_F(APIBindingsSystemTest, TestSetCustomCallback_SuccessWithPromise) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  APIBindingHooks* hooks = nullptr;
  v8::Local<v8::Object> alpha_api =
      bindings_system()->CreateAPIInstance(kAlphaAPIName, context, &hooks);
  ASSERT_FALSE(alpha_api.IsEmpty());
  ASSERT_TRUE(hooks);
  v8::Local<v8::Object> js_hooks = hooks->GetJSHookInterface(context);
  v8::Local<v8::Function> function =
      FunctionFromString(context, kCustomCallbackHook);
  v8::Local<v8::Value> args[] = {js_hooks};
  RunFunctionOnGlobal(function, context, std::size(args), args);

  const char kTestCall[] = R"(return obj.functionWithCallback('bar');)";
  v8::Local<v8::Value> result =
      CallFunctionOnObject(context, alpha_api, kTestCall);

  ValidateLastRequest("alpha.functionWithCallback", "['bar']");
  v8::Local<v8::Promise> promise;
  ASSERT_TRUE(GetValueAs(result, &promise));
  EXPECT_EQ(v8::Promise::kPending, promise->State());

  bindings_system()->CompleteRequest(
      last_request()->request_id, ListValueFromString(R"(["gamma","delta"])"),
      std::string());

  EXPECT_EQ(R"(["gamma","delta"])",
            GetStringPropertyFromObject(context->Global(), context, "results"));
  EXPECT_EQ(v8::Promise::kFulfilled, promise->State());
  EXPECT_EQ(R"("delta")", V8ToString(promise->Result(), context));
}

// Tests that an error thrown in a setCustomCallback hook while using a callback
// based call works as expected.
TEST_F(APIBindingsSystemTest, TestSetCustomCallback_ErrorWithCallback) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  APIBindingHooks* hooks = nullptr;
  v8::Local<v8::Object> alpha_api =
      bindings_system()->CreateAPIInstance(kAlphaAPIName, context, &hooks);
  ASSERT_FALSE(alpha_api.IsEmpty());
  ASSERT_TRUE(hooks);
  v8::Local<v8::Object> js_hooks = hooks->GetJSHookInterface(context);
  v8::Local<v8::Function> function =
      FunctionFromString(context, kCustomCallbackThrowHook);
  v8::Local<v8::Value> args[] = {js_hooks};
  RunFunctionOnGlobal(function, context, std::size(args), args);

  const char kTestCall[] = R"(
      obj.functionWithCallback('baz', function() {
        this.callbackCalled = true;
      });)";
  CallFunctionOnObject(context, alpha_api, kTestCall);

  ValidateLastRequest("alpha.functionWithCallback", "['baz']");
  ASSERT_TRUE(console_errors().empty());

  TestJSRunner::AllowErrors allow_errors;
  bindings_system()->CompleteRequest(
      last_request()->request_id, ListValueFromString(R"(["alpha", "beta"])"),
      std::string());

  // The callback should have never been called and there should now be a
  // console error logged.
  EXPECT_EQ("undefined", GetStringPropertyFromObject(context->Global(), context,
                                                     "callbackCalled"));
  ASSERT_EQ(1u, console_errors().size());
  EXPECT_THAT(console_errors()[0],
              testing::StartsWith(
                  "Error handling response: Error: Custom callback threw"));
}

// Tests that an error thrown in a setCustomCallback hook while using a promise
// based call works as expected.
TEST_F(APIBindingsSystemTest, TestSetCustomCallback_ErrorWithPromise) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  APIBindingHooks* hooks = nullptr;
  v8::Local<v8::Object> alpha_api =
      bindings_system()->CreateAPIInstance(kAlphaAPIName, context, &hooks);
  ASSERT_FALSE(alpha_api.IsEmpty());
  ASSERT_TRUE(hooks);
  v8::Local<v8::Object> js_hooks = hooks->GetJSHookInterface(context);
  v8::Local<v8::Function> function =
      FunctionFromString(context, kCustomCallbackThrowHook);
  v8::Local<v8::Value> args[] = {js_hooks};
  RunFunctionOnGlobal(function, context, std::size(args), args);

  const char kTestCall[] = R"(return obj.functionWithCallback('boz');)";
  v8::Local<v8::Value> result =
      CallFunctionOnObject(context, alpha_api, kTestCall);

  ValidateLastRequest("alpha.functionWithCallback", "['boz']");
  v8::Local<v8::Promise> promise;
  ASSERT_TRUE(GetValueAs(result, &promise));
  EXPECT_EQ(v8::Promise::kPending, promise->State());
  ASSERT_TRUE(console_errors().empty());

  TestJSRunner::AllowErrors allow_errors;
  bindings_system()->CompleteRequest(
      last_request()->request_id, ListValueFromString(R"(["gamma", "delta"])"),
      std::string());

  // The promise will remain pending and there should now be a console error
  // logged.
  // TODO(tjudkins): Ideally we should be rejecting the promise here instead.
  EXPECT_EQ(v8::Promise::kPending, promise->State());
  ASSERT_EQ(1u, console_errors().size());
  EXPECT_THAT(console_errors()[0],
              testing::StartsWith(
                  "Error handling response: Error: Custom callback threw"));
}

// Test that references to other API's types works.
TEST_F(APIBindingsSystemTest, CrossAPIReferences) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  // Instantiate gamma API. Note: It's important that we haven't instantiated
  // alpha API yet, since this tests that we can lazily populate the type
  // information.
  v8::Local<v8::Object> gamma_api =
      bindings_system()->CreateAPIInstance(kGammaAPIName, context, nullptr);
  ASSERT_FALSE(gamma_api.IsEmpty());

  {
    // Test a simple call -> response.
    const char kTestCall[] = "obj.functionWithExternalRef({prop1: 'foo'});";
    CallFunctionOnObject(context, gamma_api, kTestCall);
    ValidateLastRequest("gamma.functionWithExternalRef", "[{'prop1':'foo'}]");
    reset_last_request();
  }
}

TEST_F(APIBindingsSystemTest, TestCustomEvent) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  auto create_custom_event = [](v8::Local<v8::Context> context,
                                const std::string& event_name) {
    v8::Isolate* isolate = context->GetIsolate();
    v8::Local<v8::Object> ret = v8::Object::New(isolate);
    ret->Set(context, gin::StringToSymbol(isolate, "name"),
             gin::StringToSymbol(isolate, event_name))
        .ToChecked();
    return ret.As<v8::Value>();
  };

  auto test_hooks = std::make_unique<APIBindingHooksTestDelegate>();
  test_hooks->SetCustomEvent(base::BindRepeating(create_custom_event));
  bindings_system()->RegisterHooksDelegate(kAlphaAPIName,
                                           std::move(test_hooks));

  v8::Local<v8::Object> api =
      bindings_system()->CreateAPIInstance(kAlphaAPIName, context, nullptr);

  v8::Local<v8::Object> event;
  ASSERT_TRUE(GetPropertyFromObjectAs(api, context, "alphaEvent", &event));
  EXPECT_EQ(R"("alpha.alphaEvent")",
            GetStringPropertyFromObject(event, context, "name"));
  v8::Local<v8::Value> event2 =
      GetPropertyFromObject(api, context, "alphaEvent");
  EXPECT_EQ(event, event2);

  v8::Local<v8::Object> other_event;
  ASSERT_TRUE(
      GetPropertyFromObjectAs(api, context, "alphaOtherEvent", &other_event));
  EXPECT_EQ(R"("alpha.alphaOtherEvent")",
            GetStringPropertyFromObject(other_event, context, "name"));
  EXPECT_NE(event, other_event);
}

}  // namespace extensions

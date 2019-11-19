// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_bindings_system.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "extensions/common/event_filtering_info.h"
#include "extensions/renderer/bindings/api_binding.h"
#include "extensions/renderer/bindings/api_binding_hooks.h"
#include "extensions/renderer/bindings/api_binding_hooks_test_delegate.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/api_bindings_system_unittest.h"
#include "extensions/renderer/bindings/api_invocation_errors.h"
#include "extensions/renderer/bindings/test_interaction_provider.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/try_catch.h"

namespace extensions {

namespace {

// Fake API for testing.
const char kAlphaAPIName[] = "alpha";
const char kAlphaAPISpec[] =
    "{"
    "  'types': [{"
    "    'id': 'alpha.objRef',"
    "    'type': 'object',"
    "    'properties': {"
    "      'prop1': {'type': 'string'},"
    "      'prop2': {'type': 'integer', 'optional': true}"
    "    }"
    "  }, {"
    "    'id': 'alpha.enumRef',"
    "    'type': 'string',"
    "    'enum': ['cat', 'dog']"
    "  }],"
    "  'functions': [{"
    "    'name': 'functionWithCallback',"
    "    'parameters': [{"
    "      'name': 'str',"
    "      'type': 'string'"
    "    }, {"
    "      'name': 'callback',"
    "      'type': 'function'"
    "    }]"
    "  }, {"
    "    'name': 'functionWithRefAndCallback',"
    "    'parameters': [{"
    "      'name': 'ref',"
    "      '$ref': 'alpha.objRef'"
    "    }, {"
    "      'name': 'callback',"
    "      'type': 'function'"
    "    }]"
    "  }, {"
    "    'name': 'functionWithEnum',"
    "    'parameters': [{'name': 'e', '$ref': 'alpha.enumRef'}]"
    "  }],"
    "  'events': [{"
    "    'name': 'alphaEvent'"
    "  }, {"
    "    'name': 'alphaOtherEvent'"
    "  }]"
    "}";

// Another fake API for testing.
const char kBetaAPIName[] = "beta";
const char kBetaAPISpec[] =
    "{"
    "  'functions': [{"
    "    'name': 'simpleFunc',"
    "    'parameters': [{'name': 'int', 'type': 'integer'}]"
    "  }]"
    "}";

const char kGammaAPIName[] = "gamma";
const char kGammaAPISpec[] =
    "{"
    "  'functions': [{"
    "    'name': 'functionWithExternalRef',"
    "    'parameters': [{ 'name': 'someRef', '$ref': 'alpha.objRef' }]"
    "  }]"
    "}";

bool AllowAllAPIs(v8::Local<v8::Context> context, const std::string& name) {
  return true;
}

}  // namespace

APIBindingsSystemTest::APIBindingsSystemTest() {}
APIBindingsSystemTest::~APIBindingsSystemTest() = default;

void APIBindingsSystemTest::SetUp() {
  APIBindingTest::SetUp();

  // Create the fake API schemas.
  for (const auto& api : GetAPIs()) {
    std::unique_ptr<base::DictionaryValue> api_schema =
        DictionaryValueFromString(api.spec);
    ASSERT_TRUE(api_schema);
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
      base::BindRepeating(&AllowAllAPIs),
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

const base::DictionaryValue& APIBindingsSystemTest::GetAPISchema(
    const std::string& api_name) {
  EXPECT_TRUE(base::Contains(api_schemas_, api_name));
  return *api_schemas_[api_name];
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
    const base::DictionaryValue* filter,
    bool was_manual,
    v8::Local<v8::Context> context) {}

void APIBindingsSystemTest::ValidateLastRequest(
    const std::string& expected_name,
    const std::string& expected_arguments) {
  ASSERT_TRUE(last_request());
  // Note that even if no arguments are provided by the API call, we should
  // have an empty list.
  ASSERT_TRUE(last_request()->arguments);
  EXPECT_EQ(expected_name, last_request()->method_name);
  EXPECT_EQ(ReplaceSingleQuotes(expected_arguments),
            ValueToString(*last_request()->arguments));
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
    const char kTestCall[] =
        "obj.functionWithCallback('foo', function() {\n"
        "  this.callbackArguments = Array.from(arguments);\n"
        "});";
    CallFunctionOnObject(context, alpha_api, kTestCall);

    ValidateLastRequest("alpha.functionWithCallback", "['foo']");

    const char kResponseArgsJson[] = "['response',1,{'key':42}]";
    std::unique_ptr<base::ListValue> expected_args =
        ListValueFromString(kResponseArgsJson);
    bindings_system()->CompleteRequest(last_request()->request_id,
                                       *expected_args, std::string());

    EXPECT_EQ(ReplaceSingleQuotes(kResponseArgsJson),
              GetStringPropertyFromObject(context->Global(), context,
                                          "callbackArguments"));
    reset_last_request();
  }

  {
    // Test a call with references -> response.
    const char kTestCall[] =
        "obj.functionWithRefAndCallback({prop1: 'alpha', prop2: 42},\n"
        "                               function() {\n"
        "  this.callbackArguments = Array.from(arguments);\n"
        "});";

    CallFunctionOnObject(context, alpha_api, kTestCall);

    ValidateLastRequest("alpha.functionWithRefAndCallback",
                        "[{'prop1':'alpha','prop2':42}]");

    bindings_system()->CompleteRequest(last_request()->request_id,
                                       base::ListValue(), std::string());

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
        function, context, base::size(args), args,
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
    const char kTestCall[] =
        "obj.alphaEvent.addListener(function() {\n"
        "  this.eventArguments = Array.from(arguments);\n"
        "});\n";
    CallFunctionOnObject(context, alpha_api, kTestCall);

    const char kResponseArgsJson[] = "['response',1,{'key':42}]";
    std::unique_ptr<base::ListValue> expected_args =
        ListValueFromString(kResponseArgsJson);
    bindings_system()->FireEventInContext("alpha.alphaEvent", context,
                                          *expected_args, nullptr);

    EXPECT_EQ(ReplaceSingleQuotes(kResponseArgsJson),
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
                 std::vector<v8::Local<v8::Value>>* arguments,
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
  APIBindingHooks* binding_hooks =
      bindings_system()->GetHooksForAPI(kAlphaAPIName);
  binding_hooks->SetDelegate(std::move(test_hooks));

  v8::Local<v8::Object> alpha_api =
      bindings_system()->CreateAPIInstance(kAlphaAPIName, context, nullptr);
  ASSERT_FALSE(alpha_api.IsEmpty());

  {
    // Test a simple call -> response.
    const char kTestCall[] =
        "obj.functionWithCallback('foo', function() {\n"
        "  this.callbackArguments = Array.from(arguments);\n"
        "});";
    CallFunctionOnObject(context, alpha_api, kTestCall);
    EXPECT_TRUE(did_call);

    EXPECT_EQ("[\"bar\"]",
              GetStringPropertyFromObject(context->Global(), context,
                                          "callbackArguments"));
  }
}

// Tests the setCustomCallback hook.
TEST_F(APIBindingsSystemTest, TestSetCustomCallback) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  const char kHook[] =
      "(function(hooks) {\n"
      "  hooks.setCustomCallback(\n"
      "      'functionWithCallback', (name, request, originalCallback,\n"
      "                               firstResult, secondResult) => {\n"
      "    this.methodName = name;\n"
      // TODO(devlin): Currently, we don't actually pass anything useful in for
      // the |request| object. If/when we do, we should test it.
      "    this.results = [firstResult, secondResult];\n"
      "    originalCallback(secondResult);\n"
      "  });\n"
      "})";

  APIBindingHooks* hooks = nullptr;
  v8::Local<v8::Object> alpha_api =
      bindings_system()->CreateAPIInstance(kAlphaAPIName, context, &hooks);
  ASSERT_FALSE(alpha_api.IsEmpty());
  ASSERT_TRUE(hooks);
  v8::Local<v8::Object> js_hooks = hooks->GetJSHookInterface(context);
  v8::Local<v8::Function> function = FunctionFromString(context, kHook);
  v8::Local<v8::Value> args[] = {js_hooks};
  RunFunctionOnGlobal(function, context, base::size(args), args);

  {
    const char kTestCall[] =
        "obj.functionWithCallback('foo', function() {\n"
        "  this.callbackArguments = Array.from(arguments);\n"
        "});";
    CallFunctionOnObject(context, alpha_api, kTestCall);

    ValidateLastRequest("alpha.functionWithCallback", "['foo']");

    std::unique_ptr<base::ListValue> response =
        ListValueFromString("['alpha','beta']");
    bindings_system()->CompleteRequest(last_request()->request_id, *response,
                                       std::string());

    EXPECT_EQ(
        "\"alpha.functionWithCallback\"",
        GetStringPropertyFromObject(context->Global(), context, "methodName"));
    EXPECT_EQ(
        "[\"alpha\",\"beta\"]",
        GetStringPropertyFromObject(context->Global(), context, "results"));
    EXPECT_EQ("[\"beta\"]",
              GetStringPropertyFromObject(context->Global(), context,
                                          "callbackArguments"));
  }
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
  APIBindingHooks* binding_hooks =
      bindings_system()->GetHooksForAPI(kAlphaAPIName);
  binding_hooks->SetDelegate(std::move(test_hooks));

  v8::Local<v8::Object> api =
      bindings_system()->CreateAPIInstance(kAlphaAPIName, context, nullptr);

  v8::Local<v8::Value> event =
      GetPropertyFromObject(api, context, "alphaEvent");
  ASSERT_TRUE(event->IsObject());
  EXPECT_EQ(
      "\"alpha.alphaEvent\"",
      GetStringPropertyFromObject(event.As<v8::Object>(), context, "name"));
  v8::Local<v8::Value> event2 =
      GetPropertyFromObject(api, context, "alphaEvent");
  EXPECT_EQ(event, event2);

  v8::Local<v8::Value> other_event =
      GetPropertyFromObject(api, context, "alphaOtherEvent");
  ASSERT_TRUE(other_event->IsObject());
  EXPECT_EQ("\"alpha.alphaOtherEvent\"",
            GetStringPropertyFromObject(other_event.As<v8::Object>(), context,
                                        "name"));
  EXPECT_NE(event, other_event);
}

}  // namespace extensions

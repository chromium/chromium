// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_binding.h"

#include <string_view>
#include <tuple>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/renderer/bindings/api_binding_hooks.h"
#include "extensions/renderer/bindings/api_binding_hooks_test_delegate.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/api_event_handler.h"
#include "extensions/renderer/bindings/api_invocation_errors.h"
#include "extensions/renderer/bindings/api_request_handler.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/bindings/api_type_reference_map.h"
#include "extensions/renderer/bindings/binding_access_checker.h"
#include "extensions/renderer/bindings/exception_handler.h"
#include "extensions/renderer/bindings/test_interaction_provider.h"
#include "extensions/renderer/bindings/test_js_runner.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/public/context_holder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace extensions {

namespace {

const char kBindingName[] = "test";

// Function spec; we use single quotes for readability and then replace them.
const char kFunctions[] =
    "[{"
    "  'name': 'oneString',"
    "  'parameters': [{"
    "    'type': 'string',"
    "    'name': 'str'"
    "   }]"
    "}, {"
    "  'name': 'stringAndInt',"
    "  'parameters': [{"
    "    'type': 'string',"
    "    'name': 'str'"
    "   }, {"
    "     'type': 'integer',"
    "     'name': 'int'"
    "   }]"
    "}, {"
    "  'name': 'oneObject',"
    "  'parameters': [{"
    "    'type': 'object',"
    "    'name': 'foo',"
    "    'properties': {"
    "      'prop1': {'type': 'string'},"
    "      'prop2': {'type': 'string', 'optional': true}"
    "    }"
    "  }]"
    "}, {"
    "  'name': 'intAndCallback',"
    "  'parameters': [{"
    "    'name': 'int',"
    "    'type': 'integer'"
    "  }],"
    "  'returns_async': {"
    "    'name': 'callback',"
    "    'type': 'function'"
    "  }"
    "}]";

constexpr char kFunctionsWithCallbackSignatures[] = R"(
    [{
       "name": "noCallback",
       "parameters": [{
         "name": "int",
         "type": "integer"
       }]
     }, {
       "name": "intCallback",
       "parameters": [],
       "returns_async": {
         "name": "callback",
         "does_not_support_promises": "Test",
         "parameters": [{
           "name": "int",
           "type": "integer"
         }]
       }
     }, {
       "name": "noParamCallback",
       "parameters": [],
       "returns_async": {
         "name": "callback",
         "does_not_support_promises": "Test",
         "parameters": []
       }
     }])";

constexpr char kFunctionsWithPromiseSignatures[] =
    R"([{
          "name": "supportsPromises",
          "parameters": [{
            "name": "int",
            "type": "integer"
          }],
          "returns_async": {
            "name": "callback",
            "parameters": [{
              "name": "strResult",
              "type": "string"
            }]
          }
        },
        {
          "name": "callbackOptional",
          "parameters": [{
            "name": "int",
            "type": "integer"
          }],
          "returns_async": {
            "name": "callback",
            "optional": true,
            "parameters": [{
              "name": "strResult",
              "type": "string"
            }]
          }
        }])";

bool AllowAllFeatures(v8::Local<v8::Context> context, const std::string& name) {
  return true;
}

bool DisallowPromises(v8::Local<v8::Context> context) {
  return false;
}

void OnEventListenersChanged(const std::string& event_name,
                             binding::EventListenersChanged change,
                             const base::Value::Dict* filter,
                             bool was_manual,
                             v8::Local<v8::Context> context) {}

}  // namespace

class APIBindingUnittest : public APIBindingTest {
 public:
  APIBindingUnittest(const APIBindingUnittest&) = delete;
  APIBindingUnittest& operator=(const APIBindingUnittest&) = delete;

  void OnFunctionCall(std::unique_ptr<APIRequestHandler::Request> request,
                      v8::Local<v8::Context> context) {
    last_request_ = std::move(request);
  }

  using GetParentCallback = base::RepeatingCallback<v8::Local<v8::Object>()>;
  v8::Local<v8::Object> GetParent(v8::Local<v8::Context> context,
                                  v8::Local<v8::Object>* secondary_parent) {
    DCHECK(!get_last_error_parent_.is_null())
        << "You must have get_last_error_parent_ set if a test is dealing with"
           "lastError being set";
    return get_last_error_parent_.Run();
  }

  void AddConsoleError(v8::Local<v8::Context> context,
                       const std::string& error) {
    console_errors_.push_back(error);
  }

 protected:
  APIBindingUnittest()
      : type_refs_(APITypeReferenceMap::InitializeTypeCallback()) {}
  void SetUp() override {
    APIBindingTest::SetUp();
    interaction_provider_ = std::make_unique<TestInteractionProvider>();
    binding::AddConsoleError add_console_error(base::BindRepeating(
        &APIBindingUnittest::AddConsoleError, base::Unretained(this)));
    exception_handler_ = std::make_unique<ExceptionHandler>(add_console_error);
    request_handler_ = std::make_unique<APIRequestHandler>(
        base::BindRepeating(&APIBindingUnittest::OnFunctionCall,
                            base::Unretained(this)),
        APILastError(base::BindRepeating(&APIBindingUnittest::GetParent,
                                         base::Unretained(this)),
                     add_console_error),
        exception_handler_.get(), interaction_provider_.get());
  }

  void TearDown() override {
    DisposeAllContexts();
    access_checker_.reset();
    interaction_provider_.reset();
    request_handler_.reset();
    event_handler_.reset();
    binding_.reset();
    APIBindingTest::TearDown();
  }

  void OnWillDisposeContext(v8::Local<v8::Context> context) override {
    event_handler_->InvalidateContext(context);
    request_handler_->InvalidateContext(context);
  }

  void SetFunctions(const char* functions) {
    binding_functions_ = ListValueFromString(functions);
  }

  void SetEvents(const char* events) {
    binding_events_ = ListValueFromString(events);
  }

  void SetTypes(const char* types) {
    binding_types_ = ListValueFromString(types);
  }

  void SetProperties(const char* properties) {
    binding_properties_ = DictValueFromString(properties);
  }

  void SetHooks(std::unique_ptr<APIBindingHooks> hooks) {
    binding_hooks_ = std::move(hooks);
    ASSERT_TRUE(binding_hooks_);
  }

  void SetHooksDelegate(
      std::unique_ptr<APIBindingHooksDelegate> hooks_delegate) {
    binding_hooks_delegate_ = std::move(hooks_delegate);
    ASSERT_TRUE(binding_hooks_delegate_);
  }

  void SetCreateCustomType(const APIBinding::CreateCustomType& callback) {
    create_custom_type_ = callback;
  }

  void SetOnSilentRequest(const APIBinding::OnSilentRequest& callback) {
    on_silent_request_ = callback;
  }

  void SetAPIAvailabilityCallback(
      const BindingAccessChecker::APIAvailabilityCallback& callback) {
    api_availability_callback_ = callback;
  }

  void SetPromiseAvailabilityFlag(bool* availability_flag) {
    promise_availability_callback_ = base::BindRepeating(
        [](bool* flag, v8::Local<v8::Context> context) { return *flag; },
        availability_flag);
  }

  void SetLastErrorParentCallback(GetParentCallback get_parent) {
    get_last_error_parent_ = std::move(get_parent);
  }

  void ClearConsoleErrors() { console_errors_.clear(); }

  void InitializeJSHooks(
      const char* register_hook,
      v8::Local<v8::Value> additional_arg = v8::Local<v8::Value>()) {
    auto hooks =
        std::make_unique<APIBindingHooks>(kBindingName, request_handler());

    v8::HandleScope handle_scope(isolate());
    v8::Local<v8::Context> context = MainContext();

    {
      v8::Local<v8::Object> js_hooks = hooks->GetJSHookInterface(context);
      v8::Local<v8::Function> function =
          FunctionFromString(context, register_hook);
      if (!additional_arg.IsEmpty()) {
        v8::Local<v8::Value> args[] = {js_hooks, additional_arg};
        RunFunctionOnGlobal(function, context, std::size(args), args);
      } else {
        v8::Local<v8::Value> args[] = {js_hooks};
        RunFunctionOnGlobal(function, context, std::size(args), args);
      }
    }
    SetHooks(std::move(hooks));
  }

  void InitializeBinding() {
    if (!binding_hooks_)
      binding_hooks_ =
          std::make_unique<APIBindingHooks>(kBindingName, request_handler());
    if (binding_hooks_delegate_)
      binding_hooks_->SetDelegate(std::move(binding_hooks_delegate_));
    if (!on_silent_request_)
      on_silent_request_ = base::DoNothing();
    if (!api_availability_callback_)
      api_availability_callback_ = base::BindRepeating(&AllowAllFeatures);
    if (!promise_availability_callback_)
      promise_availability_callback_ = base::BindRepeating(&DisallowPromises);
    auto get_context_owner = [](v8::Local<v8::Context>) {
      return std::string("context");
    };
    event_handler_ = std::make_unique<APIEventHandler>(
        base::BindRepeating(&OnEventListenersChanged),
        base::BindRepeating(get_context_owner), nullptr);
    access_checker_ = std::make_unique<BindingAccessChecker>(
        api_availability_callback_, promise_availability_callback_);
    binding_ = std::make_unique<APIBinding>(
        kBindingName, &binding_functions_, &binding_types_, &binding_events_,
        &binding_properties_, create_custom_type_, on_silent_request_,
        std::move(binding_hooks_), &type_refs_, request_handler_.get(),
        event_handler_.get(), access_checker_.get());
  }

  v8::Local<v8::Value> ExpectPass(
      v8::Local<v8::Object> object,
      const std::string& script_source,
      const std::string& expected_json_arguments_single_quotes,
      bool expect_async_handler) {
    return ExpectPass(MainContext(), object, script_source,
                      expected_json_arguments_single_quotes,
                      expect_async_handler);
  }

  v8::Local<v8::Value> ExpectPass(
      v8::Local<v8::Context> context,
      v8::Local<v8::Object> object,
      const std::string& script_source,
      const std::string& expected_json_arguments_single_quotes,
      bool expect_async_handler) {
    return RunTest(context, object, script_source, true,
                   ReplaceSingleQuotes(expected_json_arguments_single_quotes),
                   expect_async_handler, std::string());
  }

  void ExpectFailure(v8::Local<v8::Object> object,
                     const std::string& script_source,
                     const std::string& expected_error) {
    RunTest(MainContext(), object, script_source, false, std::string(), false,
            "Uncaught TypeError: " + expected_error);
  }

  void ExpectThrow(v8::Local<v8::Object> object,
                   const std::string& script_source,
                   const std::string& expected_error) {
    RunTest(MainContext(), object, script_source, false, std::string(), false,
            "Uncaught Error: " + expected_error);
  }

  bool HandlerWasInvoked() const { return last_request_ != nullptr; }
  const APIRequestHandler::Request* last_request() const {
    return last_request_.get();
  }
  void reset_last_request() { last_request_.reset(); }
  const std::vector<std::string>& console_errors() const {
    return console_errors_;
  }
  APIBinding* binding() { return binding_.get(); }
  APIEventHandler* event_handler() { return event_handler_.get(); }
  APIRequestHandler* request_handler() { return request_handler_.get(); }
  const APITypeReferenceMap& type_refs() const { return type_refs_; }

 private:
  v8::Local<v8::Value> RunTest(v8::Local<v8::Context> context,
                               v8::Local<v8::Object> object,
                               const std::string& script_source,
                               bool should_pass,
                               const std::string& expected_json_arguments,
                               bool expect_async_handler,
                               const std::string& expected_error);

  std::unique_ptr<APIRequestHandler::Request> last_request_;
  std::vector<std::string> console_errors_;
  GetParentCallback get_last_error_parent_;
  std::unique_ptr<APIBinding> binding_;
  std::unique_ptr<APIEventHandler> event_handler_;
  std::unique_ptr<TestInteractionProvider> interaction_provider_;
  std::unique_ptr<ExceptionHandler> exception_handler_;
  std::unique_ptr<APIRequestHandler> request_handler_;
  std::unique_ptr<BindingAccessChecker> access_checker_;
  APITypeReferenceMap type_refs_;

  base::Value::List binding_functions_;
  base::Value::List binding_events_;
  base::Value::List binding_types_;
  base::Value::Dict binding_properties_;
  std::unique_ptr<APIBindingHooks> binding_hooks_;
  std::unique_ptr<APIBindingHooksDelegate> binding_hooks_delegate_;
  APIBinding::CreateCustomType create_custom_type_;
  APIBinding::OnSilentRequest on_silent_request_;
  BindingAccessChecker::APIAvailabilityCallback api_availability_callback_;
  BindingAccessChecker::PromiseAvailabilityCallback
      promise_availability_callback_;
};

using APIBindingDeathTest = APIBindingUnittest;

v8::Local<v8::Value> APIBindingUnittest::RunTest(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> object,
    const std::string& script_source,
    bool should_pass,
    const std::string& expected_json_arguments,
    bool expect_async_handler,
    const std::string& expected_error) {
  EXPECT_FALSE(last_request_);
  std::string wrapped_script_source =
      base::StringPrintf("(function(obj) { %s })", script_source.c_str());

  v8::Local<v8::Function> func =
      FunctionFromString(context, wrapped_script_source);
  if (func.IsEmpty()) {
    ADD_FAILURE() << "Script source couldn't be converted to a function: "
                  << script_source;
    return v8::Local<v8::Value>();
  }

  v8::Local<v8::Value> argv[] = {object};
  v8::Local<v8::Value> result;

  if (should_pass) {
    result = RunFunction(func, context, 1, argv);
    if (!last_request_) {
      ADD_FAILURE() << "No request was made. Script source: " << script_source;
      return v8::Local<v8::Value>();
    }
    EXPECT_EQ(expected_json_arguments,
              ValueToString(last_request_->arguments_list));
    EXPECT_EQ(expect_async_handler, last_request_->has_async_response_handler)
        << script_source;
  } else {
    RunFunctionAndExpectError(func, context, 1, argv, expected_error);
    EXPECT_FALSE(last_request_);
  }

  last_request_.reset();
  return result;
}

TEST_F(APIBindingUnittest, TestEmptyAPI) {
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);
  EXPECT_EQ(
      0u,
      binding_object->GetOwnPropertyNames(context).ToLocalChecked()->Length());
}

// Tests the basic call -> request flow of the API binding (ensuring that
// functions are set up correctly and correctly enforced).
TEST_F(APIBindingUnittest, TestBasicAPICalls) {
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // Argument parsing is tested primarily in APISignature and ArgumentSpec
  // tests, so do a few quick sanity checks...
  ExpectPass(binding_object, "obj.oneString('foo');", "['foo']", false);
  ExpectFailure(binding_object, "obj.oneString(1);",
                api_errors::InvocationError("test.oneString", "string str",
                                            api_errors::NoMatchingSignature()));
  ExpectPass(binding_object, "obj.stringAndInt('foo', 1)", "['foo',1]", false);
  ExpectFailure(binding_object, "obj.stringAndInt(1)",
                api_errors::InvocationError("test.stringAndInt",
                                            "string str, integer int",
                                            api_errors::NoMatchingSignature()));
  ExpectPass(binding_object, "obj.intAndCallback(1, function() {})", "[1]",
             true);
  ExpectFailure(binding_object, "obj.intAndCallback(function() {})",
                api_errors::InvocationError("test.intAndCallback",
                                            "integer int, function callback",
                                            api_errors::NoMatchingSignature()));

  // ...And an interesting case (throwing an error during parsing).
  ExpectThrow(binding_object,
              "obj.oneObject({ get prop1() { throw new Error('Badness'); } });",
              "Badness");
}

// Test that enum values are properly exposed on the binding object.
TEST_F(APIBindingUnittest, EnumValues) {
  const char kTypes[] =
      "[{"
      "  'id': 'first',"
      "  'type': 'string',"
      "  'enum': ['alpha', 'camelCase', 'Hyphen-ated',"
      "           'SCREAMING', 'nums123', '42nums']"
      "}, {"
      "  'id': 'last',"
      "  'type': 'string',"
      "  'enum': [{'name': 'omega'}]"
      "}]";

  SetTypes(kTypes);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  const char kExpected[] =
      "{'ALPHA':'alpha','CAMEL_CASE':'camelCase','HYPHEN_ATED':'Hyphen-ated',"
      "'NUMS123':'nums123','SCREAMING':'SCREAMING','_42NUMS':'42nums'}";
  EXPECT_EQ(ReplaceSingleQuotes(kExpected),
            GetStringPropertyFromObject(binding_object, context, "first"));
  EXPECT_EQ(ReplaceSingleQuotes("{'OMEGA':'omega'}"),
            GetStringPropertyFromObject(binding_object, context, "last"));
}

// Test that empty enum entries are (unfortunately) allowed.
TEST_F(APIBindingUnittest, EnumWithEmptyEntry) {
  const char kTypes[] =
      "[{"
      "  'id': 'enumWithEmpty',"
      "  'type': 'string',"
      "  'enum': [{'name': ''}, {'name': 'other'}]"
      "}]";

  SetTypes(kTypes);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  EXPECT_EQ(
      "{\"\":\"\",\"OTHER\":\"other\"}",
      GetStringPropertyFromObject(binding_object, context, "enumWithEmpty"));
}

// Test that type references are correctly set up in the API.
TEST_F(APIBindingUnittest, TypeRefsTest) {
  const char kTypes[] =
      "[{"
      "  'id': 'refObj',"
      "  'type': 'object',"
      "  'properties': {"
      "    'prop1': {'type': 'string'},"
      "    'prop2': {'type': 'integer', 'optional': true}"
      "  }"
      "}, {"
      "  'id': 'refEnum',"
      "  'type': 'string',"
      "  'enum': ['alpha', 'beta']"
      "}]";
  const char kRefFunctions[] =
      "[{"
      "  'name': 'takesRefObj',"
      "  'parameters': [{"
      "    'name': 'o',"
      "    '$ref': 'refObj'"
      "  }]"
      "}, {"
      "  'name': 'takesRefEnum',"
      "  'parameters': [{"
      "    'name': 'e',"
      "    '$ref': 'refEnum'"
      "   }]"
      "}]";

  SetFunctions(kRefFunctions);
  SetTypes(kTypes);
  InitializeBinding();
  EXPECT_EQ(2u, type_refs().size());
  EXPECT_TRUE(type_refs().GetSpec("refObj"));
  EXPECT_TRUE(type_refs().GetSpec("refEnum"));

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // Parsing in general is tested in APISignature and ArgumentSpec tests, but
  // we test that the binding a) correctly finds the definitions, and b) accepts
  // properties from the API object.
  ExpectPass(binding_object, "obj.takesRefObj({prop1: 'foo'})",
             "[{'prop1':'foo'}]", false);
  ExpectFailure(binding_object, "obj.takesRefObj({prop1: 'foo', prop2: 'a'})",
                api_errors::InvocationError(
                    "test.takesRefObj", "refObj o",
                    api_errors::ArgumentError(
                        "o", api_errors::PropertyError(
                                 "prop2", api_errors::InvalidType(
                                              api_errors::kTypeInteger,
                                              api_errors::kTypeString)))));
  ExpectPass(binding_object, "obj.takesRefEnum('alpha')", "['alpha']", false);
  ExpectPass(binding_object, "obj.takesRefEnum(obj.refEnum.BETA)", "['beta']",
             false);
  ExpectFailure(binding_object, "obj.takesRefEnum('gamma')",
                api_errors::InvocationError(
                    "test.takesRefEnum", "refEnum e",
                    api_errors::ArgumentError(
                        "e", api_errors::InvalidEnumValue({"alpha", "beta"}))));
}

TEST_F(APIBindingUnittest, RestrictedAPIs) {
  const char kLocalFunctions[] =
      "[{"
      "  'name': 'allowedOne',"
      "  'parameters': []"
      "}, {"
      "  'name': 'allowedTwo',"
      "  'parameters': []"
      "}, {"
      "  'name': 'restrictedOne',"
      "  'parameters': []"
      "}, {"
      "  'name': 'restrictedTwo',"
      "  'parameters': []"
      "}]";
  SetFunctions(kLocalFunctions);
  const char kEvents[] =
      "[{'name': 'allowedEvent'}, {'name': 'restrictedEvent'}]";
  SetEvents(kEvents);
  const char kProperties[] =
      R"({
           "allowedProperty": { "type": "integer", "value": 3 },
           "restrictedProperty": { "type": "string", "value": "restricted" }
         })";
  SetProperties(kProperties);
  auto is_available = [](v8::Local<v8::Context> context,
                         const std::string& name) {
    std::set<std::string> allowed = {"test.allowedOne", "test.allowedTwo",
                                     "test.allowedEvent",
                                     "test.allowedProperty"};
    std::set<std::string> restricted = {
        "test.restrictedOne", "test.restrictedTwo", "test.restrictedEvent",
        "test.restrictedProperty"};
    EXPECT_TRUE(allowed.count(name) || restricted.count(name)) << name;
    return allowed.count(name) != 0;
  };
  SetAPIAvailabilityCallback(base::BindRepeating(is_available));

  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  auto is_defined = [&binding_object, context](const std::string& name) {
    v8::Local<v8::Value> val =
        GetPropertyFromObject(binding_object, context, name);
    EXPECT_FALSE(val.IsEmpty());
    return !val->IsUndefined() && !val->IsNull();
  };

  EXPECT_TRUE(is_defined("allowedOne"));
  EXPECT_TRUE(is_defined("allowedTwo"));
  EXPECT_TRUE(is_defined("allowedEvent"));
  EXPECT_TRUE(is_defined("allowedProperty"));
  EXPECT_FALSE(is_defined("restrictedOne"));
  EXPECT_FALSE(is_defined("restrictedTwo"));
  EXPECT_FALSE(is_defined("restrictedEvent"));
  EXPECT_FALSE(is_defined("restrictedProperty"));
}

// Tests that events specified in the API are created as properties of the API
// object.
TEST_F(APIBindingUnittest, TestEventCreation) {
  SetEvents(
      R"([
           {'name': 'onFoo'},
           {'name': 'onBar'},
           {'name': 'onBaz', 'options': {'maxListeners': 1}}
         ])");
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // Event behavior is tested in the APIEventHandler unittests as well as the
  // APIBindingsSystem tests, so we really only need to check that the events
  // are being initialized on the object.
  v8::Maybe<bool> has_on_foo =
      binding_object->Has(context, gin::StringToV8(isolate(), "onFoo"));
  EXPECT_TRUE(has_on_foo.IsJust());
  EXPECT_TRUE(has_on_foo.FromJust());

  v8::Maybe<bool> has_on_bar =
      binding_object->Has(context, gin::StringToV8(isolate(), "onBar"));
  EXPECT_TRUE(has_on_bar.IsJust());
  EXPECT_TRUE(has_on_bar.FromJust());

  v8::Maybe<bool> has_on_baz =
      binding_object->Has(context, gin::StringToV8(isolate(), "onBaz"));
  EXPECT_TRUE(has_on_baz.IsJust());
  EXPECT_TRUE(has_on_baz.FromJust());

  // Test that the maxListeners property is correctly used.
  v8::Local<v8::Function> add_listener = FunctionFromString(
      context, "(function(e) { e.addListener(function() {}); })");
  v8::Local<v8::Value> args[] = {
      GetPropertyFromObject(binding_object, context, "onBaz")};
  RunFunction(add_listener, context, std::size(args), args);
  EXPECT_EQ(1u, event_handler()->GetNumEventListenersForTesting("test.onBaz",
                                                                context));
  RunFunctionAndExpectError(add_listener, context, std::size(args), args,
                            "Uncaught TypeError: Too many listeners.");
  EXPECT_EQ(1u, event_handler()->GetNumEventListenersForTesting("test.onBaz",
                                                                context));

  v8::Maybe<bool> has_nonexistent_event = binding_object->Has(
      context, gin::StringToV8(isolate(), "onNonexistentEvent"));
  EXPECT_TRUE(has_nonexistent_event.IsJust());
  EXPECT_FALSE(has_nonexistent_event.FromJust());
}

TEST_F(APIBindingUnittest, TestProperties) {
  SetProperties(
      "{"
      "  'prop1': { 'value': 17, 'type': 'integer' },"
      "  'prop2': {"
      "    'type': 'object',"
      "    'properties': {"
      "      'subprop1': { 'value': 'some value', 'type': 'string' },"
      "      'subprop2': { 'value': true, 'type': 'boolean' }"
      "    }"
      "  },"
      "  'linuxOnly': {"
      "    'value': 'linux',"
      "    'type': 'string',"
      "    'platforms': ['linux']"
      "  },"
      "  'lacrosOnly': {"
      "    'value': 'lacros',"
      "    'type': 'string',"
      "    'platforms': ['lacros']"
      "  },"
      "  'notLinuxOrLacros': {"
      "    'value': 'nonlinux',"
      "    'type': 'string',"
      "    'platforms': ["
      "       'win', 'mac', 'chromeos', 'fuchsia', 'desktop_android']"
      "  }"
      "}");
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);
  EXPECT_EQ("17",
            GetStringPropertyFromObject(binding_object, context, "prop1"));
  EXPECT_EQ(R"({"subprop1":"some value","subprop2":true})",
            GetStringPropertyFromObject(binding_object, context, "prop2"));

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_EQ("\"lacros\"",
            GetStringPropertyFromObject(binding_object, context, "lacrosOnly"));
  EXPECT_EQ("undefined",
            GetStringPropertyFromObject(binding_object, context, "linuxOnly"));
  EXPECT_EQ("undefined", GetStringPropertyFromObject(binding_object, context,
                                                     "notLinuxOrLacros"));
#elif BUILDFLAG(IS_LINUX)
  EXPECT_EQ("\"linux\"",
            GetStringPropertyFromObject(binding_object, context, "linuxOnly"));
  EXPECT_EQ("undefined", GetStringPropertyFromObject(binding_object, context,
                                                     "notLinuxOrLacros"));
  EXPECT_EQ("undefined",
            GetStringPropertyFromObject(binding_object, context, "lacrosOnly"));
#else
  EXPECT_EQ("undefined",
            GetStringPropertyFromObject(binding_object, context, "linuxOnly"));
  EXPECT_EQ("undefined",
            GetStringPropertyFromObject(binding_object, context, "lacrosOnly"));
  EXPECT_EQ("\"nonlinux\"", GetStringPropertyFromObject(binding_object, context,
                                                        "notLinuxOrLacros"));
#endif
}

TEST_F(APIBindingUnittest, TestRefProperties) {
  SetProperties(
      "{"
      "  'alpha': {"
      "    '$ref': 'AlphaRef',"
      "    'value': ['a']"
      "  },"
      "  'beta': {"
      "    '$ref': 'BetaRef',"
      "    'value': ['b']"
      "  }"
      "}");
  auto create_custom_type = [](v8::Isolate* isolate,
                               const std::string& type_name,
                               const std::string& property_name,
                               const base::Value::List* property_values) {
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Object> result = v8::Object::New(isolate);
    if (type_name == "AlphaRef") {
      EXPECT_EQ("alpha", property_name);
      EXPECT_EQ("[\"a\"]", ValueToString(*property_values));
      result
          ->Set(context, gin::StringToSymbol(isolate, "alphaProp"),
                gin::StringToV8(isolate, "alphaVal"))
          .ToChecked();
    } else if (type_name == "BetaRef") {
      EXPECT_EQ("beta", property_name);
      EXPECT_EQ("[\"b\"]", ValueToString(*property_values));
      result
          ->Set(context, gin::StringToSymbol(isolate, "betaProp"),
                gin::StringToV8(isolate, "betaVal"))
          .ToChecked();
    } else {
      EXPECT_TRUE(false) << type_name;
    }
    return result;
  };

  SetCreateCustomType(base::BindRepeating(create_custom_type));

  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);
  EXPECT_EQ(R"({"alphaProp":"alphaVal"})",
            GetStringPropertyFromObject(binding_object, context, "alpha"));
  EXPECT_EQ(
      R"({"betaProp":"betaVal"})",
      GetStringPropertyFromObject(binding_object, context, "beta"));
}

TEST_F(APIBindingUnittest, TestDisposedContext) {
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  v8::Local<v8::Function> func =
      FunctionFromString(context, "(function(obj) { obj.oneString('foo'); })");
  v8::Local<v8::Value> argv[] = {binding_object};
  DisposeContext(context);

  RunFunctionAndExpectError(func, context, std::size(argv), argv,
                            "Uncaught Error: Extension context invalidated.");

  EXPECT_FALSE(HandlerWasInvoked());
  // This test passes if this does not crash, even under AddressSanitizer
  // builds.
}

TEST_F(APIBindingUnittest, TestInvalidatedContext) {
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  v8::Local<v8::Function> func =
      FunctionFromString(context, "(function(obj) { obj.oneString('foo'); })");
  v8::Local<v8::Value> argv[] = {binding_object};
  binding::InvalidateContext(context);

  RunFunctionAndExpectError(func, context, std::size(argv), argv,
                            "Uncaught Error: Extension context invalidated.");

  EXPECT_FALSE(HandlerWasInvoked());
  // This test passes if this does not crash, even under AddressSanitizer
  // builds.
}

TEST_F(APIBindingUnittest, MultipleContexts) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context_a = MainContext();
  v8::Local<v8::Context> context_b = AddContext();

  SetFunctions(kFunctions);
  InitializeBinding();

  v8::Local<v8::Object> binding_object_a = binding()->CreateInstance(context_a);
  v8::Local<v8::Object> binding_object_b = binding()->CreateInstance(context_b);

  ExpectPass(context_a, binding_object_a, "obj.oneString('foo');", "['foo']",
             false);
  ExpectPass(context_b, binding_object_b, "obj.oneString('foo');", "['foo']",
             false);
  DisposeContext(context_b);
  ExpectPass(context_a, binding_object_a, "obj.oneString('foo');", "['foo']",
             false);
}

// Tests adding custom hooks for an API method.
TEST_F(APIBindingUnittest, TestCustomHooks) {
  SetFunctions(kFunctions);

  // Register a hook for the test.oneString method.
  auto hooks = std::make_unique<APIBindingHooksTestDelegate>();
  bool did_call = false;
  auto hook = [](bool* did_call, const APISignature* signature,
                 v8::Local<v8::Context> context,
                 v8::LocalVector<v8::Value>* arguments,
                 const APITypeReferenceMap& ref_map) {
    *did_call = true;
    APIBindingHooks::RequestResult result(
        APIBindingHooks::RequestResult::HANDLED);
    if (arguments->size() != 1u) {  // ASSERT* messes with the return type.
      EXPECT_EQ(1u, arguments->size());
      return result;
    }
    EXPECT_EQ("foo", gin::V8ToString(context->GetIsolate(), arguments->at(0)));
    return result;
  };
  hooks->AddHandler("test.oneString", base::BindRepeating(hook, &did_call));
  SetHooksDelegate(std::move(hooks));

  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // First try calling the oneString() method, which has a custom hook
  // installed.
  v8::Local<v8::Function> func =
      FunctionFromString(context, "(function(obj) { obj.oneString('foo'); })");
  v8::Local<v8::Value> args[] = {binding_object};
  RunFunction(func, context, 1, args);
  EXPECT_TRUE(did_call);

  // Other methods, like stringAndInt(), should behave normally.
  ExpectPass(binding_object, "obj.stringAndInt('foo', 42);", "['foo',42]",
             false);
}

TEST_F(APIBindingUnittest, TestJSCustomHook) {
  // Register a hook for the test.oneString method.
  const char kRegisterHook[] = R"(
      (function(hooks) {
        hooks.setHandleRequest('oneString', function() {
          this.requestArguments = Array.from(arguments);
        });
      }))";
  InitializeJSHooks(kRegisterHook);
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // First try calling with an invalid invocation. An error should be raised and
  // the hook should never have been called, since the arguments didn't match.
  ExpectFailure(binding_object, "obj.oneString(1);",
                api_errors::InvocationError("test.oneString", "string str",
                                            api_errors::NoMatchingSignature()));
  v8::Local<v8::Value> property =
      GetPropertyFromObject(context->Global(), context, "requestArguments");
  ASSERT_FALSE(property.IsEmpty());
  EXPECT_TRUE(property->IsUndefined());

  // Try calling the oneString() method with valid arguments. The hook should
  // be called.
  v8::Local<v8::Function> func =
      FunctionFromString(context, "(function(obj) { obj.oneString('foo'); })");
  v8::Local<v8::Value> args[] = {binding_object};
  RunFunction(func, context, 1, args);

  EXPECT_EQ("[\"foo\"]", GetStringPropertyFromObject(
                             context->Global(), context, "requestArguments"));

  // Other methods, like stringAndInt(), should behave normally.
  ExpectPass(binding_object, "obj.stringAndInt('foo', 42);", "['foo',42]",
             false);
}

// Tests the updateArgumentsPreValidate hook.
TEST_F(APIBindingUnittest, TestUpdateArgumentsPreValidate) {
  // Register a hook for the test.oneString method.
  const char kRegisterHook[] = R"(
      (function(hooks) {
        hooks.setUpdateArgumentsPreValidate('oneString', function() {
          this.requestArguments = Array.from(arguments);
          if (this.requestArguments[0] === true)
            return ['hooked']
          return this.requestArguments
        });
      }))";
  InitializeJSHooks(kRegisterHook);
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // Call the method with a hook. Since the hook updates arguments before
  // validation, we should be able to pass in invalid arguments and still
  // have the hook called.
  ExpectFailure(binding_object, "obj.oneString(false);",
                api_errors::InvocationError("test.oneString", "string str",
                                            api_errors::NoMatchingSignature()));
  EXPECT_EQ("[false]", GetStringPropertyFromObject(
                           context->Global(), context, "requestArguments"));

  ExpectPass(binding_object, "obj.oneString(true);", "['hooked']", false);
  EXPECT_EQ("[true]", GetStringPropertyFromObject(
                          context->Global(), context, "requestArguments"));

  // Other methods, like stringAndInt(), should behave normally.
  ExpectPass(binding_object, "obj.stringAndInt('foo', 42);", "['foo',42]",
             false);
}

// Tests the updateArgumentsPreValidate hook.
TEST_F(APIBindingUnittest, TestThrowInUpdateArgumentsPreValidate) {
  // Register a hook for the test.oneString method.
  const char kRegisterHook[] = R"(
      (function(hooks) {
        hooks.setUpdateArgumentsPreValidate('oneString', function() {
          throw new Error('Custom Hook Error');
        });
      }))";
  InitializeJSHooks(kRegisterHook);
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  v8::Local<v8::Function> function =
      FunctionFromString(context,
                         "(function(obj) { return obj.oneString('ping'); })");
  v8::Local<v8::Value> args[] = {binding_object};
  {
    TestJSRunner::AllowErrors allow_errors;
    RunFunctionAndExpectError(function, context, v8::Undefined(isolate()),
                              std::size(args), args,
                              "Uncaught Error: Custom Hook Error");
  }

  // Other methods, like stringAndInt(), should behave normally.
  ExpectPass(binding_object, "obj.stringAndInt('foo', 42);", "['foo',42]",
             false);
}

// Tests that custom JS hooks can return results synchronously.
TEST_F(APIBindingUnittest, TestReturningResultFromCustomJSHook) {
  // Register a hook for the test.oneString method.
  const char kRegisterHook[] = R"(
      (function(hooks) {
        hooks.setHandleRequest('oneString', str => {
          return str + ' pong';
        });
      }))";

  InitializeJSHooks(kRegisterHook);
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  v8::Local<v8::Function> function =
      FunctionFromString(context,
                         "(function(obj) { return obj.oneString('ping'); })");
  v8::Local<v8::Value> args[] = {binding_object};
  v8::Local<v8::Value> result =
      RunFunction(function, context, std::size(args), args);
  ASSERT_FALSE(result.IsEmpty());
  std::unique_ptr<base::Value> json_result = V8ToBaseValue(result, context);
  ASSERT_TRUE(json_result);
  EXPECT_EQ("\"ping pong\"", ValueToString(*json_result));
}

// Tests that the setHandleRequest hook can use callbacks and promises.
TEST_F(APIBindingUnittest, TestReturningPromiseFromHandleRequestHook) {
  bool context_allows_promises = true;
  SetPromiseAvailabilityFlag(&context_allows_promises);

  // Register a hook for supportsPromises.
  const char kRegisterHook[] = R"(
      (function(hooks) {
        hooks.setHandleRequest('supportsPromises', (firstArg, callback) => {
          this.firstArgument = firstArg;
          this.secondArgument = callback;
        });
      }))";

  InitializeJSHooks(kRegisterHook);
  SetFunctions(kFunctionsWithPromiseSignatures);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  {
    // Calling supportsPromises normally with a callback should work fine and
    // the callback should be invoked immediately.
    const char kFunctionCall[] =
        R"((function(obj) {
             return obj.supportsPromises(5, (arg) => {
               this.sentToCallback = arg;
             });
           }))";
    v8::Local<v8::Function> function =
        FunctionFromString(context, kFunctionCall);
    v8::Local<v8::Value> args[] = {binding_object};

    auto result = RunFunction(function, context, v8::Undefined(isolate()),
                              std::size(args), args);

    ASSERT_FALSE(result.IsEmpty());
    EXPECT_TRUE(result->IsUndefined());
    EXPECT_EQ("5", GetStringPropertyFromObject(context->Global(), context,
                                               "firstArgument"));
    v8::Local<v8::Function> resolve_callback;
    ASSERT_TRUE(GetPropertyFromObjectAs(context->Global(), context,
                                        "secondArgument", &resolve_callback));

    // The callback arg will not be set until the callback has been invoked.
    EXPECT_TRUE(
        GetPropertyFromObject(context->Global(), context, "sentToCallabck")
            ->IsUndefined());
    v8::Local<v8::Value> callback_arguments[] = {
        gin::StringToV8(isolate(), "foo")};
    RunFunctionOnGlobal(resolve_callback, context,
                        std::size(callback_arguments), callback_arguments);
    EXPECT_EQ(R"("foo")", GetStringPropertyFromObject(
                              context->Global(), context, "sentToCallback"));
  }

  {
    // Calling supportsPromises normally without the callback should work fine
    // and a promise should be returned that is resolved when the callback is
    // invoked.
    v8::Local<v8::Function> function = FunctionFromString(
        context, "(function(obj) { return obj.supportsPromises(6); })");
    v8::Local<v8::Value> args[] = {binding_object};

    v8::Local<v8::Value> result = RunFunction(
        function, context, v8::Undefined(isolate()), std::size(args), args);
    v8::Local<v8::Promise> promise;
    ASSERT_TRUE(GetValueAs(result, &promise));

    EXPECT_EQ(v8::Promise::kPending, promise->State());
    EXPECT_EQ("6", GetStringPropertyFromObject(context->Global(), context,
                                               "firstArgument"));

    // Since we trigger the promise to be resolved with a function that calls
    // back into the C++ side, the second argument is actually a function here.
    v8::Local<v8::Function> resolve_callback;
    ASSERT_TRUE(GetPropertyFromObjectAs(context->Global(), context,
                                        "secondArgument", &resolve_callback));
    // Invoking this callback should result in the promise being resolved.
    v8::Local<v8::Value> callback_arguments[] = {
        gin::StringToV8(isolate(), "bar")};
    RunFunctionOnGlobal(resolve_callback, context,
                        std::size(callback_arguments), callback_arguments);
    EXPECT_EQ(v8::Promise::kFulfilled, promise->State());
    EXPECT_EQ(R"("bar")", V8ToString(promise->Result(), context));
  }

  {
    // If the context doesn't support promises, there should be an error if a
    // required callback isn't supplied.
    context_allows_promises = false;
    v8::Local<v8::Function> function = FunctionFromString(
        context, "(function(obj) { return obj.supportsPromises(7); })");
    v8::Local<v8::Value> args[] = {binding_object};
    auto expected_error =
        "Uncaught TypeError: " +
        api_errors::InvocationError("test.supportsPromises",
                                    "integer int, function callback",
                                    api_errors::NoMatchingSignature());
    RunFunctionAndExpectError(function, context, std::size(args), args,
                              expected_error);
  }
}

// Tests that JS custom hooks can throw exceptions for bad invocations.
TEST_F(APIBindingUnittest, TestThrowingFromCustomJSHook) {
  // Register a hook for the test.oneString method.
  const char kRegisterHook[] = R"(
      (function(hooks) {
        hooks.setHandleRequest('oneString', str => {
          throw new Error('Custom Hook Error');
        });
      }))";

  InitializeJSHooks(kRegisterHook);
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  v8::Local<v8::Function> function =
      FunctionFromString(context,
                         "(function(obj) { return obj.oneString('ping'); })");
  v8::Local<v8::Value> args[] = {binding_object};

  TestJSRunner::AllowErrors allow_errors;
  RunFunctionAndExpectError(function, context, v8::Undefined(isolate()),
                            std::size(args), args,
                            "Uncaught Error: Custom Hook Error");
}

// Tests that JS setHandleRequestHooks can use the failure callback to return a
// failure result for an API.
TEST_F(APIBindingUnittest, TestHandleRequestFailureCallback) {
  bool context_allows_promises = true;
  SetPromiseAvailabilityFlag(&context_allows_promises);

  // Register a hook for supportsPromises that calls the failure callback when
  // the API is called with the integer 6.
  const char kRegisterHook[] = R"(
      (function(hooks) {
        function handler(firstArg, callback, failureCallback) {
          if (firstArg == 6)
            failureCallback('This is the error');
          else
            callback(firstArg);
        };
        hooks.setHandleRequest('supportsPromises', handler);
        hooks.setHandleRequest('callbackOptional', handler);
      }))";

  InitializeJSHooks(kRegisterHook);
  SetFunctions(kFunctionsWithPromiseSignatures);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  v8::Local<v8::Object> last_error_parent = v8::Object::New(isolate());
  auto get_last_error_parent = [&last_error_parent]() {
    return last_error_parent;
  };
  SetLastErrorParentCallback(base::BindLambdaForTesting(get_last_error_parent));

  {
    // Calling supportsPromises normally should resolve as expected with no
    // error.
    v8::Local<v8::Function> function = FunctionFromString(
        context, "(function(obj) { return obj.supportsPromises(42); })");
    v8::Local<v8::Value> args[] = {binding_object};

    v8::Local<v8::Value> result = RunFunction(
        function, context, v8::Undefined(isolate()), std::size(args), args);
    v8::Local<v8::Promise> promise;
    ASSERT_TRUE(GetValueAs(result, &promise));
    EXPECT_EQ(v8::Promise::kFulfilled, promise->State());
    EXPECT_EQ(R"(42)", V8ToString(promise->Result(), context));
  }

  {
    // Calling supportsPromises to trigger the failureCallback should result in
    // the promise being rejected.
    v8::Local<v8::Function> function = FunctionFromString(
        context, "(function(obj) { return obj.supportsPromises(6); })");
    v8::Local<v8::Value> args[] = {binding_object};

    v8::Local<v8::Value> result = RunFunction(
        function, context, v8::Undefined(isolate()), std::size(args), args);
    v8::Local<v8::Promise> promise;
    ASSERT_TRUE(GetValueAs(result, &promise));
    EXPECT_EQ(v8::Promise::kRejected, promise->State());
    ASSERT_TRUE(promise->Result()->IsObject());
    EXPECT_EQ(R"("This is the error")",
              GetStringPropertyFromObject(promise->Result().As<v8::Object>(),
                                          context, "message"));
  }

  {
    // Calling supportsPromises with a callback and triggering the
    // failureCallback should call the callback with lastError set.
    const char kFunctionCall[] =
        R"((function(obj, lastErrorParent) {
             return obj.supportsPromises(6, (arg) => {
               this.sentToCallback = arg;
               // LastError is only set for the duration of the callback, so set
               // it to a global we retrieve and can check later.
               this.lastError = lastErrorParent.lastError;
             });
           }))";
    v8::Local<v8::Function> function =
        FunctionFromString(context, kFunctionCall);
    v8::Local<v8::Value> args[] = {binding_object, last_error_parent};

    RunFunction(function, context, v8::Undefined(isolate()), std::size(args),
                args);

    // In the case of errors, callbacks are not passed any arguments.
    EXPECT_TRUE(
        GetPropertyFromObject(context->Global(), context, "sentToCallabck")
            ->IsUndefined());
    v8::Local<v8::Object> last_error;
    ASSERT_TRUE(GetPropertyFromObjectAs(context->Global(), context, "lastError",
                                        &last_error));
    EXPECT_EQ(R"("This is the error")",
              GetStringPropertyFromObject(last_error, context, "message"));
  }

  // Set the context to not support promises for the following test cases.
  context_allows_promises = false;
  {
    // Calling callbackOptional without a callback and triggering the
    // failureCallback in a context that does not support promises should result
    // in a console error about an unchecked last error.
    const char kFunctionCall[] =
        R"((function(obj) {
             return obj.callbackOptional(6);
           }))";
    v8::Local<v8::Function> function =
        FunctionFromString(context, kFunctionCall);
    v8::Local<v8::Value> args[] = {binding_object, last_error_parent};

    RunFunction(function, context, v8::Undefined(isolate()), std::size(args),
                args);
    ASSERT_EQ(1u, console_errors().size());
    EXPECT_THAT(console_errors()[0],
                "Unchecked runtime.lastError: This is the error");
    // Clear the console errors in case any other test case uses them.
    ClearConsoleErrors();
  }
}

// Tests that a JS handle request hook that calls the resolver callback more
// than once will fail gracefully on a release build. Regression test for
// https://crbug.com/1298409.
TEST_F(APIBindingUnittest, TestHandleRequestHookCalledTwiceGracefulRegression) {
  bool context_allows_promises = true;
  SetPromiseAvailabilityFlag(&context_allows_promises);

  // Register a hook for supportsPromises that calls the success callback twice.
  static const char* const kRegisterHook = R"(
      (function(hooks) {
        function handler(firstArg, callback, failureCallback) {
          callback(firstArg);
          // Calling the callback to resolve the request a second time is
          // something our custom hooks shouldn't be doing, but this test
          // intentionally does it to verify behavior if it does happen by
          // accident.
          callback(firstArg);
        };
        hooks.setHandleRequest('supportsPromises', handler);
      }))";

  InitializeJSHooks(kRegisterHook);
  SetFunctions(kFunctionsWithPromiseSignatures);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  v8::Local<v8::Function> function = FunctionFromString(
      context, "(function(obj) { return obj.supportsPromises(42); })");
  v8::Local<v8::Value> args[] = {binding_object};

  // Calling supportsPromises will trigger the HandleRequest hook which attempts
  // to resolve the request twice by calling the success callback twice. This
  // should gracefully fail without a crash and still result in the request
  // resolving as expected.
  v8::Local<v8::Value> result = RunFunction(
      function, context, v8::Undefined(isolate()), std::size(args), args);
  v8::Local<v8::Promise> promise;
  ASSERT_TRUE(GetValueAs(result, &promise));
  EXPECT_EQ(v8::Promise::kFulfilled, promise->State());
  EXPECT_EQ(R"(42)", V8ToString(promise->Result(), context));
}

// Tests that JS custom hooks correctly handle the context being invalidated.
// Regression test for https://crbug.com/944014.
TEST_F(APIBindingUnittest, TestInvalidatingInCustomHook) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  auto context_invalidator =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {
        gin::Arguments arguments(info);
        binding::InvalidateContext(arguments.GetHolderCreationContext());
      };
  v8::Local<v8::Function> v8_context_invalidator =
      v8::Function::New(context, context_invalidator).ToLocalChecked();

  // Register two hooks. Since the context is invalidated in the first, the
  // second should never run.
  const char kRegisterHook[] = R"(
      (function(hooks, contextInvalidator) {
        hooks.setUpdateArgumentsPreValidate('oneString', () => {
          contextInvalidator();
          return ['foo'];
        });
        hooks.setHandleRequest('oneString', () => {
          this.ranHandleHook = true;
        });
      }))";
  InitializeJSHooks(kRegisterHook, v8_context_invalidator);
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  v8::Local<v8::Function> function = FunctionFromString(
      context, "(function(obj) { return obj.oneString('ping'); })");
  v8::Local<v8::Value> args[] = {binding_object};

  RunFunction(function, context, v8::Undefined(isolate()), std::size(args),
              args);

  // The context should be properly invalidated, and the second hook (which
  // sets "ranHandleHook") shouldn't have ran.
  EXPECT_FALSE(binding::IsContextValid(context));
  EXPECT_EQ("undefined", GetStringPropertyFromObject(context->Global(), context,
                                                     "ranHandleHook"));
}

// Tests that native custom hooks can return results synchronously, or throw
// exceptions for bad invocations.
TEST_F(APIBindingUnittest,
       TestReturningResultAndThrowingExceptionFromCustomNativeHook) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  // Register a hook for the test.oneString method.
  auto hooks = std::make_unique<APIBindingHooksTestDelegate>();
  bool did_call = false;
  auto hook = [](bool* did_call, const APISignature* signature,
                 v8::Local<v8::Context> context,
                 v8::LocalVector<v8::Value>* arguments,
                 const APITypeReferenceMap& ref_map) {
    APIBindingHooks::RequestResult result(
        APIBindingHooks::RequestResult::HANDLED);
    if (arguments->size() != 1u) {  // ASSERT* messes with the return type.
      EXPECT_EQ(1u, arguments->size());
      return result;
    }
    v8::Isolate* isolate = context->GetIsolate();
    std::string arg_value = gin::V8ToString(isolate, arguments->at(0));
    if (arg_value == "throw") {
      isolate->ThrowException(v8::Exception::Error(
          gin::StringToV8(isolate, "Custom Hook Error")));
      result.code = APIBindingHooks::RequestResult::THROWN;
      return result;
    }
    result.return_value =
        gin::StringToV8(context->GetIsolate(), arg_value + " pong");
    return result;
  };
  hooks->AddHandler("test.oneString", base::BindRepeating(hook, &did_call));

  SetHooksDelegate(std::move(hooks));
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  {
    // Test an invocation that we expect to throw an exception.
    v8::Local<v8::Function> function =
        FunctionFromString(
            context, "(function(obj) { return obj.oneString('throw'); })");
    v8::Local<v8::Value> args[] = {binding_object};
    RunFunctionAndExpectError(function, context, v8::Undefined(isolate()),
                              std::size(args), args,
                              "Uncaught Error: Custom Hook Error");
  }

  {
    // Test an invocation we expect to succeed.
    v8::Local<v8::Function> function =
        FunctionFromString(context,
                           "(function(obj) { return obj.oneString('ping'); })");
    v8::Local<v8::Value> args[] = {binding_object};
    v8::Local<v8::Value> result =
        RunFunction(function, context, std::size(args), args);
    ASSERT_FALSE(result.IsEmpty());
    std::unique_ptr<base::Value> json_result = V8ToBaseValue(result, context);
    ASSERT_TRUE(json_result);
    EXPECT_EQ("\"ping pong\"", ValueToString(*json_result));
  }
}

// Tests the updateArgumentsPostValidate hook.
TEST_F(APIBindingUnittest, TestUpdateArgumentsPostValidate) {
  // Register a hook for the test.oneString method.
  const char kRegisterHook[] = R"(
      (function(hooks) {
        hooks.setUpdateArgumentsPostValidate('oneString', function() {
          this.requestArguments = Array.from(arguments);
          return ['pong'];
        });
      }))";

  InitializeJSHooks(kRegisterHook);
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // Try calling the method with an invalid signature. Since it's invalid, we
  // should never enter the hook.
  ExpectFailure(binding_object, "obj.oneString(false);",
                api_errors::InvocationError("test.oneString", "string str",
                                            api_errors::NoMatchingSignature()));
  EXPECT_EQ("undefined", GetStringPropertyFromObject(
                             context->Global(), context, "requestArguments"));

  // Call the method with a valid signature. The hook should be entered and
  // manipulate the arguments.
  ExpectPass(binding_object, "obj.oneString('ping');", "['pong']", false);
  EXPECT_EQ("[\"ping\"]", GetStringPropertyFromObject(
                              context->Global(), context, "requestArguments"));

  // Other methods, like stringAndInt(), should behave normally.
  ExpectPass(binding_object, "obj.stringAndInt('foo', 42);",
             "['foo',42]", false);
}

// Tests using setUpdateArgumentsPostValidate to return a list of arguments
// that violates the function schema. Sadly, this should succeed. :(
// See comment in api_binding.cc.
TEST_F(APIBindingUnittest, TestUpdateArgumentsPostValidateViolatingSchema) {
  // Register a hook for the test.oneString method.
  const char kRegisterHook[] = R"(
      (function(hooks) {
        hooks.setUpdateArgumentsPostValidate('oneString', function() {
          return [{}];
        });
      }))";

  InitializeJSHooks(kRegisterHook);
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // Call the method with a valid signature. The hook should be entered and
  // manipulate the arguments.
  ExpectPass(binding_object, "obj.oneString('ping');", "[{}]", false);
}

// Test that user gestures are properly recorded when calling APIs.
TEST_F(APIBindingUnittest, TestUserGestures) {
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  v8::Local<v8::Function> function =
      FunctionFromString(context, "(function(obj) { obj.oneString('foo');})");
  ASSERT_FALSE(function.IsEmpty());

  v8::Local<v8::Value> argv[] = {binding_object};

  RunFunction(function, context, std::size(argv), argv);
  ASSERT_TRUE(last_request());
  EXPECT_FALSE(last_request()->has_user_gesture);
  reset_last_request();

  ScopedTestUserActivation test_user_activation;
  RunFunction(function, context, std::size(argv), argv);
  ASSERT_TRUE(last_request());
  EXPECT_TRUE(last_request()->has_user_gesture);

  reset_last_request();
}

TEST_F(APIBindingUnittest, FilteredEvents) {
  const char kEvents[] =
      "[{"
      "  'name': 'unfilteredOne',"
      "  'parameters': []"
      "}, {"
      "  'name': 'unfilteredTwo',"
      "  'filters': [],"
      "  'parameters': []"
      "}, {"
      "  'name': 'unfilteredThree',"
      "  'options': {'supportsFilters': false},"
      "  'parameters': []"
      "}, {"
      "  'name': 'filteredOne',"
      "  'options': {'supportsFilters': true},"
      "  'parameters': []"
      "}, {"
      "  'name': 'filteredTwo',"
      "  'filters': ["
      "    {'name': 'url', 'type': 'array', 'items': {'type': 'any'}}"
      "  ],"
      "  'parameters': []"
      "}]";
  SetEvents(kEvents);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  const char kAddFilteredListener[] =
      "(function(evt) {\n"
      "  evt.addListener(function() {},\n"
      "                  {url: [{pathContains: 'simple2.html'}]});\n"
      "})";
  v8::Local<v8::Function> function =
      FunctionFromString(context, kAddFilteredListener);
  ASSERT_FALSE(function.IsEmpty());

  auto check_supports_filters = [context, binding_object, function](
                                    std::string_view name,
                                    bool expect_supports) {
    SCOPED_TRACE(name);
    v8::Local<v8::Value> event =
        GetPropertyFromObject(binding_object, context, name);
    v8::Local<v8::Value> args[] = {event};
    if (expect_supports) {
      RunFunction(function, context, context->Global(), std::size(args), args);
    } else {
      RunFunctionAndExpectError(
          function, context, context->Global(), std::size(args), args,
          "Uncaught TypeError: This event does not support filters");
    }
  };

  check_supports_filters("unfilteredOne", false);
  check_supports_filters("unfilteredTwo", false);
  check_supports_filters("unfilteredThree", false);
  check_supports_filters("filteredOne", true);
  check_supports_filters("filteredTwo", true);
}

TEST_F(APIBindingUnittest, HooksTemplateInitializer) {
  SetFunctions(kFunctions);

  // Register a hook for the test.oneString method.
  auto hooks = std::make_unique<APIBindingHooksTestDelegate>();
  auto hook = [](v8::Isolate* isolate,
                 v8::Local<v8::ObjectTemplate> object_template,
                 const APITypeReferenceMap& type_refs) {
    object_template->Set(gin::StringToSymbol(isolate, "hookedProperty"),
                         gin::ConvertToV8(isolate, 42));
  };
  hooks->SetTemplateInitializer(base::BindRepeating(hook));
  SetHooksDelegate(std::move(hooks));

  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // The extra property should be present on the binding object.
  EXPECT_EQ("42", GetStringPropertyFromObject(binding_object, context,
                                              "hookedProperty"));
  // Sanity check: other values should still be there.
  EXPECT_EQ("function",
            GetStringPropertyFromObject(binding_object, context, "oneString"));
}

TEST_F(APIBindingUnittest, HooksInstanceInitializer) {
  SetFunctions(kFunctions);
  static constexpr char kHookedProperty[] = "hookedProperty";

  // Register a hook for the test.oneString method.
  auto hooks = std::make_unique<APIBindingHooksTestDelegate>();
  int count = 0;
  auto hook = [](int* count, v8::Local<v8::Context> context,
                 v8::Local<v8::Object> object) {
    v8::Isolate* isolate = context->GetIsolate();
    // Add a new property only for the first instance.
    if ((*count)++ == 0) {
      object
          ->Set(context, gin::StringToSymbol(isolate, kHookedProperty),
                gin::ConvertToV8(isolate, 42))
          .ToChecked();
    }
  };

  hooks->SetInstanceInitializer(base::BindRepeating(hook, &count));
  SetHooksDelegate(std::move(hooks));

  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  // Create two instances.
  v8::Local<v8::Context> context1 = MainContext();
  v8::Local<v8::Object> binding_object1 = binding()->CreateInstance(context1);

  v8::Local<v8::Context> context2 = AddContext();
  v8::Local<v8::Object> binding_object2 = binding()->CreateInstance(context2);

  // We should have run the hooks twice (once per instance).
  EXPECT_EQ(2, count);

  // The extra property should be present on the first binding object, but not
  // the second.
  EXPECT_EQ("42", GetStringPropertyFromObject(binding_object1, context1,
                                              kHookedProperty));
  EXPECT_EQ("undefined", GetStringPropertyFromObject(binding_object2, context2,
                                                     kHookedProperty));

  // Sanity check: other values should still be there.
  EXPECT_EQ("function", GetStringPropertyFromObject(binding_object1, context1,
                                                    "oneString"));
  EXPECT_EQ("function", GetStringPropertyFromObject(binding_object2, context1,
                                                    "oneString"));
}

// Test that running hooks returning different results correctly sends requests
// or notifies of silent requests.
TEST_F(APIBindingUnittest, TestSendingRequestsAndSilentRequestsWithHooks) {
  SetFunctions(
      "[{"
      "  'name': 'modifyArgs',"
      "  'parameters': []"
      "}, {"
      "  'name': 'invalidInvocation',"
      "  'parameters': []"
      "}, {"
      "  'name': 'throwException',"
      "  'parameters': []"
      "}, {"
      "  'name': 'dontHandle',"
      "  'parameters': []"
      "}, {"
      "  'name': 'handle',"
      "  'parameters': []"
      "}, {"
      "  'name': 'handleAndSendRequest',"
      "  'parameters': []"
      "}, {"
      "  'name': 'handleWithArgs',"
      "  'parameters': [{"
      "    'name': 'first',"
      "    'type': 'string'"
      "  }, {"
      "    'name': 'second',"
      "    'type': 'integer'"
      "  }]"
      "}]");

  using RequestResult = APIBindingHooks::RequestResult;

  auto basic_handler =
      [](RequestResult::ResultCode code, const APISignature*,
         v8::Local<v8::Context> context, v8::LocalVector<v8::Value>* arguments,
         const APITypeReferenceMap& map) { return RequestResult(code); };

  auto hooks = std::make_unique<APIBindingHooksTestDelegate>();
  hooks->AddHandler(
      "test.modifyArgs",
      base::BindRepeating(basic_handler, RequestResult::ARGUMENTS_UPDATED));
  hooks->AddHandler(
      "test.invalidInvocation",
      base::BindRepeating(basic_handler, RequestResult::INVALID_INVOCATION));
  hooks->AddHandler(
      "test.dontHandle",
      base::BindRepeating(basic_handler, RequestResult::NOT_HANDLED));
  hooks->AddHandler("test.handle",
                    base::BindRepeating(basic_handler, RequestResult::HANDLED));
  hooks->AddHandler(
      "test.throwException",
      base::BindRepeating([](const APISignature*,
                             v8::Local<v8::Context> context,
                             v8::LocalVector<v8::Value>* arguments,
                             const APITypeReferenceMap& map) {
        context->GetIsolate()->ThrowException(
            gin::StringToV8(context->GetIsolate(), "some error"));
        return RequestResult(RequestResult::THROWN);
      }));
  hooks->AddHandler(
      "test.handleWithArgs",
      base::BindRepeating([](const APISignature*,
                             v8::Local<v8::Context> context,
                             v8::LocalVector<v8::Value>* arguments,
                             const APITypeReferenceMap& map) {
        arguments->push_back(v8::Integer::New(context->GetIsolate(), 42));
        return RequestResult(RequestResult::HANDLED);
      }));

  auto handle_and_send_request =
      [](APIRequestHandler* handler, const APISignature*,
         v8::Local<v8::Context> context, v8::LocalVector<v8::Value>* arguments,
         const APITypeReferenceMap& map) {
        handler->StartRequest(
            context, "test.handleAndSendRequest", base::Value::List(),
            binding::AsyncResponseType::kNone, v8::Local<v8::Function>(),
            v8::Local<v8::Function>(), binding::ResultModifierFunction());
        return RequestResult(RequestResult::HANDLED);
      };
  hooks->AddHandler(
      "test.handleAndSendRequest",
      base::BindRepeating(handle_and_send_request, request_handler()));

  SetHooksDelegate(std::move(hooks));

  auto on_silent_request = [](std::optional<std::string>* name_out,
                              std::optional<std::vector<std::string>>* args_out,
                              v8::Local<v8::Context> context,
                              const std::string& call_name,
                              const v8::LocalVector<v8::Value>& arguments) {
    *name_out = call_name;
    *args_out = std::vector<std::string>();
    (*args_out)->reserve(arguments.size());
    for (const auto& arg : arguments) {
      (*args_out)->push_back(V8ToString(arg, context));
    }
  };
  std::optional<std::string> silent_request;
  std::optional<std::vector<std::string>> request_arguments;
  SetOnSilentRequest(base::BindRepeating(on_silent_request, &silent_request,
                                         &request_arguments));

  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  auto call_api_method = [binding_object, context](
                             std::string_view name,
                             std::string_view string_args) {
    v8::Local<v8::Function> call = FunctionFromString(
        context, base::StringPrintf("(function(binding) { binding.%s(%s); })",
                                    name.data(), string_args.data()));
    v8::Local<v8::Value> args[] = {binding_object};
    v8::TryCatch try_catch(context->GetIsolate());
    // The throwException call will throw an exception; ignore it.
    std::ignore = call->Call(context, v8::Undefined(context->GetIsolate()),
                             std::size(args), args);
  };

  call_api_method("modifyArgs", "");
  ASSERT_TRUE(last_request());
  EXPECT_EQ("test.modifyArgs", last_request()->method_name);
  EXPECT_FALSE(silent_request);
  reset_last_request();
  silent_request.reset();
  request_arguments.reset();

  call_api_method("invalidInvocation", "");
  EXPECT_FALSE(last_request());
  EXPECT_FALSE(silent_request);
  reset_last_request();
  silent_request.reset();
  request_arguments.reset();

  call_api_method("throwException", "");
  EXPECT_FALSE(last_request());
  EXPECT_FALSE(silent_request);
  reset_last_request();
  silent_request.reset();
  request_arguments.reset();

  call_api_method("dontHandle", "");
  ASSERT_TRUE(last_request());
  EXPECT_EQ("test.dontHandle", last_request()->method_name);
  EXPECT_FALSE(silent_request);
  reset_last_request();
  silent_request.reset();
  request_arguments.reset();

  call_api_method("handle", "");
  EXPECT_FALSE(last_request());
  ASSERT_TRUE(silent_request);
  EXPECT_EQ("test.handle", *silent_request);
  ASSERT_TRUE(request_arguments);
  EXPECT_TRUE(request_arguments->empty());
  reset_last_request();
  silent_request.reset();
  request_arguments.reset();

  call_api_method("handleAndSendRequest", "");
  ASSERT_TRUE(last_request());
  EXPECT_EQ("test.handleAndSendRequest", last_request()->method_name);
  EXPECT_FALSE(silent_request);
  reset_last_request();
  silent_request.reset();
  request_arguments.reset();

  call_api_method("handleWithArgs", "'str'");
  EXPECT_FALSE(last_request());
  ASSERT_TRUE(silent_request);
  ASSERT_EQ("test.handleWithArgs", *silent_request);
  ASSERT_TRUE(request_arguments);
  EXPECT_THAT(
      *request_arguments,
      testing::ElementsAre("\"str\"", "42"));  // 42 was added by the handler.
  reset_last_request();
  silent_request.reset();
  request_arguments.reset();
}

// Test native hooks that don't handle the result, but set a custom callback
// instead.
TEST_F(APIBindingUnittest, TestHooksWithCustomCallback) {
  SetFunctions(kFunctions);

  // Register a hook for the test.oneString method.
  auto hooks = std::make_unique<APIBindingHooksTestDelegate>();
  auto hook_with_custom_callback =
      [](const APISignature* signature, v8::Local<v8::Context> context,
         v8::LocalVector<v8::Value>* arguments,
         const APITypeReferenceMap& ref_map) {
        constexpr char kCustomCallback[] =
            "(function() { this.calledCustomCallback = true; })";
        v8::Local<v8::Function> custom_callback =
            FunctionFromString(context, kCustomCallback);
        APIBindingHooks::RequestResult result(
            APIBindingHooks::RequestResult::NOT_HANDLED, custom_callback);
        return result;
      };
  hooks->AddHandler("test.oneString",
                    base::BindRepeating(hook_with_custom_callback));
  SetHooksDelegate(std::move(hooks));

  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // First try calling the oneString() method, which has a custom hook
  // installed.
  v8::Local<v8::Function> func =
      FunctionFromString(context, "(function(obj) { obj.oneString('foo'); })");
  v8::Local<v8::Value> args[] = {binding_object};
  RunFunction(func, context, 1, args);

  ASSERT_TRUE(last_request());
  EXPECT_TRUE(last_request()->has_async_response_handler);
  request_handler()->CompleteRequest(last_request()->request_id,
                                     base::Value::List(), std::string());

  EXPECT_EQ("true", GetStringPropertyFromObject(context->Global(), context,
                                                "calledCustomCallback"));
}

// Test native hooks that don't handle the result, but add a result modifier.
TEST_F(APIBindingUnittest, TestHooksWithResultModifier) {
  SetFunctions(kFunctionsWithPromiseSignatures);

  bool context_allows_promises = true;
  SetPromiseAvailabilityFlag(&context_allows_promises);

  // Register a hook for the test.supportsPromises method with a result modifier
  // that changes the result when the async response type is callback based.
  auto hooks = std::make_unique<APIBindingHooksTestDelegate>();
  int total_modifier_call_count = 0;
  auto result_modifier = [&total_modifier_call_count](
                             const v8::LocalVector<v8::Value>& result_args,
                             v8::Local<v8::Context> context,
                             binding::AsyncResponseType async_type) {
    total_modifier_call_count++;
    if (async_type == binding::AsyncResponseType::kCallback) {
      // For callback based calls change the result to a vector with
      // multiple arguments by appending "bar" to the end.
      v8::LocalVector<v8::Value> new_args(
          context->GetIsolate(),
          {result_args[0], gin::StringToV8(context->GetIsolate(), "bar")});
      return new_args;
    }
    return result_args;
  };

  auto hook_with_result_modifier =
      [&result_modifier](const APISignature* signature,
                         v8::Local<v8::Context> context,
                         v8::LocalVector<v8::Value>* arguments,
                         const APITypeReferenceMap& ref_map) {
        APIBindingHooks::RequestResult result(
            APIBindingHooks::RequestResult::NOT_HANDLED,
            v8::Local<v8::Function>(),
            base::BindLambdaForTesting(result_modifier));
        return result;
      };
  hooks->AddHandler("test.supportsPromises",
                    base::BindLambdaForTesting(hook_with_result_modifier));
  SetHooksDelegate(std::move(hooks));

  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // A promise-based call should remain unmodified and return as normal.
  {
    v8::Local<v8::Function> promise_api_call = FunctionFromString(
        context, "(function(api) { return api.supportsPromises(1); });");
    v8::Local<v8::Value> args[] = {binding_object};
    v8::Local<v8::Value> api_result =
        RunFunctionOnGlobal(promise_api_call, context, std::size(args), args);

    v8::Local<v8::Promise> promise;
    ASSERT_TRUE(GetValueAs(api_result, &promise));
    EXPECT_EQ(v8::Promise::kPending, promise->State());

    ASSERT_TRUE(last_request());
    request_handler()->CompleteRequest(last_request()->request_id,
                                       ListValueFromString(R"(["foo"])"),
                                       std::string());

    EXPECT_EQ(v8::Promise::kFulfilled, promise->State());
    EXPECT_EQ(R"("foo")", V8ToString(promise->Result(), context));
    EXPECT_EQ(1, total_modifier_call_count);
  }

  // A callback-based call will be modified by the hook and return with multiple
  // parameters.
  {
    constexpr char kFunctionCall[] =
        R"((function(api) {
             api.supportsPromises(2, (normalResult, addedResult) => {
               this.argument1 = normalResult;
               this.argument2 = addedResult;
             });
           }))";
    v8::Local<v8::Function> callback_api_call =
        FunctionFromString(context, kFunctionCall);
    v8::Local<v8::Value> args[] = {binding_object};
    RunFunctionOnGlobal(callback_api_call, context, std::size(args), args);

    ASSERT_TRUE(last_request());
    request_handler()->CompleteRequest(last_request()->request_id,
                                       ListValueFromString(R"(["foo"])"),
                                       std::string());

    EXPECT_EQ(R"("foo")", GetStringPropertyFromObject(context->Global(),
                                                      context, "argument1"));
    EXPECT_EQ(R"("bar")", GetStringPropertyFromObject(context->Global(),
                                                      context, "argument2"));
    EXPECT_EQ(2, total_modifier_call_count);
  }

  // A call which results in an error should reject as expected and the result
  // modifier should never be called.
  {
    v8::Local<v8::Function> promise_api_call = FunctionFromString(
        context, "(function(api) { return api.supportsPromises(3) });");
    v8::Local<v8::Value> args[] = {binding_object};
    v8::Local<v8::Value> api_result =
        RunFunctionOnGlobal(promise_api_call, context, std::size(args), args);

    v8::Local<v8::Promise> promise = api_result.As<v8::Promise>();
    ASSERT_FALSE(api_result.IsEmpty());
    EXPECT_EQ(v8::Promise::kPending, promise->State());

    ASSERT_TRUE(last_request());
    request_handler()->CompleteRequest(last_request()->request_id,
                                       base::Value::List(), "Error message");
    EXPECT_EQ(v8::Promise::kRejected, promise->State());
    ASSERT_TRUE(promise->Result()->IsObject());
    EXPECT_EQ(R"("Error message")",
              GetStringPropertyFromObject(promise->Result().As<v8::Object>(),
                                          context, "message"));
    // Since the result modifier should have never been called, the total call
    // count should still be the same as in the previous test case.
    EXPECT_EQ(2, total_modifier_call_count);
  }
}

// Test native hooks that add a result modifier are compatible with JS hooks
// which handle the request.
TEST_F(APIBindingUnittest, TestHooksWithResultModifierAndJSHook) {
  bool context_allows_promises = true;
  SetPromiseAvailabilityFlag(&context_allows_promises);

  // Register a JS hook for supportsPromises.
  const char kRegisterHook[] = R"(
      (function(hooks) {
        hooks.setHandleRequest('supportsPromises', (firstArg, callback) => {
          // Call the callback, appending "-foo" to the argument passed in.
          callback(firstArg + '-foo');
        });
      }))";

  InitializeJSHooks(kRegisterHook);
  SetFunctions(kFunctionsWithPromiseSignatures);

  // Register a native hook for test.supportsPromises with a result modifier
  // that changes the result when the async response type is callback based.
  auto hooks = std::make_unique<APIBindingHooksTestDelegate>();
  auto result_modifier = [](const v8::LocalVector<v8::Value>& result_args,
                            v8::Local<v8::Context> context,
                            binding::AsyncResponseType async_type) {
    if (async_type == binding::AsyncResponseType::kCallback) {
      // For callback based calls change the result to a vector with
      // multiple arguments by appending "bar" to the end.
      v8::LocalVector<v8::Value> new_args(
          context->GetIsolate(),
          {result_args[0], gin::StringToV8(context->GetIsolate(), "bar")});
      return new_args;
    }
    return result_args;
  };

  auto hook_with_result_modifier =
      [&result_modifier](const APISignature* signature,
                         v8::Local<v8::Context> context,
                         v8::LocalVector<v8::Value>* arguments,
                         const APITypeReferenceMap& ref_map) {
        APIBindingHooks::RequestResult result(
            APIBindingHooks::RequestResult::NOT_HANDLED,
            v8::Local<v8::Function>(), base::BindOnce(result_modifier));
        return result;
      };
  // Normally handlers are bound using base::BindRepeating, but to bind a lambda
  // with a capture we have to use BindLambdaForTesting.
  hooks->AddHandler("test.supportsPromises",
                    base::BindLambdaForTesting(hook_with_result_modifier));
  SetHooksDelegate(std::move(hooks));

  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // A promise-based call should just be modified by the JS hook..
  {
    v8::Local<v8::Function> promise_api_call = FunctionFromString(
        context, "(function(api) { return api.supportsPromises(1); });");
    v8::Local<v8::Value> args[] = {binding_object};
    v8::Local<v8::Value> api_result =
        RunFunctionOnGlobal(promise_api_call, context, std::size(args), args);

    // Since the JS callback completes the request right away, the promise
    // should already be fulfilled without us needing to manually complete the
    // request.
    v8::Local<v8::Promise> promise;
    ASSERT_TRUE(GetValueAs(api_result, &promise));
    EXPECT_EQ(v8::Promise::kFulfilled, promise->State());
    EXPECT_EQ(R"("1-foo")", V8ToString(promise->Result(), context));
  }

  // A callback-based call will be modified by the native hook to return with
  // multiple parameters, as well as having the first parameter modified by the
  // JS hook.
  {
    constexpr char kFunctionCall[] =
        R"((function(api) {
             api.supportsPromises(2, (normalResult, addedResult) => {
               this.argument1 = normalResult;
               this.argument2 = addedResult;
             });
           }))";
    v8::Local<v8::Function> promise_api_call =
        FunctionFromString(context, kFunctionCall);
    v8::Local<v8::Value> args[] = {binding_object};
    RunFunctionOnGlobal(promise_api_call, context, std::size(args), args);

    EXPECT_EQ(R"("2-foo")", GetStringPropertyFromObject(context->Global(),
                                                        context, "argument1"));
    EXPECT_EQ(R"("bar")", GetStringPropertyFromObject(context->Global(),
                                                      context, "argument2"));
  }
}

TEST_F(APIBindingUnittest, AccessAPIMethodsAndEventsAfterInvalidation) {
  SetEvents(R"([{"name": "onFoo"}])");
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  v8::Local<v8::Function> function = FunctionFromString(
      context, "(function(obj) { obj.onFoo.addListener(function() {}); })");
  binding::InvalidateContext(context);

  v8::Local<v8::Value> argv[] = {binding_object};
  RunFunctionAndExpectError(function, context, std::size(argv), argv,
                            "Uncaught Error: Extension context invalidated.");
}

TEST_F(APIBindingUnittest, CallbackSignaturesAreAdded) {
  std::unique_ptr<base::AutoReset<bool>> response_validation_override =
      binding::SetResponseValidationEnabledForTesting(true);

  SetFunctions(kFunctionsWithCallbackSignatures);
  InitializeBinding();

  {
    const APISignature* signature =
        type_refs().GetAPIMethodSignature("test.noCallback");
    ASSERT_TRUE(signature);
    EXPECT_FALSE(signature->has_async_return());
    EXPECT_FALSE(signature->has_async_return_signature());
  }

  {
    const APISignature* signature =
        type_refs().GetAPIMethodSignature("test.intCallback");
    ASSERT_TRUE(signature);
    EXPECT_TRUE(signature->has_async_return());
    EXPECT_TRUE(signature->has_async_return_signature());
  }

  {
    const APISignature* signature =
        type_refs().GetAPIMethodSignature("test.noParamCallback");
    ASSERT_TRUE(signature);
    EXPECT_TRUE(signature->has_async_return());
    EXPECT_TRUE(signature->has_async_return_signature());
  }
}

TEST_F(APIBindingUnittest,
       CallbackSignaturesAreNotAddedWhenValidationDisabled) {
  std::unique_ptr<base::AutoReset<bool>> response_validation_override =
      binding::SetResponseValidationEnabledForTesting(false);

  SetFunctions(kFunctionsWithCallbackSignatures);
  InitializeBinding();

  EXPECT_FALSE(
      type_refs().GetAPIMethodSignature("test.noCallback")->has_async_return());
  EXPECT_TRUE(type_refs()
                  .GetAPIMethodSignature("test.intCallback")
                  ->has_async_return());
  EXPECT_FALSE(type_refs()
                   .GetAPIMethodSignature("test.intCallback")
                   ->has_async_return_signature());
  EXPECT_TRUE(type_refs()
                  .GetAPIMethodSignature("test.noParamCallback")
                  ->has_async_return());
  EXPECT_FALSE(type_refs()
                   .GetAPIMethodSignature("test.noParamCallback")
                   ->has_async_return_signature());
}

// Tests promise-based APIs exposed on bindings.
TEST_F(APIBindingUnittest, PromiseBasedAPIs) {
  SetFunctions(kFunctionsWithPromiseSignatures);

  // Set a local boolean we can change to simulate if the context supports
  // promises or not.
  bool context_allows_promises = true;
  SetPromiseAvailabilityFlag(&context_allows_promises);

  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // A normal call into the promised based API should return a promise. When the
  // request is completed with a value, the promise will be resolved with that
  // value.
  {
    v8::Local<v8::Function> promise_api_call = FunctionFromString(
        context, "(function(api) { return api.supportsPromises(3); })");
    v8::Local<v8::Value> args[] = {binding_object};
    v8::Local<v8::Value> result =
        RunFunctionOnGlobal(promise_api_call, context, std::size(args), args);

    v8::Local<v8::Promise> promise;
    ASSERT_TRUE(GetValueAs(result, &promise));
    EXPECT_EQ(v8::Promise::kPending, promise->State());

    ASSERT_TRUE(last_request());
    request_handler()->CompleteRequest(last_request()->request_id,
                                       ListValueFromString(R"(["foo"])"),
                                       std::string());

    EXPECT_EQ(v8::Promise::kFulfilled, promise->State());
    EXPECT_EQ(R"("foo")", V8ToString(promise->Result(), context));
  }
  // Also test that promise-based APIs still support passing a callback.
  {
    constexpr char kFunctionCall[] =
        R"((function(api) {
             api.supportsPromises(3, (strResult) => {
               this.callbackResult = strResult
             });
           }))";
    v8::Local<v8::Function> promise_api_call =
        FunctionFromString(context, kFunctionCall);
    v8::Local<v8::Value> args[] = {binding_object};
    RunFunctionOnGlobal(promise_api_call, context, std::size(args), args);

    ASSERT_TRUE(last_request());
    request_handler()->CompleteRequest(last_request()->request_id,
                                       ListValueFromString(R"(["bar"])"),
                                       std::string());

    EXPECT_EQ(R"("bar")", GetStringPropertyFromObject(
                              context->Global(), context, "callbackResult"));
  }
  // If a request is completed with an error, the promise should be rejected.
  {
    v8::Local<v8::Function> promise_api_call = FunctionFromString(
        context, "(function(api) { return api.supportsPromises(3) });");
    v8::Local<v8::Value> args[] = {binding_object};
    v8::Local<v8::Value> api_result =
        RunFunctionOnGlobal(promise_api_call, context, std::size(args), args);

    v8::Local<v8::Promise> promise = api_result.As<v8::Promise>();
    ASSERT_FALSE(api_result.IsEmpty());
    EXPECT_EQ(v8::Promise::kPending, promise->State());

    ASSERT_TRUE(last_request());
    request_handler()->CompleteRequest(last_request()->request_id,
                                       base::Value::List(), "Error message");

    EXPECT_EQ(v8::Promise::kRejected, promise->State());
    ASSERT_TRUE(promise->Result()->IsObject());
    EXPECT_EQ(R"("Error message")",
              GetStringPropertyFromObject(promise->Result().As<v8::Object>(),
                                          context, "message"));
  }
  // If a request is completed with a result and an error, the promise should be
  // rejected and the result will not be returned. Note: ideally no APIs would
  // do this but some legacy APIs do it through returning ErrorWithArguments as
  // their ResponseValue. This testcase documents how this behaves with
  // promises.
  {
    v8::Local<v8::Function> promise_api_call = FunctionFromString(
        context, "(function(api) { return api.supportsPromises(3) });");
    v8::Local<v8::Value> args[] = {binding_object};
    v8::Local<v8::Value> api_result =
        RunFunctionOnGlobal(promise_api_call, context, std::size(args), args);

    v8::Local<v8::Promise> promise = api_result.As<v8::Promise>();
    ASSERT_FALSE(api_result.IsEmpty());
    EXPECT_EQ(v8::Promise::kPending, promise->State());

    ASSERT_TRUE(last_request());
    request_handler()->CompleteRequest(last_request()->request_id,
                                       ListValueFromString(R"(["bar"])"),
                                       "Error message");

    EXPECT_EQ(v8::Promise::kRejected, promise->State());
    ASSERT_TRUE(promise->Result()->IsObject());
    EXPECT_EQ(R"("Error message")",
              GetStringPropertyFromObject(promise->Result().As<v8::Object>(),
                                          context, "message"));
  }
  // If the context doesn't support promises, there should be an error if a
  // required callback isn't supplied.
  context_allows_promises = false;
  {
    v8::Local<v8::Function> promise_api_call = FunctionFromString(
        context, "(function(api) { return api.supportsPromises(3) });");
    v8::Local<v8::Value> args[] = {binding_object};
    auto expected_error =
        "Uncaught TypeError: " +
        api_errors::InvocationError("test.supportsPromises",
                                    "integer int, function callback",
                                    api_errors::NoMatchingSignature());
    RunFunctionAndExpectError(promise_api_call, context, std::size(args), args,
                              expected_error);
  }
  // Test that required callbacks still work when the context doesn't support
  // promises.
  {
    constexpr char kFunctionCall[] =
        R"((function(api) {
             api.supportsPromises(3, (strResult) => {
               this.callbackResult = strResult
             });
           }))";
    v8::Local<v8::Function> promise_api_call =
        FunctionFromString(context, kFunctionCall);
    v8::Local<v8::Value> args[] = {binding_object};
    RunFunctionOnGlobal(promise_api_call, context, std::size(args), args);

    ASSERT_TRUE(last_request());
    request_handler()->CompleteRequest(last_request()->request_id,
                                       ListValueFromString(R"(["foo"])"),
                                       std::string());

    EXPECT_EQ(R"("foo")", GetStringPropertyFromObject(
                              context->Global(), context, "callbackResult"));
  }
  // If a returns_async field is marked as optional, then a context which
  // doesn't support promises should be able to leave it off of the call.
  {
    v8::Local<v8::Function> promise_api_call = FunctionFromString(
        context, "(function(api) { return api.callbackOptional(3) });");
    v8::Local<v8::Value> args[] = {binding_object};
    v8::Local<v8::Value> api_result =
        RunFunctionOnGlobal(promise_api_call, context, std::size(args), args);

    ASSERT_TRUE(last_request());
    ASSERT_TRUE(api_result->IsNullOrUndefined());
  }
}

TEST_F(APIBindingUnittest, TestPromisesWithJSCustomCallback) {
  // Set a local boolean we can change to simulate if the context supports
  // promises or not.
  bool context_allows_promises = true;
  SetPromiseAvailabilityFlag(&context_allows_promises);

  // Register a custom callback hook for the supportsPromises method.
  const char kRegisterHook[] = R"(
      (function(hooks) {
        hooks.setCustomCallback('supportsPromises',
                                (callback, response) => {
          this.response = response;
          this.resolveCallback = callback;
          if (response == 'resolveNow')
            callback('bar');
        });
      }))";

  InitializeJSHooks(kRegisterHook);
  SetFunctions(kFunctionsWithPromiseSignatures);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // A normal call into the promise-based API should return a promise.
  {
    v8::Local<v8::Function> promise_api_call = FunctionFromString(
        context, "(function(api) { return api.supportsPromises(1); });");
    v8::Local<v8::Value> args[] = {binding_object};
    v8::Local<v8::Value> api_result =
        RunFunctionOnGlobal(promise_api_call, context, std::size(args), args);

    v8::Local<v8::Promise> promise;
    ASSERT_TRUE(GetValueAs(api_result, &promise));
    EXPECT_EQ(v8::Promise::kPending, promise->State());

    ASSERT_TRUE(last_request());
    request_handler()->CompleteRequest(last_request()->request_id,
                                       ListValueFromString(R"(["foo"])"),
                                       std::string());
    // The promise should still be unfulfilled until the callback is invoked.
    EXPECT_EQ(v8::Promise::kPending, promise->State());
    v8::Local<v8::Function> resolve_callback;
    ASSERT_TRUE(GetPropertyFromObjectAs(context->Global(), context,
                                        "resolveCallback", &resolve_callback));
    v8::Local<v8::Value> callback_arguments[] = {
        GetPropertyFromObject(context->Global(), context, "response")};
    EXPECT_EQ(R"("foo")", V8ToString(callback_arguments[0], context));

    RunFunctionOnGlobal(resolve_callback, context,
                        std::size(callback_arguments), callback_arguments);

    EXPECT_EQ(v8::Promise::kFulfilled, promise->State());
    EXPECT_EQ(R"("foo")", V8ToString(promise->Result(), context));
  }

  // Sending a response to the hook to make it resolve immediately should result
  // in the promise being resolved right after CompleteRequest is called.
  {
    v8::Local<v8::Function> promise_api_call = FunctionFromString(
        context, "(function(api) { return api.supportsPromises(2); });");
    v8::Local<v8::Value> args[] = {binding_object};
    v8::Local<v8::Value> api_result =
        RunFunctionOnGlobal(promise_api_call, context, std::size(args), args);

    v8::Local<v8::Promise> promise;
    ASSERT_TRUE(GetValueAs(api_result, &promise));
    EXPECT_EQ(v8::Promise::kPending, promise->State());

    ASSERT_TRUE(last_request());
    request_handler()->CompleteRequest(last_request()->request_id,
                                       ListValueFromString(R"(["resolveNow"])"),
                                       std::string());
    EXPECT_EQ(v8::Promise::kFulfilled, promise->State());
    EXPECT_EQ(R"("bar")", V8ToString(promise->Result(), context));
  }

  // Completing the request with an error should still call into the custom
  // callback, which will reject the promise with the error when the callback
  // passed to it is called.
  {
    v8::Local<v8::Function> promise_api_call = FunctionFromString(
        context, "(function(api) { return api.supportsPromises(3) });");
    v8::Local<v8::Value> args[] = {binding_object};
    v8::Local<v8::Value> api_result =
        RunFunctionOnGlobal(promise_api_call, context, std::size(args), args);

    v8::Local<v8::Promise> promise = api_result.As<v8::Promise>();
    ASSERT_FALSE(api_result.IsEmpty());
    EXPECT_EQ(v8::Promise::kPending, promise->State());

    ASSERT_TRUE(last_request());
    request_handler()->CompleteRequest(last_request()->request_id,
                                       ListValueFromString(R"(["baz"])"),
                                       "Error message");
    EXPECT_EQ(v8::Promise::kPending, promise->State());
    v8::Local<v8::Value> resolve_callback =
        GetPropertyFromObject(context->Global(), context, "resolveCallback");
    ASSERT_TRUE(resolve_callback->IsFunction());

    RunFunctionOnGlobal(resolve_callback.As<v8::Function>(), context, 0,
                        nullptr);

    EXPECT_EQ(v8::Promise::kRejected, promise->State());
    ASSERT_TRUE(promise->Result()->IsObject());
    EXPECT_EQ(R"("Error message")",
              GetStringPropertyFromObject(promise->Result().As<v8::Object>(),
                                          context, "message"));
  }
}

TEST_F(APIBindingUnittest, TestPromiseWithJSUpdateArgumentsPreValidate) {
  bool context_allows_promises = true;
  SetPromiseAvailabilityFlag(&context_allows_promises);

  // Register an update arguments pre validate hook for supportsPromises.
  const char kRegisterHook[] = R"(
      (function(hooks) {
        hooks.setUpdateArgumentsPreValidate('supportsPromises',
                                            (...arguments) => {
          this.firstArgument = arguments[0];
          this.secondArgument = arguments[1];
          if (arguments[0] == 'hooked')
            arguments[0] = 42;
          return arguments;
        });
      }))";
  InitializeJSHooks(kRegisterHook);
  SetFunctions(kFunctionsWithPromiseSignatures);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  {
    // Calling supportsPromises normally with a callback should work fine.
    auto result =
        ExpectPass(binding_object, "return obj.supportsPromises(5, () => {});",
                   "[5]", true);
    ASSERT_FALSE(result.IsEmpty());
    EXPECT_TRUE(result->IsUndefined());
    EXPECT_TRUE(
        GetPropertyFromObject(context->Global(), context, "secondArgument")
            ->IsFunction());
  }

  {
    // Calling supportsPromises normally while omitting the callback should work
    // fine.
    auto result = ExpectPass(binding_object, "return obj.supportsPromises(5);",
                             "[5]", true);
    EXPECT_TRUE(V8ValueIs<v8::Promise>(result));
  }

  {
    // Calling supportsPromises with a string which we have not set up the
    // custom hook for should cause an error.
    ExpectFailure(binding_object, "obj.supportsPromises('foo');",
                  api_errors::InvocationError(
                      "test.supportsPromises", "integer int, function callback",
                      api_errors::NoMatchingSignature()));
    EXPECT_EQ(R"("foo")", GetStringPropertyFromObject(
                              context->Global(), context, "firstArgument"));
  }

  {
    // supportsPromises expects an int, but our custom hook should allow the
    // string 'hooked' to work as well.
    auto result = ExpectPass(
        binding_object, "return obj.supportsPromises('hooked');", "[42]", true);
    EXPECT_TRUE(V8ValueIs<v8::Promise>(result));
    EXPECT_EQ(R"("hooked")", GetStringPropertyFromObject(
                                 context->Global(), context, "firstArgument"));
  }

  {
    // We should also be able to hit the custom hook with a callback still.
    auto result = ExpectPass(binding_object,
                             "return obj.supportsPromises('hooked', () => {});",
                             "[42]", true);
    ASSERT_FALSE(result.IsEmpty());
    EXPECT_TRUE(result->IsUndefined());
    EXPECT_EQ(R"("hooked")", GetStringPropertyFromObject(
                                 context->Global(), context, "firstArgument"));
    EXPECT_TRUE(
        GetPropertyFromObject(context->Global(), context, "secondArgument")
            ->IsFunction());
  }
}

TEST_F(APIBindingUnittest, TestPromiseWithJSUpdateArgumentsPostValidate) {
  bool context_allows_promises = true;
  SetPromiseAvailabilityFlag(&context_allows_promises);

  // Register an update arguments post validate hook for supportsPromises.
  const char kRegisterHook[] = R"(
      (function(hooks) {
        hooks.setUpdateArgumentsPostValidate('supportsPromises',
                                             (...arguments) => {
          this.firstArgument = arguments[0];
          this.secondArgument = arguments[1];
          arguments[0] = 'bar' + this.firstArgument;
          return arguments;
        });
      }))";
  InitializeJSHooks(kRegisterHook);
  SetFunctions(kFunctionsWithPromiseSignatures);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  {
    // Calling the method with an invalid signature should never enter the hook.
    ExpectFailure(binding_object, "return obj.supportsPromises('foo');",
                  api_errors::InvocationError(
                      "test.supportsPromises", "integer int, function callback",
                      api_errors::NoMatchingSignature()));
    EXPECT_EQ("undefined", GetStringPropertyFromObject(
                               context->Global(), context, "firstArgument"));
  }

  {
    // Calling supportsPromises normally with a callback should work fine and
    // the arguments should be manipulated.
    auto result =
        ExpectPass(binding_object, "return obj.supportsPromises(5, () => {});",
                   R"(["bar5"])", true);
    ASSERT_FALSE(result.IsEmpty());
    EXPECT_TRUE(result->IsUndefined());
    EXPECT_EQ(R"(5)", GetStringPropertyFromObject(context->Global(), context,
                                                  "firstArgument"));
    EXPECT_TRUE(
        GetPropertyFromObject(context->Global(), context, "secondArgument")
            ->IsFunction());
  }

  {
    // Calling supportsPromises normally while omitting the callback should work
    // fine, we should get a promise back and the arguments should be
    // manipulated.
    auto result = ExpectPass(binding_object, "return obj.supportsPromises(6);",
                             R"(["bar6"])", true);
    EXPECT_TRUE(V8ValueIs<v8::Promise>(result));
    EXPECT_EQ(R"(6)", GetStringPropertyFromObject(context->Global(), context,
                                                  "firstArgument"));
  }
}

}  // namespace extensions

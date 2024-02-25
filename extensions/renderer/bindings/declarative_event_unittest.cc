// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/declarative_event.h"

#include <memory>
#include <string_view>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "extensions/common/extension_api.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/api_bindings_system.h"
#include "extensions/renderer/bindings/api_bindings_system_unittest.h"
#include "extensions/renderer/bindings/api_last_error.h"
#include "extensions/renderer/bindings/api_request_handler.h"
#include "extensions/renderer/bindings/api_type_reference_map.h"
#include "extensions/renderer/bindings/argument_spec.h"
#include "extensions/renderer/bindings/test_interaction_provider.h"
#include "gin/handle.h"

namespace extensions {

namespace {

const char kDeclarativeAPIName[] = "alpha";
const char kDeclarativeAPISpec[] =
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
    "  'events': [{"
    "    'name': 'declarativeEvent',"
    "    'options': {"
    "      'supportsRules': true,"
    "      'supportsListeners': false,"
    "      'actions': ['alpha.enumRef'],"
    "      'conditions': ['alpha.objRef']"
    "    }"
    "  }]"
    "}";

}  // namespace

class DeclarativeEventTest : public APIBindingTest {
 public:
  DeclarativeEventTest()
      : type_refs_(APITypeReferenceMap::InitializeTypeCallback()) {}

  DeclarativeEventTest(const DeclarativeEventTest&) = delete;
  DeclarativeEventTest& operator=(const DeclarativeEventTest&) = delete;

  ~DeclarativeEventTest() override {}

  void OnRequest(std::unique_ptr<APIRequestHandler::Request> request,
                 v8::Local<v8::Context> context) {
    last_request_ = std::move(request);
  }

  APITypeReferenceMap* type_refs() { return &type_refs_; }
  APIRequestHandler* request_handler() { return request_handler_.get(); }
  APIRequestHandler::Request* last_request() { return last_request_.get(); }
  void reset_last_request() { last_request_.reset(); }

 private:
  void SetUp() override {
    APIBindingTest::SetUp();

    {
      auto action1 = std::make_unique<ArgumentSpec>(ArgumentType::STRING);
      action1->set_enum_values({"actionA"});
      type_refs_.AddSpec("action1", std::move(action1));
      auto action2 = std::make_unique<ArgumentSpec>(ArgumentType::STRING);
      action2->set_enum_values({"actionB"});
      type_refs_.AddSpec("action2", std::move(action2));
    }

    {
      auto condition = std::make_unique<ArgumentSpec>(ArgumentType::OBJECT);
      auto prop = std::make_unique<ArgumentSpec>(ArgumentType::STRING);
      ArgumentSpec::PropertiesMap props;
      props["url"] = std::move(prop);
      condition->set_properties(std::move(props));
      type_refs_.AddSpec("condition", std::move(condition));
    }

    interaction_provider_ = std::make_unique<TestInteractionProvider>();
    request_handler_ = std::make_unique<APIRequestHandler>(
        base::BindRepeating(&DeclarativeEventTest::OnRequest,
                            base::Unretained(this)),
        APILastError(APILastError::GetParent(), binding::AddConsoleError()),
        nullptr, interaction_provider_.get());
  }

  void TearDown() override {
    request_handler_.reset();
    interaction_provider_.reset();
    APIBindingTest::TearDown();
  }

  APITypeReferenceMap type_refs_;
  std::unique_ptr<TestInteractionProvider> interaction_provider_;
  std::unique_ptr<APIRequestHandler> request_handler_;
  std::unique_ptr<APIRequestHandler::Request> last_request_;
};

// Test that the rules schema behaves properly. This is designed to be more of
// a sanity check than a comprehensive test, since it mostly delegates the logic
// out to ArgumentSpec.
TEST_F(DeclarativeEventTest, TestRulesSchema) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  gin::Handle<DeclarativeEvent> emitter = gin::CreateHandle(
      context->GetIsolate(),
      new DeclarativeEvent("declEvent", type_refs(), request_handler(),
                           {"action1", "action2"}, {"condition"}, 0));

  v8::Local<v8::Value> emitter_value = emitter.ToV8();

  const char kAddRules[] =
      "(function(event) {\n"
      "  var rules = %s;\n"
      "  event.addRules(rules);\n"
      "})";

  {
    const char kGoodRules[] =
        "[{id: 'rule', tags: ['tag'],"
        "  actions: ['actionA'],"
        "  conditions: [{url: 'example.com'}],"
        "  priority: 1}]";
    v8::Local<v8::Function> function =
        FunctionFromString(context, base::StringPrintf(kAddRules, kGoodRules));
    v8::Local<v8::Value> args[] = {emitter_value};
    RunFunctionOnGlobal(function, context, std::size(args), args);

    EXPECT_TRUE(last_request());
    reset_last_request();
  }

  {
    // Invalid action.
    const char kBadRules1[] =
        "[{id: 'rule', tags: ['tag'],"
        "  actions: ['notAnAction'],"
        "  conditions: [{url: 'example.com'}],"
        "  priority: 1}]";
    // Missing required property: conditions.
    const char kBadRules2[] =
        "[{id: 'rule', tags: ['tag'],"
        "  actions: ['actionA'],"
        "  priority: 1}]";
    // Missing required property: actions.
    const char kBadRules3[] =
        "[{id: 'rule', tags: ['tag'],"
        "  conditions: [{url: 'example.com'}],"
        "  priority: 1}]";
    for (const char* rules : {kBadRules1, kBadRules2, kBadRules3}) {
      v8::Local<v8::Function> function =
          FunctionFromString(context, base::StringPrintf(kAddRules, rules));
      v8::Local<v8::Value> args[] = {emitter_value};
      RunFunctionAndExpectError(function, context, std::size(args), args,
                                "Uncaught TypeError: Invalid invocation");
      EXPECT_FALSE(last_request()) << rules;
      reset_last_request();
    }
  }
}

class DeclarativeEventWithSchemaTest : public APIBindingsSystemTest {
 protected:
  DeclarativeEventWithSchemaTest() {}
  ~DeclarativeEventWithSchemaTest() override {}

  std::vector<FakeSpec> GetAPIs() override {
    // events.removeRules and events.getRules are specified in the events.json
    // schema, so we need to load that.
    std::string_view events_schema =
        ExtensionAPI::GetSharedInstance()->GetSchemaStringPiece("events");
    return {{kDeclarativeAPIName, kDeclarativeAPISpec},
            {"events", events_schema.data()}};
  }
};

// Test all methods of declarative events.
TEST_F(DeclarativeEventWithSchemaTest, TestAllMethods) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> api = bindings_system()->CreateAPIInstance(
      kDeclarativeAPIName, context, nullptr);
  ASSERT_FALSE(api.IsEmpty());

  v8::Local<v8::Object> declarative_event;
  ASSERT_TRUE(GetPropertyFromObjectAs(api, context, "declarativeEvent",
                                      &declarative_event));
  v8::Local<v8::Function> add_rules;
  ASSERT_TRUE(GetPropertyFromObjectAs(declarative_event, context, "addRules",
                                      &add_rules));

  v8::Local<v8::Value> args[] = {api};

  {
    const char kAddRules[] =
        R"((function(obj) {
              obj.declarativeEvent.addRules(
                  [{
                    id: 'rule',
                    conditions: [{prop1: 'property'}],
                    actions: ['cat'],
                  }]);
             }))";
    v8::Local<v8::Function> add_rules_func =
        FunctionFromString(context, kAddRules);
    RunFunctionOnGlobal(add_rules_func, context, std::size(args), args);
    ValidateLastRequest("events.addRules",
                        "['alpha.declarativeEvent',0,"
                        "[{'actions':['cat'],"
                        "'conditions':[{'prop1':'property'}],"
                        "'id':'rule'}]]");
    reset_last_request();
  }

  {
    const char kRemoveRules[] =
        "(function(obj) {\n"
        "  obj.declarativeEvent.removeRules(['rule']);\n"
        "})";
    v8::Local<v8::Function> remove_rules =
        FunctionFromString(context, kRemoveRules);
    RunFunctionOnGlobal(remove_rules, context, std::size(args), args);
    ValidateLastRequest("events.removeRules",
                        "['alpha.declarativeEvent',0,['rule']]");
    reset_last_request();
  }

  {
    const char kGetRules[] =
        "(function(obj) {\n"
        "  obj.declarativeEvent.getRules(function() {});\n"
        "})";
    v8::Local<v8::Function> remove_rules =
        FunctionFromString(context, kGetRules);
    RunFunctionOnGlobal(remove_rules, context, std::size(args), args);
    ValidateLastRequest("events.getRules", "['alpha.declarativeEvent',0,null]");
    reset_last_request();
  }
}

}  // namespace extensions

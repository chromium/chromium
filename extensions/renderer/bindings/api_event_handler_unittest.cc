// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_event_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "extensions/common/event_filtering_info.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/exception_handler.h"
#include "extensions/renderer/bindings/test_js_runner.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/public/context_holder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

namespace {

const char kAddListenerFunction[] =
    "(function(event, listener) { event.addListener(listener); })";
const char kRemoveListenerFunction[] =
    "(function(event, listener) { event.removeListener(listener); })";

using MockEventChangeHandler = ::testing::StrictMock<
    base::MockCallback<APIEventListeners::ListenersUpdated>>;

std::string GetContextOwner(v8::Local<v8::Context> context) {
  return "context";
}

// TODO(devlin): Use these handy functions more places.
void AddListener(v8::Local<v8::Context> context,
                 v8::Local<v8::Function> listener,
                 v8::Local<v8::Object> event) {
  v8::Local<v8::Function> add_listener =
      FunctionFromString(context, kAddListenerFunction);
  v8::Local<v8::Value> argv[] = {event, listener};
  RunFunction(add_listener, context, base::size(argv), argv);
}

void RemoveListener(v8::Local<v8::Context> context,
                    v8::Local<v8::Function> listener,
                    v8::Local<v8::Object> event) {
  v8::Local<v8::Function> remove_listener =
      FunctionFromString(context, kRemoveListenerFunction);
  v8::Local<v8::Value> argv[] = {event, listener};
  RunFunction(remove_listener, context, base::size(argv), argv);
}

class APIEventHandlerTest : public APIBindingTest {
 protected:
  APIEventHandlerTest() {}
  ~APIEventHandlerTest() override {}

  void SetUp() override {
    APIBindingTest::SetUp();
    handler_ = std::make_unique<APIEventHandler>(
        base::DoNothing(), base::BindRepeating(&GetContextOwner), nullptr);
  }

  void TearDown() override {
    DisposeAllContexts();
    handler_.reset();
    APIBindingTest::TearDown();
  }

  void OnWillDisposeContext(v8::Local<v8::Context> context) override {
    ASSERT_TRUE(handler_);
    handler_->InvalidateContext(context);
  }

  void SetHandler(std::unique_ptr<APIEventHandler> handler) {
    handler_ = std::move(handler);
  }

  APIEventHandler* handler() { return handler_.get(); }

 private:
  std::unique_ptr<APIEventHandler> handler_;

  DISALLOW_COPY_AND_ASSIGN(APIEventHandlerTest);
};

}  // namespace

// Tests adding, removing, and querying event listeners by calling the
// associated methods on the JS object.
TEST_F(APIEventHandlerTest, AddingRemovingAndQueryingEventListeners) {
  const char kEventName[] = "alpha";
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> event = handler()->CreateEventInstance(
      kEventName, false, true, binding::kNoListenerMax, true, context);
  ASSERT_FALSE(event.IsEmpty());

  EXPECT_EQ(0u, handler()->GetNumEventListenersForTesting(kEventName, context));

  const char kListenerFunction[] = "(function() {})";
  v8::Local<v8::Function> listener_function =
      FunctionFromString(context, kListenerFunction);
  ASSERT_FALSE(listener_function.IsEmpty());

  v8::Local<v8::Function> add_listener_function =
      FunctionFromString(context, kAddListenerFunction);

  {
    v8::Local<v8::Value> argv[] = {event, listener_function};
    RunFunction(add_listener_function, context, base::size(argv), argv);
  }
  // There should only be one listener on the event.
  EXPECT_EQ(1u, handler()->GetNumEventListenersForTesting(kEventName, context));

  {
    v8::Local<v8::Value> argv[] = {event, listener_function};
    RunFunction(add_listener_function, context, base::size(argv), argv);
  }
  // Trying to add the same listener again should be a no-op.
  EXPECT_EQ(1u, handler()->GetNumEventListenersForTesting(kEventName, context));

  // Test hasListener returns true for a listener that is present.
  const char kHasListenerFunction[] =
      "(function(event, listener) { return event.hasListener(listener); })";
  v8::Local<v8::Function> has_listener_function =
      FunctionFromString(context, kHasListenerFunction);
  {
    v8::Local<v8::Value> argv[] = {event, listener_function};
    v8::Local<v8::Value> result =
        RunFunction(has_listener_function, context, base::size(argv), argv);
    bool has_listener = false;
    EXPECT_TRUE(gin::Converter<bool>::FromV8(isolate(), result, &has_listener));
    EXPECT_TRUE(has_listener);
  }

  // Test that hasListener returns false for a listener that isn't present.
  {
    v8::Local<v8::Function> not_a_listener =
        FunctionFromString(context, "(function() {})");
    v8::Local<v8::Value> argv[] = {event, not_a_listener};
    v8::Local<v8::Value> result =
        RunFunction(has_listener_function, context, base::size(argv), argv);
    bool has_listener = false;
    EXPECT_TRUE(gin::Converter<bool>::FromV8(isolate(), result, &has_listener));
    EXPECT_FALSE(has_listener);
  }

  // Test hasListeners returns true
  const char kHasListenersFunction[] =
      "(function(event) { return event.hasListeners(); })";
  v8::Local<v8::Function> has_listeners_function =
      FunctionFromString(context, kHasListenersFunction);
  {
    v8::Local<v8::Value> argv[] = {event};
    v8::Local<v8::Value> result =
        RunFunction(has_listeners_function, context, base::size(argv), argv);
    bool has_listeners = false;
    EXPECT_TRUE(
        gin::Converter<bool>::FromV8(isolate(), result, &has_listeners));
    EXPECT_TRUE(has_listeners);
  }

  v8::Local<v8::Function> remove_listener_function =
      FunctionFromString(context, kRemoveListenerFunction);
  {
    v8::Local<v8::Value> argv[] = {event, listener_function};
    RunFunction(remove_listener_function, context, base::size(argv), argv);
  }
  EXPECT_EQ(0u, handler()->GetNumEventListenersForTesting(kEventName, context));

  {
    v8::Local<v8::Value> argv[] = {event};
    v8::Local<v8::Value> result =
        RunFunction(has_listeners_function, context, base::size(argv), argv);
    bool has_listeners = false;
    EXPECT_TRUE(
        gin::Converter<bool>::FromV8(isolate(), result, &has_listeners));
    EXPECT_FALSE(has_listeners);
  }
}

// Tests listening for and firing different events.
TEST_F(APIEventHandlerTest, FiringEvents) {
  const char kAlphaName[] = "alpha";
  const char kBetaName[] = "beta";
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> alpha_event = handler()->CreateEventInstance(
      kAlphaName, false, true, binding::kNoListenerMax, true, context);
  v8::Local<v8::Object> beta_event = handler()->CreateEventInstance(
      kBetaName, false, true, binding::kNoListenerMax, true, context);
  ASSERT_FALSE(alpha_event.IsEmpty());
  ASSERT_FALSE(beta_event.IsEmpty());

  const char kAlphaListenerFunction1[] =
      "(function() {\n"
      "  if (!this.alphaCount1) this.alphaCount1 = 0;\n"
      "  ++this.alphaCount1;\n"
      "});\n";
  v8::Local<v8::Function> alpha_listener1 =
      FunctionFromString(context, kAlphaListenerFunction1);
  const char kAlphaListenerFunction2[] =
      "(function() {\n"
      "  if (!this.alphaCount2) this.alphaCount2 = 0;\n"
      "  ++this.alphaCount2;\n"
      "});\n";
  v8::Local<v8::Function> alpha_listener2 =
      FunctionFromString(context, kAlphaListenerFunction2);
  const char kBetaListenerFunction[] =
      "(function() {\n"
      "  if (!this.betaCount) this.betaCount = 0;\n"
      "  ++this.betaCount;\n"
      "});\n";
  v8::Local<v8::Function> beta_listener =
      FunctionFromString(context, kBetaListenerFunction);
  ASSERT_FALSE(alpha_listener1.IsEmpty());
  ASSERT_FALSE(alpha_listener2.IsEmpty());
  ASSERT_FALSE(beta_listener.IsEmpty());

  {
    v8::Local<v8::Function> add_listener_function =
        FunctionFromString(context, kAddListenerFunction);
    {
      v8::Local<v8::Value> argv[] = {alpha_event, alpha_listener1};
      RunFunction(add_listener_function, context, base::size(argv), argv);
    }
    {
      v8::Local<v8::Value> argv[] = {alpha_event, alpha_listener2};
      RunFunction(add_listener_function, context, base::size(argv), argv);
    }
    {
      v8::Local<v8::Value> argv[] = {beta_event, beta_listener};
      RunFunction(add_listener_function, context, base::size(argv), argv);
    }
  }

  EXPECT_EQ(2u, handler()->GetNumEventListenersForTesting(kAlphaName, context));
  EXPECT_EQ(1u, handler()->GetNumEventListenersForTesting(kBetaName, context));

  auto get_fired_count = [&context](const char* name) {
    v8::Local<v8::Value> res =
        GetPropertyFromObject(context->Global(), context, name);
    if (res->IsUndefined())
      return 0;
    int32_t count = 0;
    EXPECT_TRUE(
        gin::Converter<int32_t>::FromV8(context->GetIsolate(), res, &count))
        << name;
    return count;
  };

  EXPECT_EQ(0, get_fired_count("alphaCount1"));
  EXPECT_EQ(0, get_fired_count("alphaCount2"));
  EXPECT_EQ(0, get_fired_count("betaCount"));

  handler()->FireEventInContext(kAlphaName, context, base::ListValue(),
                                nullptr);
  EXPECT_EQ(2u, handler()->GetNumEventListenersForTesting(kAlphaName, context));
  EXPECT_EQ(1u, handler()->GetNumEventListenersForTesting(kBetaName, context));

  EXPECT_EQ(1, get_fired_count("alphaCount1"));
  EXPECT_EQ(1, get_fired_count("alphaCount2"));
  EXPECT_EQ(0, get_fired_count("betaCount"));

  handler()->FireEventInContext(kAlphaName, context, base::ListValue(),
                                nullptr);
  EXPECT_EQ(2, get_fired_count("alphaCount1"));
  EXPECT_EQ(2, get_fired_count("alphaCount2"));
  EXPECT_EQ(0, get_fired_count("betaCount"));

  handler()->FireEventInContext(kBetaName, context, base::ListValue(), nullptr);
  EXPECT_EQ(2, get_fired_count("alphaCount1"));
  EXPECT_EQ(2, get_fired_count("alphaCount2"));
  EXPECT_EQ(1, get_fired_count("betaCount"));
}

// Tests firing events with arguments.
TEST_F(APIEventHandlerTest, EventArguments) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  const char kEventName[] = "alpha";
  v8::Local<v8::Object> event = handler()->CreateEventInstance(
      kEventName, false, true, binding::kNoListenerMax, true, context);
  ASSERT_FALSE(event.IsEmpty());

  const char kListenerFunction[] =
      "(function() { this.eventArgs = Array.from(arguments); })";
  v8::Local<v8::Function> listener_function =
      FunctionFromString(context, kListenerFunction);
  ASSERT_FALSE(listener_function.IsEmpty());

  {
    v8::Local<v8::Function> add_listener_function =
        FunctionFromString(context, kAddListenerFunction);
    v8::Local<v8::Value> argv[] = {event, listener_function};
    RunFunction(add_listener_function, context, base::size(argv), argv);
  }

  const char kArguments[] = "['foo',1,{'prop1':'bar'}]";
  std::unique_ptr<base::ListValue> event_args = ListValueFromString(kArguments);
  ASSERT_TRUE(event_args);
  handler()->FireEventInContext(kEventName, context, *event_args, nullptr);

  EXPECT_EQ(
      ReplaceSingleQuotes(kArguments),
      GetStringPropertyFromObject(context->Global(), context, "eventArgs"));
}

// Test dispatching events to multiple contexts.
TEST_F(APIEventHandlerTest, MultipleContexts) {
  v8::HandleScope handle_scope(isolate());

  v8::Local<v8::Context> context_a = MainContext();
  v8::Local<v8::Context> context_b = AddContext();

  const char kEventName[] = "onFoo";


  v8::Local<v8::Function> listener_a = FunctionFromString(
      context_a, "(function(arg) { this.eventArgs = arg + 'alpha'; })");
  ASSERT_FALSE(listener_a.IsEmpty());
  v8::Local<v8::Function> listener_b = FunctionFromString(
      context_b, "(function(arg) { this.eventArgs = arg + 'beta'; })");
  ASSERT_FALSE(listener_b.IsEmpty());

  // Create two instances of the same event in different contexts.
  v8::Local<v8::Object> event_a = handler()->CreateEventInstance(
      kEventName, false, true, binding::kNoListenerMax, true, context_a);
  ASSERT_FALSE(event_a.IsEmpty());
  v8::Local<v8::Object> event_b = handler()->CreateEventInstance(
      kEventName, false, true, binding::kNoListenerMax, true, context_b);
  ASSERT_FALSE(event_b.IsEmpty());

  // Add two separate listeners to the event, one in each context.
  {
    v8::Local<v8::Function> add_listener_a =
        FunctionFromString(context_a, kAddListenerFunction);
    v8::Local<v8::Value> argv[] = {event_a, listener_a};
    RunFunction(add_listener_a, context_a, base::size(argv), argv);
  }
  EXPECT_EQ(1u,
            handler()->GetNumEventListenersForTesting(kEventName, context_a));
  EXPECT_EQ(0u,
            handler()->GetNumEventListenersForTesting(kEventName, context_b));

  {
    v8::Local<v8::Function> add_listener_b =
        FunctionFromString(context_b, kAddListenerFunction);
    v8::Local<v8::Value> argv[] = {event_b, listener_b};
    RunFunction(add_listener_b, context_b, base::size(argv), argv);
  }
  EXPECT_EQ(1u,
            handler()->GetNumEventListenersForTesting(kEventName, context_a));
  EXPECT_EQ(1u,
            handler()->GetNumEventListenersForTesting(kEventName, context_b));

  // Dispatch the event in context_a - the listener in context_b should not be
  // notified.
  std::unique_ptr<base::ListValue> arguments_a =
      ListValueFromString("['result_a:']");
  ASSERT_TRUE(arguments_a);

  handler()->FireEventInContext(kEventName, context_a, *arguments_a, nullptr);
  {
    EXPECT_EQ("\"result_a:alpha\"",
              GetStringPropertyFromObject(context_a->Global(), context_a,
                                          "eventArgs"));
  }
  {
    EXPECT_EQ("undefined", GetStringPropertyFromObject(context_b->Global(),
                                                       context_b, "eventArgs"));
  }

  // Dispatch the event in context_b - the listener in context_a should not be
  // notified.
  std::unique_ptr<base::ListValue> arguments_b =
      ListValueFromString("['result_b:']");
  ASSERT_TRUE(arguments_b);
  handler()->FireEventInContext(kEventName, context_b, *arguments_b, nullptr);
  {
    EXPECT_EQ("\"result_a:alpha\"",
              GetStringPropertyFromObject(context_a->Global(), context_a,
                                          "eventArgs"));
  }
  {
    EXPECT_EQ("\"result_b:beta\"",
              GetStringPropertyFromObject(context_b->Global(), context_b,
                                          "eventArgs"));
  }
}

TEST_F(APIEventHandlerTest, DifferentCallingMethods) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  const char kEventName[] = "alpha";
  v8::Local<v8::Object> event = handler()->CreateEventInstance(
      kEventName, false, true, binding::kNoListenerMax, true, context);
  ASSERT_FALSE(event.IsEmpty());

  const char kAddListenerOnNull[] =
      "(function(event) {\n"
      "  event.addListener.call(null, function() {});\n"
      "})";
  {
    v8::Local<v8::Value> args[] = {event};
    RunFunctionAndExpectError(
        FunctionFromString(context, kAddListenerOnNull), context, 1, args,
        "Uncaught TypeError: Illegal invocation: Function must be called on "
        "an object of type Event");
  }
  EXPECT_EQ(0u, handler()->GetNumEventListenersForTesting(kEventName, context));

  const char kAddListenerOnEvent[] =
      "(function(event) {\n"
      "  event.addListener.call(event, function() {});\n"
      "})";
  {
    v8::Local<v8::Value> args[] = {event};
    RunFunction(FunctionFromString(context, kAddListenerOnEvent),
                context, 1, args);
  }
  EXPECT_EQ(1u, handler()->GetNumEventListenersForTesting(kEventName, context));

  // Call addListener with a function that captures the event, creating a cycle.
  // If we don't properly clean up, the context will leak.
  const char kAddListenerOnEventWithCapture[] =
      "(function(event) {\n"
      "  event.addListener(function listener() {\n"
      "    event.hasListener(listener);\n"
      "  });\n"
      "})";
  {
    v8::Local<v8::Value> args[] = {event};
    RunFunction(FunctionFromString(context, kAddListenerOnEventWithCapture),
                context, 1, args);
  }
  EXPECT_EQ(2u, handler()->GetNumEventListenersForTesting(kEventName, context));
}

TEST_F(APIEventHandlerTest, TestDispatchFromJs) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> event = handler()->CreateEventInstance(
      "alpha", false, true, binding::kNoListenerMax, true, context);
  ASSERT_FALSE(event.IsEmpty());

  const char kListenerFunction[] =
      "(function() {\n"
      "  this.eventArgs = Array.from(arguments);\n"
      "});";
  v8::Local<v8::Function> listener =
      FunctionFromString(context, kListenerFunction);

  v8::Local<v8::Function> add_listener_function =
      FunctionFromString(context, kAddListenerFunction);

  {
    v8::Local<v8::Value> argv[] = {event, listener};
    RunFunctionOnGlobal(add_listener_function, context, base::size(argv), argv);
  }

  v8::Local<v8::Function> fire_event_function =
      FunctionFromString(
          context,
          "(function(event) { event.dispatch(42, 'foo', {bar: 'baz'}); })");
  {
    v8::Local<v8::Value> argv[] = {event};
    RunFunctionOnGlobal(fire_event_function, context, base::size(argv), argv);
  }

  EXPECT_EQ("[42,\"foo\",{\"bar\":\"baz\"}]",
            GetStringPropertyFromObject(
                context->Global(), context, "eventArgs"));
}

// Test listeners that remove themselves in their handling of the event.
TEST_F(APIEventHandlerTest, RemovingListenersWhileHandlingEvent) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  const char kEventName[] = "alpha";
  v8::Local<v8::Object> event = handler()->CreateEventInstance(
      kEventName, false, true, binding::kNoListenerMax, true, context);
  ASSERT_FALSE(event.IsEmpty());
  {
    // Cache the event object on the global in order to allow for easy removal.
    v8::Local<v8::Function> set_event_on_global =
        FunctionFromString(
            context,
           "(function(event) { this.testEvent = event; })");
    v8::Local<v8::Value> args[] = {event};
    RunFunctionOnGlobal(set_event_on_global, context, base::size(args), args);
    EXPECT_EQ(event,
              GetPropertyFromObject(context->Global(), context, "testEvent"));
  }

  // A listener function that removes itself as a listener.
  const char kListenerFunction[] =
      "(function() {\n"
      "  return function listener() {\n"
      "    this.testEvent.removeListener(listener);\n"
      "  };\n"
      "})();";

  // Create and add a bunch of listeners.
  std::vector<v8::Local<v8::Function>> listeners;
  const size_t kNumListeners = 20u;
  listeners.reserve(kNumListeners);
  for (size_t i = 0; i < kNumListeners; ++i)
    listeners.push_back(FunctionFromString(context, kListenerFunction));

  v8::Local<v8::Function> add_listener_function =
      FunctionFromString(context, kAddListenerFunction);

  for (const auto& listener : listeners) {
    v8::Local<v8::Value> argv[] = {event, listener};
    RunFunctionOnGlobal(add_listener_function, context, base::size(argv), argv);
  }

  // Fire the event. All listeners should be removed (and we shouldn't crash).
  EXPECT_EQ(kNumListeners,
            handler()->GetNumEventListenersForTesting(kEventName, context));
  handler()->FireEventInContext(kEventName, context, base::ListValue(),
                                nullptr);
  EXPECT_EQ(0u, handler()->GetNumEventListenersForTesting(kEventName, context));

  // TODO(devlin): Another possible test: register listener a and listener b,
  // where a removes b and b removes a. Theoretically, only one should be
  // notified. Investigate what we currently do in JS-style bindings.
}

// Test an event listener throwing an exception.
TEST_F(APIEventHandlerTest, TestEventListenersThrowingExceptions) {
  auto log_error =
      [](std::vector<std::string>* errors_out, v8::Local<v8::Context> context,
         const std::string& error) { errors_out->push_back(error); };

  std::vector<std::string> logged_errors;
  ExceptionHandler exception_handler(
      base::BindRepeating(log_error, &logged_errors));
  SetHandler(std::make_unique<APIEventHandler>(
      base::DoNothing(), base::BindRepeating(&GetContextOwner),
      &exception_handler));

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  const char kEventName[] = "alpha";
  v8::Local<v8::Object> event = handler()->CreateEventInstance(
      kEventName, false, true, binding::kNoListenerMax, true, context);
  ASSERT_FALSE(event.IsEmpty());

  // A listener that will throw an exception. We guarantee that we throw the
  // exception first so that we don't rely on event listener ordering.
  const char kListenerFunction[] =
      "(function() {\n"
      "  if (!this.didThrow) {\n"
      "    this.didThrow = true;\n"
      "    throw new Error('Event handler error');\n"
      "  }\n"
      "  this.eventArgs = Array.from(arguments);\n"
      "});";

  v8::Local<v8::Function> add_listener_function =
      FunctionFromString(context, kAddListenerFunction);

  for (int i = 0; i < 2; ++i) {
    v8::Local<v8::Function> listener =
        FunctionFromString(context, kListenerFunction);
    v8::Local<v8::Value> argv[] = {event, listener};
    RunFunctionOnGlobal(add_listener_function, context, base::size(argv), argv);
  }
  EXPECT_EQ(2u, handler()->GetNumEventListenersForTesting(kEventName, context));

  std::unique_ptr<base::ListValue> event_args = ListValueFromString("[42]");
  ASSERT_TRUE(event_args);

  {
    TestJSRunner::AllowErrors allow_errors;
    handler()->FireEventInContext(kEventName, context, *event_args, nullptr);
  }

  // An exception should have been thrown by the first listener and the second
  // listener should have recorded the event arguments.
  EXPECT_EQ("true", GetStringPropertyFromObject(context->Global(), context,
                                                "didThrow"));
  EXPECT_EQ("[42]", GetStringPropertyFromObject(context->Global(), context,
                                                "eventArgs"));
  ASSERT_EQ(1u, logged_errors.size());
  EXPECT_THAT(logged_errors[0],
              testing::StartsWith("Error in event handler: Error: "
                                  "Event handler error"));
}

// Tests being notified as listeners are added or removed from events.
TEST_F(APIEventHandlerTest, CallbackNotifications) {
  MockEventChangeHandler change_handler;
  SetHandler(std::make_unique<APIEventHandler>(
      change_handler.Get(), base::BindRepeating(&GetContextOwner), nullptr));

  v8::HandleScope handle_scope(isolate());

  v8::Local<v8::Context> context_a = MainContext();
  v8::Local<v8::Context> context_b = AddContext();

  const char kEventName1[] = "onFoo";
  const char kEventName2[] = "onBar";
  v8::Local<v8::Object> event1_a = handler()->CreateEventInstance(
      kEventName1, false, true, binding::kNoListenerMax, true, context_a);
  ASSERT_FALSE(event1_a.IsEmpty());
  v8::Local<v8::Object> event2_a = handler()->CreateEventInstance(
      kEventName2, false, true, binding::kNoListenerMax, true, context_a);
  ASSERT_FALSE(event2_a.IsEmpty());
  v8::Local<v8::Object> event1_b = handler()->CreateEventInstance(
      kEventName1, false, true, binding::kNoListenerMax, true, context_b);
  ASSERT_FALSE(event1_b.IsEmpty());

  // Add a listener to the first event. The APIEventHandler should notify
  // since it's a change in state (no listeners -> listeners).
  v8::Local<v8::Function> add_listener =
      FunctionFromString(context_a, kAddListenerFunction);
  v8::Local<v8::Function> listener1 =
      FunctionFromString(context_a, "(function() {})");
  {
    EXPECT_CALL(change_handler,
                Run(kEventName1,
                    binding::EventListenersChanged::
                        kFirstUnfilteredListenerForContextOwnerAdded,
                    nullptr, true, context_a))
        .Times(1);
    v8::Local<v8::Value> argv[] = {event1_a, listener1};
    RunFunction(add_listener, context_a, base::size(argv), argv);
    ::testing::Mock::VerifyAndClearExpectations(&change_handler);
  }
  EXPECT_EQ(1u,
            handler()->GetNumEventListenersForTesting(kEventName1, context_a));

  // Add a second listener to the same event. We should not be notified, since
  // the event already had listeners.
  v8::Local<v8::Function> listener2 =
      FunctionFromString(context_a, "(function() {})");
  {
    v8::Local<v8::Value> argv[] = {event1_a, listener2};
    RunFunction(add_listener, context_a, base::size(argv), argv);
  }
  EXPECT_EQ(2u,
            handler()->GetNumEventListenersForTesting(kEventName1, context_a));

  // Remove the first listener of the event. Again, since the event has
  // listeners, we shouldn't be notified.
  v8::Local<v8::Function> remove_listener =
      FunctionFromString(context_a, kRemoveListenerFunction);
  {
    v8::Local<v8::Value> argv[] = {event1_a, listener1};
    RunFunction(remove_listener, context_a, base::size(argv), argv);
  }

  EXPECT_EQ(1u,
            handler()->GetNumEventListenersForTesting(kEventName1, context_a));

  // Remove the final listener from the event. We should be notified that the
  // event no longer has listeners.
  {
    EXPECT_CALL(change_handler,
                Run(kEventName1,
                    binding::EventListenersChanged::
                        kLastUnfilteredListenerForContextOwnerRemoved,
                    nullptr, true, context_a))
        .Times(1);
    v8::Local<v8::Value> argv[] = {event1_a, listener2};
    RunFunction(remove_listener, context_a, base::size(argv), argv);
    ::testing::Mock::VerifyAndClearExpectations(&change_handler);
  }
  EXPECT_EQ(0u,
            handler()->GetNumEventListenersForTesting(kEventName1, context_a));

  // Add a listener to a separate event to ensure we receive the right
  // notifications.
  v8::Local<v8::Function> listener3 =
      FunctionFromString(context_a, "(function() {})");
  {
    EXPECT_CALL(change_handler,
                Run(kEventName2,
                    binding::EventListenersChanged::
                        kFirstUnfilteredListenerForContextOwnerAdded,
                    nullptr, true, context_a))
        .Times(1);
    v8::Local<v8::Value> argv[] = {event2_a, listener3};
    RunFunction(add_listener, context_a, base::size(argv), argv);
    ::testing::Mock::VerifyAndClearExpectations(&change_handler);
  }
  EXPECT_EQ(1u,
            handler()->GetNumEventListenersForTesting(kEventName2, context_a));

  {
    EXPECT_CALL(change_handler,
                Run(kEventName1,
                    binding::EventListenersChanged::
                        kFirstUnfilteredListenerForContextOwnerAdded,
                    nullptr, true, context_b))
        .Times(1);
    // And add a listener to an event in a different context to make sure the
    // associated context is correct.
    v8::Local<v8::Function> add_listener =
        FunctionFromString(context_b, kAddListenerFunction);
    v8::Local<v8::Function> listener =
        FunctionFromString(context_b, "(function() {})");
    v8::Local<v8::Value> argv[] = {event1_b, listener};
    RunFunction(add_listener, context_b, base::size(argv), argv);
    ::testing::Mock::VerifyAndClearExpectations(&change_handler);
  }
  EXPECT_EQ(1u,
            handler()->GetNumEventListenersForTesting(kEventName1, context_b));

  // When the contexts are invalidated, we should receive listener removed
  // notifications. Additionally, since this was the context being torn down,
  // rather than a removeListener call, was_manual should be false.
  EXPECT_CALL(change_handler,
              Run(kEventName2,
                  binding::EventListenersChanged::
                      kLastUnfilteredListenerForContextOwnerRemoved,
                  nullptr, false, context_a))
      .Times(1);
  DisposeContext(context_a);
  ::testing::Mock::VerifyAndClearExpectations(&change_handler);

  EXPECT_CALL(change_handler,
              Run(kEventName1,
                  binding::EventListenersChanged::
                      kLastUnfilteredListenerForContextOwnerRemoved,
                  nullptr, false, context_b))
      .Times(1);
  DisposeContext(context_b);
  ::testing::Mock::VerifyAndClearExpectations(&change_handler);
}

// Test registering an argument massager for a given event.
TEST_F(APIEventHandlerTest, TestArgumentMassagers) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  const char kEventName[] = "alpha";
  v8::Local<v8::Object> event = handler()->CreateEventInstance(
      kEventName, false, true, binding::kNoListenerMax, true, context);
  ASSERT_FALSE(event.IsEmpty());

  const char kArgumentMassager[] =
      "(function(originalArgs, dispatch) {\n"
      "  this.originalArgs = originalArgs;\n"
      "  dispatch(['primary', 'secondary']);\n"
      "});";
  v8::Local<v8::Function> massager =
      FunctionFromString(context, kArgumentMassager);
  handler()->RegisterArgumentMassager(context, "alpha", massager);

  const char kListenerFunction[] =
      "(function() { this.eventArgs = Array.from(arguments); })";
  v8::Local<v8::Function> listener_function =
      FunctionFromString(context, kListenerFunction);
  ASSERT_FALSE(listener_function.IsEmpty());

  {
    v8::Local<v8::Function> add_listener_function =
        FunctionFromString(context, kAddListenerFunction);
    v8::Local<v8::Value> argv[] = {event, listener_function};
    RunFunction(add_listener_function, context, base::size(argv), argv);
  }

  const char kArguments[] = "['first','second']";
  std::unique_ptr<base::ListValue> event_args = ListValueFromString(kArguments);
  ASSERT_TRUE(event_args);
  handler()->FireEventInContext(kEventName, context, *event_args, nullptr);

  EXPECT_EQ(
      "[\"first\",\"second\"]",
      GetStringPropertyFromObject(context->Global(), context, "originalArgs"));
  EXPECT_EQ(
      "[\"primary\",\"secondary\"]",
      GetStringPropertyFromObject(context->Global(), context, "eventArgs"));
}

// Test registering an argument massager for a given event and dispatching
// asynchronously.
TEST_F(APIEventHandlerTest, TestArgumentMassagersAsyncDispatch) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  const char kEventName[] = "alpha";
  v8::Local<v8::Object> event = handler()->CreateEventInstance(
      kEventName, false, true, binding::kNoListenerMax, true, context);
  ASSERT_FALSE(event.IsEmpty());

  const char kArgumentMassager[] =
      "(function(originalArgs, dispatch) {\n"
      "  this.originalArgs = originalArgs;\n"
      "  this.dispatch = dispatch;\n"
      "});";
  v8::Local<v8::Function> massager =
      FunctionFromString(context, kArgumentMassager);
  handler()->RegisterArgumentMassager(context, "alpha", massager);

  const char kListenerFunction[] =
      "(function() { this.eventArgs = Array.from(arguments); })";
  v8::Local<v8::Function> listener_function =
      FunctionFromString(context, kListenerFunction);
  ASSERT_FALSE(listener_function.IsEmpty());

  {
    v8::Local<v8::Function> add_listener_function =
        FunctionFromString(context, kAddListenerFunction);
    v8::Local<v8::Value> argv[] = {event, listener_function};
    RunFunction(add_listener_function, context, base::size(argv), argv);
  }

  const char kArguments[] = "['first','second']";
  std::unique_ptr<base::ListValue> event_args = ListValueFromString(kArguments);
  ASSERT_TRUE(event_args);
  handler()->FireEventInContext(kEventName, context, *event_args, nullptr);

  // The massager should have been triggered, but since it doesn't call
  // dispatch(), the listener shouldn't have been notified.
  EXPECT_EQ(
      "[\"first\",\"second\"]",
      GetStringPropertyFromObject(context->Global(), context, "originalArgs"));
  EXPECT_EQ("undefined", GetStringPropertyFromObject(context->Global(), context,
                                                     "eventArgs"));

  // Dispatch the event.
  v8::Local<v8::Value> dispatch_value =
      GetPropertyFromObject(context->Global(), context, "dispatch");
  ASSERT_FALSE(dispatch_value.IsEmpty());
  ASSERT_TRUE(dispatch_value->IsFunction());
  v8::Local<v8::Value> dispatch_args[] = {
      V8ValueFromScriptSource(context, "['primary', 'secondary']"),
  };
  RunFunction(dispatch_value.As<v8::Function>(), context,
              base::size(dispatch_args), dispatch_args);

  EXPECT_EQ(
      "[\"primary\",\"secondary\"]",
      GetStringPropertyFromObject(context->Global(), context, "eventArgs"));
}

// Test registering an argument massager and never dispatching.
TEST_F(APIEventHandlerTest, TestArgumentMassagersNeverDispatch) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  const char kEventName[] = "alpha";
  v8::Local<v8::Object> event = handler()->CreateEventInstance(
      kEventName, false, true, binding::kNoListenerMax, true, context);
  ASSERT_FALSE(event.IsEmpty());

  // A massager that never dispatches.
  const char kArgumentMassager[] = "(function(originalArgs, dispatch) {})";
  v8::Local<v8::Function> massager =
      FunctionFromString(context, kArgumentMassager);
  handler()->RegisterArgumentMassager(context, "alpha", massager);

  const char kListenerFunction[] = "(function() {})";
  v8::Local<v8::Function> listener_function =
      FunctionFromString(context, kListenerFunction);
  ASSERT_FALSE(listener_function.IsEmpty());

  v8::Local<v8::Function> add_listener_function =
      FunctionFromString(context, kAddListenerFunction);
  v8::Local<v8::Value> argv[] = {event, listener_function};
  RunFunction(add_listener_function, context, base::size(argv), argv);

  handler()->FireEventInContext(kEventName, context, base::ListValue(),
                                nullptr);

  // Nothing should blow up. (We tested in the previous test that the event
  // isn't notified without calling dispatch, so all there is to test here is
  // that we don't crash.)
}

// Test that event results of dispatch are passed to the calling argument
// massager. Regression test for https://crbug.com/867310.
TEST_F(APIEventHandlerTest, TestArgumentMassagersDispatchResult) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  const char kEventName[] = "alpha";
  v8::Local<v8::Object> event = handler()->CreateEventInstance(
      kEventName, false, true, binding::kNoListenerMax, true, context);
  ASSERT_FALSE(event.IsEmpty());

  const char kArgumentMassager[] =
      R"((function(originalArgs, dispatch) {
           this.dispatchResult = dispatch(['primary']);
         });)";
  v8::Local<v8::Function> massager =
      FunctionFromString(context, kArgumentMassager);
  handler()->RegisterArgumentMassager(context, kEventName, massager);

  const char kListenerFunction[] =
      R"((function(arg) {
           let res = arg == 'primary' ? 'listenerSuccess' : 'listenerFailure';
           this.listenerResult = res;
           return res;
         });)";
  v8::Local<v8::Function> listener_function =
      FunctionFromString(context, kListenerFunction);
  ASSERT_FALSE(listener_function.IsEmpty());

  {
    v8::Local<v8::Function> add_listener_function =
        FunctionFromString(context, kAddListenerFunction);
    v8::Local<v8::Value> argv[] = {event, listener_function};
    RunFunction(add_listener_function, context, base::size(argv), argv);
  }

  handler()->FireEventInContext(kEventName, context, base::ListValue(),
                                nullptr);

  EXPECT_EQ(
      R"({"results":["listenerSuccess"]})",
      GetStringPropertyFromObject(context->Global(), context,
                                  "dispatchResult"));
  EXPECT_EQ(
      R"("listenerSuccess")",
      GetStringPropertyFromObject(context->Global(), context,
                                  "listenerResult"));
}

// Test creating a custom event, as is done by a few of our custom bindings.
TEST_F(APIEventHandlerTest, TestCreateCustomEvent) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  MockEventChangeHandler change_handler;
  APIEventHandler handler(change_handler.Get(),
                          base::BindRepeating(&GetContextOwner), nullptr);

  v8::Local<v8::Object> event = handler.CreateAnonymousEventInstance(context);
  ASSERT_FALSE(event.IsEmpty());

  const char kAddListenerFunction[] =
      "(function(event) {\n"
      "  event.addListener(function() {\n"
      "    this.eventArgs = Array.from(arguments);\n"
      "  });\n"
      "})";
  v8::Local<v8::Value> add_listener_argv[] = {event};
  RunFunction(FunctionFromString(context, kAddListenerFunction), context,
              base::size(add_listener_argv), add_listener_argv);

  // Test dispatching to the listeners.
  const char kDispatchEventFunction[] =
      "(function(event) { event.dispatch(1, 2, 3); })";
  v8::Local<v8::Function> dispatch_function =
      FunctionFromString(context, kDispatchEventFunction);

  v8::Local<v8::Value> dispatch_argv[] = {event};
  RunFunction(dispatch_function, context, base::size(dispatch_argv),
              dispatch_argv);

  EXPECT_EQ("[1,2,3]", GetStringPropertyFromObject(context->Global(), context,
                                                   "eventArgs"));

  // Clean up so we can re-check eventArgs.
  ASSERT_TRUE(context->Global()
                  ->Delete(context, gin::StringToSymbol(isolate(), "eventArgs"))
                  .FromJust());

  // Invalidate the event and try dispatching again. Nothing should happen.
  handler.InvalidateCustomEvent(context, event);
  RunFunction(dispatch_function, context, base::size(dispatch_argv),
              dispatch_argv);
  EXPECT_EQ("undefined", GetStringPropertyFromObject(context->Global(), context,
                                                     "eventArgs"));
}

// Test adding a custom event with a cyclic dependency. Nothing should leak.
TEST_F(APIEventHandlerTest, TestCreateCustomEventWithCyclicDependency) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  MockEventChangeHandler change_handler;
  APIEventHandler handler(change_handler.Get(),
                          base::BindRepeating(&GetContextOwner), nullptr);

  v8::Local<v8::Object> event = handler.CreateAnonymousEventInstance(context);
  ASSERT_FALSE(event.IsEmpty());

  const char kAddListenerFunction[] =
      "(function(event) {\n"
      "  event.addListener(function() {}.bind(null, event));\n"
      "})";
  v8::Local<v8::Value> add_listener_argv[] = {event};
  RunFunction(FunctionFromString(context, kAddListenerFunction), context,
              base::size(add_listener_argv), add_listener_argv);

  DisposeContext(context);
}

TEST_F(APIEventHandlerTest, TestUnmanagedEvents) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  auto fail_on_notified =
      [](const std::string& event_name, binding::EventListenersChanged changed,
         const base::DictionaryValue* filter, bool was_manual,
         v8::Local<v8::Context> context) { ADD_FAILURE(); };

  APIEventHandler handler(base::BindRepeating(fail_on_notified),
                          base::BindRepeating(&GetContextOwner), nullptr);

  const char kEventName[] = "alpha";
  v8::Local<v8::Object> event = handler.CreateEventInstance(
      kEventName, false, true, binding::kNoListenerMax, false, context);

  const char kListener[] =
      "(function() {\n"
      "  this.eventArgs = Array.from(arguments);\n"
      "});";
  v8::Local<v8::Function> listener = FunctionFromString(context, kListener);

  {
    const char kAddListener[] =
        "(function(event, listener) { event.addListener(listener); })";
    v8::Local<v8::Value> args[] = {event, listener};
    RunFunction(FunctionFromString(context, kAddListener), context,
                base::size(args), args);
  }

  EXPECT_EQ(1u, handler.GetNumEventListenersForTesting(kEventName, context));

  handler.FireEventInContext(kEventName, context,
                             *ListValueFromString("[1, 'foo']"), nullptr);

  EXPECT_EQ("[1,\"foo\"]", GetStringPropertyFromObject(context->Global(),
                                                       context, "eventArgs"));

  {
    const char kRemoveListener[] =
        "(function(event, listener) { event.removeListener(listener); })";
    v8::Local<v8::Value> args[] = {event, listener};
    RunFunction(FunctionFromString(context, kRemoveListener), context,
                base::size(args), args);
  }

  EXPECT_EQ(0u, handler.GetNumEventListenersForTesting(kEventName, context));
}

// Test callback notifications for events that don't support lazy listeners.
TEST_F(APIEventHandlerTest, TestEventsWithoutLazyListeners) {
  MockEventChangeHandler change_handler;
  APIEventHandler handler(change_handler.Get(),
                          base::BindRepeating(&GetContextOwner), nullptr);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  const char kLazyListenersSupported[] = "supportsLazyListeners";
  const char kLazyListenersNotSupported[] = "noLazyListeners";
  v8::Local<v8::Object> lazy_listeners_supported =
      handler.CreateEventInstance(kLazyListenersSupported, false, true,
                                  binding::kNoListenerMax, true, context);
  v8::Local<v8::Object> lazy_listeners_not_supported =
      handler.CreateEventInstance(kLazyListenersNotSupported, false, false,
                                  binding::kNoListenerMax, true, context);
  ASSERT_FALSE(lazy_listeners_not_supported.IsEmpty());

  v8::Local<v8::Function> add_listener =
      FunctionFromString(context, kAddListenerFunction);
  v8::Local<v8::Function> listener =
      FunctionFromString(context, "(function() {})");
  {
    EXPECT_CALL(change_handler,
                Run(kLazyListenersSupported,
                    binding::EventListenersChanged::
                        kFirstUnfilteredListenerForContextOwnerAdded,
                    nullptr, true, context))
        .Times(1);
    v8::Local<v8::Value> argv[] = {lazy_listeners_supported, listener};
    RunFunction(add_listener, context, base::size(argv), argv);
    ::testing::Mock::VerifyAndClearExpectations(&change_handler);
  }

  {
    EXPECT_CALL(change_handler,
                Run(kLazyListenersNotSupported,
                    binding::EventListenersChanged::
                        kFirstUnfilteredListenerForContextOwnerAdded,
                    nullptr, false, context))
        .Times(1);
    v8::Local<v8::Value> argv[] = {lazy_listeners_not_supported, listener};
    RunFunction(add_listener, context, base::size(argv), argv);
    ::testing::Mock::VerifyAndClearExpectations(&change_handler);
  }

  v8::Local<v8::Function> remove_listener =
      FunctionFromString(context, kRemoveListenerFunction);
  {
    EXPECT_CALL(change_handler,
                Run(kLazyListenersSupported,
                    binding::EventListenersChanged::
                        kLastUnfilteredListenerForContextOwnerRemoved,
                    nullptr, true, context))
        .Times(1);
    v8::Local<v8::Value> argv[] = {lazy_listeners_supported, listener};
    RunFunction(remove_listener, context, base::size(argv), argv);
    ::testing::Mock::VerifyAndClearExpectations(&change_handler);
  }

  {
    EXPECT_CALL(change_handler,
                Run(kLazyListenersNotSupported,
                    binding::EventListenersChanged::
                        kLastUnfilteredListenerForContextOwnerRemoved,
                    nullptr, false, context))
        .Times(1);
    v8::Local<v8::Value> argv[] = {lazy_listeners_not_supported, listener};
    RunFunction(remove_listener, context, base::size(argv), argv);
    ::testing::Mock::VerifyAndClearExpectations(&change_handler);
  }

  DisposeContext(context);
}

// Tests dispatching events while script is suspended.
TEST_F(APIEventHandlerTest, TestDispatchingEventsWhileScriptSuspended) {
  const char kEventName[] = "alpha";
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> event = handler()->CreateEventInstance(
      kEventName, false, true, binding::kNoListenerMax, true, context);

  const char kListenerFunction[] = "(function() { this.eventFired = true; });";
  v8::Local<v8::Function> listener =
      FunctionFromString(context, kListenerFunction);

  {
    v8::Local<v8::Function> add_listener_function =
        FunctionFromString(context, kAddListenerFunction);
    v8::Local<v8::Value> argv[] = {event, listener};
    RunFunction(add_listener_function, context, base::size(argv), argv);
  }

  {
    // Suspend script and fire an event. The listener should *not* be notified
    // while script is suspended.
    TestJSRunner::Suspension script_suspension;
    handler()->FireEventInContext(kEventName, context, base::ListValue(),
                                  nullptr);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ("undefined", GetStringPropertyFromObject(context->Global(),
                                                       context, "eventFired"));
  }

  // After script resumes, the listener should be notified.
  EXPECT_EQ("true", GetStringPropertyFromObject(context->Global(), context,
                                                "eventFired"));
}

// Tests catching errors thrown by listeners while dispatching after script
// suspension.
TEST_F(APIEventHandlerTest,
       TestListenersThrowingExceptionsAfterScriptSuspension) {
  auto log_error =
      [](std::vector<std::string>* errors_out, v8::Local<v8::Context> context,
         const std::string& error) { errors_out->push_back(error); };

  std::vector<std::string> logged_errors;
  ExceptionHandler exception_handler(
      base::BindRepeating(log_error, &logged_errors));
  SetHandler(std::make_unique<APIEventHandler>(
      base::DoNothing(), base::BindRepeating(&GetContextOwner),
      &exception_handler));

  const char kEventName[] = "alpha";
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> event = handler()->CreateEventInstance(
      kEventName, false, true, binding::kNoListenerMax, true, context);

  const char kListenerFunction[] =
      "(function() {\n"
      "  this.eventFired = true;\n"
      "  throw new Error('hahaha');\n"
      "});";
  v8::Local<v8::Function> listener =
      FunctionFromString(context, kListenerFunction);

  {
    v8::Local<v8::Function> add_listener_function =
        FunctionFromString(context, kAddListenerFunction);
    v8::Local<v8::Value> argv[] = {event, listener};
    RunFunction(add_listener_function, context, base::size(argv), argv);
  }

  TestJSRunner::AllowErrors allow_errors;
  {
    // Suspend script and fire an event. The listener should not be notified,
    // and no errors should be logged.
    TestJSRunner::Suspension script_suspension;
    handler()->FireEventInContext(kEventName, context, base::ListValue(),
                                  nullptr);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ("undefined", GetStringPropertyFromObject(context->Global(),
                                                       context, "eventFired"));
    EXPECT_TRUE(logged_errors.empty());
  }

  // Once script resumes, the listener should have been notifed, and we should
  // have caught the error.
  EXPECT_EQ("true", GetStringPropertyFromObject(context->Global(), context,
                                                "eventFired"));
  ASSERT_EQ(1u, logged_errors.size());
  EXPECT_THAT(logged_errors[0],
              testing::StartsWith("Error in event handler: Error: hahaha"));
}

// Tests dispatching events when listeners are removed between when an event
// was fired (during script suspension) and when the script runs.
TEST_F(APIEventHandlerTest,
       TestDispatchingEventAfterListenersRemovedAfterScriptSuspension) {
  const char kEventName[] = "alpha";
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> event = handler()->CreateEventInstance(
      kEventName, false, true, binding::kNoListenerMax, true, context);

  const char kListenerFunction1[] =
      "(function() { this.eventFired1 = true; });";
  const char kListenerFunction2[] =
      "(function() { this.eventFired2 = true; });";
  v8::Local<v8::Function> listener1 =
      FunctionFromString(context, kListenerFunction1);
  v8::Local<v8::Function> listener2 =
      FunctionFromString(context, kListenerFunction2);

  // Add two event listeners.
  {
    v8::Local<v8::Function> add_listener_function =
        FunctionFromString(context, kAddListenerFunction);
    {
      v8::Local<v8::Value> argv[] = {event, listener1};
      RunFunction(add_listener_function, context, base::size(argv), argv);
    }
    {
      v8::Local<v8::Value> argv[] = {event, listener2};
      RunFunction(add_listener_function, context, base::size(argv), argv);
    }
  }
  EXPECT_EQ(2u, handler()->GetNumEventListenersForTesting(kEventName, context));

  {
    // Suspend script, and then queue up a call to remove the first listener.
    TestJSRunner::Suspension script_suspension;
    v8::Local<v8::Function> remove_listener_function =
        FunctionFromString(context, kRemoveListenerFunction);
    {
      v8::Local<v8::Value> argv[] = {event, listener1};
      // Note: Use JSRunner() so that script suspension is respected.
      JSRunner::Get(context)->RunJSFunction(remove_listener_function, context,
                                            base::size(argv), argv);
    }

    // Since script has been suspended, there should still be two listeners, and
    // neither should have been notified.
    EXPECT_EQ(2u,
              handler()->GetNumEventListenersForTesting(kEventName, context));
    handler()->FireEventInContext(kEventName, context, base::ListValue(),
                                  nullptr);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ("undefined", GetStringPropertyFromObject(context->Global(),
                                                       context, "eventFired1"));
    EXPECT_EQ("undefined", GetStringPropertyFromObject(context->Global(),
                                                       context, "eventFired2"));
  }

  // Once script resumes, the first listener should have been removed and the
  // event should have been fired. Since the listener was removed before the
  // event dispatch ran in JS, the first listener should *not* have been
  // notified.
  EXPECT_EQ(1u, handler()->GetNumEventListenersForTesting(kEventName, context));
  EXPECT_EQ("undefined", GetStringPropertyFromObject(context->Global(), context,
                                                     "eventFired1"));
  EXPECT_EQ("true", GetStringPropertyFromObject(context->Global(), context,
                                                "eventFired2"));
}

// Test that notifications are properly fired for multiple events with the
// same context owner.
TEST_F(APIEventHandlerTest,
       TestListenersFromDifferentContextsWithTheSameOwner) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context_alpha1 = MainContext();
  v8::Local<v8::Context> context_alpha2 = AddContext();
  v8::Local<v8::Context> context_beta1 = AddContext();

  // Associate two v8::Contexts with the same owner, and a third with a separate
  // owner.
  auto get_context_owner = [context_alpha1, context_alpha2,
                            context_beta1](v8::Local<v8::Context> context) {
    if (context == context_alpha1 || context == context_alpha2)
      return std::string("alpha");
    if (context == context_beta1)
      return std::string("beta");
    ADD_FAILURE();
    return std::string();
  };

  MockEventChangeHandler change_handler;
  APIEventHandler handler(change_handler.Get(),
                          base::BindLambdaForTesting(get_context_owner),
                          nullptr);

  const char kEventName[] = "alpha";
  v8::Local<v8::Object> event_alpha1 = handler.CreateEventInstance(
      kEventName, false, true, binding::kNoListenerMax, true, context_alpha1);
  ASSERT_FALSE(event_alpha1.IsEmpty());
  v8::Local<v8::Object> event_alpha2 = handler.CreateEventInstance(
      kEventName, false, true, binding::kNoListenerMax, true, context_alpha2);
  ASSERT_FALSE(event_alpha2.IsEmpty());
  v8::Local<v8::Object> event_beta1 = handler.CreateEventInstance(
      kEventName, false, true, binding::kNoListenerMax, true, context_beta1);
  ASSERT_FALSE(event_beta1.IsEmpty());

  // Add a listener to the first event. The APIEventHandler should notify
  // since it's a change in state (no listeners -> listeners).
  v8::Local<v8::Function> listener_alpha1 =
      FunctionFromString(context_alpha1, "(function() {})");
  EXPECT_CALL(change_handler,
              Run(kEventName,
                  binding::EventListenersChanged::
                      kFirstUnfilteredListenerForContextOwnerAdded,
                  nullptr, true, context_alpha1))
      .Times(1);
  AddListener(context_alpha1, listener_alpha1, event_alpha1);
  ::testing::Mock::VerifyAndClearExpectations(&change_handler);

  // Adding a listener to the same event in a different context that is still
  // associated with the same owner should fire a notification for the context,
  // but not the context owner.
  EXPECT_CALL(change_handler, Run(kEventName,
                                  binding::EventListenersChanged::
                                      kFirstUnfilteredListenerForContextAdded,
                                  nullptr, true, context_alpha2))
      .Times(1);
  v8::Local<v8::Function> listener_alpha2 =
      FunctionFromString(context_alpha2, "(function() {})");
  AddListener(context_alpha2, listener_alpha2, event_alpha2);
  ::testing::Mock::VerifyAndClearExpectations(&change_handler);

  // Adding a listener in a separate context should fire a notification.
  v8::Local<v8::Function> listener_beta1 =
      FunctionFromString(context_alpha1, "(function() {})");
  EXPECT_CALL(change_handler,
              Run(kEventName,
                  binding::EventListenersChanged::
                      kFirstUnfilteredListenerForContextOwnerAdded,
                  nullptr, true, context_beta1))
      .Times(1);
  AddListener(context_beta1, listener_beta1, event_beta1);
  ::testing::Mock::VerifyAndClearExpectations(&change_handler);

  // Removing one of the listeners from the alpha context should notify about
  // the context, but not the context owner (since there are multiple listeners
  // for the context owner).
  EXPECT_CALL(change_handler, Run(kEventName,
                                  binding::EventListenersChanged::
                                      kLastUnfilteredListenerForContextRemoved,
                                  nullptr, true, context_alpha1))
      .Times(1);
  RemoveListener(context_alpha1, listener_alpha1, event_alpha1);
  ::testing::Mock::VerifyAndClearExpectations(&change_handler);

  // Removing the final listener should fire a notification for the context
  // owner.
  EXPECT_CALL(change_handler,
              Run(kEventName,
                  binding::EventListenersChanged::
                      kLastUnfilteredListenerForContextOwnerRemoved,
                  nullptr, true, context_alpha2))
      .Times(1);
  RemoveListener(context_alpha2, listener_alpha2, event_alpha2);
  ::testing::Mock::VerifyAndClearExpectations(&change_handler);

  // And removing the only listener for the beta context should fire a
  // notification.
  EXPECT_CALL(change_handler,
              Run(kEventName,
                  binding::EventListenersChanged::
                      kLastUnfilteredListenerForContextOwnerRemoved,
                  nullptr, true, context_beta1))
      .Times(1);
  RemoveListener(context_beta1, listener_beta1, event_beta1);
  ::testing::Mock::VerifyAndClearExpectations(&change_handler);
}

}  // namespace extensions

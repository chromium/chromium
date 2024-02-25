// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/event_emitter.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/values.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/api_event_listeners.h"
#include "extensions/renderer/bindings/exception_handler.h"
#include "extensions/renderer/bindings/listener_tracker.h"
#include "extensions/renderer/bindings/test_js_runner.h"
#include "gin/handle.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

namespace {

APIEventListeners::ContextOwnerIdGetter CreateContextOwnerIdGetter() {
  return base::BindRepeating(
      [](v8::Local<v8::Context>) { return std::string("context"); });
}

}  // namespace

class EventEmitterUnittest : public APIBindingTest {
 public:
  EventEmitterUnittest() = default;

  EventEmitterUnittest(const EventEmitterUnittest&) = delete;
  EventEmitterUnittest& operator=(const EventEmitterUnittest&) = delete;

  ~EventEmitterUnittest() override = default;

  // A helper method to dispose of a context and set a flag.
  void DisposeContextWrapper(bool* did_invalidate,
                             v8::Local<v8::Context> context) {
    EXPECT_FALSE(*did_invalidate);
    *did_invalidate = true;
    DisposeContext(context);
  }
};

TEST_F(EventEmitterUnittest, TestDispatchMethod) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ListenerTracker tracker;
  auto listeners = std::make_unique<UnfilteredEventListeners>(
      base::DoNothing(), "event", CreateContextOwnerIdGetter(),
      binding::kNoListenerMax, true, &tracker);

  auto log_error = [](std::vector<std::string>* errors,
                      v8::Local<v8::Context> context,
                      const std::string& error) { errors->push_back(error); };

  std::vector<std::string> logged_errors;
  ExceptionHandler exception_handler(
      base::BindRepeating(log_error, &logged_errors));

  gin::Handle<EventEmitter> event = gin::CreateHandle(
      isolate(),
      new EventEmitter(false, std::move(listeners), &exception_handler));

  v8::Local<v8::Value> v8_event = event.ToV8();

  const char kAddListener[] =
      "(function(event, listener) { event.addListener(listener); })";
  v8::Local<v8::Function> add_listener_function =
      FunctionFromString(context, kAddListener);

  auto add_listener = [context, v8_event,
                       add_listener_function](std::string_view listener) {
    v8::Local<v8::Function> listener_function =
        FunctionFromString(context, listener);
    v8::Local<v8::Value> args[] = {v8_event, listener_function};
    RunFunction(add_listener_function, context, std::size(args), args);
  };

  const char kListener1[] =
      "(function() {\n"
      "  this.eventArgs1 = Array.from(arguments);\n"
      "  return 'listener1';\n"
      "})";
  add_listener(kListener1);
  const char kListener2[] =
      "(function() {\n"
      "  this.eventArgs2 = Array.from(arguments);\n"
      "  return {listener: 'listener2'};\n"
      "})";
  add_listener(kListener2);
  // Listener3 throws, but shouldn't stop the event from reaching other
  // listeners.
  const char kListener3[] =
      "(function() {\n"
      "  this.eventArgs3 = Array.from(arguments);\n"
      "  throw new Error('hahaha');\n"
      "})";
  add_listener(kListener3);
  // Returning undefined should not be added to the array of results from
  // dispatch.
  const char kListener4[] =
      "(function() {\n"
      "  this.eventArgs4 = Array.from(arguments);\n"
      "})";
  add_listener(kListener4);

  const char kDispatch[] =
      "(function(event) {\n"
      "  return event.dispatch('arg1', 2);\n"
      "})";
  v8::Local<v8::Value> dispatch_args[] = {v8_event};
  TestJSRunner::AllowErrors allow_errors;
  v8::Local<v8::Value> dispatch_result =
      RunFunctionOnGlobal(FunctionFromString(context, kDispatch), context,
                          std::size(dispatch_args), dispatch_args);

  const char kExpectedEventArgs[] = "[\"arg1\",2]";
  for (const char* property :
       {"eventArgs1", "eventArgs2", "eventArgs3", "eventArgs4"}) {
    EXPECT_EQ(kExpectedEventArgs, GetStringPropertyFromObject(
                                      context->Global(), context, property));
  }
  EXPECT_EQ("{\"results\":[\"listener1\",{\"listener\":\"listener2\"}]}",
            V8ToString(dispatch_result, context));

  ASSERT_EQ(1u, logged_errors.size());
  EXPECT_THAT(logged_errors[0],
              testing::StartsWith("Error in event handler: Error: hahaha"));
}

// Test dispatching an event when the first listener invalidates the context.
// Nothing should break, and we shouldn't continue to dispatch the event.
TEST_F(EventEmitterUnittest, ListenersDestroyingContext) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  struct ListenerClosureData {
    const raw_ref<EventEmitterUnittest> test;
    bool did_invalidate_context;
  } closure_data = {raw_ref(*this), false};

  // A wrapper that just calls DisposeContextWrapper() on the curried in data.
  auto listener_wrapper = [](const v8::FunctionCallbackInfo<v8::Value>& info) {
    ASSERT_TRUE(info.Data()->IsExternal());
    auto& data = *static_cast<ListenerClosureData*>(
        info.Data().As<v8::External>()->Value());
    data.test->DisposeContextWrapper(&data.did_invalidate_context,
                                     info.GetIsolate()->GetCurrentContext());
  };

  ListenerTracker tracker;
  auto listeners = std::make_unique<UnfilteredEventListeners>(
      base::DoNothing(), "event", CreateContextOwnerIdGetter(),
      binding::kNoListenerMax, true, &tracker);
  ExceptionHandler exception_handler(base::BindRepeating(
      [](v8::Local<v8::Context> context, const std::string& error) {}));
  gin::Handle<EventEmitter> event = gin::CreateHandle(
      isolate(),
      new EventEmitter(false, std::move(listeners), &exception_handler));

  v8::Local<v8::Value> v8_event = event.ToV8();

  const char kAddListener[] =
      "(function(event, listener) { event.addListener(listener); })";
  v8::Local<v8::Function> add_listener_function =
      FunctionFromString(context, kAddListener);

  // Queue up three listeners. The first triggered will invalidate the context.
  // The others should never be triggered.
  constexpr size_t kNumListeners = 3;
  for (size_t i = 0; i < kNumListeners; ++i) {
    v8::Local<v8::Function> listener =
        v8::Function::New(context, listener_wrapper,
                          v8::External::New(isolate(), &closure_data))
            .ToLocalChecked();
    v8::Local<v8::Value> args[] = {v8_event, listener};
    RunFunction(add_listener_function, context, std::size(args), args);
  }

  EXPECT_EQ(kNumListeners, event->GetNumListeners());

  v8::LocalVector<v8::Value> args(isolate());
  event->Fire(context, &args, nullptr, JSRunner::ResultCallback());

  EXPECT_TRUE(closure_data.did_invalidate_context);
}

}  // namespace extensions

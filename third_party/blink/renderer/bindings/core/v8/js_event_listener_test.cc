// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/js_event_listener.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_listener.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

v8::Local<v8::Value> Eval(const V8TestingScope& scope, const char* source) {
  v8::Isolate* isolate = scope.GetIsolate();
  v8::Local<v8::Context> context = scope.GetContext();
  v8::Local<v8::Script> script =
      v8::Script::Compile(context, V8String(isolate, source)).ToLocalChecked();
  return script->Run(context).ToLocalChecked();
}

TEST(JSEventListenerTest, CallableFunctionListenerReceivesEventAndReceiver) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  v8::Local<v8::Value> fn_val = Eval(scope, R"JS(
    window.receivedEvent = null;
    window.receivedThis = null;
    function listener(e) {
      window.receivedEvent = e;
      window.receivedThis = this;
    }
    listener;
  )JS");
  ASSERT_TRUE(fn_val->IsFunction());

  V8EventListener* v8_listener =
      V8EventListener::Create(fn_val.As<v8::Function>());
  JSEventListener* js_listener = JSEventListener::CreateOrNull(v8_listener);
  ASSERT_TRUE(js_listener);

  LocalDOMWindow& window = scope.GetWindow();
  Event* event = Event::Create(AtomicString("test"));

  window.addEventListener(AtomicString("test"), js_listener);
  window.DispatchEvent(*event);

  v8::Local<v8::Value> received_event = Eval(scope, "window.receivedEvent;");
  v8::Local<v8::Value> received_this = Eval(scope, "window.receivedThis;");

  EXPECT_FALSE(received_event->IsNull());
  EXPECT_FALSE(received_event->IsUndefined());
  EXPECT_TRUE(received_event->IsObject());

  // WHATWG DOM §2.9 step 10: receiver must be currentTarget (window).
  EXPECT_EQ(received_this, window.ToV8(scope.GetScriptState()));
}

TEST(JSEventListenerTest, HandleEventObjectListenerReceivesEventAndReceiver) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  v8::Local<v8::Value> obj_val = Eval(scope, R"JS(
    window.receivedEvent = null;
    window.receivedThis = null;
    const listenerObj = {
      handleEvent(e) {
        window.receivedEvent = e;
        window.receivedThis = this;
      }
    };
    listenerObj;
  )JS");
  ASSERT_TRUE(obj_val->IsObject());

  V8EventListener* v8_listener =
      V8EventListener::Create(obj_val.As<v8::Object>());
  JSEventListener* js_listener = JSEventListener::CreateOrNull(v8_listener);
  ASSERT_TRUE(js_listener);

  LocalDOMWindow& window = scope.GetWindow();
  Event* event = Event::Create(AtomicString("test"));

  window.addEventListener(AtomicString("test"), js_listener);
  window.DispatchEvent(*event);

  v8::Local<v8::Value> received_event = Eval(scope, "window.receivedEvent;");
  v8::Local<v8::Value> received_this = Eval(scope, "window.receivedThis;");

  EXPECT_FALSE(received_event->IsNull());
  EXPECT_FALSE(received_event->IsUndefined());
  EXPECT_TRUE(received_event->IsObject());

  // For { handleEvent } object listeners, Web IDL specifies receiver is the
  // callback object itself.
  EXPECT_EQ(received_this, obj_val);
}

TEST(JSEventListenerTest, CrossOriginListenerBlockedBySecurity) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope_listener(KURL("https://example.com/"));

  std::unique_ptr<DummyPageHolder> target_holder =
      DummyPageHolder::CreateAndCommitNavigation(
          KURL("https://not-example.com/"));
  LocalDOMWindow* target_window = target_holder->GetFrame().DomWindow();
  ASSERT_TRUE(target_window);

  v8::Isolate* isolate = scope_listener.GetIsolate();

  // 1. Callable function listener registered in https://example.com/
  v8::Local<v8::Value> fn_val = Eval(scope_listener, R"JS(
    window.invokedCallable = false;
    function listener(e) {
      window.invokedCallable = true;
    }
    listener;
  )JS");
  ASSERT_TRUE(fn_val->IsFunction());

  V8EventListener* v8_fn_listener =
      V8EventListener::Create(fn_val.As<v8::Function>());
  JSEventListener* js_fn_listener =
      JSEventListener::CreateOrNull(v8_fn_listener);
  ASSERT_TRUE(js_fn_listener);

  Event* event1 = Event::Create(AtomicString("test"));
  event1->SetTarget(target_window);
  event1->SetCurrentTarget(target_window);

  // Cross-origin dispatch should be blocked by security check.
  js_fn_listener->Invoke(target_window, event1);

  v8::Local<v8::Value> invoked_callable =
      Eval(scope_listener, "window.invokedCallable;");
  EXPECT_FALSE(invoked_callable->BooleanValue(isolate));

  // 2. { handleEvent } object listener registered in https://example.com/
  v8::Local<v8::Value> obj_val = Eval(scope_listener, R"JS(
    window.invokedObject = false;
    const objListener = {
      handleEvent(e) {
        window.invokedObject = true;
      }
    };
    objListener;
  )JS");
  ASSERT_TRUE(obj_val->IsObject());

  V8EventListener* v8_obj_listener =
      V8EventListener::Create(obj_val.As<v8::Object>());
  JSEventListener* js_obj_listener =
      JSEventListener::CreateOrNull(v8_obj_listener);
  ASSERT_TRUE(js_obj_listener);

  Event* event2 = Event::Create(AtomicString("test"));
  event2->SetTarget(target_window);
  event2->SetCurrentTarget(target_window);

  // Cross-origin dispatch should be blocked by security check.
  js_obj_listener->Invoke(target_window, event2);

  v8::Local<v8::Value> invoked_object =
      Eval(scope_listener, "window.invokedObject;");
  EXPECT_FALSE(invoked_object->BooleanValue(isolate));

  // 3. Same-origin dispatch succeeds for both.
  LocalDOMWindow* same_origin_window = &scope_listener.GetWindow();
  event1->SetTarget(same_origin_window);
  event1->SetCurrentTarget(same_origin_window);
  js_fn_listener->Invoke(same_origin_window, event1);

  invoked_callable = Eval(scope_listener, "window.invokedCallable;");
  EXPECT_TRUE(invoked_callable->BooleanValue(isolate));

  event2->SetTarget(same_origin_window);
  event2->SetCurrentTarget(same_origin_window);
  js_obj_listener->Invoke(same_origin_window, event2);

  invoked_object = Eval(scope_listener, "window.invokedObject;");
  EXPECT_TRUE(invoked_object->BooleanValue(isolate));
}

}  // namespace

}  // namespace blink

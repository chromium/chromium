// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/events/event_target.h"

#include "third_party/blink/renderer/bindings/core/v8/js_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_add_event_listener_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observable_event_listener_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_subscribe_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_observer_observercallback.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/dom/events/add_event_listener_options_resolved.h"
#include "third_party/blink/renderer/core/dom/observable.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

class EventTargetTest : public RenderingTest {
 public:
  EventTargetTest() = default;
  ~EventTargetTest() override = default;
};

TEST_F(EventTargetTest, UseCountPassiveTouchEventListener) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kPassiveTouchEventListener));
  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(
      "window.addEventListener('touchstart', function() {}, "
      "{passive: true});")
      ->RunScript(GetDocument().domWindow());
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kPassiveTouchEventListener));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kNonPassiveTouchEventListener));
}

TEST_F(EventTargetTest, UseCountNonPassiveTouchEventListener) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kNonPassiveTouchEventListener));
  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(
      "window.addEventListener('touchstart', function() {}, "
      "{passive: false});")
      ->RunScript(GetDocument().domWindow());
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kNonPassiveTouchEventListener));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kPassiveTouchEventListener));
}

TEST_F(EventTargetTest, UseCountPassiveTouchEventListenerPassiveNotSpecified) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kPassiveTouchEventListener));
  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(
      "window.addEventListener('touchstart', function() {});")
      ->RunScript(GetDocument().domWindow());
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kPassiveTouchEventListener));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kNonPassiveTouchEventListener));
}

TEST_F(EventTargetTest, UseCountBeforematch) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kBeforematchHandlerRegistered));
  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(R"HTML(
                       const element = document.createElement('div');
                       document.body.appendChild(element);
                       element.addEventListener('beforematch', () => {});
                      )HTML")
      ->RunScript(GetDocument().domWindow());
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kBeforematchHandlerRegistered));
}

TEST_F(EventTargetTest, UseCountAbortSignal) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kAddEventListenerWithAbortSignal));
  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(R"HTML(
                       const element = document.createElement('div');
                       const ac = new AbortController();
                       element.addEventListener(
                         'test', () => {}, {signal: ac.signal});
                      )HTML")
      ->RunScript(GetDocument().domWindow());
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kAddEventListenerWithAbortSignal));
}

TEST_F(EventTargetTest, UseCountScrollend) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kScrollend));
  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(R"HTML(
                       const element = document.createElement('div');
                       element.addEventListener('scrollend', () => {});
                       )HTML")
      ->RunScript(GetDocument().domWindow());
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kScrollend));
}

// See https://crbug.com/1357453.
// Tests that we don't crash when adding a unload event handler to a target
// that has no ExecutionContext.
TEST_F(EventTargetTest, UnloadWithoutExecutionContext) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(R"JS(
      document.createElement("track").track.addEventListener(
          "unload",() => {});
                      )JS")
      ->RunScript(GetDocument().domWindow());
}

// See https://crbug.com/1472739.
// Tests that we don't crash if the abort algorithm for a destroyed EventTarget
// runs because the associated EventListener hasn't yet been GCed.
TEST_F(EventTargetTest, EventTargetWithAbortSignalDestroyed) {
  V8TestingScope scope;
  Persistent<AbortController> controller =
      AbortController::Create(scope.GetScriptState());
  Persistent<EventListener> listener = JSEventListener::CreateOrNull(
      V8EventListener::Create(scope.GetContext()->Global()));
  {
    EventTarget* event_target = EventTarget::Create(scope.GetScriptState());
    auto* options = AddEventListenerOptions::Create();
    options->setSignal(controller->signal());
    event_target->addEventListener(
        AtomicString("test"), listener.Get(),
        MakeGarbageCollected<AddEventListenerOptionsResolved>(options));
    event_target = nullptr;
  }
  ThreadState::Current()->CollectAllGarbageForTesting();
  controller->abort(scope.GetScriptState());
}

// EventTarget-constructed Observables add an event listener for each
// subscription. Ensure that when a subscription becomes inactive, the event
// listener is removed.
TEST_F(EventTargetTest,
       ObservableSubscriptionBecomingInactiveRemovesEventListener) {
  V8TestingScope scope;
  EventTarget* event_target = EventTarget::Create(scope.GetScriptState());
  Observable* observable = event_target->when(
      AtomicString("test"),
      MakeGarbageCollected<ObservableEventListenerOptions>());
  EXPECT_FALSE(event_target->HasEventListeners());

  AbortController* controller = AbortController::Create(scope.GetScriptState());

  Observer* observer = MakeGarbageCollected<Observer>();
  V8UnionObserverOrObserverCallback* observer_union =
      MakeGarbageCollected<V8UnionObserverOrObserverCallback>(observer);
  SubscribeOptions* options = MakeGarbageCollected<SubscribeOptions>();
  options->setSignal(controller->signal());
  observable->subscribe(scope.GetScriptState(), observer_union,
                        /*options=*/options);
  EXPECT_TRUE(event_target->HasEventListeners());

  controller->abort(scope.GetScriptState());
  EXPECT_FALSE(event_target->HasEventListeners());
}

TEST_F(EventTargetTest, UseCountScrollsnapchanging) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kSnapEvent));
  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(R"HTML(
    const element = document.createElement('div');
    element.addEventListener('scrollsnapchanging', () => {});
  )HTML")
      ->RunScript(GetDocument().domWindow());
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kSnapEvent));
}

TEST_F(EventTargetTest, UseCountScrollsnapchange) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kSnapEvent));
  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(R"HTML(
    const element = document.createElement('div');
    element.addEventListener('scrollsnapchange', () => {});
  )HTML")
      ->RunScript(GetDocument().domWindow());
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kSnapEvent));
}

}  // namespace blink

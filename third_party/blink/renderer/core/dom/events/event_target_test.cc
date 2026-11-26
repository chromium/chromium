// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/events/event_target.h"

#include "third_party/blink/renderer/bindings/core/v8/js_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_add_event_listener_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observable_event_listener_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_subscribe_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_observer_observercallback.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/dom/events/add_event_listener_options_resolved.h"
#include "third_party/blink/renderer/core/dom/observable.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

class EventTargetTest : public RenderingTest {
 public:
  EventTargetTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}
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
    AddEventListenerOptionsResolved resolved_options(options);
    event_target->addEventListener(AtomicString("test"), listener.Get(),
                                   resolved_options);
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

TEST_F(EventTargetTest, UseCountMove) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kMoveEvent));
  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(R"HTML(
    window.addEventListener('move', () => {});
  )HTML")
      ->RunScript(GetDocument().domWindow());
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kMoveEvent));
}

TEST_F(EventTargetTest, UseCountOnMove) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kMoveEvent));
  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(R"HTML(
    window.onmove = () => {};
  )HTML")
      ->RunScript(GetDocument().domWindow());
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kMoveEvent));
}

TEST_F(EventTargetTest,
       IsolatedWorldRemoveAttributeDeactivatesContentAttributeEventHandler) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  v8::HandleScope scope(GetDocument().domWindow()->GetIsolate());
  constexpr int kIsolatedWorldId = 1;

  // Cover element, document-forwarded (`onselectionchange`), and
  // window-forwarded (`onresize`) content attributes.
  ClassicScript::CreateUnspecifiedScript(R"JS(
    document.body.innerHTML = '<button id="btn" onclick="1"></button>';
    document.body.setAttribute('onselectionchange', '1');
    document.body.setAttribute('onresize', '1');
  )JS")
      ->RunScriptInIsolatedWorldAndReturnValue(GetDocument().domWindow(),
                                               kIsolatedWorldId);

  Element* btn = GetDocument().getElementById(AtomicString("btn"));
  ASSERT_TRUE(btn);
  ASSERT_EQ(1, btn->NumberOfEventListeners(event_type_names::kClick));
  ASSERT_EQ(1, GetDocument().NumberOfEventListeners(
                   event_type_names::kSelectionchange));
  ASSERT_EQ(1, GetDocument().domWindow()->NumberOfEventListeners(
                   event_type_names::kResize));

  ClassicScript::CreateUnspecifiedScript(R"JS(
    document.getElementById('btn').removeAttribute('onclick');
    document.body.removeAttribute('onselectionchange');
    document.body.removeAttribute('onresize');
  )JS")
      ->RunScriptInIsolatedWorldAndReturnValue(GetDocument().domWindow(),
                                               kIsolatedWorldId);

  EXPECT_EQ(0, btn->NumberOfEventListeners(event_type_names::kClick));
  EXPECT_EQ(0, GetDocument().NumberOfEventListeners(
                   event_type_names::kSelectionchange));
  EXPECT_EQ(0, GetDocument().domWindow()->NumberOfEventListeners(
                   event_type_names::kResize));
}

TEST_F(EventTargetTest,
       DetachedDocumentRemoveAttributeDeactivatesContentAttributeEventHandler) {
  GetDocument().GetSettings()->SetScriptEnabled(true);

  // Detaching the iframe clears its document's ExecutionContext while leaving
  // listeners on already-disconnected nodes intact; removing the content
  // attribute afterwards must still remove the handler before adoption.
  ClassicScript::CreateUnspecifiedScript(R"JS(
    const iframe = document.createElement('iframe');
    document.body.appendChild(iframe);
    iframe.contentDocument.body.innerHTML =
        '<button id="detached-btn" onclick="1"></button>';
    const btn = iframe.contentDocument.getElementById('detached-btn');
    btn.remove();
    iframe.remove();
    btn.removeAttribute('onclick');
    document.body.appendChild(btn);
  )JS")
      ->RunScript(GetDocument().domWindow());

  Element* btn = GetDocument().getElementById(AtomicString("detached-btn"));
  ASSERT_TRUE(btn);
  EXPECT_EQ(0, btn->NumberOfEventListeners(event_type_names::kClick));
}

TEST_F(EventTargetTest,
       IsolatedWorldSetAttributeReplacesContentAttributeEventHandler) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  v8::HandleScope scope(GetDocument().domWindow()->GetIsolate());
  constexpr int kIsolatedWorldId = 1;

  ClassicScript::CreateUnspecifiedScript(R"JS(
    document.body.innerHTML =
        '<button id="btn" onclick="window.clickLog.push(1)"></button>';
    window.clickLog = [];
    document.getElementById('btn').addEventListener(
        'click', () => window.clickLog.push('after'));
  )JS")
      ->RunScript(GetDocument().domWindow());

  ClassicScript::CreateUnspecifiedScript(R"JS(
    const btn = document.getElementById('btn');
    btn.setAttribute('onclick', 'window.clickLog.push(2)');
    btn.setAttribute('onclick', 'window.clickLog.push(3)');
  )JS")
      ->RunScriptInIsolatedWorldAndReturnValue(GetDocument().domWindow(),
                                               kIsolatedWorldId);

  Element* btn = GetDocument().getElementById(AtomicString("btn"));
  ASSERT_TRUE(btn);
  EXPECT_EQ(2, btn->NumberOfEventListeners(event_type_names::kClick));

  ClassicScript::CreateUnspecifiedScript(R"JS(
    document.getElementById('btn').click();
    document.body.dataset.log = window.clickLog.join(',');
  )JS")
      ->RunScript(GetDocument().domWindow());

  EXPECT_EQ(AtomicString("3,after"),
            GetDocument().body()->getAttribute(AtomicString("data-log")));
}

TEST_F(EventTargetTest, IsolatedWorldAndMainWorldEventHandlerIsolation) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  v8::HandleScope scope(GetDocument().domWindow()->GetIsolate());
  constexpr int kIsolatedWorldId = 1;

  ClassicScript::CreateUnspecifiedScript(R"JS(
    window.mainLog = [];
    document.body.innerHTML =
        '<button id="btn" onclick="window.mainLog.push(1)"></button>';
  )JS")
      ->RunScript(GetDocument().domWindow());

  // Content attribute mutations (`removeAttribute` / `setAttribute`) only
  // update the main world's handler slot, whereas IDL `onclick` reads and
  // updates only the isolated world's handler slot.
  ClassicScript::CreateUnspecifiedScript(R"JS(
    const btn = document.getElementById('btn');
    document.body.dataset.initialIsolatedOnclick = String(btn.onclick);

    let isolatedCount = 0;
    btn.onclick = () => { ++isolatedCount; };

    btn.removeAttribute('onclick');
    btn.click();

    btn.setAttribute('onclick', 'window.mainLog.push(2)');
    btn.click();

    btn.onclick = null;
    btn.click();

    document.body.dataset.isolatedCount = String(isolatedCount);
  )JS")
      ->RunScriptInIsolatedWorldAndReturnValue(GetDocument().domWindow(),
                                               kIsolatedWorldId);

  ClassicScript::CreateUnspecifiedScript(R"JS(
    document.body.dataset.mainLog = window.mainLog.join(',');
  )JS")
      ->RunScript(GetDocument().domWindow());

  EXPECT_EQ(AtomicString("null"),
            GetDocument().body()->getAttribute(
                AtomicString("data-initial-isolated-onclick")));
  EXPECT_EQ(AtomicString("2"), GetDocument().body()->getAttribute(
                                   AtomicString("data-isolated-count")));
  EXPECT_EQ(AtomicString("2,2"),
            GetDocument().body()->getAttribute(AtomicString("data-main-log")));
}

}  // namespace blink

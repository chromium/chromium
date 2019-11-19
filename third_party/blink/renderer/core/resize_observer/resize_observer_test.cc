// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"

#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observation.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_controller.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

class TestResizeObserverDelegate : public ResizeObserver::Delegate {
 public:
  TestResizeObserverDelegate(Document& document)
      : document_(document), call_count_(0) {}
  void OnResize(
      const HeapVector<Member<ResizeObserverEntry>>& entries) override {
    call_count_++;
  }
  ExecutionContext* GetExecutionContext() const { return document_; }
  int CallCount() const { return call_count_; }

  void Trace(blink::Visitor* visitor) override {
    ResizeObserver::Delegate::Trace(visitor);
    visitor->Trace(document_);
  }

 private:
  Member<Document> document_;
  int call_count_;
};

}  // namespace

/* Testing:
 * getTargetSize
 * setTargetSize
 * oubservationSizeOutOfSync == false
 * modify target size
 * oubservationSizeOutOfSync == true
 */
class ResizeObserverUnitTest : public SimTest {};

TEST_F(ResizeObserverUnitTest, ResizeObservationSize) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  main_resource.Write(R"HTML(
    <div id='domTarget' style='width:100px;height:100px'>yo</div>
    <svg height='200' width='200'>
    <circle id='svgTarget' cx='100' cy='100' r='100'/>
    </svg>
  )HTML");
  main_resource.Finish();

  ResizeObserver::Delegate* delegate =
      MakeGarbageCollected<TestResizeObserverDelegate>(GetDocument());
  ResizeObserver* observer = ResizeObserver::Create(GetDocument(), delegate);
  Element* dom_target = GetDocument().getElementById("domTarget");
  Element* svg_target = GetDocument().getElementById("svgTarget");
  ResizeObservation* dom_observation =
      MakeGarbageCollected<ResizeObservation>(dom_target, observer);
  ResizeObservation* svg_observation =
      MakeGarbageCollected<ResizeObservation>(svg_target, observer);

  // Initial observation is out of sync
  ASSERT_TRUE(dom_observation->ObservationSizeOutOfSync());
  ASSERT_TRUE(svg_observation->ObservationSizeOutOfSync());

  // Target size is correct
  LayoutSize size = dom_observation->ComputeTargetSize();
  ASSERT_EQ(size.Width(), 100);
  ASSERT_EQ(size.Height(), 100);
  dom_observation->SetObservationSize(size);

  size = svg_observation->ComputeTargetSize();
  ASSERT_EQ(size.Width(), 200);
  ASSERT_EQ(size.Height(), 200);
  svg_observation->SetObservationSize(size);

  // Target size is in sync
  ASSERT_FALSE(dom_observation->ObservationSizeOutOfSync());

  // Target depths
  ASSERT_EQ(svg_observation->TargetDepth() - dom_observation->TargetDepth(),
            (size_t)1);
}

TEST_F(ResizeObserverUnitTest, TestMemoryLeaks) {
  ResizeObserverController& controller =
      GetDocument().EnsureResizeObserverController();
  const HeapHashSet<WeakMember<ResizeObserver>>& observers =
      controller.Observers();
  ASSERT_EQ(observers.size(), 0U);
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  ScriptController& script_controller =
      GetDocument().ExecutingFrame()->GetScriptController();

  //
  // Test whether ResizeObserver is kept alive by direct JS reference
  //
  script_controller.ExecuteScriptInMainWorldAndReturnValue(
      ScriptSourceCode("var ro = new ResizeObserver( entries => {});"), KURL(),
      SanitizeScriptErrors::kSanitize, ScriptFetchOptions(),
      ScriptController::kExecuteScriptWhenScriptsDisabled);
  ASSERT_EQ(observers.size(), 1U);
  script_controller.ExecuteScriptInMainWorldAndReturnValue(
      ScriptSourceCode("ro = undefined;"), KURL(),
      SanitizeScriptErrors::kSanitize, ScriptFetchOptions(),
      ScriptController::kExecuteScriptWhenScriptsDisabled);
  V8GCController::CollectAllGarbageForTesting(
      v8::Isolate::GetCurrent(),
      v8::EmbedderHeapTracer::EmbedderStackState::kEmpty);
  WebHeap::CollectAllGarbageForTesting();
  ASSERT_EQ(observers.IsEmpty(), true);

  //
  // Test whether ResizeObserver is kept alive by an Element
  //
  script_controller.ExecuteScriptInMainWorldAndReturnValue(
      ScriptSourceCode("var ro = new ResizeObserver( () => {});"
                       "var el = document.createElement('div');"
                       "ro.observe(el);"
                       "ro = undefined;"),
      KURL(), SanitizeScriptErrors::kSanitize, ScriptFetchOptions(),
      ScriptController::kExecuteScriptWhenScriptsDisabled);
  ASSERT_EQ(observers.size(), 1U);
  V8GCController::CollectAllGarbageForTesting(
      v8::Isolate::GetCurrent(),
      v8::EmbedderHeapTracer::EmbedderStackState::kEmpty);
  WebHeap::CollectAllGarbageForTesting();
  ASSERT_EQ(observers.size(), 1U);
  script_controller.ExecuteScriptInMainWorldAndReturnValue(
      ScriptSourceCode("el = undefined;"), KURL(),
      SanitizeScriptErrors::kSanitize, ScriptFetchOptions(),
      ScriptController::kExecuteScriptWhenScriptsDisabled);
  V8GCController::CollectAllGarbageForTesting(
      v8::Isolate::GetCurrent(),
      v8::EmbedderHeapTracer::EmbedderStackState::kEmpty);
  WebHeap::CollectAllGarbageForTesting();
  ASSERT_EQ(observers.IsEmpty(), true);
}

}  // namespace blink

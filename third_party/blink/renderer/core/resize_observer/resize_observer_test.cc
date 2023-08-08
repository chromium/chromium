// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"

#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_resize_observer_options.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observation.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_box_options.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_controller.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_size.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

class TestResizeObserverDelegate : public ResizeObserver::Delegate {
 public:
  explicit TestResizeObserverDelegate(LocalDOMWindow& window)
      : window_(window), call_count_(0) {}
  void OnResize(
      const HeapVector<Member<ResizeObserverEntry>>& entries) override {
    call_count_++;
  }
  ExecutionContext* GetExecutionContext() const { return window_.Get(); }
  int CallCount() const { return call_count_; }

  void Trace(Visitor* visitor) const override {
    ResizeObserver::Delegate::Trace(visitor);
    visitor->Trace(window_);
  }

 private:
  Member<LocalDOMWindow> window_;
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

TEST_F(ResizeObserverUnitTest, ResizeObserverDOMContentBoxAndSVG) {
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
      MakeGarbageCollected<TestResizeObserverDelegate>(Window());
  ResizeObserver* observer = ResizeObserver::Create(&Window(), delegate);
  Element* dom_target = GetDocument().getElementById(AtomicString("domTarget"));
  Element* svg_target = GetDocument().getElementById(AtomicString("svgTarget"));
  ResizeObservation* dom_observation = MakeGarbageCollected<ResizeObservation>(
      dom_target, observer, ResizeObserverBoxOptions::kContentBox);
  ResizeObservation* svg_observation = MakeGarbageCollected<ResizeObservation>(
      svg_target, observer, ResizeObserverBoxOptions::kContentBox);

  // Initial observation is out of sync
  ASSERT_TRUE(dom_observation->ObservationSizeOutOfSync());
  ASSERT_TRUE(svg_observation->ObservationSizeOutOfSync());

  // Target size is correct
  LogicalSize size = dom_observation->ComputeTargetSize();
  ASSERT_EQ(size.inline_size, 100);
  ASSERT_EQ(size.block_size, 100);
  dom_observation->SetObservationSize(size);

  size = svg_observation->ComputeTargetSize();
  ASSERT_EQ(size.inline_size, 200);
  ASSERT_EQ(size.block_size, 200);
  svg_observation->SetObservationSize(size);

  // Target size is in sync
  ASSERT_FALSE(dom_observation->ObservationSizeOutOfSync());
  ASSERT_FALSE(svg_observation->ObservationSizeOutOfSync());

  // Target depths
  ASSERT_EQ(svg_observation->TargetDepth() - dom_observation->TargetDepth(),
            (size_t)1);
}

TEST_F(ResizeObserverUnitTest, ResizeObserverDOMBorderBox) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  main_resource.Write(R"HTML(
    <div id='domBorderTarget' style='width:100px;height:100px;padding:5px'>
      yoyo
    </div>
  )HTML");
  main_resource.Finish();

  ResizeObserver::Delegate* delegate =
      MakeGarbageCollected<TestResizeObserverDelegate>(Window());
  ResizeObserver* observer = ResizeObserver::Create(&Window(), delegate);
  Element* dom_border_target =
      GetDocument().getElementById(AtomicString("domBorderTarget"));
  auto* dom_border_observation = MakeGarbageCollected<ResizeObservation>(
      dom_border_target, observer, ResizeObserverBoxOptions::kBorderBox);

  // Initial observation is out of sync
  ASSERT_TRUE(dom_border_observation->ObservationSizeOutOfSync());

  // Target size is correct
  LogicalSize size = dom_border_observation->ComputeTargetSize();
  ASSERT_EQ(size.inline_size, 110);
  ASSERT_EQ(size.block_size, 110);
  dom_border_observation->SetObservationSize(size);

  // Target size is in sync
  ASSERT_FALSE(dom_border_observation->ObservationSizeOutOfSync());
}

TEST_F(ResizeObserverUnitTest, ResizeObserverDOMDevicePixelContentBox) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  main_resource.Write(R"HTML(
    <div id='domTarget' style='width:100px;height:100px'>yo</div>
    <svg height='200' width='200'>
      <div style='zoom:3;'>
        <div id='domDPTarget' style='width:50px;height:30px'></div>
      </div>
    </svg>
  )HTML");
  main_resource.Finish();

  ResizeObserver::Delegate* delegate =
      MakeGarbageCollected<TestResizeObserverDelegate>(Window());
  ResizeObserver* observer = ResizeObserver::Create(&Window(), delegate);
  Element* dom_target = GetDocument().getElementById(AtomicString("domTarget"));
  Element* dom_dp_target =
      GetDocument().getElementById(AtomicString("domDPTarget"));

  auto* dom_dp_nested_observation = MakeGarbageCollected<ResizeObservation>(
      dom_dp_target, observer,
      ResizeObserverBoxOptions::kDevicePixelContentBox);
  auto* dom_dp_observation = MakeGarbageCollected<ResizeObservation>(
      dom_target, observer, ResizeObserverBoxOptions::kDevicePixelContentBox);

  // Initial observation is out of sync
  ASSERT_TRUE(dom_dp_observation->ObservationSizeOutOfSync());
  ASSERT_TRUE(dom_dp_nested_observation->ObservationSizeOutOfSync());

  // Target size is correct
  LogicalSize size = dom_dp_observation->ComputeTargetSize();
  ASSERT_EQ(size.inline_size, 100);
  ASSERT_EQ(size.block_size, 100);
  dom_dp_observation->SetObservationSize(size);

  size = dom_dp_nested_observation->ComputeTargetSize();
  ASSERT_EQ(size.inline_size, 150);
  ASSERT_EQ(size.block_size, 90);
  dom_dp_nested_observation->SetObservationSize(size);

  // Target size is in sync
  ASSERT_FALSE(dom_dp_observation->ObservationSizeOutOfSync());
  ASSERT_FALSE(dom_dp_nested_observation->ObservationSizeOutOfSync());
}

// Test whether a new observation is created when an observation's
// observed box is changed
TEST_F(ResizeObserverUnitTest, TestBoxOverwrite) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  main_resource.Write(R"HTML(
    <div id='domTarget' style='width:100px;height:100px'>yo</div>
    <svg height='200' width='200'>
    <circle id='svgTarget' cx='100' cy='100' r='100'/>
    </svg>
  )HTML");
  main_resource.Finish();

  ResizeObserverOptions* border_box_option = ResizeObserverOptions::Create();
  border_box_option->setBox("border-box");

  ResizeObserver::Delegate* delegate =
      MakeGarbageCollected<TestResizeObserverDelegate>(Window());
  ResizeObserver* observer = ResizeObserver::Create(&Window(), delegate);
  Element* dom_target = GetDocument().getElementById(AtomicString("domTarget"));

  // Assert no observations (depth returned is kDepthBottom)
  size_t min_observed_depth = ResizeObserverController::kDepthBottom;
  ASSERT_EQ(observer->GatherObservations(0), min_observed_depth);
  observer->observe(dom_target);

  // 3 is Depth of observed element
  ASSERT_EQ(observer->GatherObservations(0), (size_t)3);
  observer->observe(dom_target, border_box_option);
  // Active observations should be empty and GatherObservations should run
  ASSERT_EQ(observer->GatherObservations(0), (size_t)3);
}

// Test that default content rect, content box, and border box are created when
// a non box target's entry is made
TEST_F(ResizeObserverUnitTest, TestNonBoxTarget) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  main_resource.Write(R"HTML(
    <span id='domTarget'>yo</div>
  )HTML");
  main_resource.Finish();

  ResizeObserverOptions* border_box_option = ResizeObserverOptions::Create();
  border_box_option->setBox("border-box");

  Element* dom_target = GetDocument().getElementById(AtomicString("domTarget"));

  auto* entry = MakeGarbageCollected<ResizeObserverEntry>(dom_target);

  EXPECT_EQ(entry->contentRect()->width(), 0);
  EXPECT_EQ(entry->contentRect()->height(), 0);
  EXPECT_EQ(entry->contentBoxSize().at(0)->inlineSize(), 0);
  EXPECT_EQ(entry->contentBoxSize().at(0)->blockSize(), 0);
  EXPECT_EQ(entry->borderBoxSize().at(0)->inlineSize(), 0);
  EXPECT_EQ(entry->borderBoxSize().at(0)->blockSize(), 0);
  EXPECT_EQ(entry->devicePixelContentBoxSize().at(0)->inlineSize(), 0);
  EXPECT_EQ(entry->devicePixelContentBoxSize().at(0)->blockSize(), 0);
}

TEST_F(ResizeObserverUnitTest, TestMemoryLeaks) {
  ResizeObserverController& controller =
      *ResizeObserverController::From(Window());
  const HeapLinkedHashSet<WeakMember<ResizeObserver>>& observers =
      controller.Observers();
  ASSERT_EQ(observers.size(), 0U);

  //
  // Test whether ResizeObserver is kept alive by direct JS reference
  //
  ClassicScript::CreateUnspecifiedScript(
      "var ro = new ResizeObserver( entries => {});")
      ->RunScript(&Window());
  ASSERT_EQ(observers.size(), 1U);
  ClassicScript::CreateUnspecifiedScript("ro = undefined;")
      ->RunScript(&Window());
  ThreadState::Current()->CollectAllGarbageForTesting();
  WebHeap::CollectAllGarbageForTesting();
  ASSERT_EQ(observers.empty(), true);

  //
  // Test whether ResizeObserver is kept alive by an Element
  //
  ClassicScript::CreateUnspecifiedScript(
      "var ro = new ResizeObserver( () => {});"
      "var el = document.createElement('div');"
      "ro.observe(el);"
      "ro = undefined;")
      ->RunScript(&Window());
  ASSERT_EQ(observers.size(), 1U);
  ThreadState::Current()->CollectAllGarbageForTesting();
  WebHeap::CollectAllGarbageForTesting();
  ASSERT_EQ(observers.size(), 1U);
  ClassicScript::CreateUnspecifiedScript("el = undefined;")
      ->RunScript(&Window());
  ThreadState::Current()->CollectAllGarbageForTesting();
  WebHeap::CollectAllGarbageForTesting();
  ASSERT_EQ(observers.empty(), true);
}

}  // namespace blink

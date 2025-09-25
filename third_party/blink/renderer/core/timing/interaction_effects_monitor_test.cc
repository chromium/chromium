// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/interaction_effects_monitor.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_interaction_effects_monitor.h"
#include "third_party/blink/public/web/web_interaction_effects_monitor_observer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics_test_util.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

using TaskScope = scheduler::TaskAttributionTracker::TaskScope;
using TaskScopeType = scheduler::TaskAttributionTracker::TaskScopeType;
using EventScopeType = SoftNavigationHeuristics::EventScope::Type;

namespace {

class TestObserver : public WebInteractionEffectsMonitorObserver {
 public:
  TestObserver() = default;

  void OnContentfulPaint(uint64_t new_painted_area) override {
    total_painted_area_ += new_painted_area;
    ++num_contentful_paints_;
  }

  uint32_t NumContentfulPaints() const { return num_contentful_paints_; }
  uint64_t TotalPaintedArea() const { return total_painted_area_; }

 private:
  uint32_t num_contentful_paints_ = 0;
  uint64_t total_painted_area_ = 0;
};

}  // namespace

class InteractionEffectsMonitorTest : public testing::Test {
 protected:
  void SetUp() override {
    page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  }

  SoftNavigationHeuristics* GetSoftNavigationHeuristics() {
    return page_holder_->GetDocument()
        .domWindow()
        ->GetSoftNavigationHeuristics();
  }

  Document& GetDocument() { return page_holder_->GetDocument(); }

  LocalFrame& GetFrame() { return page_holder_->GetFrame(); }

  Node* CreateNodeForTest() {
    return GetDocument().CreateRawElement(html_names::kDivTag);
  }

  v8::Isolate* GetIsolate() {
    return GetDocument().GetExecutionContext()->GetIsolate();
  }

  ScriptState* GetScriptStateForTest() {
    return ToScriptStateForMainWorld(GetDocument().GetFrame());
  }

  // Simulates an interaction with the given `type`, returning the
  // `SoftNavigationContext` associated with the interaction if one was created.
  SoftNavigationContext* SimulateInteraction(
      EventScopeType type = EventScopeType::kClick,
      Node* node = nullptr) {
    auto* event =
        CreateEventForEventScopeType(type, GetScriptStateForTest(), node);
    std::optional<SoftNavigationHeuristics::EventScope> event_scope(
        GetSoftNavigationHeuristics()->MaybeCreateEventScopeForInputEvent(
            *event));
    scheduler::TaskAttributionInfo* task_state =
        scheduler::TaskAttributionTracker::From(GetIsolate())
            ->CurrentTaskState();
    return task_state ? task_state->GetSoftNavigationContext() : nullptr;
  }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> page_holder_;
};

TEST_F(InteractionEffectsMonitorTest, CreateMonitor) {
  auto* heuristics = GetSoftNavigationHeuristics();
  ASSERT_TRUE(heuristics);

  TestObserver observer;
  WebInteractionEffectsMonitor monitor(&GetFrame(), &observer);
  EXPECT_EQ(monitor.InteractionCount(), 0);

  SoftNavigationContext* context = SimulateInteraction();
  ASSERT_TRUE(context);
  EXPECT_EQ(monitor.InteractionCount(), 1);

  Node* node1 = CreateNodeForTest();
  context->AddPaintedArea(CreateTextRecordForTest(node1, 200, 50, context));
  heuristics->OnPaintFinished();
  EXPECT_EQ(observer.NumContentfulPaints(), 1u);
  EXPECT_EQ(observer.TotalPaintedArea(), 200u * 50u);
  EXPECT_EQ(observer.TotalPaintedArea(), monitor.TotalPaintedArea());

  Node* node2 = CreateNodeForTest();
  context->AddPaintedArea(CreateTextRecordForTest(node2, 100, 30, context));
  heuristics->OnPaintFinished();
  EXPECT_EQ(observer.NumContentfulPaints(), 2u);
  EXPECT_EQ(observer.TotalPaintedArea(), (200u * 50u) + (100u * 30u));
  EXPECT_EQ(observer.TotalPaintedArea(), monitor.TotalPaintedArea());
}

TEST_F(InteractionEffectsMonitorTest, CreateMonitorMultipleContextsSameFrame) {
  TestObserver observer;
  WebInteractionEffectsMonitor monitor(&GetFrame(), &observer);
  EXPECT_EQ(monitor.InteractionCount(), 0);
  {
    SoftNavigationContext* context = SimulateInteraction();
    EXPECT_EQ(monitor.InteractionCount(), 1);
    ASSERT_TRUE(context);
    Node* node = CreateNodeForTest();
    context->AddPaintedArea(CreateTextRecordForTest(node, 200, 50, context));
  }

  {
    SoftNavigationContext* context = SimulateInteraction();
    ASSERT_TRUE(context);
    EXPECT_EQ(monitor.InteractionCount(), 2);
    Node* node = CreateNodeForTest();
    context->AddPaintedArea(CreateTextRecordForTest(node, 100, 30, context));
  }

  EXPECT_EQ(observer.NumContentfulPaints(), 0u);
  EXPECT_EQ(observer.TotalPaintedArea(), 0u);

  GetSoftNavigationHeuristics()->OnPaintFinished();

  EXPECT_EQ(observer.NumContentfulPaints(), 2u);
  EXPECT_EQ(observer.TotalPaintedArea(), (200u * 50u) + (100u * 30u));
  EXPECT_EQ(observer.TotalPaintedArea(), monitor.TotalPaintedArea());
}

TEST_F(InteractionEffectsMonitorTest, ObserverStopsOnMonitorDestruction) {
  auto* heuristics = GetSoftNavigationHeuristics();
  ASSERT_TRUE(heuristics);

  TestObserver observer;
  {
    WebInteractionEffectsMonitor monitor(&GetFrame(), &observer);
    EXPECT_EQ(monitor.InteractionCount(), 0);

    SoftNavigationContext* context = SimulateInteraction();
    ASSERT_TRUE(context);
    EXPECT_EQ(monitor.InteractionCount(), 1);
    Node* node = CreateNodeForTest();
    context->AddPaintedArea(CreateTextRecordForTest(node, 100, 10, context));

    heuristics->OnPaintFinished();
    EXPECT_EQ(observer.NumContentfulPaints(), 1u);
    EXPECT_EQ(observer.TotalPaintedArea(), 1000u);
    EXPECT_EQ(observer.TotalPaintedArea(), monitor.TotalPaintedArea());
  }

  SoftNavigationContext* context = SimulateInteraction();
  ASSERT_TRUE(context);
  Node* node = CreateNodeForTest();
  context->AddPaintedArea(CreateTextRecordForTest(node, 1000, 20, context));

  heuristics->OnPaintFinished();
  EXPECT_EQ(observer.NumContentfulPaints(), 1u);
  EXPECT_EQ(observer.TotalPaintedArea(), 1000u);
}

TEST_F(InteractionEffectsMonitorTest, SubsequentObservers) {
  auto* heuristics = GetSoftNavigationHeuristics();
  ASSERT_TRUE(heuristics);

  for (int i = 0; i < 5; ++i) {
    TestObserver observer;
    WebInteractionEffectsMonitor monitor(&GetFrame(), &observer);
    EXPECT_EQ(monitor.InteractionCount(), 0);
    SoftNavigationContext* context = SimulateInteraction();
    ASSERT_TRUE(context);
    EXPECT_EQ(monitor.InteractionCount(), 1);

    Node* node = CreateNodeForTest();
    context->AddPaintedArea(CreateTextRecordForTest(node, 100, 10, context));
    heuristics->OnPaintFinished();
    EXPECT_EQ(observer.NumContentfulPaints(), 1u);
    EXPECT_EQ(observer.TotalPaintedArea(), 1000u);
    EXPECT_EQ(observer.TotalPaintedArea(), monitor.TotalPaintedArea());
  }
}

TEST_F(InteractionEffectsMonitorTest, ObserveNonBodyKeyEvents) {
  TestObserver observer;
  WebInteractionEffectsMonitor monitor(&GetFrame(), &observer);
  EXPECT_EQ(monitor.InteractionCount(), 0);

  Node* node = CreateNodeForTest();
  SoftNavigationContext* context =
      SimulateInteraction(EventScopeType::kKeydown, node);
  ASSERT_TRUE(context);
  EXPECT_EQ(monitor.InteractionCount(), 1);

  context->AddPaintedArea(CreateTextRecordForTest(node, 100, 10, context));
  GetSoftNavigationHeuristics()->OnPaintFinished();
  EXPECT_EQ(observer.NumContentfulPaints(), 1u);
  EXPECT_EQ(observer.TotalPaintedArea(), 1000u);
  EXPECT_EQ(observer.TotalPaintedArea(), monitor.TotalPaintedArea());
}

TEST_F(InteractionEffectsMonitorTest, NewInteractionsOnly) {
  SoftNavigationContext* context = SimulateInteraction();
  ASSERT_TRUE(context);

  TestObserver observer;
  WebInteractionEffectsMonitor monitor(&GetFrame(), &observer);

  Node* node = CreateNodeForTest();
  context->AddPaintedArea(CreateTextRecordForTest(node, 100, 10, context));
  GetSoftNavigationHeuristics()->OnPaintFinished();
  EXPECT_EQ(monitor.InteractionCount(), 0);
  EXPECT_EQ(observer.NumContentfulPaints(), 0u);
  EXPECT_EQ(observer.TotalPaintedArea(), 0u);
  EXPECT_EQ(monitor.TotalPaintedArea(), 0u);
}

TEST_F(InteractionEffectsMonitorTest, ConcurrentMonitors) {
  auto* monitor1 = MakeGarbageCollected<InteractionEffectsMonitor>(
      GetSoftNavigationHeuristics());
  TestObserver observer1;

  auto* monitor2 = MakeGarbageCollected<InteractionEffectsMonitor>(
      GetSoftNavigationHeuristics());
  TestObserver observer2;

  auto* heuristics = GetSoftNavigationHeuristics();
  ASSERT_TRUE(heuristics);

  // Initially, only `monitor1` is monitoring interactions.
  monitor1->StartMonitoring(&observer1);

  SoftNavigationContext* context = SimulateInteraction();
  ASSERT_TRUE(context);

  Node* node = CreateNodeForTest();
  context->AddPaintedArea(CreateTextRecordForTest(node, 100, 10, context));
  heuristics->OnPaintFinished();

  EXPECT_EQ(observer1.NumContentfulPaints(), 1u);
  EXPECT_EQ(observer1.TotalPaintedArea(), 1000u);
  EXPECT_EQ(monitor1->InteractionCount(), 1);
  EXPECT_EQ(monitor1->TotalPaintedArea(), 1000u);

  EXPECT_EQ(observer2.NumContentfulPaints(), 0u);
  EXPECT_EQ(observer2.TotalPaintedArea(), 0u);
  EXPECT_EQ(monitor2->InteractionCount(), 0);
  EXPECT_EQ(monitor2->TotalPaintedArea(), 0u);

  // Add a second monitor, which should only observe new interactions.
  monitor2->StartMonitoring(&observer2);

  context->AddPaintedArea(CreateTextRecordForTest(node, 100, 10, context));
  heuristics->OnPaintFinished();

  EXPECT_EQ(observer1.NumContentfulPaints(), 2u);
  EXPECT_EQ(observer1.TotalPaintedArea(), 2000u);
  EXPECT_EQ(monitor1->InteractionCount(), 1);
  EXPECT_EQ(monitor1->TotalPaintedArea(), 2000u);

  EXPECT_EQ(observer2.NumContentfulPaints(), 0u);
  EXPECT_EQ(observer2.TotalPaintedArea(), 0u);
  EXPECT_EQ(monitor2->InteractionCount(), 0);
  EXPECT_EQ(monitor2->TotalPaintedArea(), 0u);

  // Simulate a second interaction, which both monitors should observe.
  context = SimulateInteraction();
  ASSERT_TRUE(context);
  context->AddPaintedArea(CreateTextRecordForTest(node, 100, 10, context));
  heuristics->OnPaintFinished();

  EXPECT_EQ(observer1.NumContentfulPaints(), 3u);
  EXPECT_EQ(observer1.TotalPaintedArea(), 3000u);
  EXPECT_EQ(monitor1->InteractionCount(), 2);
  EXPECT_EQ(monitor1->TotalPaintedArea(), 3000u);

  EXPECT_EQ(observer2.NumContentfulPaints(), 1u);
  EXPECT_EQ(observer2.TotalPaintedArea(), 1000u);
  EXPECT_EQ(monitor2->InteractionCount(), 1);
  EXPECT_EQ(monitor2->TotalPaintedArea(), 1000u);

  // Remove the first monitor, and simulate a new interaction which only the
  // second monitor should observe.
  monitor1->StopMonitoring();

  context = SimulateInteraction();
  ASSERT_TRUE(context);
  context->AddPaintedArea(CreateTextRecordForTest(node, 100, 10, context));
  heuristics->OnPaintFinished();

  EXPECT_EQ(observer1.NumContentfulPaints(), 3u);
  EXPECT_EQ(observer1.TotalPaintedArea(), 3000u);
  EXPECT_EQ(monitor1->InteractionCount(), 2);
  EXPECT_EQ(monitor1->TotalPaintedArea(), 3000u);

  EXPECT_EQ(observer2.NumContentfulPaints(), 2u);
  EXPECT_EQ(observer2.TotalPaintedArea(), 2000u);
  EXPECT_EQ(monitor1->InteractionCount(), 2);
  EXPECT_EQ(monitor2->TotalPaintedArea(), 2000u);

  monitor2->StopMonitoring();
}

TEST_F(InteractionEffectsMonitorTest, RestartMonitor) {
  auto* heuristics = GetSoftNavigationHeuristics();
  ASSERT_TRUE(heuristics);

  auto* monitor = MakeGarbageCollected<InteractionEffectsMonitor>(
      GetSoftNavigationHeuristics());
  TestObserver observer;
  monitor->StartMonitoring(&observer);
  EXPECT_EQ(monitor->InteractionCount(), 0);

  Node* node = CreateNodeForTest();

  SoftNavigationContext* context1 = SimulateInteraction();
  ASSERT_TRUE(context1);
  context1->AddPaintedArea(CreateTextRecordForTest(node, 100, 10, context1));
  heuristics->OnPaintFinished();

  EXPECT_EQ(observer.NumContentfulPaints(), 1u);
  EXPECT_EQ(observer.TotalPaintedArea(), 1000u);
  EXPECT_EQ(monitor->InteractionCount(), 1);
  EXPECT_EQ(monitor->TotalPaintedArea(), 1000u);

  monitor->StopMonitoring();

  SoftNavigationContext* context2 = SimulateInteraction();
  ASSERT_TRUE(context2);
  context2->AddPaintedArea(CreateTextRecordForTest(node, 100, 10, context2));
  heuristics->OnPaintFinished();

  EXPECT_EQ(observer.NumContentfulPaints(), 1u);
  EXPECT_EQ(observer.TotalPaintedArea(), 1000u);
  EXPECT_EQ(monitor->InteractionCount(), 1);
  EXPECT_EQ(monitor->TotalPaintedArea(), 1000u);

  // Start monitoring again. Only the new interaction should be considered.
  monitor->StartMonitoring(&observer);

  SoftNavigationContext* context3 = SimulateInteraction();
  ASSERT_TRUE(context3);
  context1->AddPaintedArea(CreateTextRecordForTest(node, 50, 5, context1));
  context2->AddPaintedArea(CreateTextRecordForTest(node, 30, 10, context2));
  context3->AddPaintedArea(CreateTextRecordForTest(node, 20, 5, context3));
  heuristics->OnPaintFinished();

  // `observer` is cumulative since we didn't reset it, but `monitor` starts
  // over when restarting it.
  EXPECT_EQ(observer.NumContentfulPaints(), 2u);
  EXPECT_EQ(observer.TotalPaintedArea(), 1100u);
  EXPECT_EQ(monitor->InteractionCount(), 2);
  EXPECT_EQ(monitor->TotalPaintedArea(), 100u);

  monitor->StopMonitoring();
}

}  // namespace blink

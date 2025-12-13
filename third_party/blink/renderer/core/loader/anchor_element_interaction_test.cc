// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <optional>
#include <ranges>
#include <string_view>
#include <tuple>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/with_feature_override.h"
#include "base/time/time.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/mojom/preloading/anchor_element_interaction_host.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/anchor_element_metrics_sender.h"
#include "third_party/blink/renderer/core/html/anchor_element_viewport_position_tracker.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/loader/anchor_element_interaction_tracker.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

namespace {

enum class PointerEventType {
  kNone,
  kOnPointerDown,
  kOnPointerHover,
};

class MockAnchorElementInteractionHost
    : public mojom::blink::AnchorElementInteractionHost {
 public:
  explicit MockAnchorElementInteractionHost(
      mojo::PendingReceiver<mojom::blink::AnchorElementInteractionHost>
          pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
  }

  struct Call {
    KURL url;
    PointerEventType type = PointerEventType::kNone;
    std::optional<bool> is_mouse_pointer;
    std::optional<double> mouse_velocity;
    std::optional<bool> is_eager;
  };
  std::vector<Call> calls_;

 private:
  void OnPointerDown(const KURL& target) override {
    calls_.push_back({.url = target, .type = PointerEventType::kOnPointerDown});
  }
  void OnPointerHoverEager(
      const KURL& target,
      mojom::blink::AnchorElementPointerDataPtr mouse_data) override {
    calls_.push_back({.url = target,
                      .type = PointerEventType::kOnPointerHover,
                      .is_mouse_pointer = mouse_data->is_mouse_pointer,
                      .mouse_velocity = mouse_data->mouse_velocity,
                      .is_eager = true});
  }
  void OnPointerHoverModerate(
      const KURL& target,
      mojom::blink::AnchorElementPointerDataPtr mouse_data) override {
    calls_.push_back({.url = target,
                      .type = PointerEventType::kOnPointerHover,
                      .is_mouse_pointer = mouse_data->is_mouse_pointer,
                      .mouse_velocity = mouse_data->mouse_velocity,
                      .is_eager = false});
  }
  void OnModerateViewportHeuristicTriggered(const KURL& target) override {
    calls_.push_back(
        {.url = target, .type = PointerEventType::kNone, .is_eager = false});
  }
  void OnEagerViewportHeuristicTriggered(const Vector<KURL>& targets) override {
    for (const KURL& url : targets) {
      calls_.push_back(
          {.url = url, .type = PointerEventType::kNone, .is_eager = true});
    }
  }

 private:
  mojo::Receiver<mojom::blink::AnchorElementInteractionHost> receiver_{this};
};

class AnchorElementInteractionTest : public SimTest {
 protected:
  void SetUp() override {
    SimTest::SetUp();

    MainFrame().GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::AnchorElementInteractionHost::Name_,
        BindRepeating(&AnchorElementInteractionTest::Bind, Unretained(this)));
    WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 400));

    // Check our invariant about dwell times, otherwise tests that use them
    // (and some production code) will not make sense.
    ASSERT_LT(AnchorElementInteractionTracker::EagerHoverDwellTime(),
              AnchorElementInteractionTracker::kModerateHoverDwellTime);
  }

  void TearDown() override {
    MainFrame().GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::AnchorElementInteractionHost::Name_, {});
    hosts_.clear();
    SimTest::TearDown();
  }

  void Bind(mojo::ScopedMessagePipeHandle message_pipe_handle) {
    auto host = std::make_unique<MockAnchorElementInteractionHost>(
        mojo::PendingReceiver<mojom::blink::AnchorElementInteractionHost>(
            std::move(message_pipe_handle)));
    hosts_.push_back(std::move(host));
  }

  void SendMouseDownEvent() {
    gfx::PointF coordinates(100, 100);
    WebMouseEvent event(WebInputEvent::Type::kMouseDown, coordinates,
                        coordinates, WebPointerProperties::Button::kLeft, 0,
                        WebInputEvent::kLeftButtonDown,
                        WebInputEvent::GetStaticTimeStampForTests());
    GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(event);
  }

  base::TimeDelta GetShorterThanEagerHoverDwellTime() {
    return AnchorElementInteractionTracker::EagerHoverDwellTime() * 0.5;
  }
  base::TimeDelta GetBetweenEagerAndModerateHoverDwellTime() {
    return (AnchorElementInteractionTracker::EagerHoverDwellTime() +
            AnchorElementInteractionTracker::kModerateHoverDwellTime) /
           2;
  }
  base::TimeDelta GetLongerThanModerateHoverDwellTime() {
    return AnchorElementInteractionTracker::kModerateHoverDwellTime * 1.5;
  }

  std::vector<std::unique_ptr<MockAnchorElementInteractionHost>> hosts_;
};

TEST_F(AnchorElementInteractionTest, SingleAnchor) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.example/'>
      <div style='padding: 0px; width: 400px; height: 400px;'></div>
    </a>
  )HTML");
  SendMouseDownEvent();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(hosts_.size(), 1u);
  EXPECT_GE(hosts_[0]->calls_.size(), 1u);
  EXPECT_EQ(hosts_[0]->calls_[0].url, KURL("https://anchor1.example/"));
  EXPECT_EQ(hosts_[0]->calls_[0].type, PointerEventType::kOnPointerDown);
}

class AnchorElementInteractionFragmentTest
    : public base::test::WithFeatureOverride,
      public AnchorElementInteractionTest {
 public:
  AnchorElementInteractionFragmentTest()
      : base::test::WithFeatureOverride(
            blink::kPreloadingNoSamePageFragmentAnchorTracking) {}
};

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(AnchorElementInteractionFragmentTest);

TEST_P(AnchorElementInteractionFragmentTest, SamePageFragment) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='#foo'>
      <div style='padding: 0px; width: 400px; height: 400px;'></div>
    </a>
  )HTML");
  SendMouseDownEvent();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(hosts_.size(), 1u);
  if (IsParamFeatureEnabled()) {
    EXPECT_EQ(hosts_[0]->calls_.size(), 0u);
  } else {
    ASSERT_GE(hosts_[0]->calls_.size(), 1u);
    EXPECT_EQ(hosts_[0]->calls_[0].url, KURL("https://example.com/p1#foo"));
    EXPECT_EQ(hosts_[0]->calls_[0].type, PointerEventType::kOnPointerDown);
  }
}

TEST_P(AnchorElementInteractionFragmentTest, OtherPageFragment) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='bar.html#foo'>
      <div style='padding: 0px; width: 400px; height: 400px;'></div>
    </a>
  )HTML");
  SendMouseDownEvent();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(hosts_.size(), 1u);
  ASSERT_GE(hosts_[0]->calls_.size(), 1u);
  EXPECT_EQ(hosts_[0]->calls_[0].url, KURL("https://example.com/bar.html#foo"));

  EXPECT_EQ(hosts_[0]->calls_[0].type, PointerEventType::kOnPointerDown);
}

TEST_F(AnchorElementInteractionTest, InvalidHref) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='about:blank'>
      <div style='padding: 0px; width: 400px; height: 400px;'></div>
    </a>
  )HTML");
  SendMouseDownEvent();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(hosts_.size(), 1u);
  EXPECT_EQ(hosts_[0]->calls_.size(), 0u);
}

TEST_F(AnchorElementInteractionTest, RightClick) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.example/'>
      <div style='padding: 0px; width: 400px; height: 400px;'></div>
    </a>
  )HTML");
  gfx::PointF coordinates(100, 100);
  WebMouseEvent event(WebInputEvent::Type::kMouseDown, coordinates, coordinates,
                      WebPointerProperties::Button::kLeft, 0,
                      WebInputEvent::kRightButtonDown,
                      WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(event);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(hosts_.size(), 1u);
  for (const auto& call : hosts_[0]->calls_) {
    EXPECT_NE(call.type, PointerEventType::kOnPointerDown);
  }
}

TEST_F(AnchorElementInteractionTest, NestedAnchorElementCheck) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.example/'>
      <a href='https://anchor2.example/'>
        <div style='padding: 0px; width: 400px; height: 400px;'></div>
      </a>
    </a>
  )HTML");
  SendMouseDownEvent();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(hosts_.size(), 1u);
  ASSERT_GE(hosts_[0]->calls_.size(), 1u);
  EXPECT_EQ(hosts_[0]->calls_[0].url, KURL("https://anchor2.example/"));
  EXPECT_EQ(hosts_[0]->calls_[0].type, PointerEventType::kOnPointerDown);
}

TEST_F(AnchorElementInteractionTest, SiblingAnchorElements) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.example/'>
        <div style='padding: 0px; width: 400px; height: 400px;'></div>
    </a>
    <a href='https://anchor2.example/'>
        <div style='padding: 0px; width: 400px; height: 400px;'></div>
    </a>
  )HTML");
  SendMouseDownEvent();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(hosts_.size(), 1u);
  ASSERT_GE(hosts_[0]->calls_.size(), 1u);
  EXPECT_EQ(hosts_[0]->calls_[0].url, KURL("https://anchor1.example/"));
  EXPECT_EQ(hosts_[0]->calls_[0].type, PointerEventType::kOnPointerDown);
}

TEST_F(AnchorElementInteractionTest, NoAnchorElement) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <div style='padding: 0px; width: 400px; height: 400px;'></div>
  )HTML");
  SendMouseDownEvent();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(hosts_.size(), 1u);
  EXPECT_EQ(hosts_[0]->calls_.size(), 0u);
}

TEST_F(AnchorElementInteractionTest, TouchEvent) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.example/'>
      <div style='padding: 0px; width: 400px; height: 400px;'></div>
    </a>
  )HTML");

  WebPointerEvent event(
      WebInputEvent::Type::kPointerDown,
      WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                           WebPointerProperties::Button::kLeft,
                           gfx::PointF(100, 100), gfx::PointF(100, 100)),
      1, 1);
  GetDocument().GetFrame()->GetEventHandler().HandlePointerEvent(
      event, Vector<WebPointerEvent>(), Vector<WebPointerEvent>());
  GetDocument().GetFrame()->GetEventHandler().DispatchBufferedTouchEvents();

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(hosts_.size(), 1u);
  ASSERT_GE(hosts_[0]->calls_.size(), 1u);
  EXPECT_EQ(hosts_[0]->calls_[0].url, KURL("https://anchor1.example/"));
  EXPECT_EQ(hosts_[0]->calls_[0].type, PointerEventType::kOnPointerDown);
}

TEST_F(AnchorElementInteractionTest, DestroyedContext) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.example/'>
      <div style='padding: 0px; width: 400px; height: 400px;'></div>
    </a>
  )HTML");

  // Make sure getting pointer events after the execution context has been
  // destroyed but before the document has been destroyed doesn't cause a crash.
  GetDocument().GetExecutionContext()->NotifyContextDestroyed();
  WebPointerEvent event(
      WebInputEvent::Type::kPointerDown,
      WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                           WebPointerProperties::Button::kLeft,
                           gfx::PointF(100, 100), gfx::PointF(100, 100)),
      1, 1);
  GetDocument().GetFrame()->GetEventHandler().HandlePointerEvent(
      event, Vector<WebPointerEvent>(), Vector<WebPointerEvent>());
  GetDocument().GetFrame()->GetEventHandler().DispatchBufferedTouchEvents();

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(hosts_.size(), 1u);
  EXPECT_EQ(hosts_[0]->calls_.size(), 0u);
}

TEST_F(AnchorElementInteractionTest, ShorterThanEagerMouseHover) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.example/'>
      <div style='padding: 0px; width: 400px; height: 400px;'></div>
    </a>
  )HTML");

  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  GetDocument().GetAnchorElementInteractionTracker()->SetTaskRunnerForTesting(
      task_runner, task_runner->GetMockTickClock());

  gfx::PointF coordinates(100, 100);
  WebMouseEvent event(WebInputEvent::Type::kMouseMove, coordinates, coordinates,
                      WebPointerProperties::Button::kNoButton, 0,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  // Wait for hover logic to process the event
  task_runner->AdvanceTimeAndRun(GetShorterThanEagerHoverDwellTime());
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(hosts_.size(), 1u);
  EXPECT_EQ(hosts_[0]->calls_.size(), 0u);
}

class AnchorElementInteractionEagerHeuristicsTest
    : public base::test::WithFeatureOverride,
      public AnchorElementInteractionTest {
 public:
  AnchorElementInteractionEagerHeuristicsTest()
      : base::test::WithFeatureOverride(
            blink::features::kPreloadingEagerHoverHeuristics) {}
};

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    AnchorElementInteractionEagerHeuristicsTest);

TEST_P(AnchorElementInteractionEagerHeuristicsTest,
       BetweenEagerAndModerateMouseHover) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.example/'>
      <div style='padding: 0px; width: 400px; height: 400px;'></div>
    </a>
  )HTML");

  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  GetDocument().GetAnchorElementInteractionTracker()->SetTaskRunnerForTesting(
      task_runner, task_runner->GetMockTickClock());

  gfx::PointF coordinates(100, 100);
  WebMouseEvent event(WebInputEvent::Type::kMouseMove, coordinates, coordinates,
                      WebPointerProperties::Button::kNoButton, 0,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  // Wait for hover logic to process the event
  task_runner->AdvanceTimeAndRun(GetBetweenEagerAndModerateHoverDwellTime());
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(hosts_.size(), 1u);
  if (IsParamFeatureEnabled()) {
    ASSERT_EQ(hosts_[0]->calls_.size(), 1u);
    EXPECT_EQ(hosts_[0]->calls_[0].url, KURL("https://anchor1.example/"));
    EXPECT_EQ(hosts_[0]->calls_[0].type, PointerEventType::kOnPointerHover);
    EXPECT_TRUE(hosts_[0]->calls_[0].is_mouse_pointer.value());
    EXPECT_TRUE(hosts_[0]->calls_[0].mouse_velocity.has_value());
    EXPECT_TRUE(hosts_[0]->calls_[0].is_eager.value());
  } else {
    EXPECT_EQ(hosts_[0]->calls_.size(), 0u);
  }
}

TEST_P(AnchorElementInteractionEagerHeuristicsTest,
       LongerThanModerateMouseHover) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.example/'>
      <div style='padding: 0px; width: 400px; height: 400px;'></div>
    </a>
  )HTML");

  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  GetDocument().GetAnchorElementInteractionTracker()->SetTaskRunnerForTesting(
      task_runner, task_runner->GetMockTickClock());

  gfx::PointF coordinates(100, 100);
  WebMouseEvent event(WebInputEvent::Type::kMouseMove, coordinates, coordinates,
                      WebPointerProperties::Button::kNoButton, 0,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  // Wait for eager hover logic to process the event.
  task_runner->AdvanceTimeAndRun(GetBetweenEagerAndModerateHoverDwellTime());
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(hosts_.size(), 1u);

  if (IsParamFeatureEnabled()) {
    ASSERT_EQ(hosts_[0]->calls_.size(), 1u);
    EXPECT_EQ(hosts_[0]->calls_[0].url, KURL("https://anchor1.example/"));
    EXPECT_EQ(hosts_[0]->calls_[0].type, PointerEventType::kOnPointerHover);
    EXPECT_TRUE(hosts_[0]->calls_[0].is_mouse_pointer.value());
    EXPECT_TRUE(hosts_[0]->calls_[0].mouse_velocity.has_value());
    EXPECT_TRUE(hosts_[0]->calls_[0].is_eager.value());
  } else {
    EXPECT_EQ(hosts_[0]->calls_.size(), 0u);
  }

  // Wait for moderate hover logic to process the event.
  task_runner->AdvanceTimeAndRun(GetLongerThanModerateHoverDwellTime());
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(hosts_[0]->calls_.size(), IsParamFeatureEnabled() ? 2u : 1u);
  const MockAnchorElementInteractionHost::Call& final_call =
      hosts_[0]->calls_.back();
  EXPECT_EQ(final_call.url, KURL("https://anchor1.example/"));
  EXPECT_EQ(final_call.type, PointerEventType::kOnPointerHover);
  EXPECT_TRUE(final_call.is_mouse_pointer.value());
  EXPECT_TRUE(final_call.mouse_velocity.has_value());
  EXPECT_FALSE(final_call.is_eager.value());
}

TEST_F(AnchorElementInteractionTest,
       MousePointerEnterAndLeaveShorterThanEager) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.example/'>
      <div style='padding: 0px; width: 400px; height: 400px;'></div>
    </a>
  )HTML");

  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  GetDocument().GetAnchorElementInteractionTracker()->SetTaskRunnerForTesting(
      task_runner, task_runner->GetMockTickClock());

  // If mouse does not hover long enough over a link, it should be ignored.
  gfx::PointF coordinates(100, 100);
  WebMouseEvent mouse_enter_event(
      WebInputEvent::Type::kMouseMove, coordinates, coordinates,
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_enter_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  task_runner->AdvanceTimeAndRun(GetShorterThanEagerHoverDwellTime());

  WebMouseEvent mouse_leave_event(
      WebInputEvent::Type::kMouseLeave, coordinates, coordinates,
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseLeaveEvent(
      mouse_leave_event);

  // Wait for hover logic to process the event
  task_runner->AdvanceTimeAndRun(GetShorterThanEagerHoverDwellTime());

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(hosts_.size(), 1u);
  EXPECT_EQ(hosts_[0]->calls_.size(), 0u);
}

TEST_F(AnchorElementInteractionTest, MouseMotionEstimatorUnitTest) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* motion_estimator = MakeGarbageCollected<
      AnchorElementInteractionTracker::MouseMotionEstimator>(task_runner);
  motion_estimator->SetTaskRunnerForTesting(task_runner,
                                            task_runner->GetMockTickClock());

  double t = 0.0;
  double x0 = 100.0, y0 = 100.0;
  double vx0 = -5.0, vy0 = 4.0;
  double ax = 100, ay = -200;
  int i = 0;
  // Estimation error tolerance is set to 1%.
  constexpr double eps = 1e-2;
  for (double dt : {0.0, 1.0, 5.0, 15.0, 30.0, 7.0, 200.0, 50.0, 100.0, 27.0}) {
    i++;
    t += 0.001 * dt;  // `dt` is in milliseconds and `t` is in seconds.
    double x = 0.5 * ax * t * t + vx0 * t + x0;
    double y = 0.5 * ay * t * t + vy0 * t + y0;
    double vx = ax * t + vx0;
    double vy = ay * t + vy0;
    task_runner->AdvanceTimeAndRun(base::Milliseconds(dt));
    motion_estimator->OnMouseMoveEvent(
        gfx::PointF{static_cast<float>(x), static_cast<float>(y)});
    if (i == 1) {
      EXPECT_DOUBLE_EQ(0.0, motion_estimator->GetMouseAcceleration().x());
      EXPECT_DOUBLE_EQ(0.0, motion_estimator->GetMouseAcceleration().y());
      EXPECT_DOUBLE_EQ(0.0, motion_estimator->GetMouseVelocity().x());
      EXPECT_DOUBLE_EQ(0.0, motion_estimator->GetMouseVelocity().y());
    } else if (i == 2) {
      EXPECT_DOUBLE_EQ(0.0, motion_estimator->GetMouseAcceleration().x());
      EXPECT_DOUBLE_EQ(0.0, motion_estimator->GetMouseAcceleration().y());
      EXPECT_NEAR(1.0,
                  motion_estimator->GetMouseVelocity().x() /
                      /*vx0+0.5*ax*t=-5.0+0.5*0.1=*/-4.95,
                  eps);
      EXPECT_NEAR(1.0,
                  motion_estimator->GetMouseVelocity().y() /
                      /*vy0+0.5*ay*t=4.0+0.5*(-0.2)=*/3.9,
                  eps);
    } else {
      EXPECT_NEAR(1.0, motion_estimator->GetMouseAcceleration().x() / ax, eps);
      EXPECT_NEAR(1.0, motion_estimator->GetMouseAcceleration().y() / ay, eps);
      EXPECT_NEAR(1.0, motion_estimator->GetMouseVelocity().x() / vx, eps);
      EXPECT_NEAR(1.0, motion_estimator->GetMouseVelocity().y() / vy, eps);
    }
  }

  // Waiting a long time should empty the dequeue.
  EXPECT_FALSE(motion_estimator->IsEmpty());
  task_runner->AdvanceTimeAndRun(base::Seconds(10));
  EXPECT_TRUE(motion_estimator->IsEmpty());

  // Testing `GetMouseTangentialAcceleration` method.
  motion_estimator->SetMouseAccelerationForTesting(gfx::Vector2dF(1.0, 0.0));
  motion_estimator->SetMouseVelocityForTesting(gfx::Vector2dF(0.0, 1.0));
  EXPECT_NEAR(1.0, motion_estimator->GetMouseVelocity().Length(), 1e-6);
  EXPECT_NEAR(0.0, motion_estimator->GetMouseTangentialAcceleration(), 1e-6);

  motion_estimator->SetMouseAccelerationForTesting(gfx::Vector2dF(1.0, -1.0));
  motion_estimator->SetMouseVelocityForTesting(gfx::Vector2dF(-1.0, 1.0));
  EXPECT_NEAR(std::sqrt(2.0), motion_estimator->GetMouseVelocity().Length(),
              1e-6);
  EXPECT_NEAR(-std::sqrt(2.0),
              motion_estimator->GetMouseTangentialAcceleration(), 1e-6);
}

TEST_F(AnchorElementInteractionTest,
       MouseMotionEstimatorWithVariableAcceleration) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* motion_estimator = MakeGarbageCollected<
      AnchorElementInteractionTracker::MouseMotionEstimator>(task_runner);
  motion_estimator->SetTaskRunnerForTesting(task_runner,
                                            task_runner->GetMockTickClock());

  double t = 0.0;
  double x0 = 100.0, y0 = 100.0;
  double vx0 = 0.0, vy0 = 0.0;
  double ax, ay;
  const double dt = 5.0;
  // Estimation error tolerance is set to 1%.
  constexpr double eps = 1e-2;
  for (int i = 1; i <= 10; i++, t += 0.001 * dt) {
    ax = 100 * std::cos(t);
    ay = -200 * std::cos(t);
    double x = 0.5 * ax * t * t + vx0 * t + x0;
    double y = 0.5 * ay * t * t + vy0 * t + y0;
    double vx = ax * t + vx0;
    double vy = ay * t + vy0;

    task_runner->AdvanceTimeAndRun(base::Milliseconds(dt));
    motion_estimator->OnMouseMoveEvent(
        gfx::PointF{static_cast<float>(x), static_cast<float>(y)});
    if (i == 1) {
      EXPECT_DOUBLE_EQ(0.0, motion_estimator->GetMouseAcceleration().x());
      EXPECT_DOUBLE_EQ(0.0, motion_estimator->GetMouseAcceleration().y());
      EXPECT_DOUBLE_EQ(0.0, motion_estimator->GetMouseVelocity().x());
      EXPECT_DOUBLE_EQ(0.0, motion_estimator->GetMouseVelocity().y());
    } else if (i == 2) {
      EXPECT_DOUBLE_EQ(0.0, motion_estimator->GetMouseAcceleration().x());
      EXPECT_DOUBLE_EQ(0.0, motion_estimator->GetMouseAcceleration().y());
      EXPECT_NEAR(1.0,
                  motion_estimator->GetMouseVelocity().x() /
                      /*vx0+0.5*ax*t=0+0.5*100*0.005=*/0.25,
                  eps);
      EXPECT_NEAR(1.0,
                  motion_estimator->GetMouseVelocity().y() /
                      /*vy0+0.5*ay*t=0+0.5*-200*0.005=*/-0.5,
                  eps);
    } else {
      EXPECT_NEAR(1.0, motion_estimator->GetMouseAcceleration().x() / ax, eps);
      EXPECT_NEAR(1.0, motion_estimator->GetMouseAcceleration().y() / ay, eps);
      EXPECT_NEAR(1.0, motion_estimator->GetMouseVelocity().x() / vx, eps);
      EXPECT_NEAR(1.0, motion_estimator->GetMouseVelocity().y() / vy, eps);
    }
  }
}

TEST_F(AnchorElementInteractionTest, MouseVelocitySent) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.example/'>
      <div style='padding: 0px; width: 400px; height: 400px;'></div>
    </a>
  )HTML");

  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  GetDocument().GetAnchorElementInteractionTracker()->SetTaskRunnerForTesting(
      task_runner, task_runner->GetMockTickClock());

  constexpr gfx::PointF origin{200, 200};
  constexpr gfx::Vector2dF velocity{40, -30};  // sqrt(40**2 + (-30)**2) = 50
  constexpr base::TimeDelta timestep = base::Milliseconds(1);
  for (base::TimeDelta t; t <= GetLongerThanModerateHoverDwellTime();
       t += timestep) {
    gfx::PointF coordinates =
        origin + gfx::ScaleVector2d(velocity, t.InSecondsF());
    WebMouseEvent event(WebInputEvent::Type::kMouseMove, coordinates,
                        coordinates, WebPointerProperties::Button::kNoButton, 0,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests());
    GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
        event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
    task_runner->AdvanceTimeAndRun(timestep);
  }

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(hosts_.size(), 1u);
  if (base::FeatureList::IsEnabled(
          blink::features::kPreloadingEagerHoverHeuristics)) {
    // This feature doubles the `kOnPointerHover` calls by adding an `eager`
    // hover notification in advance of each `moderate` hover.
    ASSERT_EQ(hosts_[0]->calls_.size(), 2u);

    const MockAnchorElementInteractionHost::Call& eager_hover =
        hosts_[0]->calls_[0];
    EXPECT_EQ(eager_hover.url, KURL("https://anchor1.example/"));
    EXPECT_EQ(eager_hover.type, PointerEventType::kOnPointerHover);
    EXPECT_TRUE(eager_hover.is_mouse_pointer.value());
    EXPECT_TRUE(eager_hover.is_eager.value());
    EXPECT_NEAR(eager_hover.mouse_velocity.value(), 50, 0.5);
  } else {
    ASSERT_EQ(hosts_[0]->calls_.size(), 1u);
  }
  const MockAnchorElementInteractionHost::Call& moderate_hover =
      hosts_[0]->calls_.back();
  EXPECT_EQ(moderate_hover.url, KURL("https://anchor1.example/"));
  EXPECT_EQ(moderate_hover.type, PointerEventType::kOnPointerHover);
  EXPECT_TRUE(moderate_hover.is_mouse_pointer.value());
  EXPECT_FALSE(moderate_hover.is_eager.value());
  EXPECT_NEAR(moderate_hover.mouse_velocity.value(), 50, 0.5);
}

class AnchorElementInteractionViewportHeuristicsTest
    : public AnchorElementInteractionTest {
 public:
  AnchorElementInteractionViewportHeuristicsTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kNavigationPredictor, GetParamsForNavigationPredictor()},
         {features::kNavigationPredictorNewViewportFeatures, {}},
         {features::kPreloadingModerateViewportHeuristics,
          {{"delay", "100ms"},
           {"distance_from_ptr_down_low", "-0.3"},
           {"distance_from_ptr_down_hi", "0"},
           {"largest_anchor_threshold", "0.5"}}},
         {features::kPreloadingEagerViewportHeuristics,
          {{"viewport_present_time", "100ms"}}}},
        {});
    config_scope_ =
        std::make_unique<ModerateViewportHeuristicConfigTestingScope>();
  }

  static constexpr int kViewportWidth = 400;
  static constexpr int kViewportHeight = 400;

  std::map<std::string, std::string> GetParamsForNavigationPredictor() {
    return {{"random_anchor_sampling_period", "1"},
            {"intersection_observation_after_fcp_only", "true"},
            {"post_fcp_observation_delay", "10ms"}};
  }

  static base::TimeDelta EnoughWaitTimeForAllViewportHeuristics() {
    // Any larger or equal value than max(
    //   PreloadingModerateViewportHeuristics.delay,
    //   PreloadingEagerViewportHeuristics.viewport_present_time
    //   );
    return base::Milliseconds(200);
  }

  void DispatchPointerDown(gfx::PointF coordinates) {
    GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
        WebMouseEvent(WebInputEvent::Type::kMouseDown, coordinates, coordinates,
                      WebPointerProperties::Button::kLeft, 0,
                      WebInputEvent::kLeftButtonDown,
                      WebInputEvent::GetStaticTimeStampForTests()));
  }

  void DispatchPointerDownAndVerticalScroll(gfx::PointF coordinates, float dy) {
    DispatchPointerDown(coordinates);
    GetWebFrameWidget().DispatchThroughCcInputHandler(
        SyntheticWebGestureEventBuilder::BuildScrollBegin(
            /*dx_hint=*/0.0f, /*dy_hint=*/0.0f,
            WebGestureDevice::kTouchscreen));
    GetWebFrameWidget().DispatchThroughCcInputHandler(
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(
            /*dx=*/0.0f, dy, WebInputEvent::kNoModifiers,
            WebGestureDevice::kTouchscreen));
    GetWebFrameWidget().DispatchThroughCcInputHandler(
        SyntheticWebGestureEventBuilder::Build(
            WebInputEvent::Type::kGestureScrollEnd,
            WebGestureDevice::kTouchscreen));
    Compositor().BeginFrame();
  }

  void ProcessPositionUpdates() {
    platform_->RunForPeriodSeconds(ConvertDOMHighResTimeStampToSeconds(
        AnchorElementViewportPositionTracker::MaybeGetOrCreateFor(GetDocument())
            ->GetIntersectionObserverForTesting()
            ->delay()));
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    base::RunLoop().RunUntilIdle();
  }

  struct TestFixtureParams {
    KURL main_frame_url = KURL("https://example.com/");
    String main_resource_body;
    gfx::PointF pointer_down_location;
    // Vertical scroll delta; positive value will scroll up.
    int scroll_delta;
  };
  // Runs test steps used by most tests in the suite:
  // 1) Load a document (contents specified with `main_resource_body`).
  // 2) Pointer down at `pointer_down_location` and scroll vertically with by
  //    `scroll delta`.
  // 3) Process position updates.
  void RunBasicTestFixture(const TestFixtureParams& params) {
    String source(params.main_frame_url);
    SimRequest main_resource(source, "text/html");
    LoadURL(source);
    main_resource.Complete(params.main_resource_body);

    GetDocument().GetAnchorElementInteractionTracker()->SetTaskRunnerForTesting(
        platform_->test_task_runner(),
        platform_->test_task_runner()->GetMockTickClock());

    Compositor().BeginFrame();
    // The 10ms matches the "post_fcp_observation_delay" param set for
    // kNavigationPredictor.
    platform_->RunForPeriod(base::Milliseconds(10));
    DispatchPointerDownAndVerticalScroll(params.pointer_down_location,
                                         params.scroll_delta);
    ProcessPositionUpdates();

    // Wait for all activation of viewport heuristics.
    platform_->RunForPeriod(EnoughWaitTimeForAllViewportHeuristics());
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void SetUp() override {
    AnchorElementInteractionTest::SetUp();

    // Allows WidgetInputHandlerManager::InitOnInputHandlingThread() to run.
    platform_->RunForPeriod(base::Milliseconds(1));
  }

  frame_test_helpers::TestWebFrameWidget* CreateWebFrameWidget(
      base::PassKey<WebLocalFrame> pass_key,
      CrossVariantMojoAssociatedRemote<
          mojom::blink::FrameWidgetHostInterfaceBase> frame_widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
          frame_widget,
      CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
          widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
          widget,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const viz::FrameSinkId& frame_sink_id,
      bool hidden,
      bool never_composited,
      bool is_for_child_local_root,
      bool is_for_nested_main_frame,
      bool is_for_scalable_page) override {
    auto* test_web_frame_widget =
        MakeGarbageCollected<frame_test_helpers::TestWebFrameWidget>(
            std::move(pass_key), std::move(frame_widget_host),
            std::move(frame_widget), std::move(widget_host), std::move(widget),
            std::move(task_runner), frame_sink_id, hidden, never_composited,
            is_for_child_local_root, is_for_nested_main_frame,
            is_for_scalable_page);
    display::ScreenInfo screen_info;
    screen_info.rect = gfx::Rect(kViewportWidth, kViewportHeight);
    test_web_frame_widget->SetInitialScreenInfo(screen_info);
    return test_web_frame_widget;
  }

  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ModerateViewportHeuristicConfigTestingScope> config_scope_;
};

TEST_F(AnchorElementInteractionViewportHeuristicsTest, BasicTest) {
  String body = R"HTML(
    <body style="margin: 0px">
      <div style="height: 200px"></div>
      <a href="https://example.com/foo"
         style="height: 100px; display: block;">link</a>
      <div style="height: 300px"></div>
    </body>
  )HTML";
  RunBasicTestFixture({.main_resource_body = body,
                       .pointer_down_location = gfx::PointF(100, 180),
                       .scroll_delta = -100});

  ASSERT_EQ(hosts_.size(), 1u);
  EXPECT_EQ(hosts_[0]->calls_.size(), 2u);
  EXPECT_EQ(hosts_[0]->calls_[0].url, KURL("https://example.com/foo"));
  EXPECT_EQ(hosts_[0]->calls_[0].type, PointerEventType::kNone);
  EXPECT_TRUE(hosts_[0]->calls_[0].is_eager.has_value());
  EXPECT_EQ(hosts_[0]->calls_[1].url, KURL("https://example.com/foo"));
  EXPECT_EQ(hosts_[0]->calls_[1].type, PointerEventType::kNone);
  EXPECT_TRUE(hosts_[0]->calls_[1].is_eager.has_value());
  EXPECT_NE(hosts_[0]->calls_[0].is_eager.value(),
            hosts_[0]->calls_[1].is_eager.value());
}

// Tests scenario where an anchor's distance_from_pointer_down_ratio after a
// scroll is not in [-0.3, 0].
TEST_F(AnchorElementInteractionViewportHeuristicsTest,
       NotNearPreviousPointerDown) {
  String body = R"HTML(
    <body style="margin: 0px">
      <div style="height: 200px"></div>
      <a href="https://example.com/foo"
         style="height: 100px; display: block;">link</a>
      <div style="height: 300px"></div>
    </body>
  )HTML";
  RunBasicTestFixture({.main_resource_body = body,
                       .pointer_down_location = gfx::PointF(100, 350),
                       .scroll_delta = -100});

  // "moderate" viewport heuristic should not be triggered.
  ASSERT_EQ(hosts_.size(), 1u);
  EXPECT_EQ(hosts_[0]->calls_.size(), 1u);
  EXPECT_EQ(hosts_[0]->calls_[0].url, KURL("https://example.com/foo"));
  EXPECT_EQ(hosts_[0]->calls_[0].type, PointerEventType::kNone);
  EXPECT_TRUE(hosts_[0]->calls_[0].is_eager.has_value());
  EXPECT_TRUE(hosts_[0]->calls_[0].is_eager.value());
}

// Test scenario with two anchors where one anchor is < 50% larger than the
// other.
TEST_F(AnchorElementInteractionViewportHeuristicsTest,
       TwoSimilarlySizedAnchors) {
  String body = R"HTML(
    <body style="margin: 0px">
      <div style="height: 100px"></div>
      <a href="https://example.com/foo"
        style="display: block; height: 30px;">foo</a>
      <a href="https://example.com/bar"
        style="display: block; height: 25px;">bar</a>
      <div style="height: 400px"></div>
    </body>
  )HTML";
  RunBasicTestFixture({.main_resource_body = body,
                       .pointer_down_location = gfx::PointF(100, 200),
                       .scroll_delta = -25});

  // "moderate" viewport heuristic should not be triggered for both anchors.
  ASSERT_EQ(hosts_.size(), 1u);
  EXPECT_EQ(hosts_[0]->calls_.size(), 2u);
  EXPECT_EQ(hosts_[0]->calls_[0].url, KURL("https://example.com/foo"));
  EXPECT_EQ(hosts_[0]->calls_[0].type, PointerEventType::kNone);
  EXPECT_TRUE(hosts_[0]->calls_[0].is_eager.has_value());
  EXPECT_TRUE(hosts_[0]->calls_[0].is_eager.value());
  EXPECT_EQ(hosts_[0]->calls_[1].url, KURL("https://example.com/bar"));
  EXPECT_EQ(hosts_[0]->calls_[1].type, PointerEventType::kNone);
  EXPECT_TRUE(hosts_[0]->calls_[1].is_eager.has_value());
  EXPECT_TRUE(hosts_[0]->calls_[1].is_eager.value());
}

// Test scenario with two anchors where one anchor is > 50% larger than the
// other.
TEST_F(AnchorElementInteractionViewportHeuristicsTest,
       TwoDifferentlySizedAnchors) {
  String body = R"HTML(
    <body style="margin: 0px">
      <div style="height: 100px"></div>
      <a href="https://example.com/foo"
        style="display: block; height: 40px;">foo</a>
      <a href="https://example.com/bar"
        style="display: block; height: 20px;">bar</a>
      <div style="height: 400px"></div>
    </body>
  )HTML";
  RunBasicTestFixture({.main_resource_body = body,
                       .pointer_down_location = gfx::PointF(100, 150),
                       .scroll_delta = -25});

  // This will also trigger some hover and pointer down calls, but we only care
  // about the "moderate" viewport one.
  ASSERT_EQ(hosts_.size(), 1u);
  ASSERT_GE(hosts_[0]->calls_.size(), 1u);
  const auto moderate_viewport_call_it = std::ranges::find_if(
      hosts_[0]->calls_,
      [](const MockAnchorElementInteractionHost::Call& call) {
        return call.type == PointerEventType::kNone &&
               call.is_eager.has_value() && !call.is_eager.value();
      });
  EXPECT_NE(moderate_viewport_call_it, hosts_[0]->calls_.end());
  EXPECT_EQ(moderate_viewport_call_it->url, KURL("https://example.com/foo"));
  EXPECT_EQ(moderate_viewport_call_it->type, PointerEventType::kNone);
}

TEST_F(AnchorElementInteractionViewportHeuristicsTest, MultipleAnchors) {
  String body = R"HTML(
    <body style="margin: 0px">
      <div style="height: 100px"></div>
      <a href="https://example.com/one"
        style="display: block; height: 30px;">one</a>
      <a href="https://example.com/two"
        style="display: block; height: 40px;">two</a>
      <a href="https://example.com/three"
        style="display: block; height: 20px;">three</a>
      <a href="https://example.com/four"
        style="display: block; height: 20px;">four</a>
      <a href="https://example.com/five"
        style="display: block; height: 100px;">five</a>
      <div style="height: 400px"></div>
    </body>
  )HTML";
  RunBasicTestFixture({.main_resource_body = body,
                       .pointer_down_location = gfx::PointF(100, 220),
                       .scroll_delta = -25});

  // One and five are too far away from the ptr down, two is much bigger than
  // three and four.
  // This will also trigger some hover and pointer down calls, but we only care
  // about the "moderate" viewport one.
  ASSERT_EQ(hosts_.size(), 1u);
  ASSERT_GE(hosts_[0]->calls_.size(), 1u);
  const auto moderate_viewport_call_it = std::ranges::find_if(
      hosts_[0]->calls_,
      [](const MockAnchorElementInteractionHost::Call& call) {
        return call.type == PointerEventType::kNone &&
               call.is_eager.has_value() && !call.is_eager.value();
      });
  EXPECT_NE(moderate_viewport_call_it, hosts_[0]->calls_.end());
  EXPECT_EQ(moderate_viewport_call_it->url, KURL("https://example.com/two"));
}

TEST_F(AnchorElementInteractionViewportHeuristicsTest,
       PointerDownImmediatelyAfterScroll) {
  String source(KURL("https://example.com"));
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <div style="height: 200px"></div>
      <a href="https://example.com/foo"
         style="height: 100px; display: block;">link</a>
    <div style="height: 300px"></div>
  )HTML");

  GetDocument().GetAnchorElementInteractionTracker()->SetTaskRunnerForTesting(
      platform_->test_task_runner(),
      platform_->test_task_runner()->GetMockTickClock());

  Compositor().BeginFrame();
  // The 10ms matches the "post_fcp_observation_delay" param set for
  // kNavigationPredictor.
  platform_->RunForPeriod(base::Milliseconds(10));
  DispatchPointerDownAndVerticalScroll(gfx::PointF(100, 200), -100);
  ProcessPositionUpdates();

  platform_->RunForPeriod(base::Milliseconds(10));
  // Second pointerdown happens 10ms after the scroll end, which is within the
  // configured delay period of 100ms.
  DispatchPointerDown(gfx::PointF(200, 375));
  // Ensure we go past the configured delay period.
  platform_->RunForPeriodSeconds(0.1);
  base::RunLoop().RunUntilIdle();

  // Second pointerdown happening during the delay period should prevent the
  // anchor from being selected from "moderate" viewport heuristics.
  ASSERT_EQ(hosts_.size(), 1u);
  EXPECT_EQ(hosts_[0]->calls_.size(), 1u);
  EXPECT_EQ(hosts_[0]->calls_[0].url, KURL("https://example.com/foo"));
  EXPECT_EQ(hosts_[0]->calls_[0].type, PointerEventType::kNone);
  EXPECT_TRUE(hosts_[0]->calls_[0].is_eager.has_value());
  EXPECT_TRUE(hosts_[0]->calls_[0].is_eager.value());
}

TEST_F(AnchorElementInteractionViewportHeuristicsTest,
       EagerHeuristicsTriggerForAnchorsInViewport) {
  String body = R"HTML(
    <body style="margin: 0px">
      <div style="height: 50px"></div>
      <a href="https://example.com/one"
        style="display: block; height: 150px;">one</a>
      <a href="https://example.com/two"
        style="display: block; height: 200px;">two</a>
      <a href="https://example.com/three"
        style="display: block; height: 200px;">three</a>
      <a href="https://example.com/four"
        style="display: block; height: 200px;">four</a>
      <div style="height: 400px"></div>
    </body>
  )HTML";
  RunBasicTestFixture({.main_resource_body = body,
                       .pointer_down_location = gfx::PointF(100, 10),
                       .scroll_delta = -400});

  // Only the third and fourth links are in viewport and triggered.
  ASSERT_EQ(hosts_.size(), 1u);
  ASSERT_GE(hosts_[0]->calls_.size(), 2u);
  EXPECT_EQ(hosts_[0]->calls_[0].url, KURL("https://example.com/three"));
  EXPECT_EQ(hosts_[0]->calls_[0].type, PointerEventType::kNone);
  EXPECT_TRUE(hosts_[0]->calls_[0].is_eager.has_value());
  EXPECT_TRUE(hosts_[0]->calls_[0].is_eager.value());
  EXPECT_EQ(hosts_[0]->calls_[1].url, KURL("https://example.com/four"));
  EXPECT_EQ(hosts_[0]->calls_[1].type, PointerEventType::kNone);
  EXPECT_TRUE(hosts_[0]->calls_[1].is_eager.has_value());
  EXPECT_TRUE(hosts_[0]->calls_[1].is_eager.value());
}

TEST_F(AnchorElementInteractionViewportHeuristicsTest,
       PredictorDisabledIfAllAnchorsNotSampledIn) {
  std::map<std::string, std::string> params = GetParamsForNavigationPredictor();
  params["random_anchor_sampling_period"] = "2";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kNavigationPredictor, params);

  String body = R"HTML(
    <body style="margin: 0px">
      <div style="height: 200px"></div>
      <a href="https://example.com/foo"
         style="height: 100px; display: block;">link</a>
      <div style="height: 300px"></div>
    </body>
  )HTML";
  RunBasicTestFixture({.main_resource_body = body,
                       .pointer_down_location = gfx::PointF(100, 180),
                       .scroll_delta = -100});

  // A prediction should not have been made because the sampling rate is not
  // 1 (not all anchors are sampled in).
  ASSERT_EQ(hosts_.size(), 1u);
  EXPECT_EQ(hosts_[0]->calls_.size(), 0u);
}

// Regression test for https://crbug.com/458237344.
TEST_F(AnchorElementInteractionViewportHeuristicsTest,
       IgnoreSameDocumentNavigation) {
  String body = R"HTML(
    <body style="margin: 0px">
      <div style="height: 50px"></div>
      <a href="https://example.com/#head" style="height: 50px; display: block;">foo</a>
    </body>
  )HTML";
  RunBasicTestFixture({.main_resource_body = body,
                       .pointer_down_location = gfx::PointF(100, 150),
                       .scroll_delta = -25});

  ASSERT_EQ(hosts_.size(), 1u);
  EXPECT_EQ(hosts_[0]->calls_.size(), 0u);
}

}  // namespace
}  // namespace blink

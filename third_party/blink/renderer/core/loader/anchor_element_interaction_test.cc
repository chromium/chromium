// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <string_view>
#include <tuple>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/mojom/preloading/anchor_element_interaction_host.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/loader/anchor_element_interaction_tracker.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"

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

  std::optional<KURL> url_received_ = std::nullopt;
  PointerEventType event_type_{PointerEventType::kNone};
  double mouse_velocity_{0.0};
  bool is_mouse_pointer_{false};

 private:
  void OnPointerDown(const KURL& target) override {
    url_received_ = target;
    event_type_ = PointerEventType::kOnPointerDown;
  }
  void OnPointerHover(
      const KURL& target,
      mojom::blink::AnchorElementPointerDataPtr mouse_data) override {
    url_received_ = target;
    event_type_ = PointerEventType::kOnPointerHover;
    is_mouse_pointer_ = mouse_data->is_mouse_pointer;
    mouse_velocity_ = mouse_data->mouse_velocity;
  }

 private:
  mojo::Receiver<mojom::blink::AnchorElementInteractionHost> receiver_{this};
};

class AnchorElementInteractionTest : public SimTest {
 public:
 protected:
  void SetUp() override {
    SimTest::SetUp();

    MainFrame().GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::AnchorElementInteractionHost::Name_,
        WTF::BindRepeating(&AnchorElementInteractionTest::Bind,
                           WTF::Unretained(this)));
    WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 400));
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

  std::vector<std::unique_ptr<MockAnchorElementInteractionHost>> hosts_;
};

TEST_F(AnchorElementInteractionTest, SingleAnchor) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.com/'>
      <div style='padding: 0px; width: 400px; height: 400px;'></div>
    </a>
  )HTML");
  SendMouseDownEvent();
  base::RunLoop().RunUntilIdle();
  KURL expected_url = KURL("https://anchor1.com/");
  EXPECT_EQ(1u, hosts_.size());
  std::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_TRUE(url_received.has_value());
  EXPECT_EQ(expected_url, url_received);
  EXPECT_EQ(PointerEventType::kOnPointerDown, hosts_[0]->event_type_);
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
  EXPECT_EQ(1u, hosts_.size());
  std::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_FALSE(url_received.has_value());
}

TEST_F(AnchorElementInteractionTest, RightClick) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.com/'>
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
  EXPECT_EQ(1u, hosts_.size());
  std::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_FALSE(url_received.has_value());
}

TEST_F(AnchorElementInteractionTest, NestedAnchorElementCheck) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.com/'>
      <a href='https://anchor2.com/'>
        <div style='padding: 0px; width: 400px; height: 400px;'></div>
      </a>
    </a>
  )HTML");
  SendMouseDownEvent();
  base::RunLoop().RunUntilIdle();
  KURL expected_url = KURL("https://anchor2.com/");
  EXPECT_EQ(1u, hosts_.size());
  std::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_TRUE(url_received.has_value());
  EXPECT_EQ(expected_url, url_received);
  EXPECT_EQ(PointerEventType::kOnPointerDown, hosts_[0]->event_type_);
}

TEST_F(AnchorElementInteractionTest, SiblingAnchorElements) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.com/'>
        <div style='padding: 0px; width: 400px; height: 400px;'></div>
    </a>
    <a href='https://anchor2.com/'>
        <div style='padding: 0px; width: 400px; height: 400px;'></div>
    </a>
  )HTML");
  SendMouseDownEvent();
  base::RunLoop().RunUntilIdle();
  KURL expected_url = KURL("https://anchor1.com/");
  EXPECT_EQ(1u, hosts_.size());
  std::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_TRUE(url_received.has_value());
  EXPECT_EQ(expected_url, url_received);
  EXPECT_EQ(PointerEventType::kOnPointerDown, hosts_[0]->event_type_);
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
  EXPECT_EQ(1u, hosts_.size());
  std::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_FALSE(url_received.has_value());
}

TEST_F(AnchorElementInteractionTest, TouchEvent) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.com/'>
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
  KURL expected_url = KURL("https://anchor1.com/");
  EXPECT_EQ(1u, hosts_.size());
  std::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_TRUE(url_received.has_value());
  EXPECT_EQ(expected_url, url_received);
  EXPECT_EQ(PointerEventType::kOnPointerDown, hosts_[0]->event_type_);
}

TEST_F(AnchorElementInteractionTest, DestroyedContext) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.com/'>
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
  KURL expected_url = KURL("https://anchor1.com/");
  EXPECT_EQ(1u, hosts_.size());
  std::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_FALSE(url_received.has_value());
}

TEST_F(AnchorElementInteractionTest, ValidMouseHover) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.com/'>
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
  task_runner->AdvanceTimeAndRun(
      AnchorElementInteractionTracker::GetHoverDwellTime());
  base::RunLoop().RunUntilIdle();

  KURL expected_url = KURL("https://anchor1.com/");
  EXPECT_EQ(1u, hosts_.size());
  std::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_TRUE(url_received.has_value());
  EXPECT_EQ(expected_url, url_received);
  EXPECT_EQ(PointerEventType::kOnPointerHover, hosts_[0]->event_type_);
}

TEST_F(AnchorElementInteractionTest, ShortMouseHover) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.com/'>
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
  task_runner->AdvanceTimeAndRun(
      0.5 * AnchorElementInteractionTracker::GetHoverDwellTime());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, hosts_.size());
  std::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_FALSE(url_received.has_value());
  EXPECT_EQ(PointerEventType::kNone, hosts_[0]->event_type_);
}

TEST_F(AnchorElementInteractionTest, MousePointerEnterAndLeave) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.com/'>
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

  task_runner->AdvanceTimeAndRun(
      0.5 * AnchorElementInteractionTracker::GetHoverDwellTime());

  WebMouseEvent mouse_leave_event(
      WebInputEvent::Type::kMouseLeave, coordinates, coordinates,
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseLeaveEvent(
      mouse_leave_event);

  // Wait for hover logic to process the event
  task_runner->AdvanceTimeAndRun(
      AnchorElementInteractionTracker::GetHoverDwellTime());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, hosts_.size());
  std::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_FALSE(url_received.has_value());
  EXPECT_EQ(PointerEventType::kNone, hosts_[0]->event_type_);
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
    <a href='https://anchor1.com/'>
      <div style='padding: 0px; width: 400px; height: 400px;'></div>
    </a>
  )HTML");

  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  GetDocument().GetAnchorElementInteractionTracker()->SetTaskRunnerForTesting(
      task_runner, task_runner->GetMockTickClock());

  constexpr gfx::PointF origin{200, 200};
  constexpr gfx::Vector2dF velocity{40, -30};
  constexpr base::TimeDelta timestep = base::Milliseconds(20);
  for (base::TimeDelta t;
       t <= AnchorElementInteractionTracker::GetHoverDwellTime();
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

  KURL expected_url = KURL("https://anchor1.com/");
  EXPECT_EQ(1u, hosts_.size());
  std::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_TRUE(url_received.has_value());
  EXPECT_EQ(expected_url, url_received);
  EXPECT_EQ(PointerEventType::kOnPointerHover, hosts_[0]->event_type_);
  EXPECT_TRUE(hosts_[0]->is_mouse_pointer_);
  EXPECT_NEAR(50.0, hosts_[0]->mouse_velocity_, 0.5);
}

}  // namespace
}  // namespace blink

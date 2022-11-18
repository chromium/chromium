// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>

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

  absl::optional<KURL> url_received_ = absl::nullopt;
  PointerEventType event_type_{PointerEventType::kNone};

 private:
  void OnPointerDown(const KURL& target) override {
    url_received_ = target;
    event_type_ = PointerEventType::kOnPointerDown;
  }
  void OnPointerHover(const KURL& target) override {
    url_received_ = target;
    event_type_ = PointerEventType::kOnPointerHover;
  }

 private:
  mojo::Receiver<mojom::blink::AnchorElementInteractionHost> receiver_{this};
};

class AnchorElementInteractionTest : public SimTest {
 public:
 protected:
  void SetUp() override {
    SimTest::SetUp();

    feature_list_.InitWithFeatures(
        {features::kAnchorElementInteraction,
         features::kSpeculationRulesPointerHoverHeuristics},
        {});

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

 private:
  base::test::ScopedFeatureList feature_list_;
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
  absl::optional<KURL> url_received = hosts_[0]->url_received_;
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
  absl::optional<KURL> url_received = hosts_[0]->url_received_;
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
  absl::optional<KURL> url_received = hosts_[0]->url_received_;
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
  absl::optional<KURL> url_received = hosts_[0]->url_received_;
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
  absl::optional<KURL> url_received = hosts_[0]->url_received_;
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
  absl::optional<KURL> url_received = hosts_[0]->url_received_;
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
  absl::optional<KURL> url_received = hosts_[0]->url_received_;
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
  absl::optional<KURL> url_received = hosts_[0]->url_received_;
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

  base::SimpleTestTickClock clock;
  GetDocument().GetAnchorElementInteractionTracker()->SetTickClockForTesting(
      &clock);

  gfx::PointF coordinates(100, 100);
  WebMouseEvent event(WebInputEvent::Type::kMouseMove, coordinates, coordinates,
                      WebPointerProperties::Button::kNoButton, 0,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  // Wait for hover logic to process the event
  clock.Advance(AnchorElementInteractionTracker::GetHoverDwellTime());
  GetDocument()
      .GetAnchorElementInteractionTracker()
      ->FireHoverTimerForTesting();
  base::RunLoop().RunUntilIdle();

  KURL expected_url = KURL("https://anchor1.com/");
  EXPECT_EQ(1u, hosts_.size());
  absl::optional<KURL> url_received = hosts_[0]->url_received_;
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

  base::SimpleTestTickClock clock;
  GetDocument().GetAnchorElementInteractionTracker()->SetTickClockForTesting(
      &clock);

  gfx::PointF coordinates(100, 100);
  WebMouseEvent event(WebInputEvent::Type::kMouseMove, coordinates, coordinates,
                      WebPointerProperties::Button::kNoButton, 0,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  // Wait for hover logic to process the event
  clock.Advance(0.5 * AnchorElementInteractionTracker::GetHoverDwellTime());
  GetDocument()
      .GetAnchorElementInteractionTracker()
      ->FireHoverTimerForTesting();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, hosts_.size());
  absl::optional<KURL> url_received = hosts_[0]->url_received_;
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

  base::SimpleTestTickClock clock;
  GetDocument().GetAnchorElementInteractionTracker()->SetTickClockForTesting(
      &clock);

  // If mouse does not hover long enough over a link, it should be ignored.
  gfx::PointF coordinates(100, 100);
  WebMouseEvent mouse_enter_event(
      WebInputEvent::Type::kMouseMove, coordinates, coordinates,
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_enter_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  clock.Advance(0.5 * AnchorElementInteractionTracker::GetHoverDwellTime());
  GetDocument()
      .GetAnchorElementInteractionTracker()
      ->FireHoverTimerForTesting();
  base::RunLoop().RunUntilIdle();

  WebMouseEvent mouse_leave_event(
      WebInputEvent::Type::kMouseLeave, coordinates, coordinates,
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseLeaveEvent(
      mouse_leave_event);

  // Wait for hover logic to process the event
  clock.Advance(AnchorElementInteractionTracker::GetHoverDwellTime());
  GetDocument()
      .GetAnchorElementInteractionTracker()
      ->FireHoverTimerForTesting();
  base::RunLoop().RunUntilIdle();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, hosts_.size());
  absl::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_FALSE(url_received.has_value());
  EXPECT_EQ(PointerEventType::kNone, hosts_[0]->event_type_);
}

}  // namespace
}  // namespace blink

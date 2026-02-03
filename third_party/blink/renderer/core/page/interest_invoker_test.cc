// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/fake_web_plugin.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {

class TestWebFrameClientImpl : public frame_test_helpers::TestWebFrameClient {
 public:
  void UpdateContextMenuDataForTesting(
      const ContextMenuData& data,
      const std::optional<gfx::Point>& host_context_menu_location) override {
    context_menu_data_ = data;
  }

  const ContextMenuData& GetContextMenuData() const {
    return context_menu_data_;
  }

 private:
  ContextMenuData context_menu_data_;
};

}  // namespace

class InterestInvokerTest : public SimTest {
 public:
  InterestInvokerTest() = default;

  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(gfx::Size(640, 480));
    WebView().MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
    SimTest::TearDown();
  }

  const TestWebFrameClientImpl& GetWebFrameClient() {
    return static_cast<const TestWebFrameClientImpl&>(WebFrameClient());
  }

 protected:
  std::unique_ptr<frame_test_helpers::TestWebFrameClient>
  CreateWebFrameClientForMainFrame() override {
    return std::make_unique<TestWebFrameClientImpl>();
  }
};

TEST_F(InterestInvokerTest, ButtonWithInterestForPopover_TouchRelease) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(R"(
    <button interestfor=target id=button type=button>Button</button>
    <div id=target popover=hint>Target</div>
    )");
  Document& document = GetDocument();
  Element* button = document.getElementById(AtomicString("button"));
  Element* target = document.getElementById(AtomicString("target"));
  HTMLElement* target_popover = To<HTMLElement>(target);

  document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ(button->GetInterestState(), Element::InterestState::kNoInterest);
  EXPECT_FALSE(target_popover->popoverOpen());

  // Long-press the button
  gfx::PointF gesture_location = button->GetBoundingClientRect()->Center();
  WebGestureEvent gesture_event(
      WebInputEvent::Type::kGestureLongPress, WebInputEvent::kNoModifiers,
      base::TimeTicks::Now(), WebGestureDevice::kTouchscreen);
  gesture_event.SetPositionInWidget(gesture_location);
  GetWebFrameWidget().HandleInputEvent(
      WebCoalescedInputEvent(gesture_event, ui::LatencyInfo()));

  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_TRUE(context_menu_data.opened_from_interest_for);

  // Interest is shown immediately for buttons.
  EXPECT_NE(button->GetInterestState(), Element::InterestState::kNoInterest);
  EXPECT_TRUE(target_popover->popoverOpen());

  // Now simulate the pointerup that happens when the touch is released.
  WebPointerEvent pointerup_event(
      WebInputEvent::Type::kPointerUp,
      WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                           WebPointerProperties::Button::kLeft,
                           gesture_location, gesture_location),
      1.0f, 1.0f);
  GetWebFrameWidget().HandleInputEvent(
      WebCoalescedInputEvent(pointerup_event, ui::LatencyInfo()));

  document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // Advance time to allow interest lost task to run (if it was scheduled)
  test::RunDelayedTasks(base::Seconds(1));

  // The popover should remain open.
  EXPECT_NE(button->GetInterestState(), Element::InterestState::kNoInterest);
  EXPECT_TRUE(target_popover->popoverOpen());
}

TEST_F(InterestInvokerTest, ButtonWithInterestForPopover_TouchDrag) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(R"(
    <button interestfor=target id=button type=button>Button</button>
    <div id=target popover=hint>Target</div>
    )");
  Document& document = GetDocument();
  Element* button = document.getElementById(AtomicString("button"));
  Element* target = document.getElementById(AtomicString("target"));
  HTMLElement* target_popover = To<HTMLElement>(target);

  document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ(button->GetInterestState(), Element::InterestState::kNoInterest);
  EXPECT_FALSE(target_popover->popoverOpen());

  // Long-press the button
  gfx::PointF gesture_location = button->GetBoundingClientRect()->Center();
  WebGestureEvent gesture_event(
      WebInputEvent::Type::kGestureLongPress, WebInputEvent::kNoModifiers,
      base::TimeTicks::Now(), WebGestureDevice::kTouchscreen);
  gesture_event.SetPositionInWidget(gesture_location);
  GetWebFrameWidget().HandleInputEvent(
      WebCoalescedInputEvent(gesture_event, ui::LatencyInfo()));

  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_TRUE(context_menu_data.opened_from_interest_for);

  // Interest is shown immediately for buttons.
  EXPECT_NE(button->GetInterestState(), Element::InterestState::kNoInterest);
  EXPECT_TRUE(target_popover->popoverOpen());

  // Fire a pointermove event - this simulates a user long-pressing the
  // element, and then drags their finger off the control.
  gfx::PointF outside_location = gesture_location + gfx::Vector2dF(100, 100);
  WebPointerEvent pointermove_event(
      WebInputEvent::Type::kPointerMove,
      WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                           WebPointerProperties::Button::kLeft,
                           outside_location, outside_location),
      1.0f, 1.0f);
  GetWebFrameWidget().HandleInputEvent(
      WebCoalescedInputEvent(pointermove_event, ui::LatencyInfo()));

  document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // Advance time to allow interest lost task to run (if it was scheduled)
  test::RunDelayedTasks(base::Seconds(1));

  // The popover should remain open.
  EXPECT_NE(button->GetInterestState(), Element::InterestState::kNoInterest);
  EXPECT_TRUE(target_popover->popoverOpen());
}

}  // namespace blink

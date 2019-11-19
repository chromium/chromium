// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_per_process_browsertest.h"

#include <Cocoa/Cocoa.h>

#include "base/bind.h"
#include "base/mac/mac_util.h"
#include "base/task/post_task.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/ocmock_extensions.h"

namespace content {

namespace {

// Helper class for TextInputClientMac.
class TextInputClientMacHelper {
 public:
  TextInputClientMacHelper() {}
  ~TextInputClientMacHelper() {}

  void WaitForStringFromRange(RenderWidgetHost* rwh, const gfx::Range& range) {
    GetStringFromRangeForRenderWidget(
        rwh, range, base::Bind(&TextInputClientMacHelper::OnResult,
                               base::Unretained(this)));
    loop_runner_ = new MessageLoopRunner();
    loop_runner_->Run();
  }

  void WaitForStringAtPoint(RenderWidgetHost* rwh, const gfx::Point& point) {
    GetStringAtPointForRenderWidget(
        rwh, point, base::Bind(&TextInputClientMacHelper::OnResult,
                               base::Unretained(this)));
    loop_runner_ = new MessageLoopRunner();
    loop_runner_->Run();
  }
  const std::string& word() const { return word_; }
  const gfx::Point& point() const { return point_; }

 private:
  void OnResult(const std::string& string, const gfx::Point& point) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      base::PostTask(FROM_HERE, {BrowserThread::UI},
                     base::BindOnce(&TextInputClientMacHelper::OnResult,
                                    base::Unretained(this), string, point));
      return;
    }
    word_ = string;
    point_ = point;

    if (loop_runner_ && loop_runner_->loop_running())
      loop_runner_->Quit();
  }

  std::string word_;
  gfx::Point point_;
  scoped_refptr<MessageLoopRunner> loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TextInputClientMacHelper);
};

}  // namespace

// Site per process browser tests inside content which are specific to Mac OSX
// platform.
class SitePerProcessMacBrowserTest : public SitePerProcessBrowserTest {};

// This test will load a text only page inside a child frame and then queries
// the string range which includes the first word. Then it uses the returned
// point to query the text again and verifies that correct result is returned.
// Finally, the returned words are compared against the first word in the html
// file which is "This".
IN_PROC_BROWSER_TEST_F(SitePerProcessMacBrowserTest,
                       GetStringFromRangeAndPointChildFrame) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);
  NavigateFrameToURL(child,
                     embedded_test_server()->GetURL("b.com", "/title1.html"));

  RenderWidgetHost* child_widget_host =
      child->current_frame_host()->GetRenderWidgetHost();
  TextInputClientMacHelper helper;

  // Get string from range.
  helper.WaitForStringFromRange(child_widget_host, gfx::Range(0, 4));
  gfx::Point point = helper.point();
  std::string word = helper.word();

  // Now get it at a given point.
  helper.WaitForStringAtPoint(child_widget_host, point);
  EXPECT_EQ(word, helper.word());
  EXPECT_EQ("This", word);
}

// This test will load a text only page and then queries the string range which
// includes the first word. Then it uses the returned point to query the text
// again and verifies that correct result is returned. Finally, the returned
// words are compared against the first word in the html file which is "This".
IN_PROC_BROWSER_TEST_F(SitePerProcessMacBrowserTest,
                       GetStringFromRangeAndPointMainFrame) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  RenderWidgetHost* widget_host =
      root->current_frame_host()->GetRenderWidgetHost();
  TextInputClientMacHelper helper;

  // Get string from range.
  helper.WaitForStringFromRange(widget_host, gfx::Range(0, 4));
  gfx::Point point = helper.point();
  std::string word = helper.word();

  // Now get it at a given point.
  helper.WaitForStringAtPoint(widget_host, point);
  EXPECT_EQ(word, helper.word());
  EXPECT_EQ("This", word);
}

// Ensure that the RWHVCF forwards wheel events with phase ending information.
// RWHVCF may see wheel events with phase ending information that have deltas
// of 0. These should not be dropped, otherwise MouseWheelEventQueue will not
// be informed that the user's gesture has ended.
// See crbug.com/628742
IN_PROC_BROWSER_TEST_F(SitePerProcessMacBrowserTest,
                       ForwardWheelEventsWithPhaseEndingInformation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_iframe_node = root->child_at(0);
  RenderWidgetHost* child_rwh =
      child_iframe_node->current_frame_host()->GetRenderWidgetHost();

  InputEventAckWaiter gesture_scroll_begin_ack_observer(
      child_rwh, base::BindRepeating([](InputEventAckSource, InputEventAckState,
                                        const blink::WebInputEvent& event) {
        return event.GetType() == blink::WebInputEvent::kGestureScrollBegin &&
               !static_cast<const blink::WebGestureEvent&>(event)
                    .data.scroll_begin.synthetic;
      }));
  InputEventAckWaiter gesture_scroll_end_ack_observer(
      child_rwh, base::BindRepeating([](InputEventAckSource, InputEventAckState,
                                        const blink::WebInputEvent& event) {
        return event.GetType() == blink::WebInputEvent::kGestureScrollEnd &&
               !static_cast<const blink::WebGestureEvent&>(event)
                    .data.scroll_end.synthetic;
      }));

  RenderWidgetHostViewBase* child_rwhv =
      static_cast<RenderWidgetHostViewBase*>(child_rwh->GetView());

  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::kMouseWheel, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  scroll_event.SetPositionInWidget(1, 1);
  scroll_event.delta_units =
      ui::input_types::ScrollGranularity::kScrollByPrecisePixel;
  scroll_event.delta_x = 0.0f;

  // Have the RWHVCF process a sequence of touchpad scroll events that contain
  // phase informaiton. We start scrolling normally, then we fling.
  // We wait for GestureScrollBegin/Ends that result from these wheel events.
  // If we don't see them, this test will time out indicating failure.

  // Begin scrolling.
  scroll_event.delta_y = -1.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  scroll_event.momentum_phase = blink::WebMouseWheelEvent::kPhaseNone;
  child_rwhv->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());
  gesture_scroll_begin_ack_observer.Wait();

  scroll_event.delta_y = -2.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseChanged;
  scroll_event.momentum_phase = blink::WebMouseWheelEvent::kPhaseNone;
  child_rwhv->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  // We now go into a fling.
  scroll_event.delta_y = -2.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseNone;
  scroll_event.momentum_phase = blink::WebMouseWheelEvent::kPhaseBegan;
  child_rwhv->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  scroll_event.delta_y = -2.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseNone;
  scroll_event.momentum_phase = blink::WebMouseWheelEvent::kPhaseChanged;
  child_rwhv->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  // End of fling momentum.
  scroll_event.delta_y = 0.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseNone;
  scroll_event.momentum_phase = blink::WebMouseWheelEvent::kPhaseEnded;
  child_rwhv->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());
  gesture_scroll_end_ack_observer.Wait();
}

namespace {

id MockGestureEvent(NSEventType type,
                    double magnification,
                    int x,
                    int y,
                    NSEventPhase phase) {
  id event = [OCMockObject mockForClass:[NSEvent class]];
  NSPoint locationInWindow = NSMakePoint(x, y);
  CGFloat deltaX = 0;
  CGFloat deltaY = 0;
  NSTimeInterval timestamp = 1;
  NSUInteger modifierFlags = 0;

  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(type)] type];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(phase)] phase];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(locationInWindow)]
      locationInWindow];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(deltaX)] deltaX];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(deltaY)] deltaY];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(timestamp)] timestamp];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(modifierFlags)]
      modifierFlags];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(magnification)]
      magnification];
  return event;
}

bool ShouldSendGestureEvents() {
#if defined(MAC_OS_X_VERSION_10_11) && \
    MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_11
  return base::mac::IsAtMostOS10_10();
#endif
  return true;
}

void SendMacTouchpadPinchSequenceWithExpectedTarget(
    RenderWidgetHostViewBase* root_view,
    const gfx::Point& gesture_point,
    RenderWidgetHostViewBase*& router_touchpad_gesture_target,
    RenderWidgetHostViewBase* expected_target) {
  auto* root_view_mac = static_cast<RenderWidgetHostViewMac*>(root_view);
  RenderWidgetHostViewCocoa* cocoa_view = root_view_mac->GetInProcessNSView();

  NSEvent* pinchBeginEvent =
      MockGestureEvent(NSEventTypeMagnify, 0, gesture_point.x(),
                       gesture_point.y(), NSEventPhaseBegan);
  if (ShouldSendGestureEvents())
    [cocoa_view beginGestureWithEvent:pinchBeginEvent];
  [cocoa_view magnifyWithEvent:pinchBeginEvent];
  // We don't check the gesture target yet, since on mac the GesturePinchBegin
  // isn't sent until the first PinchUpdate.

  InputEventAckWaiter waiter(expected_target->GetRenderWidgetHost(),
                             blink::WebInputEvent::kGesturePinchBegin);
  NSEvent* pinchUpdateEvent =
      MockGestureEvent(NSEventTypeMagnify, 0.25, gesture_point.x(),
                       gesture_point.y(), NSEventPhaseChanged);
  [cocoa_view magnifyWithEvent:pinchUpdateEvent];
  waiter.Wait();
  EXPECT_EQ(expected_target, router_touchpad_gesture_target);

  NSEvent* pinchEndEvent =
      MockGestureEvent(NSEventTypeMagnify, 0, gesture_point.x(),
                       gesture_point.y(), NSEventPhaseEnded);
  [cocoa_view magnifyWithEvent:pinchEndEvent];
  if (ShouldSendGestureEvents())
    [cocoa_view endGestureWithEvent:pinchEndEvent];
  EXPECT_EQ(nullptr, router_touchpad_gesture_target);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(SitePerProcessMacBrowserTest,
                       InputEventRouterTouchpadGestureTargetTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(
      embedded_test_server()->GetURL("b.com", "/page_with_click_handler.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);
  auto* child_frame_host = root->child_at(0)->current_frame_host();

  // Synchronize with the child and parent renderers to guarantee that the
  // surface information required for event hit testing is ready.
  auto* rwhv_child =
      static_cast<RenderWidgetHostViewBase*>(child_frame_host->GetView());
  WaitForHitTestData(child_frame_host);

  // All touches & gestures are sent to the main frame's view, and should be
  // routed appropriately from there.
  auto* rwhv_parent = static_cast<RenderWidgetHostViewBase*>(
      contents->GetRenderWidgetHostView());

  RenderWidgetHostInputEventRouter* router = contents->GetInputEventRouter();
  EXPECT_EQ(nullptr, router->touchpad_gesture_target_);

  gfx::Point main_frame_point(25, 575);
  gfx::Point child_center(150, 450);

  // Send touchpad pinch sequence to main-frame.
  SendMacTouchpadPinchSequenceWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touchpad_gesture_target_,
      rwhv_parent);

  // Send touchpad pinch sequence to child.
  SendMacTouchpadPinchSequenceWithExpectedTarget(
      rwhv_parent, child_center, router->touchpad_gesture_target_, rwhv_child);
}

}  // namespace content

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/media_controls_display_cutout_delegate.h"

#include "third_party/blink/public/mojom/page/display_cutout.mojom-blink.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/input/touch.h"
#include "third_party/blink/renderer/core/input/touch_list.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

class DisplayCutoutMockChromeClient : public EmptyChromeClient {
 public:
  // ChromeClient overrides:
  void EnterFullscreen(LocalFrame& frame, const FullscreenOptions&) override {
    Fullscreen::DidEnterFullscreen(*frame.GetDocument());
  }
  void ExitFullscreen(LocalFrame& frame) override {
    Fullscreen::DidExitFullscreen(*frame.GetDocument());
  }
};

}  // namespace

class MediaControlsDisplayCutoutDelegateTest : public PageTestBase {
 public:
  void SetUp() override {
    chrome_client_ = new DisplayCutoutMockChromeClient();

    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = chrome_client_.Get();
    SetupPageWithClients(&clients, EmptyLocalFrameClient::Create());

    RuntimeEnabledFeatures::SetDisplayCutoutAPIEnabled(true);
    RuntimeEnabledFeatures::SetMediaControlsExpandGestureEnabled(true);

    GetDocument().write("<body><video id=video></body>");
  }

  void SimulateEnterFullscreen() {
    {
      std::unique_ptr<UserGestureIndicator> gesture =
          LocalFrame::NotifyUserActivation(GetDocument().GetFrame());
      Fullscreen::RequestFullscreen(GetVideoElement());
    }

    test::RunPendingTasks();
    GetDocument().ServiceScriptedAnimations(base::TimeTicks());

    EXPECT_TRUE(GetVideoElement().IsFullscreen());
  }

  void SimulateExitFullscreen() {
    Fullscreen::FullyExitFullscreen(GetDocument());

    GetDocument().ServiceScriptedAnimations(base::TimeTicks());

    EXPECT_FALSE(GetVideoElement().IsFullscreen());
  }

  void SimulateContractingGesture() {
    TouchList* list = CreateTouchListWithTwoPoints(5, 5, -5, -5);
    SimulateEvent(CreateTouchEventWithList(EventTypeNames::touchstart, list));

    list = CreateTouchListWithTwoPoints(4, 4, -4, -4);
    SimulateEvent(CreateTouchEventWithList(EventTypeNames::touchmove, list));

    list = CreateTouchListWithTwoPoints(0, 0, 0, 0);
    SimulateEvent(CreateTouchEventWithList(EventTypeNames::touchend, list));
  }

  void SimulateExpandingGesture() {
    TouchList* list = CreateTouchListWithTwoPoints(1, 1, -1, -1);
    SimulateEvent(CreateTouchEventWithList(EventTypeNames::touchstart, list));

    list = CreateTouchListWithTwoPoints(4, 4, -4, -4);
    SimulateEvent(CreateTouchEventWithList(EventTypeNames::touchmove, list));

    list = CreateTouchListWithTwoPoints(5, 5, -5, -5);
    SimulateEvent(CreateTouchEventWithList(EventTypeNames::touchend, list));
  }

  void SimulateSingleTouchGesture() {
    TouchList* list = CreateTouchListWithOnePoint(1, 1);
    SimulateEvent(CreateTouchEventWithList(EventTypeNames::touchstart, list));

    list = CreateTouchListWithOnePoint(4, 4);
    SimulateEvent(CreateTouchEventWithList(EventTypeNames::touchmove, list));

    list = CreateTouchListWithOnePoint(5, 5);
    SimulateEvent(CreateTouchEventWithList(EventTypeNames::touchend, list));
  }

  bool HasGestureState() { return GetDelegate().previous_.has_value(); }

  bool DirectionIsExpanding() {
    return GetDelegate().previous_->second ==
           MediaControlsDisplayCutoutDelegate::Direction::kExpanding;
  }

  bool DirectionIsUnknown() {
    return GetDelegate().previous_->second ==
           MediaControlsDisplayCutoutDelegate::Direction::kUnknown;
  }

  void SimulateEvent(TouchEvent* event) {
    DCHECK(event);
    GetVideoElement().FireEventListeners(*event);
  }

  TouchList* CreateTouchListWithOnePoint(int x, int y) {
    TouchList* list = TouchList::Create();
    list->Append(CreateTouchAtPoint(x, y));
    return list;
  }

  TouchList* CreateTouchListWithTwoPoints(int x1, int y1, int x2, int y2) {
    TouchList* list = TouchList::Create();
    list->Append(CreateTouchAtPoint(x1, y1));
    list->Append(CreateTouchAtPoint(x2, y2));
    return list;
  }

  TouchEvent* CreateTouchEventWithList(const AtomicString& type,
                                       TouchList* list) {
    TouchEvent* event = TouchEvent::Create();
    event->initEvent(type, true, false);
    event->SetTouches(list);
    return event;
  }

  Touch* CreateTouchAtPoint(int x, int y) {
    return Touch::Create(GetDocument().GetFrame(), &GetVideoElement(),
                         1 /* identifier */, FloatPoint(x, y), FloatPoint(x, y),
                         FloatSize(1, 1), 90, 0, "test");
  }

  mojom::ViewportFit CurrentViewportFit() const {
    return GetDocument().GetViewportData().GetCurrentViewportFitForTests();
  }

 private:
  MediaControlsDisplayCutoutDelegate& GetDelegate() {
    MediaControlsImpl* controls =
        static_cast<MediaControlsImpl*>(GetVideoElement().GetMediaControls());
    return *controls->display_cutout_delegate_;
  }

  HTMLVideoElement& GetVideoElement() {
    return *ToHTMLVideoElement(GetDocument().getElementById("video"));
  }

  Persistent<DisplayCutoutMockChromeClient> chrome_client_;
};

TEST_F(MediaControlsDisplayCutoutDelegateTest, CombinedGesture) {
  SimulateEnterFullscreen();

  // Simulate the an expanding gesture but do not finish it.
  TouchList* list = CreateTouchListWithTwoPoints(1, 1, -1, -1);
  SimulateEvent(CreateTouchEventWithList(EventTypeNames::touchstart, list));
  list = CreateTouchListWithTwoPoints(4, 4, -4, -4);
  SimulateEvent(CreateTouchEventWithList(EventTypeNames::touchmove, list));

  // Check the viewport fit value has been correctly set.
  EXPECT_EQ(mojom::ViewportFit::kCoverForcedByUserAgent, CurrentViewportFit());

  // Finish the gesture by contracting.
  list = CreateTouchListWithTwoPoints(0, 0, 0, 0);
  SimulateEvent(CreateTouchEventWithList(EventTypeNames::touchend, list));

  // Check the viewport fit value has been correctly set.
  EXPECT_EQ(mojom::ViewportFit::kAuto, CurrentViewportFit());

  // Make sure we recorded a UseCounter metric.
  EXPECT_TRUE(UseCounter::IsCounted(
      GetDocument(), WebFeature::kMediaControlsDisplayCutoutGesture));
}

TEST_F(MediaControlsDisplayCutoutDelegateTest, ContractingGesture) {
  // Go fullscreen and simulate an expanding gesture.
  SimulateEnterFullscreen();
  SimulateExpandingGesture();

  // Check the viewport fit value has been correctly set.
  EXPECT_EQ(mojom::ViewportFit::kCoverForcedByUserAgent, CurrentViewportFit());

  // Simulate a contracting gesture and check the value has been restored.
  SimulateContractingGesture();
  EXPECT_EQ(mojom::ViewportFit::kAuto, CurrentViewportFit());

  // Make sure we recorded a UseCounter metric.
  EXPECT_TRUE(UseCounter::IsCounted(
      GetDocument(), WebFeature::kMediaControlsDisplayCutoutGesture));
}

TEST_F(MediaControlsDisplayCutoutDelegateTest, ContractingGesture_Noop) {
  // Go fullscreen and simulate a contracting gesture.
  SimulateEnterFullscreen();
  SimulateContractingGesture();

  // Check that the value did not change.
  EXPECT_EQ(mojom::ViewportFit::kAuto, CurrentViewportFit());
}

TEST_F(MediaControlsDisplayCutoutDelegateTest, ExpandingGesture) {
  // Go fullscreen and simulate an expanding gesture.
  SimulateEnterFullscreen();
  SimulateExpandingGesture();

  // Check the viewport fit value has been correctly set.
  EXPECT_EQ(mojom::ViewportFit::kCoverForcedByUserAgent, CurrentViewportFit());

  // Exit fullscreen and check the value has been restored.
  SimulateExitFullscreen();
  EXPECT_EQ(mojom::ViewportFit::kAuto, CurrentViewportFit());

  // Make sure we recorded a UseCounter metric.
  EXPECT_TRUE(UseCounter::IsCounted(
      GetDocument(), WebFeature::kMediaControlsDisplayCutoutGesture));
}

TEST_F(MediaControlsDisplayCutoutDelegateTest, ExpandingGesture_DoubleNoop) {
  // Go fullscreen and simulate an expanding gesture.
  SimulateEnterFullscreen();
  SimulateExpandingGesture();

  // Check the viewport fit value has been correctly set.
  EXPECT_EQ(mojom::ViewportFit::kCoverForcedByUserAgent, CurrentViewportFit());

  // Simulate another expanding gesture and make sure nothing changed.
  SimulateExpandingGesture();
  EXPECT_EQ(mojom::ViewportFit::kCoverForcedByUserAgent, CurrentViewportFit());
}

TEST_F(MediaControlsDisplayCutoutDelegateTest, IncompleteGestureClearsState) {
  SimulateEnterFullscreen();

  // Simulate a gesture and check we have state.
  TouchList* list = CreateTouchListWithTwoPoints(1, 1, -1, -1);
  SimulateEvent(CreateTouchEventWithList(EventTypeNames::touchstart, list));

  list = CreateTouchListWithTwoPoints(2, 2, -2, -2);
  SimulateEvent(CreateTouchEventWithList(EventTypeNames::touchmove, list));
  EXPECT_TRUE(DirectionIsExpanding());

  // Simulate another start gesture and make sure we do not have a direction.
  list = CreateTouchListWithTwoPoints(3, 3, -3, -3);
  SimulateEvent(CreateTouchEventWithList(EventTypeNames::touchstart, list));
  EXPECT_TRUE(DirectionIsUnknown());
}

TEST_F(MediaControlsDisplayCutoutDelegateTest, MetricsNoop) {
  EXPECT_FALSE(UseCounter::IsCounted(
      GetDocument(), WebFeature::kMediaControlsDisplayCutoutGesture));
}

TEST_F(MediaControlsDisplayCutoutDelegateTest, NoFullscreen_Noop) {
  // Simulate an expanding gesture and make sure it had no effect.
  SimulateExpandingGesture();
  EXPECT_EQ(mojom::ViewportFit::kAuto, CurrentViewportFit());
}

TEST_F(MediaControlsDisplayCutoutDelegateTest, SingleTouchGesture_Noop) {
  // Simulate a single touch gesture and make sure it had no effect.
  SimulateEnterFullscreen();
  SimulateSingleTouchGesture();
  EXPECT_EQ(mojom::ViewportFit::kAuto, CurrentViewportFit());
}

TEST_F(MediaControlsDisplayCutoutDelegateTest, TouchCancelShouldClearState) {
  SimulateEnterFullscreen();

  // Simulate a gesture and check we have state.
  TouchList* list = CreateTouchListWithTwoPoints(1, 1, -1, -1);
  SimulateEvent(CreateTouchEventWithList(EventTypeNames::touchstart, list));
  EXPECT_TRUE(HasGestureState());

  // Simulate a touchcancel gesture and check that clears the state.
  list = CreateTouchListWithTwoPoints(1, 1, -1, -1);
  SimulateEvent(CreateTouchEventWithList(EventTypeNames::touchcancel, list));
  EXPECT_FALSE(HasGestureState());
  EXPECT_EQ(mojom::ViewportFit::kAuto, CurrentViewportFit());
}

TEST_F(MediaControlsDisplayCutoutDelegateTest, TouchEndShouldClearState) {
  SimulateEnterFullscreen();

  // Simulate a gesture and check we have state.
  TouchList* list = CreateTouchListWithTwoPoints(1, 1, -1, -1);
  SimulateEvent(CreateTouchEventWithList(EventTypeNames::touchstart, list));
  EXPECT_TRUE(HasGestureState());

  // Simulate a touchend gesture and check that clears the state.
  list = CreateTouchListWithTwoPoints(1, 1, -1, -1);
  SimulateEvent(CreateTouchEventWithList(EventTypeNames::touchend, list));
  EXPECT_FALSE(HasGestureState());
  EXPECT_EQ(mojom::ViewportFit::kAuto, CurrentViewportFit());
}

}  // namespace blink

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_display_cutout_fullscreen_button_element.h"

#include "third_party/blink/public/mojom/page/display_cutout.mojom-blink.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

class MockDisplayCutoutChromeClient : public EmptyChromeClient {
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

class MediaControlDisplayCutoutFullscreenButtonElementTest
    : public PageTestBase {
 public:
  static TouchEventInit GetValidTouchEventInit() { return TouchEventInit(); }

  void SetUp() override {
    chrome_client_ = new MockDisplayCutoutChromeClient();

    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = chrome_client_.Get();
    SetupPageWithClients(&clients, EmptyLocalFrameClient::Create());

    RuntimeEnabledFeatures::SetDisplayCutoutAPIEnabled(true);

    video_ = HTMLVideoElement::Create(GetDocument());
    GetDocument().body()->AppendChild(video_);
    controls_ = new MediaControlsImpl(*video_);
    controls_->InitializeControls();
    display_cutout_fullscreen_button_ =
        controls_->display_cutout_fullscreen_button_;
  }

  mojom::ViewportFit CurrentViewportFit() const {
    return GetDocument().GetViewportData().GetCurrentViewportFitForTests();
  }

  void SimulateEnterFullscreen() {
    {
      std::unique_ptr<UserGestureIndicator> gesture =
          LocalFrame::NotifyUserActivation(GetDocument().GetFrame());
      Fullscreen::RequestFullscreen(*video_);
    }

    test::RunPendingTasks();
    GetDocument().ServiceScriptedAnimations(base::TimeTicks());

    EXPECT_TRUE(video_->IsFullscreen());
  }

  void SimulateExitFullscreen() {
    Fullscreen::FullyExitFullscreen(GetDocument());

    GetDocument().ServiceScriptedAnimations(base::TimeTicks());

    EXPECT_FALSE(video_->IsFullscreen());
  }

 protected:
  Persistent<MockDisplayCutoutChromeClient> chrome_client_;
  Persistent<HTMLVideoElement> video_;
  Persistent<MediaControlDisplayCutoutFullscreenButtonElement>
      display_cutout_fullscreen_button_;
  Persistent<MediaControlsImpl> controls_;
};

TEST_F(MediaControlDisplayCutoutFullscreenButtonElementTest,
       Fullscreen_ButtonVisiblilty) {
  EXPECT_FALSE(display_cutout_fullscreen_button_->IsWanted());

  SimulateEnterFullscreen();

  EXPECT_TRUE(display_cutout_fullscreen_button_->IsWanted());

  SimulateExitFullscreen();

  EXPECT_FALSE(display_cutout_fullscreen_button_->IsWanted());
}

TEST_F(MediaControlDisplayCutoutFullscreenButtonElementTest,
       Fullscreen_ButtonTogglesDisplayCutoutFullscreen) {
  SimulateEnterFullscreen();

  EXPECT_EQ(mojom::ViewportFit::kAuto, CurrentViewportFit());

  display_cutout_fullscreen_button_->DispatchSimulatedClick(
      nullptr, kSendNoEvents, SimulatedClickCreationScope::kFromUserAgent);
  EXPECT_EQ(mojom::ViewportFit::kCoverForcedByUserAgent, CurrentViewportFit());

  display_cutout_fullscreen_button_->DispatchSimulatedClick(
      nullptr, kSendNoEvents, SimulatedClickCreationScope::kFromUserAgent);
  EXPECT_EQ(mojom::ViewportFit::kAuto, CurrentViewportFit());
}

}  // namespace blink

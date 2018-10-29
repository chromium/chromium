// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/media_controls_rotate_to_fullscreen_delegate.h"

#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "services/device/public/mojom/screen_orientation.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_type.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/media/html_audio_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_controller.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_data.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation_controller_impl.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

using testing::AtLeast;
using testing::Return;

namespace blink {

using namespace HTMLNames;

namespace {

class MockVideoWebMediaPlayer : public EmptyWebMediaPlayer {
 public:
  // EmptyWebMediaPlayer overrides:
  bool HasVideo() const override { return true; }

  MOCK_CONST_METHOD0(NaturalSize, WebSize());
};

class MockChromeClient : public EmptyChromeClient {
 public:
  // ChromeClient overrides:
  void InstallSupplements(LocalFrame& frame) override {
    EmptyChromeClient::InstallSupplements(frame);
    ScreenOrientationControllerImpl::ProvideTo(frame);
    device::mojom::blink::ScreenOrientationAssociatedPtr screen_orientation;
    mojo::MakeRequestAssociatedWithDedicatedPipe(&screen_orientation);
    ScreenOrientationControllerImpl::From(frame)
        ->SetScreenOrientationAssociatedPtrForTests(
            std::move(screen_orientation));
  }
  void EnterFullscreen(LocalFrame& frame, const FullscreenOptions&) override {
    Fullscreen::DidEnterFullscreen(*frame.GetDocument());
  }
  void ExitFullscreen(LocalFrame& frame) override {
    Fullscreen::DidExitFullscreen(*frame.GetDocument());
  }

  MOCK_CONST_METHOD0(GetScreenInfo, WebScreenInfo());
};

class StubLocalFrameClient : public EmptyLocalFrameClient {
 public:
  static StubLocalFrameClient* Create() { return new StubLocalFrameClient; }

  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*,
      WebLayerTreeView*) override {
    return std::make_unique<MockVideoWebMediaPlayer>();
  }
};

}  // anonymous namespace

class MediaControlsRotateToFullscreenDelegateTest
    : public PageTestBase,
      private ScopedVideoFullscreenOrientationLockForTest,
      private ScopedVideoRotateToFullscreenForTest {
 public:
  MediaControlsRotateToFullscreenDelegateTest()
      : ScopedVideoFullscreenOrientationLockForTest(true),
        ScopedVideoRotateToFullscreenForTest(true) {}

 protected:
  using SimpleOrientation =
      MediaControlsRotateToFullscreenDelegate::SimpleOrientation;

  void SetUp() override {
    chrome_client_ = new MockChromeClient();

    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = chrome_client_.Get();

    SetupPageWithClients(&clients, StubLocalFrameClient::Create());
    video_ = HTMLVideoElement::Create(GetDocument());
    GetVideo().setAttribute(controlsAttr, g_empty_atom);
    // Most tests should call GetDocument().body()->AppendChild(&GetVideo());
    // This is not done automatically, so that tests control timing of `Attach`.
  }

  static bool HasDelegate(const MediaControls& media_controls) {
    return !!static_cast<const MediaControlsImpl*>(&media_controls)
                 ->rotate_to_fullscreen_delegate_;
  }

  void SimulateVideoReadyState(HTMLMediaElement::ReadyState state) {
    GetVideo().SetReadyState(state);
  }

  SimpleOrientation ObservedScreenOrientation() const {
    return GetMediaControls()
        .rotate_to_fullscreen_delegate_->current_screen_orientation_;
  }

  SimpleOrientation ComputeVideoOrientation() const {
    return GetMediaControls()
        .rotate_to_fullscreen_delegate_->ComputeVideoOrientation();
  }

  bool IsObservingVisibility() const {
    return GetMediaControls()
        .rotate_to_fullscreen_delegate_->visibility_observer_;
  }

  bool ObservedVisibility() const {
    return GetMediaControls().rotate_to_fullscreen_delegate_->is_visible_;
  }

  void DisableControls() {
    // If scripts are not enabled, controls will always be shown.
    GetFrame().GetSettings()->SetScriptEnabled(true);

    GetVideo().removeAttribute(controlsAttr);
  }

  void DispatchEvent(EventTarget& target, const AtomicString& type) {
    target.DispatchEvent(*Event::Create(type));
  }

  void InitScreenAndVideo(WebScreenOrientationType initial_screen_orientation,
                          WebSize video_size,
                          bool with_device_orientation = true);

  void PlayVideo();

  void UpdateVisibilityObserver() {
    // Let IntersectionObserver update.
    GetDocument().View()->UpdateAllLifecyclePhases();
    test::RunPendingTasks();
  }

  void RotateTo(WebScreenOrientationType new_screen_orientation);

  MockChromeClient& GetChromeClient() const { return *chrome_client_; }
  LocalDOMWindow& GetWindow() const { return *GetDocument().domWindow(); }
  HTMLVideoElement& GetVideo() const { return *video_; }
  MediaControlsImpl& GetMediaControls() const {
    return *static_cast<MediaControlsImpl*>(GetVideo().GetMediaControls());
  }
  MockVideoWebMediaPlayer& GetWebMediaPlayer() const {
    return *static_cast<MockVideoWebMediaPlayer*>(
        GetVideo().GetWebMediaPlayer());
  }

 private:
  Persistent<MockChromeClient> chrome_client_;
  Persistent<HTMLVideoElement> video_;
};

void MediaControlsRotateToFullscreenDelegateTest::InitScreenAndVideo(
    WebScreenOrientationType initial_screen_orientation,
    WebSize video_size,
    bool with_device_orientation /* = true */) {
  // Set initial screen orientation (called by `Attach` during `AppendChild`).
  WebScreenInfo screen_info;
  screen_info.orientation_type = initial_screen_orientation;
  EXPECT_CALL(GetChromeClient(), GetScreenInfo())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(screen_info));

  // Set up the WebMediaPlayer instance.
  GetDocument().body()->AppendChild(&GetVideo());
  GetVideo().SetSrc("https://example.com");
  test::RunPendingTasks();
  SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);

  // Set video size.
  EXPECT_CALL(GetWebMediaPlayer(), NaturalSize())
      .WillRepeatedly(Return(video_size));

  if (with_device_orientation) {
    // Dispatch an arbitrary Device Orientation event to satisfy
    // MediaControlsRotateToFullscreenDelegate's requirement that the device
    // supports the API and can provide beta and gamma values. The orientation
    // will be ignored.
    DeviceOrientationController::From(GetDocument())
        .SetOverride(DeviceOrientationData::Create(
            0.0 /* alpha */, 90.0 /* beta */, 0.0 /* gamma */,
            false /* absolute */));
    test::RunPendingTasks();
  }
}

void MediaControlsRotateToFullscreenDelegateTest::PlayVideo() {
  {
    std::unique_ptr<UserGestureIndicator> gesture =
        LocalFrame::NotifyUserActivation(GetDocument().GetFrame());
    GetVideo().Play();
  }
  test::RunPendingTasks();
}

void MediaControlsRotateToFullscreenDelegateTest::RotateTo(
    WebScreenOrientationType new_screen_orientation) {
  WebScreenInfo screen_info;
  screen_info.orientation_type = new_screen_orientation;
  testing::Mock::VerifyAndClearExpectations(&GetChromeClient());
  EXPECT_CALL(GetChromeClient(), GetScreenInfo())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(screen_info));
  DispatchEvent(GetWindow(), EventTypeNames::orientationchange);
  test::RunPendingTasks();
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, DelegateRequiresFlag) {
  // SetUp turns the flag on by default.
  GetDocument().body()->AppendChild(&GetVideo());
  EXPECT_TRUE(HasDelegate(GetMediaControls()));

  // No delegate when flag is off.
  ScopedVideoRotateToFullscreenForTest video_rotate_to_fullscreen(false);
  HTMLVideoElement* video = HTMLVideoElement::Create(GetDocument());
  GetDocument().body()->AppendChild(video);
  EXPECT_FALSE(HasDelegate(*video->GetMediaControls()));
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, DelegateRequiresVideo) {
  HTMLAudioElement* audio = HTMLAudioElement::Create(GetDocument());
  GetDocument().body()->AppendChild(audio);
  EXPECT_FALSE(HasDelegate(*audio->GetMediaControls()));
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, ComputeVideoOrientation) {
  // Set up the WebMediaPlayer instance.
  GetDocument().body()->AppendChild(&GetVideo());
  GetVideo().SetSrc("https://example.com");
  test::RunPendingTasks();

  // Each `ComputeVideoOrientation` calls `NaturalSize` twice, except the first
  // one where the video is not yet ready.
  EXPECT_CALL(GetWebMediaPlayer(), NaturalSize())
      .Times(12)
      .WillOnce(Return(WebSize(400, 400)))
      .WillOnce(Return(WebSize(400, 400)))
      .WillOnce(Return(WebSize(300, 200)))
      .WillOnce(Return(WebSize(300, 200)))
      .WillOnce(Return(WebSize(200, 300)))
      .WillOnce(Return(WebSize(200, 300)))
      .WillOnce(Return(WebSize(300, 199)))
      .WillOnce(Return(WebSize(300, 199)))
      .WillOnce(Return(WebSize(199, 300)))
      .WillOnce(Return(WebSize(199, 300)))
      .WillOnce(Return(WebSize(0, 0)))
      .WillOnce(Return(WebSize(0, 0)));

  // Video is not yet ready.
  EXPECT_EQ(SimpleOrientation::kUnknown, ComputeVideoOrientation());

  SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);

  // 400x400 is square, which is currently treated as landscape.
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());
  // 300x200 is landscape.
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());
  // 200x300 is portrait.
  EXPECT_EQ(SimpleOrientation::kPortrait, ComputeVideoOrientation());
  // 300x199 is too small.
  EXPECT_EQ(SimpleOrientation::kUnknown, ComputeVideoOrientation());
  // 199x300 is too small.
  EXPECT_EQ(SimpleOrientation::kUnknown, ComputeVideoOrientation());
  // 0x0 is empty.
  EXPECT_EQ(SimpleOrientation::kUnknown, ComputeVideoOrientation());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       OnlyObserveVisibilityWhenPlaying) {
  // Should not initially be observing visibility.
  GetDocument().body()->AppendChild(&GetVideo());
  EXPECT_FALSE(IsObservingVisibility());

  // Should start observing visibility when played.
  {
    std::unique_ptr<UserGestureIndicator> gesture =
        LocalFrame::NotifyUserActivation(GetDocument().GetFrame());
    GetVideo().Play();
  }
  test::RunPendingTasks();
  EXPECT_TRUE(IsObservingVisibility());
  EXPECT_FALSE(ObservedVisibility());

  // Should have observed visibility once compositor updates.
  GetDocument().View()->UpdateAllLifecyclePhases();
  test::RunPendingTasks();
  EXPECT_TRUE(ObservedVisibility());

  // Should stop observing visibility when paused.
  GetVideo().pause();
  test::RunPendingTasks();
  EXPECT_FALSE(IsObservingVisibility());
  EXPECT_FALSE(ObservedVisibility());

  // Should resume observing visibility when playback resumes.
  {
    std::unique_ptr<UserGestureIndicator> gesture =
        LocalFrame::NotifyUserActivation(GetDocument().GetFrame());
    GetVideo().Play();
  }
  test::RunPendingTasks();
  EXPECT_TRUE(IsObservingVisibility());
  EXPECT_FALSE(ObservedVisibility());

  // Should have observed visibility once compositor updates.
  GetDocument().View()->UpdateAllLifecyclePhases();
  test::RunPendingTasks();
  EXPECT_TRUE(ObservedVisibility());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterSuccessPortraitToLandscape) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationPortraitPrimary, WebSize(640, 480));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());
  EXPECT_FALSE(GetVideo().IsFullscreen());

  // Rotate screen to landscape.
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should enter fullscreen.
  EXPECT_TRUE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterSuccessLandscapeToPortrait) {
  // Landscape screen, portrait video.
  InitScreenAndVideo(kWebScreenOrientationLandscapePrimary, WebSize(480, 640));
  EXPECT_EQ(SimpleOrientation::kLandscape, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kPortrait, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());
  EXPECT_FALSE(GetVideo().IsFullscreen());

  // Rotate screen to portrait.
  RotateTo(kWebScreenOrientationPortraitPrimary);

  // Should enter fullscreen.
  EXPECT_TRUE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterSuccessSquarePortraitToLandscape) {
  // Portrait screen, square video.
  InitScreenAndVideo(kWebScreenOrientationPortraitPrimary, WebSize(400, 400));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());
  EXPECT_FALSE(GetVideo().IsFullscreen());

  // Rotate screen to landscape.
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should enter fullscreen, since square videos are currently treated the same
  // as landscape videos.
  EXPECT_TRUE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, EnterFailWrongOrientation) {
  // Landscape screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationLandscapePrimary, WebSize(640, 480));
  EXPECT_EQ(SimpleOrientation::kLandscape, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to portrait.
  RotateTo(kWebScreenOrientationPortraitPrimary);

  // Should not enter fullscreen since the orientation that the device was
  // rotated to does not match the orientation of the video.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterFailSquareWrongOrientation) {
  // Landscape screen, square video.
  InitScreenAndVideo(kWebScreenOrientationLandscapePrimary, WebSize(400, 400));
  EXPECT_EQ(SimpleOrientation::kLandscape, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to portrait.
  RotateTo(kWebScreenOrientationPortraitPrimary);

  // Should not enter fullscreen since square videos are treated as landscape,
  // so rotating to portrait does not match the orientation of the video.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, EnterFailNoControls) {
  DisableControls();

  // Portrait screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationPortraitPrimary, WebSize(640, 480));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should not enter fullscreen since video has no controls.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterFailNoDeviceOrientation) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationPortraitPrimary, WebSize(640, 480),
                     false /* with_device_orientation */);
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Dispatch an null Device Orientation event, as happens when the device lacks
  // the necessary hardware to support the Device Orientation API.
  DeviceOrientationController::From(GetDocument())
      .SetOverride(DeviceOrientationData::Create());
  test::RunPendingTasks();

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should not enter fullscreen since Device Orientation is not available.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterFailZeroDeviceOrientation) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationPortraitPrimary, WebSize(640, 480),
                     false /* with_device_orientation */);
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Dispatch a Device Orientation event where all values are zero, as happens
  // on poorly configured devices that lack the necessary hardware to support
  // the Device Orientation API, but don't properly expose that lack.
  DeviceOrientationController::From(GetDocument())
      .SetOverride(
          DeviceOrientationData::Create(0.0 /* alpha */, 0.0 /* beta */,
                                        0.0 /* gamma */, false /* absolute */));
  test::RunPendingTasks();

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should not enter fullscreen since Device Orientation is not available.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, EnterFailPaused) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationPortraitPrimary, WebSize(640, 480));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  EXPECT_FALSE(ObservedVisibility());

  UpdateVisibilityObserver();

  EXPECT_FALSE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should not enter fullscreen since video is paused.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, EnterFailHidden) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationPortraitPrimary, WebSize(640, 480));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Move video offscreen.
  GetDocument().body()->style()->setProperty(&GetDocument(), "margin-top",
                                             "-999px", "", ASSERT_NO_EXCEPTION);

  UpdateVisibilityObserver();

  EXPECT_FALSE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should not enter fullscreen since video is not visible.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterFail180DegreeRotation) {
  // Landscape screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationLandscapeSecondary,
                     WebSize(640, 480));
  EXPECT_EQ(SimpleOrientation::kLandscape, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen 180 degrees to the opposite landscape (without passing via a
  // portrait orientation).
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should not enter fullscreen since this is a 180 degree orientation.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, EnterFailSmall) {
  // Portrait screen, small landscape video.
  InitScreenAndVideo(kWebScreenOrientationPortraitPrimary, WebSize(300, 199));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kUnknown, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should not enter fullscreen since video is too small.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterFailDocumentFullscreen) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationPortraitPrimary, WebSize(640, 480));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Simulate the webpage requesting fullscreen on some other element than the
  // video (in this case document.body).
  {
    std::unique_ptr<UserGestureIndicator> gesture =
        LocalFrame::NotifyUserActivation(GetDocument().GetFrame());
    Fullscreen::RequestFullscreen(*GetDocument().body());
  }
  test::RunPendingTasks();
  EXPECT_TRUE(Fullscreen::IsFullscreenElement(*GetDocument().body()));
  EXPECT_FALSE(GetVideo().IsFullscreen());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should not enter fullscreen on video, since document is already fullscreen.
  EXPECT_TRUE(Fullscreen::IsFullscreenElement(*GetDocument().body()));
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       ExitSuccessLandscapeFullscreenToPortraitInline) {
  // Landscape screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationLandscapePrimary, WebSize(640, 480));
  EXPECT_EQ(SimpleOrientation::kLandscape, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Start in fullscreen.
  {
    std::unique_ptr<UserGestureIndicator> gesture =
        LocalFrame::NotifyUserActivation(GetDocument().GetFrame());
    GetMediaControls().EnterFullscreen();
  }
  // n.b. omit to call Fullscreen::From(GetDocument()).DidEnterFullscreen() so
  // that MediaControlsOrientationLockDelegate doesn't trigger, which avoids
  // having to create deviceorientation events here to unlock it again.
  test::RunPendingTasks();
  EXPECT_TRUE(GetVideo().IsFullscreen());

  // Leave video paused (playing is not a requirement to exit fullscreen).
  EXPECT_TRUE(GetVideo().paused());
  EXPECT_FALSE(ObservedVisibility());

  // Rotate screen to portrait. This relies on the screen orientation not being
  // locked by MediaControlsOrientationLockDelegate (which has its own tests).
  RotateTo(kWebScreenOrientationPortraitPrimary);

  // Should exit fullscreen.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       ExitSuccessPortraitFullscreenToLandscapeInline) {
  // Portrait screen, portrait video.
  InitScreenAndVideo(kWebScreenOrientationPortraitPrimary, WebSize(480, 640));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kPortrait, ComputeVideoOrientation());

  // Start in fullscreen.
  {
    std::unique_ptr<UserGestureIndicator> gesture =
        LocalFrame::NotifyUserActivation(GetDocument().GetFrame());
    GetMediaControls().EnterFullscreen();
  }
  // n.b. omit to call Fullscreen::From(GetDocument()).DidEnterFullscreen() so
  // that MediaControlsOrientationLockDelegate doesn't trigger, which avoids
  // having to create deviceorientation events here to unlock it again.
  test::RunPendingTasks();
  EXPECT_TRUE(GetVideo().IsFullscreen());

  // Leave video paused (playing is not a requirement to exit fullscreen).
  EXPECT_TRUE(GetVideo().paused());
  EXPECT_FALSE(ObservedVisibility());

  // Rotate screen to landscape. This relies on the screen orientation not being
  // locked by MediaControlsOrientationLockDelegate (which has its own tests).
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should exit fullscreen.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       ExitFailDocumentFullscreen) {
  // Landscape screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationLandscapePrimary, WebSize(640, 480));
  EXPECT_EQ(SimpleOrientation::kLandscape, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Simulate the webpage requesting fullscreen on some other element than the
  // video (in this case document.body).
  {
    std::unique_ptr<UserGestureIndicator> gesture =
        LocalFrame::NotifyUserActivation(GetDocument().GetFrame());
    Fullscreen::RequestFullscreen(*GetDocument().body());
  }
  test::RunPendingTasks();
  EXPECT_TRUE(Fullscreen::IsFullscreenElement(*GetDocument().body()));
  EXPECT_FALSE(GetVideo().IsFullscreen());

  // Leave video paused (playing is not a requirement to exit fullscreen).
  EXPECT_TRUE(GetVideo().paused());
  EXPECT_FALSE(ObservedVisibility());

  // Rotate screen to portrait.
  RotateTo(kWebScreenOrientationPortraitPrimary);

  // Should not exit fullscreen, since video was not the fullscreen element.
  EXPECT_TRUE(Fullscreen::IsFullscreenElement(*GetDocument().body()));
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterFailControlsListNoFullscreen) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationPortraitPrimary, WebSize(640, 480));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  EXPECT_FALSE(ObservedVisibility());

  GetVideo().setAttribute("controlslist", "nofullscreen");

  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should not enter fullscreen when controlsList=nofullscreen.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterSuccessControlsListNoDownload) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationPortraitPrimary, WebSize(640, 480));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  EXPECT_FALSE(ObservedVisibility());

  GetVideo().setAttribute("controlslist", "nodownload");

  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should enter fullscreen when controlsList is not set to nofullscreen.
  EXPECT_TRUE(GetVideo().IsFullscreen());
}

}  // namespace blink

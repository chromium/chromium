// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/media_controls_rotate_to_fullscreen_delegate.h"

#include <tuple>

#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/device/public/mojom/screen_orientation.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/media/html_audio_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_controller.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_data.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation_controller.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "ui/display/mojom/screen_orientation.mojom-blink.h"

namespace blink {

namespace {

class MockVideoWebMediaPlayer : public EmptyWebMediaPlayer {
 public:
  ~MockVideoWebMediaPlayer() override = default;

  // EmptyWebMediaPlayer overrides:
  bool HasVideo() const override { return true; }
  gfx::Size NaturalSize() const override { return mock_natural_size_; }

  gfx::Size& MockNaturalSize() { return mock_natural_size_; }

 private:
  gfx::Size mock_natural_size_ = {};
};

class MockChromeClient : public EmptyChromeClient {
 public:
  // ChromeClient overrides:
  void InstallSupplements(LocalFrame& frame) override {
    EmptyChromeClient::InstallSupplements(frame);
    HeapMojoAssociatedRemote<device::mojom::blink::ScreenOrientation>
        screen_orientation(frame.DomWindow());
    std::ignore = screen_orientation.BindNewEndpointAndPassDedicatedReceiver();
    ScreenOrientationController::From(*frame.DomWindow())
        ->SetScreenOrientationAssociatedRemoteForTests(
            std::move(screen_orientation));
  }
  void EnterFullscreen(LocalFrame& frame,
                       const FullscreenOptions*,
                       FullscreenRequestType) override {
    Fullscreen::DidResolveEnterFullscreenRequest(*frame.GetDocument(),
                                                 true /* granted */);
  }
  void ExitFullscreen(LocalFrame& frame) override {
    Fullscreen::DidExitFullscreen(*frame.GetDocument());
  }

  const display::ScreenInfo& GetScreenInfo(LocalFrame&) const override {
    return mock_screen_info_;
  }

  display::ScreenInfo& MockScreenInfo() { return mock_screen_info_; }

 private:
  display::ScreenInfo mock_screen_info_ = {};
};

class StubLocalFrameClient : public EmptyLocalFrameClient {
 public:
  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*) override {
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
    chrome_client_ = MakeGarbageCollected<MockChromeClient>();
    SetupPageWithClients(chrome_client_,
                         MakeGarbageCollected<StubLocalFrameClient>());
    video_ = MakeGarbageCollected<HTMLVideoElement>(GetDocument());
    GetVideo().setAttribute(html_names::kControlsAttr, g_empty_atom);
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
               .rotate_to_fullscreen_delegate_->intersection_observer_ !=
           nullptr;
  }

  bool ObservedVisibility() const {
    return GetMediaControls().rotate_to_fullscreen_delegate_->is_visible_;
  }

  void DisableControls() {
    // If scripts are not enabled, controls will always be shown.
    GetFrame().GetSettings()->SetScriptEnabled(true);

    GetVideo().removeAttribute(html_names::kControlsAttr);
  }

  void DispatchEvent(EventTarget& target, const AtomicString& type) {
    target.DispatchEvent(*Event::Create(type));
  }

  void InitScreenAndVideo(
      display::mojom::blink::ScreenOrientation initial_screen_orientation,
      gfx::Size video_size,
      bool with_device_orientation = true);

  void PlayVideo();

  void UpdateVisibilityObserver() {
    // Let IntersectionObserver update.
    UpdateAllLifecyclePhasesForTest();
    test::RunPendingTasks();
  }

  void RotateTo(
      display::mojom::blink::ScreenOrientation new_screen_orientation);

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
    display::mojom::blink::ScreenOrientation initial_screen_orientation,
    gfx::Size video_size,
    bool with_device_orientation /* = true */) {
  // Set initial screen orientation (called by `Attach` during `AppendChild`).
  GetChromeClient().MockScreenInfo().orientation_type =
      initial_screen_orientation;

  // Set up the WebMediaPlayer instance.
  GetDocument().body()->AppendChild(&GetVideo());
  GetVideo().SetSrc(AtomicString("https://example.com"));
  test::RunPendingTasks();
  SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);

  // Set video size.
  GetWebMediaPlayer().MockNaturalSize() = video_size;

  if (with_device_orientation) {
    // Dispatch an arbitrary Device Orientation event to satisfy
    // MediaControlsRotateToFullscreenDelegate's requirement that the device
    // supports the API and can provide beta and gamma values. The orientation
    // will be ignored.
    DeviceOrientationController::From(GetWindow())
        .SetOverride(DeviceOrientationData::Create(
            0.0 /* alpha */, 90.0 /* beta */, 0.0 /* gamma */,
            false /* absolute */));
    test::RunPendingTasks();
  }
}

void MediaControlsRotateToFullscreenDelegateTest::PlayVideo() {
  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  GetVideo().Play();
  test::RunPendingTasks();
}

void MediaControlsRotateToFullscreenDelegateTest::RotateTo(
    display::mojom::blink::ScreenOrientation new_screen_orientation) {
  GetChromeClient().MockScreenInfo().orientation_type = new_screen_orientation;
  DispatchEvent(GetWindow(), event_type_names::kOrientationchange);
  test::RunPendingTasks();
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, DelegateRequiresFlag) {
  // SetUp turns the flag on by default.
  GetDocument().body()->AppendChild(&GetVideo());
  EXPECT_TRUE(HasDelegate(GetMediaControls()));

  // No delegate when flag is off.
  ScopedVideoRotateToFullscreenForTest video_rotate_to_fullscreen(false);
  auto* video = MakeGarbageCollected<HTMLVideoElement>(GetDocument());
  GetDocument().body()->AppendChild(video);
  EXPECT_FALSE(HasDelegate(*video->GetMediaControls()));
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, DelegateRequiresVideo) {
  auto* audio = MakeGarbageCollected<HTMLAudioElement>(GetDocument());
  GetDocument().body()->AppendChild(audio);
  EXPECT_FALSE(HasDelegate(*audio->GetMediaControls()));
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, ComputeVideoOrientation) {
  // Set up the WebMediaPlayer instance.
  GetDocument().body()->AppendChild(&GetVideo());
  GetVideo().SetSrc(AtomicString("https://example.com"));
  test::RunPendingTasks();

  // Video is not yet ready.
  EXPECT_EQ(SimpleOrientation::kUnknown, ComputeVideoOrientation());

  SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);

  // 400x400 is square, which is currently treated as landscape.
  GetWebMediaPlayer().MockNaturalSize() = gfx::Size(400, 400);
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());
  // 300x200 is landscape.
  GetWebMediaPlayer().MockNaturalSize() = gfx::Size(300, 200);
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());
  // 200x300 is portrait.
  GetWebMediaPlayer().MockNaturalSize() = gfx::Size(200, 300);
  EXPECT_EQ(SimpleOrientation::kPortrait, ComputeVideoOrientation());
  // 300x199 is too small.
  GetWebMediaPlayer().MockNaturalSize() = gfx::Size(300, 199);
  EXPECT_EQ(SimpleOrientation::kUnknown, ComputeVideoOrientation());
  // 199x300 is too small.
  GetWebMediaPlayer().MockNaturalSize() = gfx::Size(199, 300);
  EXPECT_EQ(SimpleOrientation::kUnknown, ComputeVideoOrientation());
  // 0x0 is empty.
  GetWebMediaPlayer().MockNaturalSize() = gfx::Size(0, 0);
  EXPECT_EQ(SimpleOrientation::kUnknown, ComputeVideoOrientation());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       OnlyObserveVisibilityWhenPlaying) {
  // Should not initially be observing visibility.
  GetDocument().body()->AppendChild(&GetVideo());
  EXPECT_FALSE(IsObservingVisibility());

  // Should start observing visibility when played.
  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  GetVideo().Play();
  test::RunPendingTasks();
  EXPECT_TRUE(IsObservingVisibility());
  EXPECT_FALSE(ObservedVisibility());

  // Should have observed visibility once compositor updates.
  UpdateAllLifecyclePhasesForTest();
  test::RunPendingTasks();
  EXPECT_TRUE(ObservedVisibility());

  // Should stop observing visibility when paused.
  GetVideo().pause();
  test::RunPendingTasks();
  EXPECT_FALSE(IsObservingVisibility());
  EXPECT_FALSE(ObservedVisibility());

  // Should resume observing visibility when playback resumes.
  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  GetVideo().Play();
  test::RunPendingTasks();
  EXPECT_TRUE(IsObservingVisibility());
  EXPECT_FALSE(ObservedVisibility());

  // Should have observed visibility once compositor updates.
  UpdateAllLifecyclePhasesForTest();
  test::RunPendingTasks();
  EXPECT_TRUE(ObservedVisibility());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterSuccessPortraitToLandscape) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(display::mojom::blink::ScreenOrientation::kPortraitPrimary,
                     gfx::Size(640, 480));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());
  EXPECT_FALSE(GetVideo().IsFullscreen());

  // Rotate screen to landscape.
  RotateTo(display::mojom::blink::ScreenOrientation::kLandscapePrimary);

  // Should enter fullscreen.
  EXPECT_TRUE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterSuccessLandscapeToPortrait) {
  // Landscape screen, portrait video.
  InitScreenAndVideo(
      display::mojom::blink::ScreenOrientation::kLandscapePrimary,
      gfx::Size(480, 640));
  EXPECT_EQ(SimpleOrientation::kLandscape, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kPortrait, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());
  EXPECT_FALSE(GetVideo().IsFullscreen());

  // Rotate screen to portrait.
  RotateTo(display::mojom::blink::ScreenOrientation::kPortraitPrimary);

  // Should enter fullscreen.
  EXPECT_TRUE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterSuccessSquarePortraitToLandscape) {
  // Portrait screen, square video.
  InitScreenAndVideo(display::mojom::blink::ScreenOrientation::kPortraitPrimary,
                     gfx::Size(400, 400));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());
  EXPECT_FALSE(GetVideo().IsFullscreen());

  // Rotate screen to landscape.
  RotateTo(display::mojom::blink::ScreenOrientation::kLandscapePrimary);

  // Should enter fullscreen, since square videos are currently treated the same
  // as landscape videos.
  EXPECT_TRUE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, EnterFailWrongOrientation) {
  // Landscape screen, landscape video.
  InitScreenAndVideo(
      display::mojom::blink::ScreenOrientation::kLandscapePrimary,
      gfx::Size(640, 480));
  EXPECT_EQ(SimpleOrientation::kLandscape, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to portrait.
  RotateTo(display::mojom::blink::ScreenOrientation::kPortraitPrimary);

  // Should not enter fullscreen since the orientation that the device was
  // rotated to does not match the orientation of the video.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterFailSquareWrongOrientation) {
  // Landscape screen, square video.
  InitScreenAndVideo(
      display::mojom::blink::ScreenOrientation::kLandscapePrimary,
      gfx::Size(400, 400));
  EXPECT_EQ(SimpleOrientation::kLandscape, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to portrait.
  RotateTo(display::mojom::blink::ScreenOrientation::kPortraitPrimary);

  // Should not enter fullscreen since square videos are treated as landscape,
  // so rotating to portrait does not match the orientation of the video.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, EnterFailNoControls) {
  DisableControls();

  // Portrait screen, landscape video.
  InitScreenAndVideo(display::mojom::blink::ScreenOrientation::kPortraitPrimary,
                     gfx::Size(640, 480));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(display::mojom::blink::ScreenOrientation::kLandscapePrimary);

  // Should not enter fullscreen since video has no controls.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterFailNoDeviceOrientation) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(display::mojom::blink::ScreenOrientation::kPortraitPrimary,
                     gfx::Size(640, 480), false /* with_device_orientation */);
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Dispatch an null Device Orientation event, as happens when the device lacks
  // the necessary hardware to support the Device Orientation API.
  DeviceOrientationController::From(GetWindow())
      .SetOverride(DeviceOrientationData::Create());
  test::RunPendingTasks();

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(display::mojom::blink::ScreenOrientation::kLandscapePrimary);

  // Should not enter fullscreen since Device Orientation is not available.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterFailZeroDeviceOrientation) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(display::mojom::blink::ScreenOrientation::kPortraitPrimary,
                     gfx::Size(640, 480), false /* with_device_orientation */);
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Dispatch a Device Orientation event where all values are zero, as happens
  // on poorly configured devices that lack the necessary hardware to support
  // the Device Orientation API, but don't properly expose that lack.
  DeviceOrientationController::From(GetWindow())
      .SetOverride(
          DeviceOrientationData::Create(0.0 /* alpha */, 0.0 /* beta */,
                                        0.0 /* gamma */, false /* absolute */));
  test::RunPendingTasks();

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(display::mojom::blink::ScreenOrientation::kLandscapePrimary);

  // Should not enter fullscreen since Device Orientation is not available.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, EnterFailPaused) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(display::mojom::blink::ScreenOrientation::kPortraitPrimary,
                     gfx::Size(640, 480));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  EXPECT_FALSE(ObservedVisibility());

  UpdateVisibilityObserver();

  EXPECT_FALSE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(display::mojom::blink::ScreenOrientation::kLandscapePrimary);

  // Should not enter fullscreen since video is paused.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, EnterFailHidden) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(display::mojom::blink::ScreenOrientation::kPortraitPrimary,
                     gfx::Size(640, 480));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Move video offscreen.
  GetDocument().body()->style()->setProperty(
      GetDocument().GetExecutionContext(), "margin-top", "-999px", "",
      ASSERT_NO_EXCEPTION);

  UpdateVisibilityObserver();

  EXPECT_FALSE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(display::mojom::blink::ScreenOrientation::kLandscapePrimary);

  // Should not enter fullscreen since video is not visible.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterFail180DegreeRotation) {
  // Landscape screen, landscape video.
  InitScreenAndVideo(
      display::mojom::blink::ScreenOrientation::kLandscapeSecondary,
      gfx::Size(640, 480));
  EXPECT_EQ(SimpleOrientation::kLandscape, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen 180 degrees to the opposite landscape (without passing via a
  // portrait orientation).
  RotateTo(display::mojom::blink::ScreenOrientation::kLandscapePrimary);

  // Should not enter fullscreen since this is a 180 degree orientation.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, EnterFailSmall) {
  // Portrait screen, small landscape video.
  InitScreenAndVideo(display::mojom::blink::ScreenOrientation::kPortraitPrimary,
                     gfx::Size(300, 199));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kUnknown, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(display::mojom::blink::ScreenOrientation::kLandscapePrimary);

  // Should not enter fullscreen since video is too small.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterFailDocumentFullscreen) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(display::mojom::blink::ScreenOrientation::kPortraitPrimary,
                     gfx::Size(640, 480));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Simulate the webpage requesting fullscreen on some other element than the
  // video (in this case document.body).
  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*GetDocument().body());
  test::RunPendingTasks();
  EXPECT_TRUE(Fullscreen::IsFullscreenElement(*GetDocument().body()));
  EXPECT_FALSE(GetVideo().IsFullscreen());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(display::mojom::blink::ScreenOrientation::kLandscapePrimary);

  // Should not enter fullscreen on video, since document is already fullscreen.
  EXPECT_TRUE(Fullscreen::IsFullscreenElement(*GetDocument().body()));
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       ExitSuccessLandscapeFullscreenToPortraitInline) {
  // Landscape screen, landscape video.
  InitScreenAndVideo(
      display::mojom::blink::ScreenOrientation::kLandscapePrimary,
      gfx::Size(640, 480));
  EXPECT_EQ(SimpleOrientation::kLandscape, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Start in fullscreen.
  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  GetMediaControls().EnterFullscreen();
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
  RotateTo(display::mojom::blink::ScreenOrientation::kPortraitPrimary);

  // Should exit fullscreen.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       ExitSuccessPortraitFullscreenToLandscapeInline) {
  // Portrait screen, portrait video.
  InitScreenAndVideo(display::mojom::blink::ScreenOrientation::kPortraitPrimary,
                     gfx::Size(480, 640));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kPortrait, ComputeVideoOrientation());

  // Start in fullscreen.
  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  GetMediaControls().EnterFullscreen();
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
  RotateTo(display::mojom::blink::ScreenOrientation::kLandscapePrimary);

  // Should exit fullscreen.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       ExitFailDocumentFullscreen) {
  // Landscape screen, landscape video.
  InitScreenAndVideo(
      display::mojom::blink::ScreenOrientation::kLandscapePrimary,
      gfx::Size(640, 480));
  EXPECT_EQ(SimpleOrientation::kLandscape, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Simulate the webpage requesting fullscreen on some other element than the
  // video (in this case document.body).
  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*GetDocument().body());
  test::RunPendingTasks();
  EXPECT_TRUE(Fullscreen::IsFullscreenElement(*GetDocument().body()));
  EXPECT_FALSE(GetVideo().IsFullscreen());

  // Leave video paused (playing is not a requirement to exit fullscreen).
  EXPECT_TRUE(GetVideo().paused());
  EXPECT_FALSE(ObservedVisibility());

  // Rotate screen to portrait.
  RotateTo(display::mojom::blink::ScreenOrientation::kPortraitPrimary);

  // Should not exit fullscreen, since video was not the fullscreen element.
  EXPECT_TRUE(Fullscreen::IsFullscreenElement(*GetDocument().body()));
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterFailControlsListNoFullscreen) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(display::mojom::blink::ScreenOrientation::kPortraitPrimary,
                     gfx::Size(640, 480));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  EXPECT_FALSE(ObservedVisibility());

  GetVideo().setAttribute(AtomicString("controlslist"),
                          AtomicString("nofullscreen"));

  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(display::mojom::blink::ScreenOrientation::kLandscapePrimary);

  // Should not enter fullscreen when controlsList=nofullscreen.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, EnterFailPictureInPicture) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(display::mojom::blink::ScreenOrientation::kPortraitPrimary,
                     gfx::Size(640, 480));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  EXPECT_FALSE(ObservedVisibility());

  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Simulate Picture-in-Picture.
  GetVideo().SetPersistentState(true);

  // Rotate screen to landscape.
  RotateTo(display::mojom::blink::ScreenOrientation::kLandscapePrimary);

  // Should not enter fullscreen when Picture-in-Picture.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterSuccessControlsListNoDownload) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(display::mojom::blink::ScreenOrientation::kPortraitPrimary,
                     gfx::Size(640, 480));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  EXPECT_FALSE(ObservedVisibility());

  GetVideo().setAttribute(AtomicString("controlslist"),
                          AtomicString("nodownload"));

  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(display::mojom::blink::ScreenOrientation::kLandscapePrimary);

  // Should enter fullscreen when controlsList is not set to nofullscreen.
  EXPECT_TRUE(GetVideo().IsFullscreen());
}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/media_controls_orientation_lock_delegate.h"

#include <memory>

#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "services/device/public/mojom/screen_orientation.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/frame/frame_view.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/screen_orientation_controller.h"
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
#include "third_party/blink/renderer/modules/screen_orientation/web_lock_orientation_callback.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/layout_test_support.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using testing::_;
using testing::AtLeast;
using testing::Return;

namespace blink {

namespace {

// WebLockOrientationCallback implementation that will not react to a success
// nor a failure.
class DummyScreenOrientationCallback final : public WebLockOrientationCallback {
 public:
  void OnSuccess() override {}
  void OnError(WebLockOrientationError) override {}
};

class MockWebMediaPlayerForOrientationLockDelegate final
    : public EmptyWebMediaPlayer {
 public:
  bool HasVideo() const override { return true; }

  MOCK_CONST_METHOD0(NaturalSize, WebSize());
};

class MockScreenOrientation final
    : public device::mojom::blink::ScreenOrientation {
 public:
  MockScreenOrientation() : binding_(this) {}

  // device::mojom::blink::ScreenOrientation overrides:
  void LockOrientation(WebScreenOrientationLockType type,
                       LockOrientationCallback callback) override {
    std::move(callback).Run(device::mojom::ScreenOrientationLockResult::
                                SCREEN_ORIENTATION_LOCK_RESULT_SUCCESS);
    LockOrientation(type);
  }

  void BindRequest(
      device::mojom::blink::ScreenOrientationAssociatedRequest request) {
    binding_.Bind(std::move(request));
  }

  void Close() { binding_.Close(); }

  MOCK_METHOD0(UnlockOrientation, void());

  MOCK_METHOD1(LockOrientation, void(WebScreenOrientationLockType));

 private:
  mojo::AssociatedBinding<device::mojom::blink::ScreenOrientation> binding_;
};

void DidEnterFullscreen(Document* document) {
  DCHECK(document);
  Fullscreen::DidEnterFullscreen(*document);
  document->ServiceScriptedAnimations(WTF::CurrentTimeTicks());
}

void DidExitFullscreen(Document* document) {
  DCHECK(document);
  Fullscreen::DidExitFullscreen(*document);
  document->ServiceScriptedAnimations(WTF::CurrentTimeTicks());
}

class MockChromeClientForOrientationLockDelegate final
    : public EmptyChromeClient {
 public:
  // ChromeClient overrides:
  void InstallSupplements(LocalFrame& frame) override {
    EmptyChromeClient::InstallSupplements(frame);
    ScreenOrientationControllerImpl::ProvideTo(frame);
    device::mojom::blink::ScreenOrientationAssociatedPtr screen_orientation;
    ScreenOrientationClient().BindRequest(
        mojo::MakeRequestAssociatedWithDedicatedPipe(&screen_orientation));
    ScreenOrientationControllerImpl::From(frame)
        ->SetScreenOrientationAssociatedPtrForTests(
            std::move(screen_orientation));
  }
  // The real ChromeClient::EnterFullscreen/ExitFullscreen implementation is
  // async due to IPC, emulate that by posting tasks:
  void EnterFullscreen(LocalFrame& frame, const FullscreenOptions&) override {
    Platform::Current()->CurrentThread()->GetTaskRunner()->PostTask(
        FROM_HERE,
        WTF::Bind(DidEnterFullscreen, WrapPersistent(frame.GetDocument())));
  }
  void ExitFullscreen(LocalFrame& frame) override {
    Platform::Current()->CurrentThread()->GetTaskRunner()->PostTask(
        FROM_HERE,
        WTF::Bind(DidExitFullscreen, WrapPersistent(frame.GetDocument())));
  }

  MOCK_CONST_METHOD0(GetScreenInfo, WebScreenInfo());

  MockScreenOrientation& ScreenOrientationClient() {
    return mock_screen_orientation_;
  }

 private:
  MockScreenOrientation mock_screen_orientation_;
};

class StubLocalFrameClientForOrientationLockDelegate final
    : public EmptyLocalFrameClient {
 public:
  static StubLocalFrameClientForOrientationLockDelegate* Create() {
    return new StubLocalFrameClientForOrientationLockDelegate;
  }

  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*,
      WebLayerTreeView*) override {
    return std::make_unique<MockWebMediaPlayerForOrientationLockDelegate>();
  }
};

}  // anonymous namespace

class MediaControlsOrientationLockDelegateTest
    : public PageTestBase,
      private ScopedVideoFullscreenOrientationLockForTest,
      private ScopedVideoRotateToFullscreenForTest {
 public:
  MediaControlsOrientationLockDelegateTest()
      : ScopedVideoFullscreenOrientationLockForTest(true),
        ScopedVideoRotateToFullscreenForTest(false) {}

  MediaControlsOrientationLockDelegateTest(bool video_rotate_to_fullscreen)
      : ScopedVideoFullscreenOrientationLockForTest(true),
        ScopedVideoRotateToFullscreenForTest(video_rotate_to_fullscreen) {}

 protected:
  using DeviceOrientationType =
      MediaControlsOrientationLockDelegate::DeviceOrientationType;

  static constexpr TimeDelta GetUnlockDelay() {
    return MediaControlsOrientationLockDelegate::kLockToAnyDelay;
  }

  void SetUp() override {
    chrome_client_ = new MockChromeClientForOrientationLockDelegate();

    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = chrome_client_.Get();

    SetupPageWithClients(
        &clients, StubLocalFrameClientForOrientationLockDelegate::Create());
    previous_orientation_event_value_ =
        RuntimeEnabledFeatures::OrientationEventEnabled();

    GetDocument().write("<body><video></body>");
    video_ = ToHTMLVideoElement(*GetDocument().QuerySelector("video"));
  }

  void TearDown() override {
    testing::Mock::VerifyAndClear(&ScreenOrientationClient());
    ScreenOrientationClient().Close();
  }

  static bool HasDelegate(const MediaControls& media_controls) {
    return !!static_cast<const MediaControlsImpl*>(&media_controls)
                 ->orientation_lock_delegate_;
  }

  void SimulateEnterFullscreen() {
    std::unique_ptr<UserGestureIndicator> gesture =
        LocalFrame::NotifyUserActivation(GetDocument().GetFrame());
    Fullscreen::RequestFullscreen(Video());
    test::RunPendingTasks();
  }

  void SimulateExitFullscreen() {
    Fullscreen::ExitFullscreen(GetDocument());
    test::RunPendingTasks();
  }

  void SimulateOrientationLock() {
    ScreenOrientationController* controller =
        ScreenOrientationController::From(*GetDocument().GetFrame());
    controller->lock(kWebScreenOrientationLockLandscape,
                     std::make_unique<DummyScreenOrientationCallback>());
    EXPECT_TRUE(controller->MaybeHasActiveLock());
  }

  void SimulateVideoReadyState(HTMLMediaElement::ReadyState state) {
    Video().SetReadyState(state);
  }

  void SimulateVideoNetworkState(HTMLMediaElement::NetworkState state) {
    Video().SetNetworkState(state);
  }

  MediaControlsImpl* MediaControls() const {
    return static_cast<MediaControlsImpl*>(video_->GetMediaControls());
  }

  void CheckStatePendingFullscreen() const {
    EXPECT_EQ(MediaControlsOrientationLockDelegate::State::kPendingFullscreen,
              MediaControls()->orientation_lock_delegate_->state_);
  }

  void CheckStatePendingMetadata() const {
    EXPECT_EQ(MediaControlsOrientationLockDelegate::State::kPendingMetadata,
              MediaControls()->orientation_lock_delegate_->state_);
  }

  void CheckStateMaybeLockedFullscreen() const {
    EXPECT_EQ(
        MediaControlsOrientationLockDelegate::State::kMaybeLockedFullscreen,
        MediaControls()->orientation_lock_delegate_->state_);
  }

  bool DelegateWillUnlockFullscreen() const {
    return DelegateOrientationLock() !=
           kWebScreenOrientationLockDefault /* unlocked */;
  }

  WebScreenOrientationLockType DelegateOrientationLock() const {
    return MediaControls()->orientation_lock_delegate_->locked_orientation_;
  }

  WebScreenOrientationLockType ComputeOrientationLock() const {
    return MediaControls()
        ->orientation_lock_delegate_->ComputeOrientationLock();
  }

  MockChromeClientForOrientationLockDelegate& ChromeClient() const {
    return *chrome_client_;
  }

  HTMLVideoElement& Video() const { return *video_; }
  MockScreenOrientation& ScreenOrientationClient() const {
    return ChromeClient().ScreenOrientationClient();
  }
  MockWebMediaPlayerForOrientationLockDelegate& MockWebMediaPlayer() const {
    return *static_cast<MockWebMediaPlayerForOrientationLockDelegate*>(
        Video().GetWebMediaPlayer());
  }

 private:
  friend class MediaControlsOrientationLockAndRotateToFullscreenDelegateTest;

  bool previous_orientation_event_value_;
  Persistent<HTMLVideoElement> video_;
  Persistent<MockChromeClientForOrientationLockDelegate> chrome_client_;
};

class MediaControlsOrientationLockAndRotateToFullscreenDelegateTest
    : public MediaControlsOrientationLockDelegateTest,
      private ScopedOrientationEventForTest {
 public:
  MediaControlsOrientationLockAndRotateToFullscreenDelegateTest()
      : MediaControlsOrientationLockDelegateTest(true),
        ScopedOrientationEventForTest(true) {}

 protected:
  enum DeviceNaturalOrientation { kNaturalIsPortrait, kNaturalIsLandscape };

  void SetUp() override {
    // Unset this to fix ScreenOrientationControllerImpl::ComputeOrientation.
    // TODO(mlamouri): Refactor to avoid this (crbug.com/726817).
    was_running_layout_test_ = LayoutTestSupport::IsRunningLayoutTest();
    LayoutTestSupport::SetIsRunningLayoutTest(false);

    MediaControlsOrientationLockDelegateTest::SetUp();

    // Reset the <video> element now we've enabled the runtime feature.
    video_->parentElement()->RemoveChild(video_);
    video_ = HTMLVideoElement::Create(GetDocument());
    video_->setAttribute(HTMLNames::controlsAttr, g_empty_atom);
    // Most tests should call GetDocument().body()->AppendChild(&Video());
    // This is not done automatically, so that tests control timing of `Attach`,
    // which is important for MediaControlsRotateToFullscreenDelegate since
    // that's when it reads the initial screen orientation.
  }

  void TearDown() override {
    MediaControlsOrientationLockDelegateTest::TearDown();
    LayoutTestSupport::SetIsRunningLayoutTest(was_running_layout_test_);
  }

  void SetIsAutoRotateEnabledByUser(bool enabled) {
    MediaControls()
        ->orientation_lock_delegate_
        ->is_auto_rotate_enabled_by_user_override_for_testing_ = enabled;
  }

  WebRect ScreenRectFromAngle(uint16_t screen_orientation_angle) {
    uint16_t portrait_angle_mod_180 = natural_orientation_is_portrait_ ? 0 : 90;
    bool screen_rect_is_portrait =
        screen_orientation_angle % 180 == portrait_angle_mod_180;
    return screen_rect_is_portrait ? IntRect(0, 0, 1080, 1920)
                                   : IntRect(0, 0, 1920, 1080);
  }

  void RotateDeviceTo(uint16_t new_device_orientation_angle) {
    // Pick one of the many (beta,gamma) pairs that should map to each angle.
    switch (new_device_orientation_angle) {
      case 0:
        RotateDeviceTo(90, 0);
        break;
      case 90:
        RotateDeviceTo(0, -90);
        break;
      case 180:
        RotateDeviceTo(-90, 0);
        break;
      case 270:
        RotateDeviceTo(0, 90);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  void RotateDeviceTo(double beta, double gamma) {
    DeviceOrientationController::From(GetDocument())
        .SetOverride(DeviceOrientationData::Create(0.0 /* alpha */, beta, gamma,
                                                   false /* absolute */));
    test::RunPendingTasks();
  }

  // Calls must be wrapped in ASSERT_NO_FATAL_FAILURE.
  void RotateScreenTo(WebScreenOrientationType screen_orientation_type,
                      uint16_t screen_orientation_angle) {
    WebScreenInfo screen_info;
    screen_info.orientation_type = screen_orientation_type;
    screen_info.orientation_angle = screen_orientation_angle;
    screen_info.rect = ScreenRectFromAngle(screen_orientation_angle);
    ASSERT_TRUE(screen_info.orientation_type ==
                ScreenOrientationControllerImpl::ComputeOrientation(
                    screen_info.rect, screen_info.orientation_angle));

    testing::Mock::VerifyAndClearExpectations(&ChromeClient());
    EXPECT_CALL(ChromeClient(), GetScreenInfo())
        .Times(AtLeast(1))
        .WillRepeatedly(Return(screen_info));

    // Screen Orientation API
    ScreenOrientationController::From(*GetDocument().GetFrame())
        ->NotifyOrientationChanged();

    // Legacy window.orientation API
    GetDocument().domWindow()->SendOrientationChangeEvent();

    test::RunPendingTasks();
  }

  void InitVideo(int video_width, int video_height) {
    // Set up the WebMediaPlayer instance.
    GetDocument().body()->AppendChild(&Video());
    Video().SetSrc("https://example.com");
    test::RunPendingTasks();
    SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);

    // Set video size.
    EXPECT_CALL(MockWebMediaPlayer(), NaturalSize())
        .WillRepeatedly(Return(WebSize(video_width, video_height)));

    // Dispatch an arbitrary Device Orientation event to satisfy
    // MediaControlsRotateToFullscreenDelegate's requirement that the device
    // supports the API and can provide beta and gamma values. The orientation
    // will be ignored.
    RotateDeviceTo(0);
  }

  void PlayVideo() {
    {
      std::unique_ptr<UserGestureIndicator> gesture =
          LocalFrame::NotifyUserActivation(GetDocument().GetFrame());
      Video().Play();
    }
    test::RunPendingTasks();
  }

  void UpdateVisibilityObserver() {
    // Let IntersectionObserver update.
    GetDocument().View()->UpdateAllLifecyclePhases();
    test::RunPendingTasks();
  }

  DeviceOrientationType ComputeDeviceOrientation(
      DeviceOrientationData* data) const {
    return MediaControls()
        ->orientation_lock_delegate_->ComputeDeviceOrientation(data);
  }

  bool was_running_layout_test_ = false;
  bool natural_orientation_is_portrait_ = true;
};

TEST_F(MediaControlsOrientationLockDelegateTest, DelegateRequiresFlag) {
  // Flag on by default.
  EXPECT_TRUE(HasDelegate(*Video().GetMediaControls()));

  // Same with flag off.
  ScopedVideoFullscreenOrientationLockForTest video_fullscreen_orientation_lock(
      false);
  HTMLVideoElement* video = HTMLVideoElement::Create(GetDocument());
  GetDocument().body()->AppendChild(video);
  EXPECT_FALSE(HasDelegate(*video->GetMediaControls()));
}

TEST_F(MediaControlsOrientationLockDelegateTest, DelegateRequiresVideo) {
  HTMLAudioElement* audio = HTMLAudioElement::Create(GetDocument());
  GetDocument().body()->AppendChild(audio);
  EXPECT_FALSE(HasDelegate(*audio->GetMediaControls()));
}

TEST_F(MediaControlsOrientationLockDelegateTest, InitialState) {
  CheckStatePendingFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, EnterFullscreenNoMetadata) {
  EXPECT_CALL(ScreenOrientationClient(), LockOrientation(_)).Times(0);

  SimulateEnterFullscreen();

  CheckStatePendingMetadata();
}

TEST_F(MediaControlsOrientationLockDelegateTest, LeaveFullscreenNoMetadata) {
  EXPECT_CALL(ScreenOrientationClient(), LockOrientation(_)).Times(0);
  EXPECT_CALL(ScreenOrientationClient(), UnlockOrientation()).Times(0);

  SimulateEnterFullscreen();
  // State set to PendingMetadata.
  SimulateExitFullscreen();

  CheckStatePendingFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, EnterFullscreenWithMetadata) {
  SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);

  EXPECT_CALL(ScreenOrientationClient(), LockOrientation(_)).Times(1);
  EXPECT_FALSE(DelegateWillUnlockFullscreen());

  SimulateEnterFullscreen();

  EXPECT_TRUE(DelegateWillUnlockFullscreen());
  CheckStateMaybeLockedFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, LeaveFullscreenWithMetadata) {
  SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);

  EXPECT_CALL(ScreenOrientationClient(), LockOrientation(_)).Times(1);
  EXPECT_CALL(ScreenOrientationClient(), UnlockOrientation()).Times(1);

  SimulateEnterFullscreen();
  // State set to MaybeLockedFullscreen.
  SimulateExitFullscreen();

  EXPECT_FALSE(DelegateWillUnlockFullscreen());
  CheckStatePendingFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, EnterFullscreenAfterPageLock) {
  SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);
  SimulateOrientationLock();
  test::RunPendingTasks();

  EXPECT_FALSE(DelegateWillUnlockFullscreen());
  EXPECT_CALL(ScreenOrientationClient(), LockOrientation(_)).Times(0);

  SimulateEnterFullscreen();

  EXPECT_FALSE(DelegateWillUnlockFullscreen());
  CheckStateMaybeLockedFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, LeaveFullscreenAfterPageLock) {
  SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);
  SimulateOrientationLock();
  test::RunPendingTasks();

  EXPECT_CALL(ScreenOrientationClient(), LockOrientation(_)).Times(0);
  EXPECT_CALL(ScreenOrientationClient(), UnlockOrientation()).Times(0);

  SimulateEnterFullscreen();
  // State set to MaybeLockedFullscreen.
  SimulateExitFullscreen();

  EXPECT_FALSE(DelegateWillUnlockFullscreen());
  CheckStatePendingFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest,
       ReceivedMetadataAfterExitingFullscreen) {
  EXPECT_CALL(ScreenOrientationClient(), LockOrientation(_)).Times(1);

  SimulateEnterFullscreen();
  // State set to PendingMetadata.

  // Set up the WebMediaPlayer instance.
  Video().SetSrc("http://example.com");
  test::RunPendingTasks();

  SimulateVideoNetworkState(HTMLMediaElement::kNetworkIdle);
  SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);
  test::RunPendingTasks();

  CheckStateMaybeLockedFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, ReceivedMetadataLater) {
  EXPECT_CALL(ScreenOrientationClient(), LockOrientation(_)).Times(0);
  EXPECT_CALL(ScreenOrientationClient(), UnlockOrientation()).Times(0);

  SimulateEnterFullscreen();
  // State set to PendingMetadata.
  SimulateExitFullscreen();

  // Set up the WebMediaPlayer instance.
  Video().SetSrc("http://example.com");
  test::RunPendingTasks();

  SimulateVideoNetworkState(HTMLMediaElement::kNetworkIdle);
  SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);
  test::RunPendingTasks();

  CheckStatePendingFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, ComputeOrientationLock) {
  // Set up the WebMediaPlayer instance.
  Video().SetSrc("http://example.com");
  test::RunPendingTasks();

  SimulateVideoNetworkState(HTMLMediaElement::kNetworkIdle);
  SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);

  EXPECT_CALL(MockWebMediaPlayer(), NaturalSize())
      .Times(14)  // Each `computeOrientationLock` calls the method twice.
      .WillOnce(Return(WebSize(100, 50)))
      .WillOnce(Return(WebSize(100, 50)))
      .WillOnce(Return(WebSize(50, 100)))
      .WillOnce(Return(WebSize(50, 100)))
      .WillRepeatedly(Return(WebSize(100, 100)));

  // 100x50
  EXPECT_EQ(kWebScreenOrientationLockLandscape, ComputeOrientationLock());

  // 50x100
  EXPECT_EQ(kWebScreenOrientationLockPortrait, ComputeOrientationLock());

  // 100x100 has more subtilities, it depends on the current screen orientation.
  WebScreenInfo screen_info;
  screen_info.orientation_type = kWebScreenOrientationUndefined;
  EXPECT_CALL(ChromeClient(), GetScreenInfo())
      .Times(1)
      .WillOnce(Return(screen_info));
  EXPECT_EQ(kWebScreenOrientationLockLandscape, ComputeOrientationLock());

  screen_info.orientation_type = kWebScreenOrientationPortraitPrimary;
  EXPECT_CALL(ChromeClient(), GetScreenInfo())
      .Times(1)
      .WillOnce(Return(screen_info));
  EXPECT_EQ(kWebScreenOrientationLockPortrait, ComputeOrientationLock());

  screen_info.orientation_type = kWebScreenOrientationPortraitPrimary;
  EXPECT_CALL(ChromeClient(), GetScreenInfo())
      .Times(1)
      .WillOnce(Return(screen_info));
  EXPECT_EQ(kWebScreenOrientationLockPortrait, ComputeOrientationLock());

  screen_info.orientation_type = kWebScreenOrientationLandscapePrimary;
  EXPECT_CALL(ChromeClient(), GetScreenInfo())
      .Times(1)
      .WillOnce(Return(screen_info));
  EXPECT_EQ(kWebScreenOrientationLockLandscape, ComputeOrientationLock());

  screen_info.orientation_type = kWebScreenOrientationLandscapeSecondary;
  EXPECT_CALL(ChromeClient(), GetScreenInfo())
      .Times(1)
      .WillOnce(Return(screen_info));
  EXPECT_EQ(kWebScreenOrientationLockLandscape, ComputeOrientationLock());
}

TEST_F(MediaControlsOrientationLockAndRotateToFullscreenDelegateTest,
       ComputeDeviceOrientation) {
  InitVideo(400, 400);

  // Repeat test with natural_orientation_is_portrait_ = false then true.
  for (int n_o_i_p = 0; n_o_i_p <= 1; n_o_i_p++) {
    natural_orientation_is_portrait_ = static_cast<bool>(n_o_i_p);
    SCOPED_TRACE(testing::Message() << "natural_orientation_is_portrait_="
                                    << natural_orientation_is_portrait_);

    DeviceOrientationType natural_orientation =
        natural_orientation_is_portrait_ ? DeviceOrientationType::kPortrait
                                         : DeviceOrientationType::kLandscape;
    DeviceOrientationType perpendicular_to_natural_orientation =
        natural_orientation_is_portrait_ ? DeviceOrientationType::kLandscape
                                         : DeviceOrientationType::kPortrait;

    // There are four valid combinations of orientation type and orientation
    // angle for a naturally portrait device, and they should all calculate the
    // same device orientation (since this doesn't depend on the screen
    // orientation, it only depends on whether the device is naturally portrait
    // or naturally landscape). Similarly for a naturally landscape device.
    for (int screen_angle = 0; screen_angle < 360; screen_angle += 90) {
      SCOPED_TRACE(testing::Message() << "screen_angle=" << screen_angle);
      WebScreenOrientationType screen_type = kWebScreenOrientationUndefined;
      switch (screen_angle) {
        case 0:
          screen_type = natural_orientation_is_portrait_
                            ? kWebScreenOrientationPortraitPrimary
                            : kWebScreenOrientationLandscapePrimary;
          break;
        case 90:
          screen_type = natural_orientation_is_portrait_
                            ? kWebScreenOrientationLandscapePrimary
                            : kWebScreenOrientationPortraitSecondary;
          break;
        case 180:
          screen_type = natural_orientation_is_portrait_
                            ? kWebScreenOrientationPortraitSecondary
                            : kWebScreenOrientationLandscapeSecondary;
          break;
        case 270:
          screen_type = natural_orientation_is_portrait_
                            ? kWebScreenOrientationLandscapeSecondary
                            : kWebScreenOrientationPortraitPrimary;
          break;
      }
      ASSERT_NO_FATAL_FAILURE(RotateScreenTo(screen_type, screen_angle));

      // Compass heading is irrelevant to this calculation.
      double alpha = 0.0;
      bool absolute = false;

      // These beta and gamma values should all map to r < sin(24 degrees), so
      // orientation == kFlat, irrespective of their device_orientation_angle.
      EXPECT_EQ(DeviceOrientationType::kFlat,  // face up
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, 0. /* beta */, 0. /* gamma */, absolute)));
      EXPECT_EQ(DeviceOrientationType::kFlat,  // face down
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, 180. /* beta */, 0. /* gamma */, absolute)));
      EXPECT_EQ(DeviceOrientationType::kFlat,  // face down
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, -180. /* beta */, 0. /* gamma */, absolute)));
      EXPECT_EQ(DeviceOrientationType::kFlat,  // face up, angle=0
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, 20. /* beta */, 0. /* gamma */, absolute)));
      EXPECT_EQ(DeviceOrientationType::kFlat,  // face up, angle=90
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, 0. /* beta */, -20. /* gamma */, absolute)));
      EXPECT_EQ(DeviceOrientationType::kFlat,  // face up, angle=180
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, -20. /* beta */, 0. /* gamma */, absolute)));
      EXPECT_EQ(DeviceOrientationType::kFlat,  // face up, angle=270
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, 0. /* beta */, 20. /* gamma */, absolute)));

      // These beta and gamma values should all map to r ~= 1 and
      // device_orientation_angle % 90 ~= 45, hence orientation == kDiagonal.
      EXPECT_EQ(DeviceOrientationType::kDiagonal,  // angle=45
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, 135. /* beta */, 90. /* gamma */, absolute)));
      EXPECT_EQ(DeviceOrientationType::kDiagonal,  // angle=45
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, 45. /* beta */, -90. /* gamma */, absolute)));
      EXPECT_EQ(DeviceOrientationType::kDiagonal,  // angle=135
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, -135. /* beta */, 90. /* gamma */, absolute)));
      EXPECT_EQ(DeviceOrientationType::kDiagonal,  // angle=135
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, -45. /* beta */, -90. /* gamma */, absolute)));
      EXPECT_EQ(DeviceOrientationType::kDiagonal,  // angle=225
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, -45. /* beta */, 90. /* gamma */, absolute)));
      EXPECT_EQ(DeviceOrientationType::kDiagonal,  // angle=225
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, -135. /* beta */, -90. /* gamma */, absolute)));
      EXPECT_EQ(DeviceOrientationType::kDiagonal,  // angle=315
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, 45. /* beta */, 90. /* gamma */, absolute)));
      EXPECT_EQ(DeviceOrientationType::kDiagonal,  // angle=315
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, 135. /* beta */, -90. /* gamma */, absolute)));

      // These beta and gamma values should all map to r ~= 1 and
      // device_orientation_angle ~= 0, hence orientation == kPortrait.
      EXPECT_EQ(natural_orientation,
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, 90. /* beta */, 0. /* gamma */, absolute)));
      EXPECT_EQ(natural_orientation,
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, 90. /* beta */, 90. /* gamma */, absolute)));
      EXPECT_EQ(natural_orientation,
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, 90. /* beta */, -90. /* gamma */, absolute)));
      EXPECT_EQ(natural_orientation,
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, 85. /* beta */, 90. /* gamma */, absolute)));
      EXPECT_EQ(natural_orientation,
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, 85. /* beta */, -90. /* gamma */, absolute)));
      EXPECT_EQ(natural_orientation,
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, 95. /* beta */, 90. /* gamma */, absolute)));
      EXPECT_EQ(natural_orientation,
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, 95. /* beta */, -90. /* gamma */, absolute)));

      // These beta and gamma values should all map to r == 1 and
      // device_orientation_angle == 90, hence orientation == kLandscape.
      EXPECT_EQ(perpendicular_to_natural_orientation,
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, 0. /* beta */, -90. /* gamma */, absolute)));
      EXPECT_EQ(perpendicular_to_natural_orientation,
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, 180. /* beta */, 90. /* gamma */, absolute)));
      EXPECT_EQ(perpendicular_to_natural_orientation,
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, -180. /* beta */, 90. /* gamma */, absolute)));

      // These beta and gamma values should all map to r ~= 1 and
      // device_orientation_angle ~= 180, hence orientation == kPortrait.
      EXPECT_EQ(natural_orientation,
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, -90. /* beta */, 0. /* gamma */, absolute)));
      EXPECT_EQ(natural_orientation,
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, -90. /* beta */, 90. /* gamma */, absolute)));
      EXPECT_EQ(natural_orientation,
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, -90. /* beta */, -90. /* gamma */, absolute)));
      EXPECT_EQ(natural_orientation,
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, -85. /* beta */, 90. /* gamma */, absolute)));
      EXPECT_EQ(natural_orientation,
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, -85. /* beta */, -90. /* gamma */, absolute)));
      EXPECT_EQ(natural_orientation,
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, -95. /* beta */, 90. /* gamma */, absolute)));
      EXPECT_EQ(natural_orientation,
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, -95. /* beta */, -90. /* gamma */, absolute)));

      // These beta and gamma values should all map to r == 1 and
      // device_orientation_angle == 270, hence orientation == kLandscape.
      EXPECT_EQ(perpendicular_to_natural_orientation,
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, 0. /* beta */, 90. /* gamma */, absolute)));
      EXPECT_EQ(perpendicular_to_natural_orientation,
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, 180. /* beta */, -90. /* gamma */, absolute)));
      EXPECT_EQ(perpendicular_to_natural_orientation,
                ComputeDeviceOrientation(DeviceOrientationData::Create(
                    alpha, -180. /* beta */, -90. /* gamma */, absolute)));
    }
  }
}

TEST_F(MediaControlsOrientationLockAndRotateToFullscreenDelegateTest,
       PortraitInlineRotateToLandscapeFullscreen) {
  // Naturally portrait device, initially portrait, with landscape video.
  natural_orientation_is_portrait_ = true;
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationPortraitPrimary, 0));
  InitVideo(640, 480);
  SetIsAutoRotateEnabledByUser(true);
  PlayVideo();
  UpdateVisibilityObserver();

  // Initially inline, unlocked orientation.
  ASSERT_FALSE(Video().IsFullscreen());
  CheckStatePendingFullscreen();
  ASSERT_FALSE(DelegateWillUnlockFullscreen());

  // Simulate user rotating their device to landscape triggering a screen
  // orientation change.
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationLandscapePrimary, 90));

  // MediaControlsRotateToFullscreenDelegate should enter fullscreen, so
  // MediaControlsOrientationLockDelegate should lock orientation to landscape
  // (even though the screen is already landscape).
  EXPECT_TRUE(Video().IsFullscreen());
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockLandscape, DelegateOrientationLock());

  // Device orientation events received by MediaControlsOrientationLockDelegate
  // will confirm that the device is already landscape.
  RotateDeviceTo(90 /* landscape primary */);
  test::RunDelayedTasks(GetUnlockDelay());

  // MediaControlsOrientationLockDelegate should lock to "any" orientation.
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockAny, DelegateOrientationLock());
  EXPECT_TRUE(DelegateWillUnlockFullscreen());
}

TEST_F(MediaControlsOrientationLockAndRotateToFullscreenDelegateTest,
       PortraitInlineButtonToPortraitLockedLandscapeFullscreen) {
  // Naturally portrait device, initially portrait, with landscape video.
  natural_orientation_is_portrait_ = true;
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationPortraitPrimary, 0));
  InitVideo(640, 480);
  SetIsAutoRotateEnabledByUser(true);

  // Initially inline, unlocked orientation.
  ASSERT_FALSE(Video().IsFullscreen());
  CheckStatePendingFullscreen();
  ASSERT_FALSE(DelegateWillUnlockFullscreen());

  // Simulate user clicking on media controls fullscreen button.
  SimulateEnterFullscreen();
  EXPECT_TRUE(Video().IsFullscreen());

  // MediaControlsOrientationLockDelegate should lock to landscape.
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockLandscape, DelegateOrientationLock());

  // This will trigger a screen orientation change to landscape.
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationLandscapePrimary, 90));

  // Even though the device is still held in portrait.
  RotateDeviceTo(0 /* portrait primary */);
  test::RunDelayedTasks(GetUnlockDelay());

  // MediaControlsOrientationLockDelegate should remain locked to landscape.
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockLandscape, DelegateOrientationLock());
}

TEST_F(MediaControlsOrientationLockAndRotateToFullscreenDelegateTest,
       PortraitLockedLandscapeFullscreenRotateToLandscapeFullscreen) {
  // Naturally portrait device, initially portrait device orientation but locked
  // to landscape screen orientation, with landscape video.
  natural_orientation_is_portrait_ = true;
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationLandscapePrimary, 90));
  InitVideo(640, 480);
  SetIsAutoRotateEnabledByUser(true);

  // Initially fullscreen, locked orientation.
  SimulateEnterFullscreen();
  ASSERT_TRUE(Video().IsFullscreen());
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockLandscape, DelegateOrientationLock());

  // Simulate user rotating their device to landscape (matching the screen
  // orientation lock).
  RotateDeviceTo(90 /* landscape primary */);
  test::RunDelayedTasks(GetUnlockDelay());

  // MediaControlsOrientationLockDelegate should lock to "any" orientation.
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockAny, DelegateOrientationLock());
  EXPECT_TRUE(DelegateWillUnlockFullscreen());
  EXPECT_TRUE(Video().IsFullscreen());
}

TEST_F(MediaControlsOrientationLockAndRotateToFullscreenDelegateTest,
       PortraitLockedLandscapeFullscreenBackToPortraitInline) {
  // Naturally portrait device, initially portrait device orientation but locked
  // to landscape screen orientation, with landscape video.
  natural_orientation_is_portrait_ = true;
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationLandscapePrimary, 90));
  InitVideo(640, 480);
  SetIsAutoRotateEnabledByUser(true);

  // Initially fullscreen, locked orientation.
  SimulateEnterFullscreen();
  ASSERT_TRUE(Video().IsFullscreen());
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockLandscape, DelegateOrientationLock());

  // Simulate user clicking on media controls exit fullscreen button.
  SimulateExitFullscreen();
  EXPECT_FALSE(Video().IsFullscreen());

  // MediaControlsOrientationLockDelegate should unlock orientation.
  CheckStatePendingFullscreen();
  EXPECT_FALSE(DelegateWillUnlockFullscreen());

  // Play the video and make it visible, just to make sure
  // MediaControlsRotateToFullscreenDelegate doesn't react to the
  // orientationchange event.
  PlayVideo();
  UpdateVisibilityObserver();

  // Unlocking the orientation earlier will trigger a screen orientation change
  // to portrait (since the device orientation was already portrait, even though
  // the screen was locked to landscape).
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationPortraitPrimary, 0));

  // Video should remain inline, unlocked.
  CheckStatePendingFullscreen();
  EXPECT_FALSE(DelegateWillUnlockFullscreen());
  EXPECT_FALSE(Video().IsFullscreen());
}

TEST_F(MediaControlsOrientationLockAndRotateToFullscreenDelegateTest,
       LandscapeInlineRotateToPortraitInline) {
  // Naturally portrait device, initially landscape, with landscape video.
  natural_orientation_is_portrait_ = true;
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationLandscapePrimary, 90));
  InitVideo(640, 480);
  SetIsAutoRotateEnabledByUser(true);
  PlayVideo();
  UpdateVisibilityObserver();

  // Initially inline, unlocked orientation.
  ASSERT_FALSE(Video().IsFullscreen());
  CheckStatePendingFullscreen();
  ASSERT_FALSE(DelegateWillUnlockFullscreen());

  // Simulate user rotating their device to portrait triggering a screen
  // orientation change.
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationPortraitPrimary, 0));
  test::RunDelayedTasks(GetUnlockDelay());

  // Video should remain inline, unlocked.
  CheckStatePendingFullscreen();
  EXPECT_FALSE(DelegateWillUnlockFullscreen());
  EXPECT_FALSE(Video().IsFullscreen());
}

TEST_F(MediaControlsOrientationLockAndRotateToFullscreenDelegateTest,
       LandscapeInlineButtonToLandscapeFullscreen) {
  // Naturally portrait device, initially landscape, with landscape video.
  natural_orientation_is_portrait_ = true;
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationLandscapePrimary, 90));
  InitVideo(640, 480);
  SetIsAutoRotateEnabledByUser(true);

  // Initially inline, unlocked orientation.
  ASSERT_FALSE(Video().IsFullscreen());
  CheckStatePendingFullscreen();
  ASSERT_FALSE(DelegateWillUnlockFullscreen());

  // Simulate user clicking on media controls fullscreen button.
  SimulateEnterFullscreen();
  EXPECT_TRUE(Video().IsFullscreen());

  // MediaControlsOrientationLockDelegate should lock to landscape (even though
  // the screen is already landscape).
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockLandscape, DelegateOrientationLock());

  // Device orientation events received by MediaControlsOrientationLockDelegate
  // will confirm that the device is already landscape.
  RotateDeviceTo(90 /* landscape primary */);
  test::RunDelayedTasks(GetUnlockDelay());

  // MediaControlsOrientationLockDelegate should lock to "any" orientation.
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockAny, DelegateOrientationLock());
  EXPECT_TRUE(DelegateWillUnlockFullscreen());
}

TEST_F(MediaControlsOrientationLockAndRotateToFullscreenDelegateTest,
       LandscapeFullscreenRotateToPortraitInline) {
  // Naturally portrait device, initially landscape, with landscape video.
  natural_orientation_is_portrait_ = true;
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationLandscapePrimary, 90));
  InitVideo(640, 480);
  SetIsAutoRotateEnabledByUser(true);

  // Initially fullscreen, locked to "any" orientation.
  SimulateEnterFullscreen();
  RotateDeviceTo(90 /* landscape primary */);
  test::RunDelayedTasks(GetUnlockDelay());
  ASSERT_TRUE(Video().IsFullscreen());
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockAny, DelegateOrientationLock());
  EXPECT_TRUE(DelegateWillUnlockFullscreen());

  // Simulate user rotating their device to portrait triggering a screen
  // orientation change.
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationPortraitPrimary, 0));
  test::RunDelayedTasks(GetUnlockDelay());

  // MediaControlsRotateToFullscreenDelegate should exit fullscreen.
  EXPECT_FALSE(Video().IsFullscreen());

  // MediaControlsOrientationLockDelegate should unlock screen orientation.
  CheckStatePendingFullscreen();
  EXPECT_FALSE(DelegateWillUnlockFullscreen());
}

TEST_F(MediaControlsOrientationLockAndRotateToFullscreenDelegateTest,
       LandscapeFullscreenBackToLandscapeInline) {
  // Naturally portrait device, initially landscape, with landscape video.
  natural_orientation_is_portrait_ = true;
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationLandscapePrimary, 90));
  InitVideo(640, 480);
  SetIsAutoRotateEnabledByUser(true);

  // Initially fullscreen, locked to "any" orientation.
  SimulateEnterFullscreen();
  RotateDeviceTo(90 /* landscape primary */);
  test::RunDelayedTasks(GetUnlockDelay());
  ASSERT_TRUE(Video().IsFullscreen());
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockAny, DelegateOrientationLock());
  EXPECT_TRUE(DelegateWillUnlockFullscreen());

  // Simulate user clicking on media controls exit fullscreen button.
  SimulateExitFullscreen();
  EXPECT_FALSE(Video().IsFullscreen());

  // MediaControlsOrientationLockDelegate should unlock screen orientation.
  CheckStatePendingFullscreen();
  EXPECT_FALSE(DelegateWillUnlockFullscreen());
}

TEST_F(
    MediaControlsOrientationLockAndRotateToFullscreenDelegateTest,
    AutoRotateDisabledPortraitInlineButtonToPortraitLockedLandscapeFullscreen) {
  // Naturally portrait device, initially portrait, with landscape video.
  natural_orientation_is_portrait_ = true;
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationPortraitPrimary, 0));
  InitVideo(640, 480);
  // But this time the user has disabled auto rotate, e.g. locked to portrait.
  SetIsAutoRotateEnabledByUser(false);

  // Initially inline, unlocked orientation.
  ASSERT_FALSE(Video().IsFullscreen());
  CheckStatePendingFullscreen();
  ASSERT_FALSE(DelegateWillUnlockFullscreen());

  // Simulate user clicking on media controls fullscreen button.
  SimulateEnterFullscreen();
  EXPECT_TRUE(Video().IsFullscreen());

  // MediaControlsOrientationLockDelegate should lock to landscape.
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockLandscape, DelegateOrientationLock());

  // This will trigger a screen orientation change to landscape, since the app's
  // lock overrides the user's orientation lock (at least on Android).
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationLandscapePrimary, 90));

  // Even though the device is still held in portrait.
  RotateDeviceTo(0 /* portrait primary */);
  test::RunDelayedTasks(GetUnlockDelay());

  // MediaControlsOrientationLockDelegate should remain locked to landscape.
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockLandscape, DelegateOrientationLock());
}

TEST_F(
    MediaControlsOrientationLockAndRotateToFullscreenDelegateTest,
    AutoRotateDisabledPortraitLockedLandscapeFullscreenRotateToLandscapeLockedLandscapeFullscreen) {
  // Naturally portrait device, initially portrait device orientation but locked
  // to landscape screen orientation, with landscape video.
  natural_orientation_is_portrait_ = true;
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationLandscapePrimary, 90));
  InitVideo(640, 480);
  // But this time the user has disabled auto rotate, e.g. locked to portrait
  // (even though the app's landscape screen orientation lock overrides it).
  SetIsAutoRotateEnabledByUser(false);

  // Initially fullscreen, locked orientation.
  SimulateEnterFullscreen();
  ASSERT_TRUE(Video().IsFullscreen());
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockLandscape, DelegateOrientationLock());

  // Simulate user rotating their device to landscape (matching the screen
  // orientation lock).
  RotateDeviceTo(90 /* landscape primary */);
  test::RunDelayedTasks(GetUnlockDelay());

  // MediaControlsOrientationLockDelegate should remain locked to landscape even
  // though the screen orientation is now landscape, since the user has disabled
  // auto rotate, so unlocking now would cause the device to return to the
  // portrait orientation.
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockLandscape, DelegateOrientationLock());
  EXPECT_TRUE(Video().IsFullscreen());
}

TEST_F(
    MediaControlsOrientationLockAndRotateToFullscreenDelegateTest,
    AutoRotateDisabledPortraitLockedLandscapeFullscreenBackToPortraitInline) {
  // Naturally portrait device, initially portrait device orientation but locked
  // to landscape screen orientation, with landscape video.
  natural_orientation_is_portrait_ = true;
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationLandscapePrimary, 90));
  InitVideo(640, 480);
  // But this time the user has disabled auto rotate, e.g. locked to portrait
  // (even though the app's landscape screen orientation lock overrides it).
  SetIsAutoRotateEnabledByUser(false);

  // Initially fullscreen, locked orientation.
  SimulateEnterFullscreen();
  ASSERT_TRUE(Video().IsFullscreen());
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockLandscape, DelegateOrientationLock());

  // Simulate user clicking on media controls exit fullscreen button.
  SimulateExitFullscreen();
  EXPECT_FALSE(Video().IsFullscreen());

  // MediaControlsOrientationLockDelegate should unlock orientation.
  CheckStatePendingFullscreen();
  EXPECT_FALSE(DelegateWillUnlockFullscreen());

  // Play the video and make it visible, just to make sure
  // MediaControlsRotateToFullscreenDelegate doesn't react to the
  // orientationchange event.
  PlayVideo();
  UpdateVisibilityObserver();

  // Unlocking the orientation earlier will trigger a screen orientation change
  // to portrait, since the user had locked the screen orientation to portrait,
  // (which happens to also match the device orientation) and
  // MediaControlsOrientationLockDelegate is no longer overriding that lock.
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationPortraitPrimary, 0));

  // Video should remain inline, unlocked.
  CheckStatePendingFullscreen();
  EXPECT_FALSE(DelegateWillUnlockFullscreen());
  EXPECT_FALSE(Video().IsFullscreen());
}

TEST_F(
    MediaControlsOrientationLockAndRotateToFullscreenDelegateTest,
    AutoRotateDisabledLandscapeLockedLandscapeFullscreenRotateToPortraitLockedLandscapeFullscreen) {
  // Naturally portrait device, initially landscape device orientation yet also
  // locked to landscape screen orientation since the user had disabled auto
  // rotate, with landscape video.
  natural_orientation_is_portrait_ = true;
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationLandscapePrimary, 90));
  InitVideo(640, 480);
  // The user has disabled auto rotate, e.g. locked to portrait (even though the
  // app's landscape screen orientation lock overrides it).
  SetIsAutoRotateEnabledByUser(false);

  // Initially fullscreen, locked orientation.
  SimulateEnterFullscreen();
  ASSERT_TRUE(Video().IsFullscreen());
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockLandscape, DelegateOrientationLock());

  // Simulate user rotating their device to portrait (matching the user's
  // rotation lock, but perpendicular to MediaControlsOrientationLockDelegate's
  // screen orientation lock which overrides it).
  RotateDeviceTo(0 /* portrait primary */);
  test::RunDelayedTasks(GetUnlockDelay());

  // Video should remain locked and fullscreen. This may disappoint users who
  // expect MediaControlsRotateToFullscreenDelegate to let them always leave
  // fullscreen by rotating perpendicular to the video's orientation (i.e.
  // rotating to portrait for a landscape video), however in this specific case,
  // since the user disabled auto rotate at the OS level, it's likely that they
  // wish to be able to use their phone whilst their head is lying sideways on a
  // pillow (or similar), in which case it's essential to keep the fullscreen
  // orientation lock.
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockLandscape, DelegateOrientationLock());
  EXPECT_TRUE(Video().IsFullscreen());
}

TEST_F(
    MediaControlsOrientationLockAndRotateToFullscreenDelegateTest,
    AutoRotateDisabledLandscapeLockedLandscapeFullscreenBackToPortraitInline) {
  // Naturally portrait device, initially landscape device orientation yet also
  // locked to landscape screen orientation since the user had disabled auto
  // rotate, with landscape video.
  natural_orientation_is_portrait_ = true;
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationLandscapePrimary, 90));
  InitVideo(640, 480);
  // The user has disabled auto rotate, e.g. locked to portrait (even though the
  // app's landscape screen orientation lock overrides it).
  SetIsAutoRotateEnabledByUser(false);

  // Initially fullscreen, locked orientation.
  SimulateEnterFullscreen();
  ASSERT_TRUE(Video().IsFullscreen());
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockLandscape, DelegateOrientationLock());

  // Simulate user clicking on media controls exit fullscreen button.
  SimulateExitFullscreen();
  EXPECT_FALSE(Video().IsFullscreen());

  // MediaControlsOrientationLockDelegate should unlock orientation.
  CheckStatePendingFullscreen();
  EXPECT_FALSE(DelegateWillUnlockFullscreen());

  // Play the video and make it visible, just to make sure
  // MediaControlsRotateToFullscreenDelegate doesn't react to the
  // orientationchange event.
  PlayVideo();
  UpdateVisibilityObserver();

  // Unlocking the orientation earlier will trigger a screen orientation change
  // to portrait even though the device orientation is landscape, since the user
  // had locked the screen orientation to portrait, and
  // MediaControlsOrientationLockDelegate is no longer overriding that.
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationPortraitPrimary, 0));

  // Video should remain inline, unlocked.
  CheckStatePendingFullscreen();
  EXPECT_FALSE(DelegateWillUnlockFullscreen());
  EXPECT_FALSE(Video().IsFullscreen());
}

TEST_F(MediaControlsOrientationLockAndRotateToFullscreenDelegateTest,
       PortraitVideoRotateEnterExit) {
  // Naturally portrait device, initially landscape, with *portrait* video.
  natural_orientation_is_portrait_ = true;
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationLandscapePrimary, 90));
  InitVideo(480, 640);
  SetIsAutoRotateEnabledByUser(true);
  PlayVideo();
  UpdateVisibilityObserver();

  // Initially inline, unlocked orientation.
  ASSERT_FALSE(Video().IsFullscreen());
  CheckStatePendingFullscreen();
  ASSERT_FALSE(DelegateWillUnlockFullscreen());

  // Simulate user rotating their device to portrait triggering a screen
  // orientation change.
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationPortraitPrimary, 0));

  // MediaControlsRotateToFullscreenDelegate should enter fullscreen, so
  // MediaControlsOrientationLockDelegate should lock orientation to portrait
  // (even though the screen is already portrait).
  EXPECT_TRUE(Video().IsFullscreen());
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockPortrait, DelegateOrientationLock());

  // Device orientation events received by MediaControlsOrientationLockDelegate
  // will confirm that the device is already portrait.
  RotateDeviceTo(0 /* portrait primary */);
  test::RunDelayedTasks(GetUnlockDelay());

  // MediaControlsOrientationLockDelegate should lock to "any" orientation.
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockAny, DelegateOrientationLock());
  EXPECT_TRUE(DelegateWillUnlockFullscreen());
  EXPECT_TRUE(Video().IsFullscreen());

  // Simulate user rotating their device to landscape triggering a screen
  // orientation change.
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationLandscapePrimary, 90));

  // MediaControlsRotateToFullscreenDelegate should exit fullscreen.
  EXPECT_FALSE(Video().IsFullscreen());

  // MediaControlsOrientationLockDelegate should unlock screen orientation.
  CheckStatePendingFullscreen();
  EXPECT_FALSE(DelegateWillUnlockFullscreen());
}

TEST_F(MediaControlsOrientationLockAndRotateToFullscreenDelegateTest,
       LandscapeDeviceRotateEnterExit) {
  // Naturally *landscape* device, initially portrait, with landscape video.
  natural_orientation_is_portrait_ = false;
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationPortraitPrimary, 270));
  InitVideo(640, 480);
  SetIsAutoRotateEnabledByUser(true);
  PlayVideo();
  UpdateVisibilityObserver();

  // Initially inline, unlocked orientation.
  ASSERT_FALSE(Video().IsFullscreen());
  CheckStatePendingFullscreen();
  ASSERT_FALSE(DelegateWillUnlockFullscreen());

  // Simulate user rotating their device to landscape triggering a screen
  // orientation change.
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationLandscapePrimary, 0));

  // MediaControlsRotateToFullscreenDelegate should enter fullscreen, so
  // MediaControlsOrientationLockDelegate should lock orientation to landscape
  // (even though the screen is already landscape).
  EXPECT_TRUE(Video().IsFullscreen());
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockLandscape, DelegateOrientationLock());

  // Device orientation events received by MediaControlsOrientationLockDelegate
  // will confirm that the device is already landscape.
  RotateDeviceTo(0 /* landscape primary */);
  test::RunDelayedTasks(GetUnlockDelay());

  // MediaControlsOrientationLockDelegate should lock to "any" orientation.
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockAny, DelegateOrientationLock());
  EXPECT_TRUE(DelegateWillUnlockFullscreen());
  EXPECT_TRUE(Video().IsFullscreen());

  // Simulate user rotating their device to portrait triggering a screen
  // orientation change.
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationPortraitPrimary, 270));

  // MediaControlsRotateToFullscreenDelegate should exit fullscreen.
  EXPECT_FALSE(Video().IsFullscreen());

  // MediaControlsOrientationLockDelegate should unlock screen orientation.
  CheckStatePendingFullscreen();
  EXPECT_FALSE(DelegateWillUnlockFullscreen());
}

TEST_F(MediaControlsOrientationLockAndRotateToFullscreenDelegateTest,
       ScreenOrientationRaceCondition) {
  // Naturally portrait device, initially portrait, with landscape video.
  natural_orientation_is_portrait_ = true;
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationPortraitPrimary, 0));
  InitVideo(640, 480);
  SetIsAutoRotateEnabledByUser(true);

  // Initially inline, unlocked orientation.
  ASSERT_FALSE(Video().IsFullscreen());
  CheckStatePendingFullscreen();
  ASSERT_FALSE(DelegateWillUnlockFullscreen());

  // Simulate user clicking on media controls fullscreen button.
  SimulateEnterFullscreen();
  EXPECT_TRUE(Video().IsFullscreen());

  // MediaControlsOrientationLockDelegate should lock to landscape.
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockLandscape, DelegateOrientationLock());

  // This will trigger a screen orientation change to landscape.
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationLandscapePrimary, 90));

  // Even though the device is still held in portrait.
  RotateDeviceTo(0 /* portrait primary */);

  // MediaControlsOrientationLockDelegate should remain locked to landscape
  // indefinitely.
  test::RunDelayedTasks(GetUnlockDelay());
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockLandscape, DelegateOrientationLock());

  // Now suppose the user actually rotates from portrait-primary to landscape-
  // secondary, despite the screen currently being landscape-primary.
  RotateDeviceTo(270 /* landscape secondary */);

  // There can be a significant delay, between the device orientation changing
  // and the OS updating the screen orientation to match the new device
  // orientation. Manual testing showed that it could take longer than 200ms,
  // but less than 250ms, on common Android devices. Partly this is because OSes
  // often low-pass filter the device orientation to ignore high frequency
  // noise.
  //
  // During this period, MediaControlsOrientationLockDelegate should
  // remain locked to landscape. This prevents a race condition where the
  // delegate unlocks the screen orientation, so Android changes the screen
  // orientation back to portrait because it hasn't yet processed the device
  // orientation change to landscape.
  constexpr TimeDelta kMinUnlockDelay = TimeDelta::FromMilliseconds(249);
  static_assert(GetUnlockDelay() > kMinUnlockDelay,
                "GetUnlockDelay() should significantly exceed kMinUnlockDelay");
  test::RunDelayedTasks(kMinUnlockDelay);
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockLandscape, DelegateOrientationLock());

  // Simulate the OS processing the device orientation change after a delay of
  // `kMinUnlockDelay` and hence changing the screen orientation.
  ASSERT_NO_FATAL_FAILURE(
      RotateScreenTo(kWebScreenOrientationLandscapeSecondary, 270));

  // MediaControlsOrientationLockDelegate should remain locked to landscape.
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockLandscape, DelegateOrientationLock());

  // Wait for the rest of the unlock delay.
  test::RunDelayedTasks(GetUnlockDelay() - kMinUnlockDelay);

  // MediaControlsOrientationLockDelegate should've locked to "any" orientation.
  CheckStateMaybeLockedFullscreen();
  EXPECT_EQ(kWebScreenOrientationLockAny, DelegateOrientationLock());
  EXPECT_TRUE(DelegateWillUnlockFullscreen());
}

}  // namespace blink

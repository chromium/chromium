// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/media_controls_orientation_lock_delegate.h"

#include <memory>

#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/screen.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_data.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_event.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation_controller.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_screen_orientation.h"
#include "third_party/blink/renderer/modules/screen_orientation/web_lock_orientation_callback.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "ui/display/screen_info.h"

#if BUILDFLAG(IS_ANDROID)
#include "third_party/blink/public/platform/platform.h"
#endif  // BUILDFLAG(IS_ANDROID)

#undef atan2  // to use std::atan2 instead of wtf_atan2
#undef fmod   // to use std::fmod instead of wtf_fmod
#include <cmath>

namespace blink {

namespace {

// WebLockOrientationCallback implementation that will not react to a success
// nor a failure.
class DummyScreenOrientationCallback : public WebLockOrientationCallback {
 public:
  void OnSuccess() override {}
  void OnError(WebLockOrientationError) override {}
};

}  // anonymous namespace

constexpr base::TimeDelta MediaControlsOrientationLockDelegate::kLockToAnyDelay;

MediaControlsOrientationLockDelegate::MediaControlsOrientationLockDelegate(
    HTMLVideoElement& video)
    : monitor_(video.GetExecutionContext()), video_element_(video) {
  if (VideoElement().isConnected())
    Attach();
}

void MediaControlsOrientationLockDelegate::Attach() {
  DCHECK(VideoElement().isConnected());

  GetDocument().addEventListener(event_type_names::kFullscreenchange, this,
                                 true);
  VideoElement().addEventListener(event_type_names::kWebkitfullscreenchange,
                                  this, true);
  VideoElement().addEventListener(event_type_names::kLoadedmetadata, this,
                                  true);
}

void MediaControlsOrientationLockDelegate::Detach() {
  DCHECK(!VideoElement().isConnected());

  GetDocument().removeEventListener(event_type_names::kFullscreenchange, this,
                                    true);
  VideoElement().removeEventListener(event_type_names::kWebkitfullscreenchange,
                                     this, true);
  VideoElement().removeEventListener(event_type_names::kLoadedmetadata, this,
                                     true);
}

void MediaControlsOrientationLockDelegate::MaybeLockOrientation() {
  DCHECK(state_ != State::kMaybeLockedFullscreen);

  if (VideoElement().getReadyState() == HTMLMediaElement::kHaveNothing) {
    state_ = State::kPendingMetadata;
    return;
  }

  state_ = State::kMaybeLockedFullscreen;

  if (!GetDocument().domWindow())
    return;

  auto* controller =
      ScreenOrientationController::From(*GetDocument().domWindow());
  if (controller->MaybeHasActiveLock())
    return;

  locked_orientation_ = ComputeOrientationLock();
  DCHECK_NE(locked_orientation_,
            device::mojom::blink::ScreenOrientationLockType::DEFAULT);
  controller->lock(locked_orientation_,
                   std::make_unique<DummyScreenOrientationCallback>());

  MaybeListenToDeviceOrientation();
}

void MediaControlsOrientationLockDelegate::ChangeLockToAnyOrientation() {
  // Must already be locked.
  DCHECK_EQ(state_, State::kMaybeLockedFullscreen);
  DCHECK_NE(locked_orientation_,
            device::mojom::blink::ScreenOrientationLockType::DEFAULT);

  locked_orientation_ = device::mojom::blink::ScreenOrientationLockType::ANY;

  // The document could have been detached from the frame.
  if (LocalDOMWindow* window = GetDocument().domWindow()) {
    ScreenOrientationController::From(*window)->lock(
        locked_orientation_,
        std::make_unique<DummyScreenOrientationCallback>());
  }
}

void MediaControlsOrientationLockDelegate::MaybeUnlockOrientation() {
  DCHECK(state_ != State::kPendingFullscreen);

  state_ = State::kPendingFullscreen;

  if (locked_orientation_ ==
      device::mojom::blink::ScreenOrientationLockType::DEFAULT /* unlocked */)
    return;

  monitor_.reset();  // Cancel any GotIsAutoRotateEnabledByUser Mojo callback.
  LocalDOMWindow* dom_window = GetDocument().domWindow();
  dom_window->removeEventListener(event_type_names::kDeviceorientation, this,
                                  false);
  ScreenOrientationController::From(*dom_window)->unlock();
  locked_orientation_ =
      device::mojom::blink::ScreenOrientationLockType::DEFAULT /* unlocked */;

  lock_to_any_task_.Cancel();
}

void MediaControlsOrientationLockDelegate::MaybeListenToDeviceOrientation() {
  DCHECK_EQ(state_, State::kMaybeLockedFullscreen);
  DCHECK_NE(locked_orientation_,
            device::mojom::blink::ScreenOrientationLockType::DEFAULT);

  // If the rotate-to-fullscreen feature is also enabled, then start listening
  // to deviceorientation events so the orientation can be unlocked once the
  // user rotates the device to match the video's orientation (allowing the user
  // to then exit fullscreen by rotating their device back to the opposite
  // orientation). Otherwise, don't listen for deviceorientation events and just
  // hold the orientation lock until the user exits fullscreen (which prevents
  // the user rotating to the wrong fullscreen orientation).
  if (!RuntimeEnabledFeatures::VideoRotateToFullscreenEnabled())
    return;

  if (is_auto_rotate_enabled_by_user_override_for_testing_ != std::nullopt) {
    GotIsAutoRotateEnabledByUser(
        is_auto_rotate_enabled_by_user_override_for_testing_.value());
    return;
  }

// Check whether the user locked screen orientation at the OS level.
#if BUILDFLAG(IS_ANDROID)
  DCHECK(!monitor_.is_bound());
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      monitor_.BindNewPipeAndPassReceiver(
          GetDocument().GetTaskRunner(TaskType::kMediaElementEvent)));
  monitor_->IsAutoRotateEnabledByUser(WTF::BindOnce(
      &MediaControlsOrientationLockDelegate::GotIsAutoRotateEnabledByUser,
      WrapPersistent(this)));
#else
  GotIsAutoRotateEnabledByUser(true);  // Assume always enabled on other OSes.
#endif  // BUILDFLAG(IS_ANDROID)
}

void MediaControlsOrientationLockDelegate::GotIsAutoRotateEnabledByUser(
    bool enabled) {
  monitor_.reset();

  if (!enabled) {
    // Since the user has locked their screen orientation, prevent
    // MediaControlsRotateToFullscreenDelegate from exiting fullscreen by not
    // listening for deviceorientation events and instead continuing to hold the
    // orientation lock until the user exits fullscreen. This enables users to
    // watch videos in bed with their head facing sideways (which requires a
    // landscape screen orientation when the device is portrait and vice versa).
    // TODO(johnme): Ideally we would start listening for deviceorientation
    // events and allow rotating to exit if a user enables screen auto rotation
    // after we have locked to landscape. That would require listening for
    // changes to the auto rotate setting, rather than only checking it once.
    return;
  }

  if (LocalDOMWindow* dom_window = GetDocument().domWindow()) {
    dom_window->addEventListener(event_type_names::kDeviceorientation, this,
                                 false);
  }
}

HTMLVideoElement& MediaControlsOrientationLockDelegate::VideoElement() const {
  return *video_element_;
}

Document& MediaControlsOrientationLockDelegate::GetDocument() const {
  return VideoElement().GetDocument();
}

void MediaControlsOrientationLockDelegate::Invoke(
    ExecutionContext* execution_context,
    Event* event) {
  if (event->type() == event_type_names::kFullscreenchange ||
      event->type() == event_type_names::kWebkitfullscreenchange) {
    if (VideoElement().IsFullscreen()) {
      if (state_ == State::kPendingFullscreen)
        MaybeLockOrientation();
    } else {
      if (state_ != State::kPendingFullscreen)
        MaybeUnlockOrientation();
    }

    return;
  }

  if (event->type() == event_type_names::kLoadedmetadata) {
    if (state_ == State::kPendingMetadata)
      MaybeLockOrientation();

    return;
  }

  if (event->type() == event_type_names::kDeviceorientation) {
    if (event->isTrusted() &&
        event->InterfaceName() ==
            event_interface_names::kDeviceOrientationEvent) {
      MaybeLockToAnyIfDeviceOrientationMatchesVideo(
          To<DeviceOrientationEvent>(event));
    }

    return;
  }

  NOTREACHED_IN_MIGRATION();
}

device::mojom::blink::ScreenOrientationLockType
MediaControlsOrientationLockDelegate::ComputeOrientationLock() const {
  DCHECK(VideoElement().getReadyState() != HTMLMediaElement::kHaveNothing);

  const unsigned width = VideoElement().videoWidth();
  const unsigned height = VideoElement().videoHeight();

  if (width > height)
    return device::mojom::blink::ScreenOrientationLockType::LANDSCAPE;

  if (height > width)
    return device::mojom::blink::ScreenOrientationLockType::PORTRAIT;

  // For square videos, try to lock to the current screen orientation for
  // consistency. Use device::mojom::blink::ScreenOrientationLockType::LANDSCAPE
  // as a fallback value.
  // TODO(mlamouri): we could improve this by having direct access to
  // `window.screen.orientation.type`.
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame)
    return device::mojom::blink::ScreenOrientationLockType::LANDSCAPE;

  ChromeClient& chrome_client = frame->GetChromeClient();
  switch (chrome_client.GetScreenInfo(*frame).orientation_type) {
    case display::mojom::blink::ScreenOrientation::kPortraitPrimary:
    case display::mojom::blink::ScreenOrientation::kPortraitSecondary:
      return device::mojom::blink::ScreenOrientationLockType::PORTRAIT;
    case display::mojom::blink::ScreenOrientation::kLandscapePrimary:
    case display::mojom::blink::ScreenOrientation::kLandscapeSecondary:
      return device::mojom::blink::ScreenOrientationLockType::LANDSCAPE;
    case display::mojom::blink::ScreenOrientation::kUndefined:
      return device::mojom::blink::ScreenOrientationLockType::LANDSCAPE;
  }

  NOTREACHED_IN_MIGRATION();
  return device::mojom::blink::ScreenOrientationLockType::LANDSCAPE;
}

MediaControlsOrientationLockDelegate::DeviceOrientationType
MediaControlsOrientationLockDelegate::ComputeDeviceOrientation(
    DeviceOrientationData* data) const {
  LocalDOMWindow* dom_window = GetDocument().domWindow();
  if (!dom_window)
    return DeviceOrientationType::kUnknown;

  if (!data->CanProvideBeta() || !data->CanProvideGamma())
    return DeviceOrientationType::kUnknown;
  double beta = data->Beta();
  double gamma = data->Gamma();

  // Calculate the projection of the up vector (normal to the earth's surface)
  // onto the device's screen in its natural orientation. (x,y) will lie within
  // the unit circle centered on (0,0), e.g. if the top of the device is
  // pointing upwards (x,y) will be (0,-1).
  double x = -std::sin(Deg2rad(gamma)) * std::cos(Deg2rad(beta));
  double y = -std::sin(Deg2rad(beta));

  // Convert (x,y) to polar coordinates: 0 <= device_orientation_angle < 360 and
  // 0 <= r <= 1, such that device_orientation_angle is the clockwise angle in
  // degrees between the current physical orientation of the device and the
  // natural physical orientation of the device (ignoring the screen
  // orientation). Thus snapping device_orientation_angle to the nearest
  // multiple of 90 gives the value screen.orientation.angle would have if the
  // screen orientation was allowed to rotate freely to match the device
  // orientation. Note that we want device_orientation_angle==0 when the top of
  // the device is pointing upwards, but atan2's zero angle points to the right,
  // so we pass y=x and x=-y to atan2 to rotate by 90 degrees.
  double r = std::sqrt(x * x + y * y);
  double device_orientation_angle =
      std::fmod(Rad2deg(std::atan2(/* y= */ x, /* x= */ -y)) + 360, 360);

  // If angle between device's screen and the horizontal plane is less than
  // kMinElevationAngle (chosen to approximately match Android's behavior), then
  // device is too flat to reliably determine orientation.
  constexpr double kMinElevationAngle = 24;  // degrees from horizontal plane
  if (r < std::sin(Deg2rad(kMinElevationAngle)))
    return DeviceOrientationType::kFlat;

  // device_orientation_angle snapped to nearest multiple of 90.
  int device_orientation_angle90 =
      static_cast<int>(std::lround(device_orientation_angle / 90) * 90);

  // To be considered portrait or landscape, allow the device to be rotated 23
  // degrees (chosen to approximately match Android's behavior) to either side
  // of those orientations. In the remaining 90 - 2*23 = 44 degree hysteresis
  // zones, consider the device to be diagonal. These hysteresis zones prevent
  // the computed orientation from oscillating rapidly between portrait and
  // landscape when the device is in between the two orientations.
  if (std::abs(device_orientation_angle - device_orientation_angle90) > 23)
    return DeviceOrientationType::kDiagonal;

  // screen.orientation.angle is the standardized replacement for
  // window.orientation. They are equal, except -90 was replaced by 270.
  int screen_orientation_angle =
      ScreenScreenOrientation::orientation(*dom_window->screen())->angle();

  // This is equivalent to screen.orientation.type.startsWith('landscape').
  bool screen_orientation_is_portrait =
      dom_window->screen()->width() <= dom_window->screen()->height();

  // The natural orientation of the device could either be portrait (almost
  // all phones, and some tablets like Nexus 7) or landscape (other tablets
  // like Pixel C). Detect this by comparing angle to orientation.
  // TODO(johnme): This might get confused on square screens.
  bool screen_orientation_is_natural_or_flipped_natural =
      screen_orientation_angle % 180 == 0;
  bool natural_orientation_is_portrait =
      screen_orientation_is_portrait ==
      screen_orientation_is_natural_or_flipped_natural;

  // If natural_orientation_is_portrait_, then angles 0 and 180 are portrait,
  // otherwise angles 90 and 270 are portrait.
  int portrait_angle_mod_180 = natural_orientation_is_portrait ? 0 : 90;
  return device_orientation_angle90 % 180 == portrait_angle_mod_180
             ? DeviceOrientationType::kPortrait
             : DeviceOrientationType::kLandscape;
}

void MediaControlsOrientationLockDelegate::
    MaybeLockToAnyIfDeviceOrientationMatchesVideo(
        DeviceOrientationEvent* event) {
  DCHECK_EQ(state_, State::kMaybeLockedFullscreen);
  DCHECK(locked_orientation_ ==
             device::mojom::blink::ScreenOrientationLockType::PORTRAIT ||
         locked_orientation_ ==
             device::mojom::blink::ScreenOrientationLockType::LANDSCAPE);

  DeviceOrientationType device_orientation =
      ComputeDeviceOrientation(event->Orientation());

  DeviceOrientationType video_orientation =
      locked_orientation_ ==
              device::mojom::blink::ScreenOrientationLockType::PORTRAIT
          ? DeviceOrientationType::kPortrait
          : DeviceOrientationType::kLandscape;

  if (device_orientation != video_orientation)
    return;

  // Job done: the user rotated their device to match the orientation of the
  // video that we locked to, so now we can stop listening.
  if (LocalDOMWindow* dom_window = GetDocument().domWindow()) {
    dom_window->removeEventListener(event_type_names::kDeviceorientation, this,
                                    false);
  }
  // Delay before changing lock, as a workaround for the case where the device
  // is initially portrait-primary, then fullscreen orientation lock locks it to
  // landscape and the screen orientation changes to landscape-primary, but the
  // user actually rotates the device to landscape-secondary. In that case, if
  // this delegate unlocks the orientation before Android has detected the
  // rotation to landscape-secondary (which is slow due to low-pass filtering),
  // Android would change the screen orientation back to portrait-primary. This
  // is avoided by delaying unlocking long enough to ensure that Android has
  // detected the orientation change.
  lock_to_any_task_ = PostDelayedCancellableTask(
      *GetDocument().GetTaskRunner(TaskType::kMediaElementEvent), FROM_HERE,
      // Conceptually, this callback will unlock the screen orientation,
      // so that the user can now rotate their device to the opposite
      // orientation in order to exit fullscreen. But unlocking
      // corresponds to
      // device::mojom::blink::ScreenOrientationLockType::DEFAULT, which is
      // sometimes a specific orientation. For example in a webapp added to
      // homescreen that has set its orientation to portrait using the manifest,
      // unlocking actually locks to portrait, which would immediately exit
      // fullscreen if we're watching a landscape video in landscape
      // orientation! So instead, this locks to
      // device::mojom::blink::ScreenOrientationLockType::ANY which will
      // auto-rotate according to the accelerometer, and only exit
      // fullscreen once the user actually rotates their device. We only
      // fully unlock to
      // device::mojom::blink::ScreenOrientationLockType::DEFAULT once
      // fullscreen is exited.
      WTF::BindOnce(
          &MediaControlsOrientationLockDelegate::ChangeLockToAnyOrientation,
          WrapPersistent(this)),
      kLockToAnyDelay);
}

void MediaControlsOrientationLockDelegate::Trace(Visitor* visitor) const {
  NativeEventListener::Trace(visitor);
  visitor->Trace(monitor_);
  visitor->Trace(video_element_);
}

}  // namespace blink

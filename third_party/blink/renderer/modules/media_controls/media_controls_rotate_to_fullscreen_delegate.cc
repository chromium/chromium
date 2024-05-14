// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/media_controls_rotate_to_fullscreen_delegate.h"

#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/user_metrics_action.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/media/html_media_element_controls_list.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_data.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_event.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/display/mojom/screen_orientation.mojom-blink.h"
#include "ui/display/screen_info.h"

namespace blink {

namespace {

// Videos must be at least this big in both dimensions to qualify.
constexpr unsigned kMinVideoSize = 200;

// At least this fraction of the video must be visible.
constexpr float kIntersectionThreshold = 0.75;

}  // anonymous namespace

MediaControlsRotateToFullscreenDelegate::
    MediaControlsRotateToFullscreenDelegate(HTMLVideoElement& video)
    : video_element_(video) {}

void MediaControlsRotateToFullscreenDelegate::Attach() {
  DCHECK(video_element_->isConnected());

  LocalDOMWindow* dom_window = video_element_->GetDocument().domWindow();
  if (!dom_window)
    return;

  video_element_->addEventListener(event_type_names::kPlay, this, true);
  video_element_->addEventListener(event_type_names::kPause, this, true);

  // Listen to two different fullscreen events in order to make sure the new and
  // old APIs are handled.
  video_element_->addEventListener(event_type_names::kWebkitfullscreenchange,
                                   this, true);
  video_element_->GetDocument().addEventListener(
      event_type_names::kFullscreenchange, this, true);

  current_screen_orientation_ = ComputeScreenOrientation();
  // TODO(johnme): Check this is battery efficient (note that this doesn't need
  // to receive events for 180 deg rotations).
  dom_window->addEventListener(event_type_names::kOrientationchange, this,
                               false);
  dom_window->addEventListener(event_type_names::kDeviceorientation, this,
                               false);
}

void MediaControlsRotateToFullscreenDelegate::Detach() {
  DCHECK(!video_element_->isConnected());

  if (intersection_observer_) {
    // TODO(johnme): Should I also call disconnect in a prefinalizer?
    intersection_observer_->disconnect();
    intersection_observer_ = nullptr;
    is_visible_ = false;
  }

  video_element_->removeEventListener(event_type_names::kPlay, this, true);
  video_element_->removeEventListener(event_type_names::kPause, this, true);

  video_element_->removeEventListener(event_type_names::kWebkitfullscreenchange,
                                      this, true);
  video_element_->GetDocument().removeEventListener(
      event_type_names::kFullscreenchange, this, true);

  LocalDOMWindow* dom_window = video_element_->GetDocument().domWindow();
  if (!dom_window)
    return;
  dom_window->removeEventListener(event_type_names::kOrientationchange, this,
                                  false);
  dom_window->removeEventListener(event_type_names::kDeviceorientation, this,
                                  false);
}

void MediaControlsRotateToFullscreenDelegate::Invoke(
    ExecutionContext* execution_context,
    Event* event) {
  if (event->type() == event_type_names::kPlay ||
      event->type() == event_type_names::kPause ||
      event->type() == event_type_names::kFullscreenchange ||
      event->type() == event_type_names::kWebkitfullscreenchange) {
    OnStateChange();
    return;
  }
  if (event->type() == event_type_names::kDeviceorientation) {
    if (event->isTrusted() &&
        event->InterfaceName() ==
            event_interface_names::kDeviceOrientationEvent) {
      OnDeviceOrientationAvailable(To<DeviceOrientationEvent>(event));
    }
    return;
  }
  if (event->type() == event_type_names::kOrientationchange) {
    OnScreenOrientationChange();
    return;
  }

  NOTREACHED_IN_MIGRATION();
}

void MediaControlsRotateToFullscreenDelegate::OnStateChange() {
  // TODO(johnme): Check this aggressive disabling doesn't lead to race
  // conditions where we briefly don't know if the video is visible.
  bool needs_intersection_observer =
      !video_element_->paused() && !video_element_->IsFullscreen();
  DVLOG(3) << __func__ << " " << !!intersection_observer_ << " -> "
           << needs_intersection_observer;

  if (needs_intersection_observer && !intersection_observer_) {
    intersection_observer_ = IntersectionObserver::Create(
        video_element_->GetDocument(),
        WTF::BindRepeating(
            &MediaControlsRotateToFullscreenDelegate::OnIntersectionChange,
            WrapWeakPersistent(this)),
        LocalFrameUkmAggregator::kMediaIntersectionObserver,
        IntersectionObserver::Params{.thresholds = {kIntersectionThreshold}});
    intersection_observer_->observe(video_element_);
  } else if (!needs_intersection_observer && intersection_observer_) {
    intersection_observer_->disconnect();
    intersection_observer_ = nullptr;
    is_visible_ = false;
  }
}

void MediaControlsRotateToFullscreenDelegate::OnIntersectionChange(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  bool is_visible =
      (entries.back()->intersectionRatio() > kIntersectionThreshold);
  DVLOG(3) << __func__ << " " << is_visible_ << " -> " << is_visible;
  is_visible_ = is_visible;
}

void MediaControlsRotateToFullscreenDelegate::OnDeviceOrientationAvailable(
    DeviceOrientationEvent* event) {
  LocalDOMWindow* dom_window = video_element_->GetDocument().domWindow();
  if (!dom_window)
    return;
  // Stop listening after the first event. Just need to know if it's available.
  dom_window->removeEventListener(event_type_names::kDeviceorientation, this,
                                  false);

  // MediaControlsOrientationLockDelegate needs Device Orientation events with
  // beta and gamma in order to unlock screen orientation and exit fullscreen
  // when the device is rotated. Some devices cannot provide beta and/or gamma
  // values and must be excluded. Unfortunately, some other devices incorrectly
  // return true for both CanProvideBeta() and CanProvideGamma() but their
  // Beta() and Gamma() values are permanently stuck on zero (crbug/760737); so
  // we have to also exclude devices where both of these values are exactly
  // zero, even though that's a valid (albeit unlikely) device orientation.
  DeviceOrientationData* data = event->Orientation();
  device_orientation_supported_ =
      std::make_optional(data->CanProvideBeta() && data->CanProvideGamma() &&
                         (data->Beta() != 0.0 || data->Gamma() != 0.0));
}

void MediaControlsRotateToFullscreenDelegate::OnScreenOrientationChange() {
  SimpleOrientation previous_screen_orientation = current_screen_orientation_;
  current_screen_orientation_ = ComputeScreenOrientation();
  DVLOG(3) << __func__ << " " << static_cast<int>(previous_screen_orientation)
           << " -> " << static_cast<int>(current_screen_orientation_);

  // Do not enable if video is in Picture-in-Picture.
  if (video_element_->GetDisplayType() == DisplayType::kPictureInPicture)
    return;

  // Only enable if native media controls are used.
  if (!video_element_->ShouldShowControls())
    return;

  // Do not enable if controlsList=nofullscreen is used.
  if (video_element_->ControlsListInternal()->ShouldHideFullscreen())
    return;

  // Only enable if the Device Orientation API can provide beta and gamma values
  // that will be needed for MediaControlsOrientationLockDelegate to
  // automatically unlock, such that it will be possible to exit fullscreen by
  // rotating back to the previous orientation.
  if (!device_orientation_supported_.value_or(false))
    return;

  // Don't enter/exit fullscreen if some other element is fullscreen.
  Element* fullscreen_element =
      Fullscreen::FullscreenElementFrom(video_element_->GetDocument());
  if (fullscreen_element && fullscreen_element != video_element_)
    return;

  // To enter fullscreen, video must be visible and playing.
  // TODO(johnme): If orientation changes whilst this tab is in the background,
  // we'll get an orientationchange event when this tab next becomes active.
  // Check that those events don't trigger rotate-to-fullscreen.
  if (!video_element_->IsFullscreen() &&
      (!is_visible_ || video_element_->paused())) {
    return;
  }

  // Ignore (unexpected) events where we have incomplete information.
  if (previous_screen_orientation == SimpleOrientation::kUnknown ||
      current_screen_orientation_ == SimpleOrientation::kUnknown) {
    return;
  }

  // Ignore 180 degree rotations between PortraitPrimary and PortraitSecondary,
  // or between LandscapePrimary and LandscapeSecondary.
  if (previous_screen_orientation == current_screen_orientation_)
    return;

  SimpleOrientation video_orientation = ComputeVideoOrientation();

  // Ignore videos that are too small or of unknown size.
  if (video_orientation == SimpleOrientation::kUnknown)
    return;

  MediaControlsImpl& media_controls =
      *static_cast<MediaControlsImpl*>(video_element_->GetMediaControls());

  {
    LocalFrame::NotifyUserActivation(
        video_element_->GetDocument().GetFrame(),
        mojom::blink::UserActivationNotificationType::kInteraction);

    bool should_be_fullscreen =
        current_screen_orientation_ == video_orientation;
    if (should_be_fullscreen && !video_element_->IsFullscreen()) {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Video.RotateToFullscreen.Enter"));
      media_controls.EnterFullscreen();
    } else if (!should_be_fullscreen && video_element_->IsFullscreen()) {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Video.RotateToFullscreen.Exit"));
      media_controls.ExitFullscreen();
    }
  }
}

MediaControlsRotateToFullscreenDelegate::SimpleOrientation
MediaControlsRotateToFullscreenDelegate::ComputeVideoOrientation() const {
  if (video_element_->getReadyState() == HTMLMediaElement::kHaveNothing)
    return SimpleOrientation::kUnknown;

  const unsigned width = video_element_->videoWidth();
  const unsigned height = video_element_->videoHeight();

  if (width < kMinVideoSize || height < kMinVideoSize)
    return SimpleOrientation::kUnknown;  // Too small, ignore this video.

  if (width >= height)
    return SimpleOrientation::kLandscape;  // Includes square videos.
  return SimpleOrientation::kPortrait;
}

MediaControlsRotateToFullscreenDelegate::SimpleOrientation
MediaControlsRotateToFullscreenDelegate::ComputeScreenOrientation() const {
  LocalFrame* frame = video_element_->GetDocument().GetFrame();
  if (!frame)
    return SimpleOrientation::kUnknown;

  ChromeClient& chrome_client = frame->GetChromeClient();
  const display::ScreenInfo& screen_info = chrome_client.GetScreenInfo(*frame);
  switch (screen_info.orientation_type) {
    case display::mojom::blink::ScreenOrientation::kPortraitPrimary:
    case display::mojom::blink::ScreenOrientation::kPortraitSecondary:
      return SimpleOrientation::kPortrait;
    case display::mojom::blink::ScreenOrientation::kLandscapePrimary:
    case display::mojom::blink::ScreenOrientation::kLandscapeSecondary:
      return SimpleOrientation::kLandscape;
    case display::mojom::blink::ScreenOrientation::kUndefined:
      return SimpleOrientation::kUnknown;
  }

  NOTREACHED_IN_MIGRATION();
  return SimpleOrientation::kUnknown;
}

void MediaControlsRotateToFullscreenDelegate::Trace(Visitor* visitor) const {
  NativeEventListener::Trace(visitor);
  visitor->Trace(video_element_);
  visitor->Trace(intersection_observer_);
}

}  // namespace blink

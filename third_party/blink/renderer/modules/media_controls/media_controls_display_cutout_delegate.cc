// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/media_controls_display_cutout_delegate.h"

#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

WebPoint ExtractWebPoint(Touch* touch) {
  return WebPoint(touch->pageX(), touch->pageY());
}

double CalculateDistance(WebPoint first, WebPoint second) {
  double dx = first.x - second.x;
  double dy = first.y - second.y;
  return sqrt(dx * dx + dy * dy);
}

}  // namespace

// static
bool MediaControlsDisplayCutoutDelegate::IsEnabled() {
  return RuntimeEnabledFeatures::DisplayCutoutAPIEnabled() &&
         RuntimeEnabledFeatures::MediaControlsExpandGestureEnabled();
}

MediaControlsDisplayCutoutDelegate::MediaControlsDisplayCutoutDelegate(
    HTMLVideoElement& video_element)
    : video_element_(video_element) {}

void MediaControlsDisplayCutoutDelegate::Attach() {
  DCHECK(video_element_->isConnected());

  GetDocument().addEventListener(event_type_names::kFullscreenchange, this,
                                 true);
  GetDocument().addEventListener(event_type_names::kWebkitfullscreenchange,
                                 this, true);
}

void MediaControlsDisplayCutoutDelegate::Detach() {
  DCHECK(!video_element_->isConnected());

  GetDocument().removeEventListener(event_type_names::kFullscreenchange, this,
                                    true);
  GetDocument().removeEventListener(event_type_names::kWebkitfullscreenchange,
                                    this, true);
}

void MediaControlsDisplayCutoutDelegate::Trace(blink::Visitor* visitor) {
  NativeEventListener::Trace(visitor);
  visitor->Trace(video_element_);
}

void MediaControlsDisplayCutoutDelegate::DidEnterFullscreen() {
  video_element_->addEventListener(event_type_names::kTouchstart, this, true);
  video_element_->addEventListener(event_type_names::kTouchend, this, true);
  video_element_->addEventListener(event_type_names::kTouchmove, this, true);
  video_element_->addEventListener(event_type_names::kTouchcancel, this, true);
}

void MediaControlsDisplayCutoutDelegate::DidExitFullscreen() {
  GetDocument().GetViewportData().SetExpandIntoDisplayCutout(false);

  video_element_->removeEventListener(event_type_names::kTouchstart, this,
                                      true);
  video_element_->removeEventListener(event_type_names::kTouchend, this, true);
  video_element_->removeEventListener(event_type_names::kTouchmove, this, true);
  video_element_->removeEventListener(event_type_names::kTouchcancel, this,
                                      true);
}

void MediaControlsDisplayCutoutDelegate::Invoke(
    ExecutionContext* execution_context,
    Event* event) {
  if (event->IsTouchEvent()) {
    HandleTouchEvent(ToTouchEvent(event));
    return;
  }
  if (event->type() == event_type_names::kFullscreenchange ||
      event->type() == event_type_names::kWebkitfullscreenchange) {
    // The fullscreen state has changed.
    if (video_element_->IsFullscreen()) {
      DidEnterFullscreen();
    } else if (!Fullscreen::FullscreenElementFrom(GetDocument())) {
      DidExitFullscreen();
    }

    return;
  }

  NOTREACHED();
}

void MediaControlsDisplayCutoutDelegate::HandleTouchEvent(TouchEvent* event) {
  // Check if the current media element is fullscreen.
  DCHECK(video_element_->IsFullscreen());

  // Filter out any touch events that are not two fingered.
  if (event->touches()->length() != 2)
    return;

  // Mark the event as handled.
  event->SetDefaultHandled();

  // If it is a touch start event then we should flush any previous points we
  // have stored.
  if (event->type() == event_type_names::kTouchstart)
    previous_.reset();

  // Extract the two touch points and calculate the distance.
  WebPoint first = ExtractWebPoint(event->touches()->item(0));
  WebPoint second = ExtractWebPoint(event->touches()->item(1));
  double distance = CalculateDistance(first, second);
  Direction direction = Direction::kUnknown;

  // Compare the current distance with the previous to work out the direction we
  // are going in. If we are idle then we should just copy the direction we had
  // previously.
  if (previous_.has_value()) {
    if (distance > previous_->first) {
      direction = Direction::kExpanding;
    } else if (distance < previous_->first) {
      direction = Direction::kContracting;
    } else {
      direction = previous_->second;
    }
  }

  // If we have a |previous| value and that is different from |direction| then
  // we have either identified the direction and |previous| is kUnknown or the
  // direction has changed. In either case we should update the display cutout.
  if (previous_.has_value() && previous_->second != direction) {
    DCHECK(direction != Direction::kUnknown);

    UseCounter::Count(GetDocument(),
                      WebFeature::kMediaControlsDisplayCutoutGesture);
    GetDocument().GetViewportData().SetExpandIntoDisplayCutout(
        direction == Direction::kExpanding);
  }

  // If we are finishing a touch then clear any stored value, otherwise store
  // the latest distance.
  if (event->type() == event_type_names::kTouchend ||
      event->type() == event_type_names::kTouchcancel) {
    DCHECK(previous_.has_value());
    previous_.reset();
  } else {
    previous_ = ResultPair(distance, direction);
  }
}

Document& MediaControlsDisplayCutoutDelegate::GetDocument() {
  return video_element_->GetDocument();
}

}  // namespace blink

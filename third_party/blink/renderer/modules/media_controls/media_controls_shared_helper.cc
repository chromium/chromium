// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/media_controls_shared_helper.h"

#include <cmath>

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element_controls_list.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/time_ranges.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace {

const double kCurrentTimeBufferedDelta = 1.0;

}

namespace blink {

// |element| is the element to listen for the 'transitionend' event on.
// |callback| is the callback to call when the event is handled.
MediaControlsSharedHelpers::TransitionEventListener::TransitionEventListener(
    Element* element,
    Callback callback)
    : callback_(callback), element_(element) {
  DCHECK(callback_);
  DCHECK(element_);
}

void MediaControlsSharedHelpers::TransitionEventListener::Attach() {
  DCHECK(!attached_);
  attached_ = true;

  element_->addEventListener(event_type_names::kTransitionend, this, false);
}

void MediaControlsSharedHelpers::TransitionEventListener::Detach() {
  DCHECK(attached_);
  attached_ = false;

  element_->removeEventListener(event_type_names::kTransitionend, this, false);
}

bool MediaControlsSharedHelpers::TransitionEventListener::IsAttached() const {
  return attached_;
}

void MediaControlsSharedHelpers::TransitionEventListener::Invoke(
    ExecutionContext* context,
    Event* event) {
  if (event->target() != element_)
    return;

  if (event->type() == event_type_names::kTransitionend) {
    callback_.Run();
    return;
  }

  NOTREACHED_IN_MIGRATION();
}

void MediaControlsSharedHelpers::TransitionEventListener::Trace(
    blink::Visitor* visitor) const {
  NativeEventListener::Trace(visitor);
  visitor->Trace(element_);
}

std::optional<unsigned> MediaControlsSharedHelpers::GetCurrentBufferedTimeRange(
    HTMLMediaElement& media_element) {
  double current_time = media_element.currentTime();
  double duration = media_element.duration();
  TimeRanges* buffered_time_ranges = media_element.buffered();

  DCHECK(buffered_time_ranges);

  if (!std::isfinite(duration) || !duration || std::isnan(current_time)) {
    return std::nullopt;
  }

  // Calculate the size of the after segment (i.e. what has been buffered).
  for (unsigned i = 0; i < buffered_time_ranges->length(); ++i) {
    float start = buffered_time_ranges->start(i, ASSERT_NO_EXCEPTION);
    float end = buffered_time_ranges->end(i, ASSERT_NO_EXCEPTION);
    // The delta is there to avoid corner cases when buffered
    // ranges is out of sync with current time because of
    // asynchronous media pipeline and current time caching in
    // HTMLMediaElement.
    // This is related to https://www.w3.org/Bugs/Public/show_bug.cgi?id=28125
    // FIXME: Remove this workaround when WebMediaPlayer
    // has an asynchronous pause interface.
    if (!std::isnan(start) && !std::isnan(end) &&
        start <= current_time + kCurrentTimeBufferedDelta &&
        end > current_time) {
      return i;
    }
  }

  return std::nullopt;
}

String MediaControlsSharedHelpers::FormatTime(double time) {
  if (!std::isfinite(time))
    time = 0;

  int seconds = static_cast<int>(fabs(time));
  int minutes = seconds / 60;
  int hours = minutes / 60;

  seconds %= 60;
  minutes %= 60;

  const char* negative_sign = (time < 0 ? "-" : "");

  // [0-10) minutes duration is m:ss
  // [10-60) minutes duration is mm:ss
  // [1-10) hours duration is h:mm:ss
  // [10-100) hours duration is hh:mm:ss
  // [100-1000) hours duration is hhh:mm:ss
  // etc.

  if (hours > 0) {
    return String::Format("%s%d:%02d:%02d", negative_sign, hours, minutes,
                          seconds);
  }

  return String::Format("%s%d:%02d", negative_sign, minutes, seconds);
}

bool MediaControlsSharedHelpers::ShouldShowFullscreenButton(
    const HTMLMediaElement& media_element) {
  // Unconditionally allow the user to exit fullscreen if we are in it
  // now.  Especially on android, when we might not yet know if
  // fullscreen is supported, we sometimes guess incorrectly and show
  // the button earlier, and we don't want to remove it here if the
  // user chose to enter fullscreen.  crbug.com/500732 .
  if (media_element.IsFullscreen())
    return true;

  if (!IsA<HTMLVideoElement>(media_element))
    return false;

  if (!media_element.HasVideo())
    return false;

  if (!Fullscreen::FullscreenEnabled(media_element.GetDocument()))
    return false;

  if (media_element.ControlsListInternal()->ShouldHideFullscreen() &&
      !media_element.UserWantsControlsVisible()) {
    UseCounter::Count(media_element.GetDocument(),
                      WebFeature::kHTMLMediaElementControlsListNoFullscreen);
    return false;
  }

  return true;
}

}  // namespace blink

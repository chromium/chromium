// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_timeline_element.h"

#include "third_party/blink/public/common/widget/screen_info.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html/time_ranges.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/touch.h"
#include "third_party/blink/renderer/core/input/touch_list.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_current_time_display_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_remaining_time_display_element.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_shared_helper.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace {

const int kThumbRadius = 6;

// Only respond to main button of primary pointer(s).
bool IsValidPointerEvent(const blink::Event& event) {
  DCHECK(blink::IsA<blink::PointerEvent>(event));
  const auto& pointer_event = blink::To<blink::PointerEvent>(event);
  return pointer_event.isPrimary() &&
         pointer_event.button() ==
             static_cast<int16_t>(blink::WebPointerProperties::Button::kLeft);
}

}  // namespace.

namespace blink {

// The DOM structure looks like:
//
// MediaControlTimelineElement
//   (-webkit-media-controls-timeline)
// +-div#thumb (created by the HTMLSliderElement)
MediaControlTimelineElement::MediaControlTimelineElement(
    MediaControlsImpl& media_controls)
    : MediaControlSliderElement(media_controls) {
  SetShadowPseudoId(AtomicString("-webkit-media-controls-timeline"));
}

bool MediaControlTimelineElement::WillRespondToMouseClickEvents() {
  return isConnected() && GetDocument().IsActive();
}

void MediaControlTimelineElement::UpdateAria() {
  String aria_label =
      GetLocale().QueryString(IsA<HTMLVideoElement>(MediaElement())
                                  ? IDS_AX_MEDIA_VIDEO_SLIDER_HELP
                                  : IDS_AX_MEDIA_AUDIO_SLIDER_HELP) +
      " " + GetMediaControls().CurrentTimeDisplay().textContent(true) + " " +
      GetMediaControls().RemainingTimeDisplay().textContent(true);
  setAttribute(html_names::kAriaLabelAttr, AtomicString(aria_label));

  setAttribute(html_names::kAriaValuetextAttr,
               AtomicString(GetLocale().QueryString(
                   IDS_AX_MEDIA_CURRENT_TIME_DISPLAY,
                   GetMediaControls().CurrentTimeDisplay().textContent(true))));
}

void MediaControlTimelineElement::SetPosition(double current_time,
                                              bool suppress_aria) {
  setValue(String::Number(current_time));

  if (!suppress_aria)
    UpdateAria();

  RenderBarSegments();
}

void MediaControlTimelineElement::SetDuration(double duration) {
  double duration_value = std::isfinite(duration) ? duration : 0;
  SetFloatingPointAttribute(html_names::kMaxAttr, duration_value);
  RenderBarSegments();
}

const char* MediaControlTimelineElement::GetNameForHistograms() const {
  return "TimelineSlider";
}

void MediaControlTimelineElement::DefaultEventHandler(Event& event) {
  if (!isConnected() || !GetDocument().IsActive() || controls_hidden_)
    return;

  RenderBarSegments();

  if (BeginScrubbingEvent(event)) {
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.ScrubbingBegin"));
    GetMediaControls().BeginScrubbing(MediaControlsImpl::IsTouchEvent(&event));
    Element* thumb = UserAgentShadowRoot()->getElementById(
        shadow_element_names::kIdSliderThumb);
    bool started_from_thumb = thumb && thumb == event.target()->ToNode();
    metrics_.StartGesture(started_from_thumb);
  } else if (EndScrubbingEvent(event)) {
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.ScrubbingEnd"));
    GetMediaControls().EndScrubbing();
    metrics_.RecordEndGesture(TrackWidth(), MediaElement().duration());
  }

  if (event.type() == event_type_names::kFocus)
    UpdateAria();

  if (event.type() == event_type_names::kKeydown)
    metrics_.StartKey();

  auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
  if (event.type() == event_type_names::kKeyup && keyboard_event)
    metrics_.RecordEndKey(TrackWidth(), keyboard_event->keyCode());

  MediaControlInputElement::DefaultEventHandler(event);

  if (IsA<MouseEvent>(event) || keyboard_event || IsA<GestureEvent>(event) ||
      IsA<PointerEvent>(event)) {
    MaybeRecordInteracted();
  }

  // Update the value based on the touchmove event.
  if (is_touching_ && event.type() == event_type_names::kTouchmove) {
    auto& touch_event = To<TouchEvent>(event);
    if (touch_event.touches()->length() != 1)
      return;

    const Touch* touch = touch_event.touches()->item(0);
    double position =
        max(0.0, fmin(1.0, touch->clientX() / TrackWidth() * ZoomFactor()));
    SetPosition(position * MediaElement().duration());
  } else if (event.type() != event_type_names::kInput) {
    return;
  }

  double time = value().ToDouble();
  double duration = MediaElement().duration();
  // Workaround for floating point error - it's possible for this element's max
  // attribute to be rounded to a value slightly higher than the duration. If
  // this happens and scrubber is dragged near the max, seek to duration.
  if (time > duration)
    time = duration;

  metrics_.OnInput(MediaElement().currentTime(), time);

  // FIXME: This will need to take the timeline offset into consideration
  // once that concept is supported, see https://crbug.com/312699
  if (MediaElement().seekable()->Contain(time))
    MediaElement().setCurrentTime(time);

  // Provide immediate feedback (without waiting for media to seek) to make it
  // easier for user to seek to a precise time.
  GetMediaControls().UpdateCurrentTimeDisplay();
}

bool MediaControlTimelineElement::KeepEventInNode(const Event& event) const {
  return MediaControlElementsHelper::IsUserInteractionEventForSlider(
      event, GetLayoutObject());
}

void MediaControlTimelineElement::RenderBarSegments() {
  SetupBarSegments();

  double current_time = MediaElement().currentTime();
  double duration = MediaElement().duration();

  // Draw the buffered range. Since the element may have multiple buffered
  // ranges and it'd be distracting/'busy' to show all of them, show only the
  // buffered range containing the current play head.
  TimeRanges* buffered_time_ranges = MediaElement().buffered();
  DCHECK(buffered_time_ranges);
  if (std::isnan(duration) || std::isinf(duration) || !duration ||
      std::isnan(current_time)) {
    SetBeforeSegmentPosition(MediaControlSliderElement::Position(0, 0));
    SetAfterSegmentPosition(MediaControlSliderElement::Position(0, 0));
    return;
  }

  double current_position = current_time / duration;

  // Transform the current_position to always align with the center of thumb
  // At time 0, the thumb's center is 6px away from beginning of progress bar
  // At the end of video, thumb's center is -6px away from end of progress bar
  // Convert 6px into ratio respect to progress bar width since
  // current_position is range from 0 to 1
  double width = TrackWidth() / ZoomFactor();
  if (width != 0 && current_position != 0 && !MediaElement().ended()) {
    double offset = kThumbRadius / width;
    current_position += offset - (2 * offset * current_position);
  }

  MediaControlSliderElement::Position before_segment(0, 0);
  MediaControlSliderElement::Position after_segment(0, 0);

  // The before segment (i.e. what has been played) should be purely be based on
  // the current time.
  before_segment.width = current_position;

  base::Optional<unsigned> current_buffered_time_range =
      MediaControlsSharedHelpers::GetCurrentBufferedTimeRange(MediaElement());

  if (current_buffered_time_range) {
    float end = buffered_time_ranges->end(current_buffered_time_range.value(),
                                          ASSERT_NO_EXCEPTION);

    double end_position = end / duration;

    // Draw dark grey highlight to show what we have loaded. This just uses a
    // width since it just starts at zero just like the before segment.
    // We use |std::max()| here because |current_position| has an offset added
    // to it and can therefore be greater than |end_position| in some cases.
    after_segment.width = std::max(current_position, end_position);
  }

  // Update the positions of the segments.
  SetBeforeSegmentPosition(before_segment);
  SetAfterSegmentPosition(after_segment);
}

void MediaControlTimelineElement::Trace(Visitor* visitor) const {
  MediaControlSliderElement::Trace(visitor);
}

bool MediaControlTimelineElement::BeginScrubbingEvent(Event& event) {
  if (event.type() == event_type_names::kTouchstart) {
    is_touching_ = true;
    return true;
  }
  if (event.type() == event_type_names::kPointerdown)
    return IsValidPointerEvent(event);

  return false;
}

void MediaControlTimelineElement::OnControlsHidden() {
  controls_hidden_ = true;

  // End scrubbing state.
  is_touching_ = false;
  MediaControlSliderElement::OnControlsHidden();
}

void MediaControlTimelineElement::OnControlsShown() {
  controls_hidden_ = false;
  MediaControlSliderElement::OnControlsShown();
}

bool MediaControlTimelineElement::EndScrubbingEvent(Event& event) {
  if (is_touching_) {
    if (event.type() == event_type_names::kTouchend ||
        event.type() == event_type_names::kTouchcancel ||
        event.type() == event_type_names::kChange) {
      is_touching_ = false;
      return true;
    }
  } else if (event.type() == event_type_names::kPointerup ||
             event.type() == event_type_names::kPointercancel) {
    return IsValidPointerEvent(event);
  }

  return false;
}

}  // namespace blink

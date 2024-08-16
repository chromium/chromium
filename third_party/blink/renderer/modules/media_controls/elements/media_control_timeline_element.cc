// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_timeline_element.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/user_metrics_action.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
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
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "ui/display/screen_info.h"
#include "ui/strings/grit/ax_strings.h"

namespace {

const int kThumbRadius = 6;
const base::TimeDelta kRenderTimelineInterval = base::Seconds(1);

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
    : MediaControlSliderElement(media_controls),
      render_timeline_timer_(
          GetDocument().GetTaskRunner(TaskType::kInternalMedia),
          this,
          &MediaControlTimelineElement::RenderTimelineTimerFired) {
  SetShadowPseudoId(AtomicString("-webkit-media-controls-timeline"));
}

bool MediaControlTimelineElement::WillRespondToMouseClickEvents() {
  return isConnected() && GetDocument().IsActive();
}

void MediaControlTimelineElement::UpdateAria() {
  String aria_label = GetLocale().QueryString(
      IsA<HTMLVideoElement>(MediaElement()) ? IDS_AX_MEDIA_VIDEO_SLIDER_HELP
                                            : IDS_AX_MEDIA_AUDIO_SLIDER_HELP);
  setAttribute(html_names::kAriaLabelAttr, AtomicString(aria_label));

  // The aria-valuetext is a human-friendly description of the current value
  // of the slider, as opposed to the natural slider value which will be read
  // out as a percentage.
  setAttribute(html_names::kAriaValuetextAttr,
               AtomicString(GetLocale().QueryString(
                   IDS_AX_MEDIA_CURRENT_TIME_DISPLAY,
                   GetMediaControls().CurrentTimeDisplay().FormatTime())));

  // The total time is exposed as aria-description, which will be read after the
  // aria-label and aria-valuetext. Unfortunately, aria-valuenow will not work,
  // because it must be numeric. ARIA and platform APIs do not provide a means
  // of setting a friendly max value, similar to aria-valuetext. Note:
  // IDS_AX_MEDIA_TIME_REMAINING_DISPLAY is a misnomer and refers to the total
  // time.
  setAttribute(html_names::kAriaDescriptionAttr,
               AtomicString(GetLocale().QueryString(
                   IDS_AX_MEDIA_TIME_REMAINING_DISPLAY,
                   GetMediaControls()
                       .RemainingTimeDisplay()
                       .MediaControlTimeDisplayElement::FormatTime())));
}

void MediaControlTimelineElement::SetPosition(double current_time,
                                              bool suppress_aria) {
  if (is_live_ && !live_anchor_time_ && current_time != 0) {
    live_anchor_time_.emplace(LiveAnchorTime());
    live_anchor_time_->clock_time_ = base::TimeTicks::Now();
    live_anchor_time_->media_time_ = MediaElement().currentTime();
  }

  MaybeUpdateTimelineInterval();
  SetValue(String::Number(current_time));

  if (!suppress_aria)
    UpdateAria();

  RenderBarSegments();
}

void MediaControlTimelineElement::SetDuration(double duration) {
  is_live_ = std::isinf(duration);
  double duration_value = duration;
  SetFloatingPointAttribute(html_names::kMaxAttr,
                            is_live_ ? 0.0 : duration_value);
  SetFloatingPointAttribute(html_names::kMinAttr, 0.0);
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
    is_scrubbing_ = true;
    GetMediaControls().BeginScrubbing(MediaControlsImpl::IsTouchEvent(&event));
  } else if (EndScrubbingEvent(event)) {
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.ScrubbingEnd"));
    is_scrubbing_ = false;
    GetMediaControls().EndScrubbing();
  }

  if (event.type() == event_type_names::kFocus)
    UpdateAria();

  MediaControlInputElement::DefaultEventHandler(event);

  if (IsA<MouseEvent>(event) || IsA<KeyboardEvent>(event) ||
      IsA<GestureEvent>(event) || IsA<PointerEvent>(event)) {
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

  double time = Value().ToDouble();
  double duration = MediaElement().duration();
  // Workaround for floating point error - it's possible for this element's max
  // attribute to be rounded to a value slightly higher than the duration. If
  // this happens and scrubber is dragged near the max, seek to duration.
  if (time > duration)
    time = duration;

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

void MediaControlTimelineElement::OnMediaPlaying() {
  if (!is_live_)
    return;

  render_timeline_timer_.Stop();
}

void MediaControlTimelineElement::OnMediaStoppedPlaying() {
  if (!is_live_ || is_scrubbing_ || !live_anchor_time_)
    return;

  render_timeline_timer_.StartRepeating(kRenderTimelineInterval, FROM_HERE);
}

void MediaControlTimelineElement::OnProgress() {
  MaybeUpdateTimelineInterval();
  RenderBarSegments();
}

void MediaControlTimelineElement::RenderTimelineTimerFired(TimerBase*) {
  MaybeUpdateTimelineInterval();
  RenderBarSegments();
}

void MediaControlTimelineElement::MaybeUpdateTimelineInterval() {
  if (!is_live_ || !MediaElement().seekable()->length() || !live_anchor_time_)
    return;

  int last_seekable = MediaElement().seekable()->length() - 1;
  double seekable_start =
      MediaElement().seekable()->start(last_seekable, ASSERT_NO_EXCEPTION);
  double seekable_end =
      MediaElement().seekable()->end(last_seekable, ASSERT_NO_EXCEPTION);
  double expected_media_time_now =
      live_anchor_time_->media_time_ +
      (base::TimeTicks::Now() - live_anchor_time_->clock_time_).InSecondsF();

  // Cap the current live time in seekable range.
  if (expected_media_time_now > seekable_end) {
    live_anchor_time_->media_time_ = seekable_end;
    live_anchor_time_->clock_time_ = base::TimeTicks::Now();
    expected_media_time_now = seekable_end;
  }

  SetFloatingPointAttribute(html_names::kMinAttr, seekable_start);
  SetFloatingPointAttribute(html_names::kMaxAttr, expected_media_time_now);
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

  // Calculate |current_time| and |duration| for live media base on the timeline
  // value since timeline's minimum value is not necessarily zero.
  if (is_live_) {
    current_time =
        Value().ToDouble() - GetFloatingPointAttribute(html_names::kMinAttr);
    duration = GetFloatingPointAttribute(html_names::kMaxAttr) -
               GetFloatingPointAttribute(html_names::kMinAttr);
  }

  if (!std::isfinite(duration) || !duration || std::isnan(current_time)) {
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

  std::optional<unsigned> current_buffered_time_range =
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
  visitor->Trace(render_timeline_timer_);
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

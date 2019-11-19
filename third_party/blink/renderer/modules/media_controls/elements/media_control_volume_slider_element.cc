// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_volume_slider_element.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_consts.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"

namespace blink {

MediaControlVolumeSliderElement::MediaControlVolumeSliderElement(
    MediaControlsImpl& media_controls)
    : MediaControlSliderElement(media_controls) {
  setAttribute(html_names::kMaxAttr, "1");
  setAttribute(html_names::kAriaValuemaxAttr, "100");
  setAttribute(html_names::kAriaValueminAttr, "0");
  setAttribute(html_names::kAriaLabelAttr, "volume");
  SetShadowPseudoId(AtomicString("-webkit-media-controls-volume-slider"));
  SetVolumeInternal(MediaElement().volume());

  CloseSlider();
}

void MediaControlVolumeSliderElement::SetVolume(double volume) {
  if (value().ToDouble() == volume)
    return;

  setValue(String::Number(volume));
  SetVolumeInternal(volume);
}

void MediaControlVolumeSliderElement::OpenSlider() {
  classList().Remove(kClosedCSSClass);
}

void MediaControlVolumeSliderElement::CloseSlider() {
  classList().Add(kClosedCSSClass);
}

bool MediaControlVolumeSliderElement::WillRespondToMouseMoveEvents() {
  if (!isConnected() || !GetDocument().IsActive())
    return false;

  return MediaControlInputElement::WillRespondToMouseMoveEvents();
}

bool MediaControlVolumeSliderElement::WillRespondToMouseClickEvents() {
  if (!isConnected() || !GetDocument().IsActive())
    return false;

  return MediaControlInputElement::WillRespondToMouseClickEvents();
}

const char* MediaControlVolumeSliderElement::GetNameForHistograms() const {
  return "VolumeSlider";
}

void MediaControlVolumeSliderElement::DefaultEventHandler(Event& event) {
  if (!isConnected() || !GetDocument().IsActive())
    return;

  MediaControlInputElement::DefaultEventHandler(event);

  if (event.IsMouseEvent() || event.IsKeyboardEvent() ||
      event.IsGestureEvent() || event.IsPointerEvent()) {
    MaybeRecordInteracted();
  }

  if (event.type() == event_type_names::kPointerdown) {
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.VolumeChangeBegin"));
  }

  if (event.type() == event_type_names::kPointerup) {
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.VolumeChangeEnd"));
  }

  if (event.type() == event_type_names::kInput) {
    double volume = value().ToDouble();
    MediaElement().setVolume(volume);
    MediaElement().setMuted(false);
    SetVolumeInternal(volume);
  }

  if (event.type() == event_type_names::kFocus)
    GetMediaControls().OpenVolumeSliderIfNecessary();

  if (event.type() == event_type_names::kBlur)
    GetMediaControls().CloseVolumeSliderIfNecessary();
}

void MediaControlVolumeSliderElement::SetVolumeInternal(double volume) {
  SetupBarSegments();
  SetAfterSegmentPosition(MediaControlSliderElement::Position(0, volume));
  int percent_vol = 100 * volume;
  setAttribute(html_names::kAriaValuenowAttr,
               WTF::AtomicString::Number(percent_vol));
}

bool MediaControlVolumeSliderElement::KeepEventInNode(
    const Event& event) const {
  return MediaControlElementsHelper::IsUserInteractionEventForSlider(
      event, GetLayoutObject());
}

}  // namespace blink

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_volume_slider_element.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/user_metrics_action.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/events/wheel_event.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_consts.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_volume_control_container_element.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"

namespace blink {

namespace {

// The amount to change the volume by for a wheel event.
constexpr double kScrollVolumeDelta = 0.1;

}  // namespace

class MediaControlVolumeSliderElement::WheelEventListener
    : public NativeEventListener {
 public:
  WheelEventListener(MediaControlVolumeSliderElement* volume_slider,
                     MediaControlVolumeControlContainerElement* container)
      : volume_slider_(volume_slider), container_(container) {
    DCHECK(volume_slider);
    DCHECK(container);
  }
  WheelEventListener(const WheelEventListener&) = delete;
  WheelEventListener& operator=(const WheelEventListener&) = delete;
  ~WheelEventListener() override = default;

  void StartListening() {
    if (is_listening_)
      return;
    is_listening_ = true;

    container_->addEventListener(event_type_names::kWheel, this, false);
  }

  void StopListening() {
    if (!is_listening_)
      return;
    is_listening_ = false;

    container_->removeEventListener(event_type_names::kWheel, this, false);
  }

  void Trace(Visitor* visitor) const override {
    NativeEventListener::Trace(visitor);
    visitor->Trace(volume_slider_);
    visitor->Trace(container_);
  }

 private:
  void Invoke(ExecutionContext*, Event* event) override {
    auto* wheel_event = DynamicTo<WheelEvent>(event);
    if (wheel_event)
      volume_slider_->OnWheelEvent(wheel_event);
  }

  Member<MediaControlVolumeSliderElement> volume_slider_;
  Member<MediaControlVolumeControlContainerElement> container_;
  bool is_listening_ = false;
};

MediaControlVolumeSliderElement::MediaControlVolumeSliderElement(
    MediaControlsImpl& media_controls,
    MediaControlVolumeControlContainerElement* container)
    : MediaControlSliderElement(media_controls),
      wheel_event_listener_(
          MakeGarbageCollected<WheelEventListener>(this, container)) {
  setAttribute(html_names::kMaxAttr, AtomicString("1"));
  setAttribute(html_names::kAriaValuemaxAttr, AtomicString("100"));
  setAttribute(html_names::kAriaValueminAttr, AtomicString("0"));
  setAttribute(html_names::kAriaLabelAttr, AtomicString("volume"));
  SetShadowPseudoId(AtomicString("-webkit-media-controls-volume-slider"));
  SetVolumeInternal(MediaElement().volume());

  CloseSlider();
}

void MediaControlVolumeSliderElement::SetVolume(double volume) {
  if (Value().ToDouble() == volume)
    return;

  SetValue(String::Number(volume));
  SetVolumeInternal(volume);
}

void MediaControlVolumeSliderElement::OpenSlider() {
  wheel_event_listener_->StartListening();
  classList().Remove(AtomicString(kClosedCSSClass));
}

void MediaControlVolumeSliderElement::CloseSlider() {
  wheel_event_listener_->StopListening();
  classList().Add(AtomicString(kClosedCSSClass));
}

bool MediaControlVolumeSliderElement::WillRespondToMouseMoveEvents() const {
  if (!isConnected() || !GetDocument().IsActive())
    return false;

  return MediaControlInputElement::WillRespondToMouseMoveEvents();
}

bool MediaControlVolumeSliderElement::WillRespondToMouseClickEvents() {
  if (!isConnected() || !GetDocument().IsActive())
    return false;

  return MediaControlInputElement::WillRespondToMouseClickEvents();
}

void MediaControlVolumeSliderElement::Trace(Visitor* visitor) const {
  MediaControlSliderElement::Trace(visitor);
  visitor->Trace(wheel_event_listener_);
}

const char* MediaControlVolumeSliderElement::GetNameForHistograms() const {
  return "VolumeSlider";
}

void MediaControlVolumeSliderElement::DefaultEventHandler(Event& event) {
  if (!isConnected() || !GetDocument().IsActive())
    return;

  MediaControlInputElement::DefaultEventHandler(event);

  if (IsA<MouseEvent>(event) || IsA<KeyboardEvent>(event) ||
      IsA<GestureEvent>(event) || IsA<PointerEvent>(event)) {
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

  if (event.type() == event_type_names::kInput)
    UnmuteAndSetVolume(Value().ToDouble());

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

void MediaControlVolumeSliderElement::OnWheelEvent(WheelEvent* wheel_event) {
  double current_volume = Value().ToDouble();
  double new_volume = (wheel_event->wheelDelta() > 0)
                          ? current_volume + kScrollVolumeDelta
                          : current_volume - kScrollVolumeDelta;
  new_volume = std::max(0.0, std::min(1.0, new_volume));

  UnmuteAndSetVolume(new_volume);
  wheel_event->SetDefaultHandled();
}

void MediaControlVolumeSliderElement::UnmuteAndSetVolume(double volume) {
  MediaElement().setVolume(volume);
  MediaElement().setMuted(false);
  SetVolumeInternal(volume);
}

}  // namespace blink

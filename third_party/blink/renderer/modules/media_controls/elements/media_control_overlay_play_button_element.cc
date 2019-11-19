// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overlay_play_button_element.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace {

// The size of the inner circle button in pixels.
constexpr int kInnerButtonSize = 56;

// The CSS class to add to hide the element.
const char kHiddenClassName[] = "hidden";

}  // namespace.

namespace blink {

// The DOM structure looks like:
//
// MediaControlOverlayPlayButtonElement
//   (-webkit-media-controls-overlay-play-button)
// +-div (-internal-media-controls-overlay-play-button-internal)
//   This contains the inner circle with the actual play/pause icon.
MediaControlOverlayPlayButtonElement::MediaControlOverlayPlayButtonElement(
    MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls), internal_button_(nullptr) {
  EnsureUserAgentShadowRoot();
  setType(input_type_names::kButton);
  SetShadowPseudoId(AtomicString("-webkit-media-controls-overlay-play-button"));

  internal_button_ = MediaControlElementsHelper::CreateDiv(
      "-internal-media-controls-overlay-play-button-internal", GetShadowRoot());
}

void MediaControlOverlayPlayButtonElement::UpdateDisplayType() {
  SetIsWanted(MediaElement().ShouldShowControls());

  int state = MediaElement().paused() ? IDS_AX_MEDIA_PLAY_BUTTON
                                      : IDS_AX_MEDIA_PAUSE_BUTTON;
  setAttribute(html_names::kAriaLabelAttr,
               WTF::AtomicString(GetLocale().QueryString(state)));

  MediaControlInputElement::UpdateDisplayType();
}

const char* MediaControlOverlayPlayButtonElement::GetNameForHistograms() const {
  return "PlayOverlayButton";
}

void MediaControlOverlayPlayButtonElement::MaybePlayPause() {
  if (MediaElement().paused()) {
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.PlayOverlay"));
  } else {
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.PauseOverlay"));
  }

  // Allow play attempts for plain src= media to force a reload in the error
  // state. This allows potential recovery for transient network and decoder
  // resource issues.
  if (MediaElement().error() && !MediaElement().HasMediaSource())
    MediaElement().load();

  MediaElement().TogglePlayState();

  // If we triggered a play event then we should quickly hide the button.
  if (!MediaElement().paused())
    SetIsDisplayed(false);

  MaybeRecordInteracted();
  UpdateDisplayType();
}

void MediaControlOverlayPlayButtonElement::DefaultEventHandler(Event& event) {
  if (!IsDisabled() && (event.type() == event_type_names::kClick ||
                        event.type() == event_type_names::kGesturetap)) {
    event.SetDefaultHandled();
    MaybePlayPause();
  }
  MediaControlInputElement::DefaultEventHandler(event);
}

bool MediaControlOverlayPlayButtonElement::KeepEventInNode(
    const Event& event) const {
  return MediaControlElementsHelper::IsUserInteractionEvent(event);
}

WebSize MediaControlOverlayPlayButtonElement::GetSizeOrDefault() const {
  // The size should come from the internal button which actually displays the
  // button.
  return MediaControlElementsHelper::GetSizeOrDefault(
      *internal_button_, WebSize(kInnerButtonSize, kInnerButtonSize));
}

void MediaControlOverlayPlayButtonElement::SetIsDisplayed(bool displayed) {
  if (displayed == displayed_)
    return;

  SetClass(kHiddenClassName, !displayed);
  displayed_ = displayed;
}

void MediaControlOverlayPlayButtonElement::Trace(blink::Visitor* visitor) {
  MediaControlInputElement::Trace(visitor);
  visitor->Trace(internal_button_);
}

}  // namespace blink

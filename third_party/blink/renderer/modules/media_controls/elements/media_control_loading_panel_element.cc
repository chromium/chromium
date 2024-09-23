// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_loading_panel_element.h"

#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_resource_loader.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "ui/strings/grit/ax_strings.h"

namespace {

static const char kInfinite[] = "infinite";

bool IsInLoadingState(blink::MediaControlsImpl& controls) {
  return controls.State() == blink::MediaControlsImpl::kLoadingMetadataPaused ||
         controls.State() ==
             blink::MediaControlsImpl::kLoadingMetadataPlaying ||
         controls.State() == blink::MediaControlsImpl::kBuffering;
}

}  // namespace

namespace blink {

MediaControlLoadingPanelElement::MediaControlLoadingPanelElement(
    MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls) {
  SetShadowPseudoId(AtomicString("-internal-media-controls-loading-panel"));
  setAttribute(html_names::kRoleAttr, AtomicString("group"));
  setAttribute(
      html_names::kAriaLabelAttr,
      WTF::AtomicString(GetLocale().QueryString(IDS_AX_MEDIA_LOADING_PANEL)));
  setAttribute(html_names::kAriaLiveAttr, AtomicString("polite"));
  CreateUserAgentShadowRoot();

  // The loading panel should always start hidden.
  SetIsWanted(false);
}

// The shadow DOM structure looks like:
//
// #root
// +- #spinner-frame
//   +- #spinner
//     +- #layer
//     | +- #spinner-mask-1
//     | | +- #spinner-mask-1-background
//     \ +- #spinner-mask-2
//         +- #spinner-mask-2-background
void MediaControlLoadingPanelElement::PopulateShadowDOM() {
  ShadowRoot* shadow_root = GetShadowRoot();
  DCHECK(!shadow_root->HasChildren());

  // This stylesheet element and will contain rules that are specific to the
  // loading panel. The shadow DOM protects these rules and rules from the
  // parent DOM from bleeding across the shadow DOM boundary.
  auto* style = MakeGarbageCollected<HTMLStyleElement>(GetDocument());
  style->setTextContent(
      MediaControlsResourceLoader::GetShadowLoadingStyleSheet());
  shadow_root->ParserAppendChild(style);

  // The spinner frame is centers the spinner in the middle of the element and
  // cuts off any overflowing content. It also contains a SVG mask which will
  // overlay the spinner and cover up any rough edges created by the moving
  // elements.
  HTMLDivElement* spinner_frame = MediaControlElementsHelper::CreateDivWithId(
      AtomicString("spinner-frame"), shadow_root);
  spinner_frame->SetShadowPseudoId(
      AtomicString("-internal-media-controls-loading-panel-spinner-frame"));

  // The spinner is responsible for rotating the elements below. The square
  // edges will be cut off by the frame above.
  HTMLDivElement* spinner = MediaControlElementsHelper::CreateDivWithId(
      AtomicString("spinner"), spinner_frame);

  // The layer performs a secondary "fill-unfill-rotate" animation.
  HTMLDivElement* layer = MediaControlElementsHelper::CreateDivWithId(
      AtomicString("layer"), spinner);

  // The spinner is split into two halves, one on the left (1) and the other
  // on the right (2). The mask elements stop the background from overlapping
  // each other. The background elements rotate a SVG mask from the bottom to
  // the top. The mask contains a white background with a transparent cutout
  // that forms the look of the transparent spinner. The background should
  // always be bigger than the mask in order to ensure there are no gaps
  // created by the animation.
  HTMLDivElement* mask1 = MediaControlElementsHelper::CreateDivWithId(
      AtomicString("spinner-mask-1"), layer);
  mask1_background_ = MediaControlElementsHelper::CreateDiv(
      AtomicString(
          "-internal-media-controls-loading-panel-spinner-mask-1-background"),
      mask1);
  HTMLDivElement* mask2 = MediaControlElementsHelper::CreateDivWithId(
      AtomicString("spinner-mask-2"), layer);
  mask2_background_ = MediaControlElementsHelper::CreateDiv(
      AtomicString(
          "-internal-media-controls-loading-panel-spinner-mask-2-background"),
      mask2);

  event_listener_ =
      MakeGarbageCollected<MediaControlAnimationEventListener>(this);
}

void MediaControlLoadingPanelElement::RemovedFrom(
    ContainerNode& insertion_point) {
  if (event_listener_) {
    event_listener_->Detach();
    event_listener_.Clear();
  }

  MediaControlDivElement::RemovedFrom(insertion_point);
}

void MediaControlLoadingPanelElement::CleanupShadowDOM() {
  // Clear the shadow DOM children and all references to it.
  ShadowRoot* shadow_root = GetShadowRoot();
  DCHECK(shadow_root->HasChildren());
  if (event_listener_) {
    event_listener_->Detach();
    event_listener_.Clear();
  }
  shadow_root->RemoveChildren();

  mask1_background_.Clear();
  mask2_background_.Clear();
}

void MediaControlLoadingPanelElement::SetAnimationIterationCount(
    const String& count_value) {
  if (mask1_background_) {
    mask1_background_->SetInlineStyleProperty(
        CSSPropertyID::kAnimationIterationCount, count_value);
  }
  if (mask2_background_) {
    mask2_background_->SetInlineStyleProperty(
        CSSPropertyID::kAnimationIterationCount, count_value);
  }
}

void MediaControlLoadingPanelElement::UpdateDisplayState() {
  // If the media consols are playing then we should hide the element as
  // soon as possible since we are obscuring the video.
  if (GetMediaControls().State() == MediaControlsImpl::kPlaying &&
      state_ != State::kHidden) {
    HideAnimation();
    return;
  }

  switch (state_) {
    case State::kHidden:
      // If the media controls are loading metadata then we should show the
      // loading panel and insert it into the DOM.
      if (IsInLoadingState(GetMediaControls()) && !controls_hidden_) {
        PopulateShadowDOM();
        SetIsWanted(true);
        SetAnimationIterationCount(kInfinite);
        state_ = State::kPlaying;
      }
      break;
    case State::kPlaying:
      // If the media controls are stopped then we should hide the loading
      // panel, but not until the current cycle of animations is complete.
      if (!IsInLoadingState(GetMediaControls())) {
        SetAnimationIterationCount(WTF::String::Number(animation_count_ + 1));
        state_ = State::kCoolingDown;
      }
      break;
    case State::kCoolingDown:
      // Do nothing.
      break;
  }
}

void MediaControlLoadingPanelElement::OnControlsHidden() {
  controls_hidden_ = true;

  // If the animation is currently playing, clean it up.
  if (state_ != State::kHidden)
    HideAnimation();
}

void MediaControlLoadingPanelElement::HideAnimation() {
  DCHECK(state_ != State::kHidden);

  SetIsWanted(false);
  state_ = State::kHidden;
  animation_count_ = 0;
  CleanupShadowDOM();
}

void MediaControlLoadingPanelElement::OnControlsShown() {
  controls_hidden_ = false;
  UpdateDisplayState();
}

void MediaControlLoadingPanelElement::OnAnimationEnd() {
  // If we have gone back to the loading metadata state (e.g. the source
  // changed). Then we should jump back to playing.
  if (IsInLoadingState(GetMediaControls())) {
    state_ = State::kPlaying;
    SetAnimationIterationCount(kInfinite);
    return;
  }

  // The animation has finished so we can go back to the hidden state and
  // cleanup the shadow DOM.
  HideAnimation();
}

void MediaControlLoadingPanelElement::OnAnimationIteration() {
  animation_count_ += 1;
}

Element& MediaControlLoadingPanelElement::WatchedAnimationElement() const {
  DCHECK(mask1_background_);
  return *mask1_background_;
}

void MediaControlLoadingPanelElement::Trace(Visitor* visitor) const {
  MediaControlAnimationEventListener::Observer::Trace(visitor);
  MediaControlDivElement::Trace(visitor);
  visitor->Trace(event_listener_);
  visitor->Trace(mask1_background_);
  visitor->Trace(mask2_background_);
}

}  // namespace blink

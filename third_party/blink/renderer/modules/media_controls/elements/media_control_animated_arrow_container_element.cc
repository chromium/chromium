// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_animated_arrow_container_element.h"

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_resource_loader.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

MediaControlAnimatedArrowContainerElement::AnimatedArrow::AnimatedArrow(
    const AtomicString& id,
    Document& document)
    : HTMLDivElement(document) {
  SetIdAttribute(id);
}

void MediaControlAnimatedArrowContainerElement::AnimatedArrow::HideInternal() {
  DCHECK(!hidden_);
  svg_container_->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                         CSSValueID::kNone);
  hidden_ = true;
}

void MediaControlAnimatedArrowContainerElement::AnimatedArrow::ShowInternal() {
  DCHECK(hidden_);
  hidden_ = false;

  if (svg_container_) {
    svg_container_->RemoveInlineStyleProperty(CSSPropertyID::kDisplay);
    return;
  }

  setInnerHTML(MediaControlsResourceLoader::GetJumpSVGImage());

  last_arrow_ = getElementById(AtomicString("arrow-3"));
  svg_container_ = getElementById(AtomicString("jump"));

  event_listener_ =
      MakeGarbageCollected<MediaControlAnimationEventListener>(this);
}

void MediaControlAnimatedArrowContainerElement::AnimatedArrow::
    OnAnimationIteration() {
  counter_--;

  if (counter_ == 0)
    HideInternal();
}

void MediaControlAnimatedArrowContainerElement::AnimatedArrow::Show() {
  if (hidden_)
    ShowInternal();

  counter_++;
}

Element& MediaControlAnimatedArrowContainerElement::AnimatedArrow::
    WatchedAnimationElement() const {
  return *last_arrow_;
}

void MediaControlAnimatedArrowContainerElement::AnimatedArrow::Trace(
    Visitor* visitor) const {
  MediaControlAnimationEventListener::Observer::Trace(visitor);
  HTMLDivElement::Trace(visitor);
  visitor->Trace(last_arrow_);
  visitor->Trace(svg_container_);
  visitor->Trace(event_listener_);
}

MediaControlAnimatedArrowContainerElement::
    MediaControlAnimatedArrowContainerElement(MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls),
      left_jump_arrow_(nullptr),
      right_jump_arrow_(nullptr) {
  EnsureUserAgentShadowRoot();
  SetShadowPseudoId(
      AtomicString("-internal-media-controls-animated-arrow-container"));
}

void MediaControlAnimatedArrowContainerElement::ShowArrowAnimation(
    MediaControlAnimatedArrowContainerElement::ArrowDirection direction) {
  // Load the arrow icons and associate CSS the first time we jump.
  if (!left_jump_arrow_) {
    DCHECK(!right_jump_arrow_);
    ShadowRoot* shadow_root = GetShadowRoot();

    // This stylesheet element and will contain rules that are specific to the
    // jump arrows. The shadow DOM protects these rules from the parent DOM
    // from bleeding across the shadow DOM boundary.
    auto* style = MakeGarbageCollected<HTMLStyleElement>(GetDocument());
    style->setTextContent(
        MediaControlsResourceLoader::GetAnimatedArrowStyleSheet());
    shadow_root->ParserAppendChild(style);

    left_jump_arrow_ = MakeGarbageCollected<
        MediaControlAnimatedArrowContainerElement::AnimatedArrow>(
        AtomicString("left-arrow"), GetDocument());
    shadow_root->ParserAppendChild(left_jump_arrow_);

    right_jump_arrow_ = MakeGarbageCollected<
        MediaControlAnimatedArrowContainerElement::AnimatedArrow>(
        AtomicString("right-arrow"), GetDocument());
    shadow_root->ParserAppendChild(right_jump_arrow_);
  }

  DCHECK(left_jump_arrow_ && right_jump_arrow_);

  if (direction ==
      MediaControlAnimatedArrowContainerElement::ArrowDirection::kLeft) {
    left_jump_arrow_->Show();
  } else {
    right_jump_arrow_->Show();
  }
}

void MediaControlAnimatedArrowContainerElement::Trace(Visitor* visitor) const {
  MediaControlDivElement::Trace(visitor);
  visitor->Trace(left_jump_arrow_);
  visitor->Trace(right_jump_arrow_);
}

}  // namespace blink

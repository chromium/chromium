// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_panel_element.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// This is the class name to hide the panel.
const char kTransparentClassName[] = "transparent";

}  // anonymous namespace

MediaControlPanelElement::MediaControlPanelElement(
    MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls), event_listener_(nullptr) {
  SetShadowPseudoId(AtomicString("-webkit-media-controls-panel"));
}

void MediaControlPanelElement::SetIsDisplayed(bool is_displayed) {
  if (is_displayed_ == is_displayed)
    return;

  is_displayed_ = is_displayed;
  if (is_displayed_ && opaque_)
    DidBecomeVisible();
}

bool MediaControlPanelElement::IsOpaque() const {
  return opaque_;
}

void MediaControlPanelElement::MakeOpaque() {
  if (opaque_)
    return;

  opaque_ = true;
  removeAttribute("class");

  if (is_displayed_) {
    // Make sure we are listening for the 'transitionend' event.
    EnsureTransitionEventListener();

    SetIsWanted(true);
    DidBecomeVisible();
  }
}

void MediaControlPanelElement::MakeTransparent() {
  if (!opaque_)
    return;

  // Make sure we are listening for the 'transitionend' event.
  EnsureTransitionEventListener();

  setAttribute("class", kTransparentClassName);

  opaque_ = false;
}

void MediaControlPanelElement::RemovedFrom(ContainerNode& insertion_point) {
  MediaControlDivElement::RemovedFrom(insertion_point);
  DetachTransitionEventListener();
}

void MediaControlPanelElement::Trace(blink::Visitor* visitor) {
  MediaControlDivElement::Trace(visitor);
  visitor->Trace(event_listener_);
}

bool MediaControlPanelElement::KeepDisplayedForAccessibility() {
  return keep_displayed_for_accessibility_;
}

void MediaControlPanelElement::SetKeepDisplayedForAccessibility(bool value) {
  keep_displayed_for_accessibility_ = value;
}

bool MediaControlPanelElement::EventListenerIsAttachedForTest() const {
  return event_listener_->IsAttached();
}

void MediaControlPanelElement::EnsureTransitionEventListener() {
  // Create the event listener if it doesn't exist.
  if (!event_listener_) {
    event_listener_ = MakeGarbageCollected<
        MediaControlsSharedHelpers::TransitionEventListener>(
        this,
        WTF::BindRepeating(&MediaControlPanelElement::HandleTransitionEndEvent,
                           WrapWeakPersistent(this)));
  }

  // Attach the event listener if we are not attached.
  if (!event_listener_->IsAttached())
    event_listener_->Attach();
}

void MediaControlPanelElement::DetachTransitionEventListener() {
  if (!event_listener_)
    return;

  // Detach the event listener if we are attached.
  if (event_listener_->IsAttached())
    event_listener_->Detach();
}

bool MediaControlPanelElement::KeepEventInNode(const Event& event) const {
  return GetMediaControls().ShouldShowAudioControls() &&
         MediaControlElementsHelper::IsUserInteractionEvent(event);
}

void MediaControlPanelElement::DidBecomeVisible() {
  DCHECK(is_displayed_ && opaque_);
  MediaElement().MediaControlsDidBecomeVisible();
}

void MediaControlPanelElement::HandleTransitionEndEvent() {
  // Hide the element in the DOM once we have finished the transition.
  if (!opaque_ && !keep_displayed_for_accessibility_)
    SetIsWanted(false);

  // Now that we have received the 'transitionend' event we can dispose of
  // the listener.
  DetachTransitionEventListener();
}

}  // namespace blink

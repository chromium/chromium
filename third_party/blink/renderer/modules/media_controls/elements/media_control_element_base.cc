// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_element_base.h"

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"

namespace blink {

void MediaControlElementBase::SetIsWanted(bool wanted) {
  if (is_wanted_ == wanted)
    return;

  is_wanted_ = wanted;
  UpdateShownState();
}

bool MediaControlElementBase::IsWanted() const {
  return is_wanted_;
}

void MediaControlElementBase::SetDoesFit(bool fits) {
  does_fit_ = fits;
  UpdateShownState();
}

bool MediaControlElementBase::DoesFit() const {
  return does_fit_;
}

bool MediaControlElementBase::HasOverflowButton() const {
  return false;
}

MediaControlElementBase::MediaControlElementBase(
    MediaControlsImpl& media_controls,
    HTMLElement* element)
    : media_controls_(&media_controls),
      element_(element),
      is_wanted_(true),
      does_fit_(true) {}

void MediaControlElementBase::UpdateShownState() {
  if (is_wanted_ && does_fit_) {
    element_->RemoveInlineStyleProperty(CSSPropertyID::kDisplay);
  } else {
    element_->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                     CSSValueID::kNone);
  }
}

MediaControlsImpl& MediaControlElementBase::GetMediaControls() const {
  DCHECK(media_controls_);
  return *media_controls_;
}

HTMLMediaElement& MediaControlElementBase::MediaElement() const {
  return GetMediaControls().MediaElement();
}

void MediaControlElementBase::Trace(blink::Visitor* visitor) {
  visitor->Trace(media_controls_);
  visitor->Trace(element_);
}

}  // namespace blink

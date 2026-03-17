// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/user_media_element_constraints.h"

#include "third_party/blink/renderer/core/html/html_user_media_element.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

const char UserMediaElementConstraints::kSupplementName[] =
    "UserMediaElementConstraints";

UserMediaElementConstraints& UserMediaElementConstraints::From(
    HTMLUserMediaElement& element) {
  UserMediaElementConstraints* supplement =
      Supplement<HTMLUserMediaElement>::From<UserMediaElementConstraints>(
          element);
  if (!supplement) {
    supplement = MakeGarbageCollected<UserMediaElementConstraints>(element);
    ProvideTo(element, supplement);
  }
  return *supplement;
}

void UserMediaElementConstraints::setConstraints(
    HTMLUserMediaElement& element,
    const MediaStreamConstraints* constraints) {
  UserMediaElementConstraints& self = From(element);
  if (!self.did_set_constraints_) {
    self.SetConstraints(constraints);
    self.did_set_constraints_ = true;
  }
}

UserMediaElementConstraints::UserMediaElementConstraints(
    HTMLUserMediaElement& element)
    : Supplement<HTMLUserMediaElement>(element) {}

void UserMediaElementConstraints::Trace(Visitor* visitor) const {
  visitor->Trace(constraints_);
  Supplement<HTMLUserMediaElement>::Trace(visitor);
}

}  // namespace blink

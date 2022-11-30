// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/user_activation.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

UserActivation* UserActivation::CreateSnapshot(LocalDOMWindow* window) {
  LocalFrame* frame = window->GetFrame();
  return MakeGarbageCollected<UserActivation>(
      frame ? frame->HasStickyUserActivation() : false,
      LocalFrame::HasTransientUserActivation(frame));
}

UserActivation::UserActivation(bool has_been_active, bool is_active)
    : has_been_active_(has_been_active), is_active_(is_active) {}

UserActivation::UserActivation(LocalDOMWindow* window) : window_(window) {}

UserActivation::~UserActivation() = default;

void UserActivation::Trace(Visitor* visitor) const {
  visitor->Trace(window_);
  ScriptWrappable::Trace(visitor);
}

bool UserActivation::hasBeenActive() const {
  LocalFrame* frame = window_ ? window_->GetFrame() : nullptr;
  if (!frame)
    return has_been_active_;
  return frame->HasStickyUserActivation();
}

bool UserActivation::isActive() const {
  LocalFrame* frame = window_ ? window_->GetFrame() : nullptr;
  if (!frame)
    return is_active_;
  return LocalFrame::HasTransientUserActivation(frame);
}

}  // namespace blink

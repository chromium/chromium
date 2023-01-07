// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/websockets/close_event.h"

namespace blink {

CloseEvent::CloseEvent(const AtomicString& type,
                       const CloseEventInit* initializer)
    : Event(type, initializer), was_clean_(false), code_(0) {
  if (initializer->hasWasClean())
    was_clean_ = initializer->wasClean();
  if (initializer->hasCode())
    code_ = initializer->code();
  if (initializer->hasReason())
    reason_ = initializer->reason();
}

}  // namespace blink

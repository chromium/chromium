// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/overconstrained_error.h"

namespace blink {

OverconstrainedError* OverconstrainedError::Create(const String& constraint,
                                                   const String& message) {
  return MakeGarbageCollected<OverconstrainedError>(constraint, message);
}

OverconstrainedError::OverconstrainedError(const String& constraint,
                                           const String& message)
    : DOMException(DOMExceptionCode::kOverconstrainedError, message),
      constraint_(constraint) {}

}  // namespace blink

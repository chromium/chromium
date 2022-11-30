// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/identifiability_study_helper.h"

namespace blink {

// The maximum number of canvas context operations to incorportate into digest
// computation -- constant, but may be overridden by tests using
// IdentifiabilityStudyHelper::ScopedMaxOperationsSetter.
/*static*/ int IdentifiabilityStudyHelper::max_operations_ = 1 << 20;

void IdentifiabilityStudyHelper::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
}

}  // namespace blink

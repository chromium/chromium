// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/part.h"
#include "third_party/blink/renderer/core/dom/part_root.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

Part::Part(PartRoot& root) : root_(root) {
  CHECK(root.SupportsContainedParts());
  root.AddPart(*this);
}

void Part::Trace(Visitor* visitor) const {
  visitor->Trace(root_);
  PartRoot::Trace(visitor);
}

void Part::disconnect() {
  if (!root_) {
    return;
  }
  root_->RemovePart(*this);
  root_ = nullptr;
}

}  // namespace blink

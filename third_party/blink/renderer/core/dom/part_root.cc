// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/part_root.h"

#include "third_party/blink/renderer/core/dom/part.h"

namespace blink {

void PartRoot::Trace(Visitor* visitor) const {
  visitor->Trace(parts_);
  ScriptWrappable::Trace(visitor);
}

void PartRoot::addPart(Part& new_part) {
  parts_.push_back(new_part);
}

HeapVector<Member<Part>> PartRoot::getParts() {
  return parts_;
}

}  // namespace blink

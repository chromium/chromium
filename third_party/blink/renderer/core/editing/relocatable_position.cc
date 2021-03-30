// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/relocatable_position.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

RelocatablePosition::RelocatablePosition(const Position& position)
    : range_(position.IsNotNull()
                 ? MakeGarbageCollected<Range>(*position.GetDocument(),
                                               position,
                                               position)
                 : nullptr) {}

void RelocatablePosition::SetPosition(const Position& position) {
  DCHECK(position.IsNotNull());
  DCHECK(range_);
  DCHECK_EQ(position.GetDocument(), range_->StartPosition().GetDocument());
  range_->setStart(position);
  range_->setEnd(position);
}

Position RelocatablePosition::GetPosition() const {
  if (!range_)
    return Position();
  DCHECK(range_->collapsed());
  return range_->StartPosition();
}

void RelocatablePosition::Trace(Visitor* visitor) const {
  visitor->Trace(range_);
}

}  // namespace blink

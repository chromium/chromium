// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/relocatable_position.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

RelocatablePosition::RelocatablePosition(const Position& position)
    : range_(position.IsNotNull()
                 ? MakeGarbageCollected<Range>(*position.GetDocument(),
                                               position,
                                               position)
                 : nullptr),
      original_position_(position) {}

void RelocatablePosition::SetPosition(const Position& position) {
  DCHECK(position.IsNotNull());
  DCHECK(range_);
  DCHECK_EQ(position.GetDocument(), range_->StartPosition().GetDocument());
  range_->setStart(position);
  range_->setEnd(position);
  original_position_ = position;
}

Position RelocatablePosition::GetPosition() const {
  if (!range_)
    return Position();
  DCHECK(range_->collapsed());
  const Position& position = range_->StartPosition();
  DCHECK(position.IsNotNull());
  DCHECK(position.IsOffsetInAnchor());

  // The Range converted the position into one of type kOffsetInAnchor.
  // Return the original one if it's equivalent to the relocated one.
  if (original_position_.IsConnected() &&
      original_position_.IsEquivalent(position))
    return original_position_;
  return position;
}

void RelocatablePosition::Trace(Visitor* visitor) const {
  visitor->Trace(range_);
  visitor->Trace(original_position_);
}

}  // namespace blink

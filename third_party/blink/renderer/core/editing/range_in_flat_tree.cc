// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/editing/range_in_flat_tree.h"

#include "third_party/blink/renderer/core/editing/ephemeral_range.h"

namespace blink {

RangeInFlatTree::RangeInFlatTree()
    : start_(MakeGarbageCollected<RelocatablePosition>(Position())),
      end_(MakeGarbageCollected<RelocatablePosition>(Position())) {
  DCHECK(IsNull());
}

RangeInFlatTree::RangeInFlatTree(const PositionInFlatTree& start,
                                 const PositionInFlatTree& end)
    : start_(MakeGarbageCollected<RelocatablePosition>(
          ToPositionInDOMTree(start))),
      end_(
          MakeGarbageCollected<RelocatablePosition>(ToPositionInDOMTree(end))) {
  DCHECK_LE(start, end);
}

void RangeInFlatTree::SetStart(const PositionInFlatTree& start) {
  start_->SetPosition(ToPositionInDOMTree(start));
}

void RangeInFlatTree::SetEnd(const PositionInFlatTree& end) {
  end_->SetPosition(ToPositionInDOMTree(end));
}

PositionInFlatTree RangeInFlatTree::StartPosition() const {
  return ToPositionInFlatTree(start_->GetPosition());
}

PositionInFlatTree RangeInFlatTree::EndPosition() const {
  return ToPositionInFlatTree(end_->GetPosition());
}

bool RangeInFlatTree::IsCollapsed() const {
  return start_ == end_;
}

bool RangeInFlatTree::IsConnected() const {
  return StartPosition().ComputeContainerNode()->isConnected() &&
         EndPosition().ComputeContainerNode()->isConnected();
}

bool RangeInFlatTree::IsNull() const {
  return StartPosition().IsNull() || EndPosition().IsNull();
}

EphemeralRangeInFlatTree RangeInFlatTree::ToEphemeralRange() const {
  return EphemeralRangeInFlatTree(StartPosition(), EndPosition());
}

void RangeInFlatTree::Trace(Visitor* visitor) const {
  visitor->Trace(start_);
  visitor->Trace(end_);
}
}  // namespace blink

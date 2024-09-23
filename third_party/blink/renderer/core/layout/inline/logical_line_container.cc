// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/logical_line_container.h"

#include "third_party/blink/renderer/core/layout/inline/logical_line_item.h"

namespace blink {

LogicalLineContainer::LogicalLineContainer()
    : base_line_(MakeGarbageCollected<LogicalLineItems>()) {}

void LogicalLineContainer::Trace(Visitor* visitor) const {
  visitor->Trace(base_line_);
  visitor->Trace(annotation_line_list_);
}

void LogicalLineContainer::Clear() {
  base_line_->clear();
  for (auto& line : annotation_line_list_) {
    line->clear();
  }
  annotation_line_list_.clear();
}

void LogicalLineContainer::Shrink() {
  base_line_->Shrink(0);
  for (auto& line : annotation_line_list_) {
    line->clear();
  }
  annotation_line_list_.Shrink(0);
}

void LogicalLineContainer::MoveInBlockDirection(LayoutUnit delta) {
  base_line_->MoveInBlockDirection(delta);
  for (auto& line : annotation_line_list_) {
    line->MoveInBlockDirection(delta);
  }
}

wtf_size_t LogicalLineContainer::EstimatedFragmentItemCount() const {
  wtf_size_t count = 1 + base_line_->size();
  for (const auto& line : annotation_line_list_) {
    count += 1 + line->size();
  }
  return count;
}

}  // namespace blink

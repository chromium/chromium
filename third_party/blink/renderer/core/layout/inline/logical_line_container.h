// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_LOGICAL_LINE_CONTAINER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_LOGICAL_LINE_CONTAINER_H_

#include "third_party/blink/renderer/platform/fonts/font_height.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class LogicalLineItems;

// Represents a line with optional ruby annotations.
// This is used to pass InlineLayoutAlgorithm::CreateLine() deliverables to
// FragmentItemsBuilder.
class LogicalLineContainer : public GarbageCollected<LogicalLineContainer> {
 public:
  LogicalLineContainer();
  void Trace(Visitor* visitor) const;

  LogicalLineItems& BaseLine() const { return *base_line_; }
  wtf_size_t AnnotationSize() const { return annotation_line_list_.size(); }
  FontHeight AnnotationMetricsAt(wtf_size_t index) const {
    return annotation_metrics_list_[index];
  }
  LogicalLineItems& AnnotationLineAt(wtf_size_t index) const {
    return *annotation_line_list_[index];
  }
  wtf_size_t EstimatedFragmentItemCount() const;

  void AddAnnotation(FontHeight metrics, LogicalLineItems& line_items) {
    annotation_metrics_list_.push_back(metrics);
    annotation_line_list_.push_back(line_items);
  }
  // Release all collection buffers.
  void Clear();
  // Set collection sizes to zero without releasing their buffers.
  void Shrink();
  void MoveInBlockDirection(LayoutUnit);

 private:
  Member<LogicalLineItems> base_line_;
  Vector<FontHeight> annotation_metrics_list_;
  HeapVector<Member<LogicalLineItems>> annotation_line_list_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_LOGICAL_LINE_CONTAINER_H_

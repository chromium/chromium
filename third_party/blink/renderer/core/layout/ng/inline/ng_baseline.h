// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBaseline_h
#define NGBaseline_h

#include "third_party/blink/renderer/platform/fonts/font_baseline.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class LayoutBox;
class NGLayoutInputNode;

enum class NGBaselineAlgorithmType {
  // Compute baselines for atomic inlines.
  kAtomicInline,
  // Compute baseline of first line box.
  kFirstLine
};

// Baselines are products of layout.
// To compute baseline, add requests to NGConstraintSpace and run Layout().
struct NGBaselineRequest {
  NGBaselineAlgorithmType algorithm_type;
  FontBaseline baseline_type;

  bool operator==(const NGBaselineRequest& other) const;
  bool operator!=(const NGBaselineRequest& other) const {
    return !(*this == other);
  }
};

// Represents a computed baseline position.
struct NGBaseline {
  NGBaselineRequest request;
  LayoutUnit offset;

  // @return if the node needs to propagate baseline requests/results.
  static bool ShouldPropagateBaselines(const NGLayoutInputNode);
  static bool ShouldPropagateBaselines(LayoutBox*);
};

}  // namespace blink

#endif  // NGBaseline_h

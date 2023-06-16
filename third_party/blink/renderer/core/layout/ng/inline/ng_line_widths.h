// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_WIDTHS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_WIDTHS_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_layout_opportunity.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class NGInlineBreakToken;
class NGInlineNode;

//
// This class computes the line width of each line for _simple_ nodes without
// actually laying them out.
//
class CORE_EXPORT NGLineWidths {
  STACK_ALLOCATED();

 public:
  NGLineWidths() = default;
  // Construct with the given `width`, without any exclusions.
  explicit NGLineWidths(LayoutUnit width) : default_width_(width) {}

  LayoutUnit Default() const { return default_width_; }
  bool HasExclusions() const { return num_excluded_lines_; }

  // Returns the width of a line. The `index` is 0-based line index.
  LayoutUnit operator[](wtf_size_t index) const;

  // Compute the line widths. Returns `false` if the `node` is not _simple_.
  bool Set(const NGInlineNode& node,
           base::span<const NGLayoutOpportunity> opportunities,
           const NGInlineBreakToken* break_token = nullptr);

 private:
  LayoutUnit default_width_;
  LayoutUnit excluded_width_;
  wtf_size_t num_excluded_lines_ = 0;
};

inline LayoutUnit NGLineWidths::operator[](wtf_size_t index) const {
  if (UNLIKELY(index < num_excluded_lines_)) {
    return excluded_width_;
  }
  return default_width_;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_WIDTHS_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_FIT_TEXT_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_FIT_TEXT_UTILS_H_

#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/style/fit_text.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"

namespace blink {

class LineInfo;
class PhysicalFragment;

bool ShouldApplyFitText(const InlineNode node);

// This function calculates and returns the overall scale factor for an IFC
// based on the given `node` and its PhysicalFragment.
//
// For each line within the IFC, it calculates a scale factor that would make
// the line fit exactly within the `available_width` and returns the minimum
// of these scale factors.
float MeasurePerBlockScale(const InlineNode node,
                           const PhysicalFragment& fragment,
                           LayoutUnit available_width);

// LineFitter class is responsible for computing a line scaling factor,
// and scaling a line.
class LineFitter {
  STACK_ALLOCATED();

 public:
  LineFitter(const InlineNode node, LineInfo* line_info);

  // Updates text scaling factor of InlineItemResults in `line_info`.
  // Returns true if LogicalLineBuilder needs to scale line-height.
  bool FitLine(float scale_factor);

  // Measures the scaling factor for the current line, and applies it.
  bool MeasureAndFitLine();

 private:
  // Measures the scaling factor for the current line.
  float MeasureScale();

  const InlineNode node_;
  LineInfo& line_info_;
  const InlineItemsData& items_data_;
  HarfBuzzShaper shaper_;
  ShapeResultSpacing<String> spacing_;
  const double device_pixel_ratio_;
  const LayoutUnit epsilon_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_FIT_TEXT_UTILS_H_

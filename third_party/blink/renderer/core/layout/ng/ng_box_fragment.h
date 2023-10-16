// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CORE_EXPORT NGBoxFragment final : public NGFragment {
 public:
  NGBoxFragment(WritingDirectionMode writing_direction,
                const NGPhysicalBoxFragment& physical_fragment)
      : NGFragment(writing_direction, physical_fragment) {}

  const NGPhysicalBoxFragment& PhysicalBoxFragment() const {
    return To<NGPhysicalBoxFragment>(physical_fragment_);
  }

  bool IsWritingModeEqual() const {
    return writing_direction_.GetWritingMode() ==
           physical_fragment_.Style().GetWritingMode();
  }

  static LayoutUnit SynthesizedBaseline(FontBaseline baseline_type,
                                        bool is_flipped_lines,
                                        LayoutUnit block_size) {
    if (baseline_type == kAlphabeticBaseline)
      return is_flipped_lines ? LayoutUnit() : block_size;

    return block_size / 2;
  }

  absl::optional<LayoutUnit> FirstBaseline() const {
    if (!IsWritingModeEqual())
      return absl::nullopt;

    auto baseline = PhysicalBoxFragment().FirstBaseline();
    if (baseline && physical_fragment_.IsScrollContainer())
      baseline = std::max(LayoutUnit(), std::min(*baseline, BlockSize()));

    return baseline;
  }

  LayoutUnit FirstBaselineOrSynthesize(FontBaseline baseline_type) const {
    if (auto first_baseline = FirstBaseline())
      return *first_baseline;

    return SynthesizedBaseline(
        baseline_type, writing_direction_.IsFlippedLines(), BlockSize());
  }

  absl::optional<LayoutUnit> LastBaseline() const {
    if (!IsWritingModeEqual())
      return absl::nullopt;

    auto baseline = PhysicalBoxFragment().LastBaseline();
    if (baseline && physical_fragment_.IsScrollContainer())
      baseline = std::max(LayoutUnit(), std::min(*baseline, BlockSize()));

    return baseline;
  }

  LayoutUnit LastBaselineOrSynthesize(FontBaseline baseline_type) const {
    if (auto last_baseline = LastBaseline())
      return *last_baseline;

    return SynthesizedBaseline(
        baseline_type, writing_direction_.IsFlippedLines(), BlockSize());
  }

  // Compute baseline metrics (ascent/descent) for this box.
  //
  // This will synthesize baseline metrics if no baseline is available. See
  // |Baseline()| for when this may occur.
  FontHeight BaselineMetrics(const LineBoxStrut& margins, FontBaseline) const;

  BoxStrut Borders() const {
    return PhysicalBoxFragment().Borders().ConvertToLogical(writing_direction_);
  }
  BoxStrut Padding() const {
    return PhysicalBoxFragment().Padding().ConvertToLogical(writing_direction_);
  }

  bool HasDescendantsForTablePart() const {
    return PhysicalBoxFragment().HasDescendantsForTablePart();
  }

  LayoutUnit BlockEndLayoutOverflow() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_H_

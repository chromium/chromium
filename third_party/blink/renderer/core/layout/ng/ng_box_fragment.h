// Copyright 2016 The Chromium Authors. All rights reserved.
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

  absl::optional<LayoutUnit> FirstBaseline() const {
    if (writing_direction_.GetWritingMode() !=
        physical_fragment_.Style().GetWritingMode())
      return absl::nullopt;

    return PhysicalBoxFragment().Baseline();
  }

  LayoutUnit FirstBaselineOrSynthesize(FontBaseline baseline_type) const {
    if (auto first_baseline = FirstBaseline())
      return *first_baseline;

    if (baseline_type == kAlphabeticBaseline)
      return BlockSize();

    return BlockSize() / 2;
  }

  // Returns the baseline for this fragment wrt. the parent writing mode. Will
  // return a null baseline if:
  //  - The fragment has no baseline.
  //  - The writing modes differ.
  absl::optional<LayoutUnit> Baseline() const {
    if (writing_direction_.GetWritingMode() !=
        physical_fragment_.Style().GetWritingMode())
      return absl::nullopt;

    if (auto last_baseline = PhysicalBoxFragment().LastBaseline())
      return last_baseline;

    return PhysicalBoxFragment().Baseline();
  }

  LayoutUnit BaselineOrSynthesize(FontBaseline baseline_type) const {
    if (auto baseline = Baseline())
      return *baseline;

    if (baseline_type == kAlphabeticBaseline)
      return BlockSize();

    return BlockSize() / 2;
  }

  // Compute baseline metrics (ascent/descent) for this box.
  //
  // This will synthesize baseline metrics if no baseline is available. See
  // |Baseline()| for when this may occur.
  FontHeight BaselineMetrics(const NGLineBoxStrut& margins, FontBaseline) const;

  NGBoxStrut Borders() const {
    return PhysicalBoxFragment().Borders().ConvertToLogical(writing_direction_);
  }
  NGBoxStrut Padding() const {
    return PhysicalBoxFragment().Padding().ConvertToLogical(writing_direction_);
  }

  bool HasDescendantsForTablePart() const {
    return PhysicalBoxFragment().HasDescendantsForTablePart();
  }

  bool HasBlockLayoutOverflow() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_H_

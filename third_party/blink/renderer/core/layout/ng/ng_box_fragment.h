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

  absl::optional<LayoutUnit> FirstBaseline() const {
    if (writing_direction_.GetWritingMode() !=
        physical_fragment_.Style().GetWritingMode())
      return absl::nullopt;

    return To<NGPhysicalBoxFragment>(physical_fragment_).Baseline();
  }

  LayoutUnit FirstBaselineOrSynthesize() const {
    if (auto first_baseline = FirstBaseline())
      return *first_baseline;

    // TODO(layout-dev): See |NGBoxFragment::BaselineOrSynthesize()|.
    if (writing_direction_.GetWritingMode() == WritingMode::kHorizontalTb)
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

    if (auto last_baseline =
            To<NGPhysicalBoxFragment>(physical_fragment_).LastBaseline())
      return last_baseline;

    return To<NGPhysicalBoxFragment>(physical_fragment_).Baseline();
  }

  LayoutUnit BaselineOrSynthesize() const {
    if (auto baseline = Baseline())
      return *baseline;

    // TODO(layout-dev): With a vertical writing-mode, and "text-orientation:
    // sideways" we should also synthesize using the block-end border edge. We
    // need to pass in the text-orientation (or just parent style) to do this.
    if (writing_direction_.GetWritingMode() == WritingMode::kHorizontalTb)
      return BlockSize();

    return BlockSize() / 2;
  }

  // Compute baseline metrics (ascent/descent) for this box.
  //
  // This will synthesize baseline metrics if no baseline is available. See
  // |Baseline()| for when this may occur.
  FontHeight BaselineMetrics(const NGLineBoxStrut& margins, FontBaseline) const;

  NGBoxStrut Borders() const {
    const NGPhysicalBoxFragment& physical_box_fragment =
        To<NGPhysicalBoxFragment>(physical_fragment_);
    return physical_box_fragment.Borders().ConvertToLogical(writing_direction_);
  }
  NGBoxStrut Padding() const {
    const NGPhysicalBoxFragment& physical_box_fragment =
        To<NGPhysicalBoxFragment>(physical_fragment_);
    return physical_box_fragment.Padding().ConvertToLogical(writing_direction_);
  }

  bool HasDescendantsForTablePart() const {
    const NGPhysicalBoxFragment& box_fragment =
        To<NGPhysicalBoxFragment>(physical_fragment_);
    return box_fragment.HasDescendantsForTablePart();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_H_

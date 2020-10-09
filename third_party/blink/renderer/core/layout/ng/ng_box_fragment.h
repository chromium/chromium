// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CORE_EXPORT NGBoxFragment final : public NGFragment {
 public:
  NGBoxFragment(WritingDirectionMode writing_direction,
                const NGPhysicalBoxFragment& physical_fragment)
      : NGFragment(writing_direction, physical_fragment) {}

  base::Optional<LayoutUnit> FirstBaseline() const {
    if (writing_direction_.GetWritingMode() !=
        physical_fragment_.Style().GetWritingMode())
      return base::nullopt;

    return To<NGPhysicalBoxFragment>(physical_fragment_).Baseline();
  }

  LayoutUnit FirstBaselineOrSynthesize() const {
    return FirstBaseline().value_or(BlockSize());
  }

  // Returns the baseline for this fragment wrt. the parent writing mode. Will
  // return a null baseline if:
  //  - The fragment has no baseline.
  //  - The writing modes differ.
  base::Optional<LayoutUnit> Baseline() const {
    if (writing_direction_.GetWritingMode() !=
        physical_fragment_.Style().GetWritingMode())
      return base::nullopt;

    if (auto last_baseline =
            To<NGPhysicalBoxFragment>(physical_fragment_).LastBaseline())
      return last_baseline;

    return To<NGPhysicalBoxFragment>(physical_fragment_).Baseline();
  }

  LayoutUnit BaselineOrSynthesize() const {
    return Baseline().value_or(BlockSize());
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
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_H_

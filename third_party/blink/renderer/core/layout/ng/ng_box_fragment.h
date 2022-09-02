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

    return PhysicalBoxFragment().FirstBaseline();
  }

  LayoutUnit FirstBaselineOrSynthesize(FontBaseline baseline_type) const {
    if (auto first_baseline = FirstBaseline())
      return *first_baseline;

    if (baseline_type == kAlphabeticBaseline)
      return writing_direction_.IsFlippedLines() ? LayoutUnit() : BlockSize();

    return BlockSize() / 2;
  }

  absl::optional<LayoutUnit> LastBaseline() const {
    if (writing_direction_.GetWritingMode() !=
        physical_fragment_.Style().GetWritingMode())
      return absl::nullopt;

    return PhysicalBoxFragment().LastBaseline();
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

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

class NGBaselineRequest;
struct NGLineHeightMetrics;

class CORE_EXPORT NGBoxFragment final : public NGFragment {
 public:
  NGBoxFragment(WritingMode writing_mode,
                TextDirection direction,
                const NGPhysicalBoxFragment& physical_fragment)
      : NGFragment(writing_mode, physical_fragment), direction_(direction) {}

  // Compute baseline metrics (ascent/descent) for this box.
  //
  // Baseline requests must be added to constraint space when this fragment was
  // laid out.
  //
  // The "WithoutSynthesize" version returns an empty metrics if this box does
  // not have any baselines, while the other version synthesize the baseline
  // from the box.
  NGLineHeightMetrics BaselineMetricsWithoutSynthesize(
      const NGBaselineRequest&) const;
  NGLineHeightMetrics BaselineMetrics(const NGBaselineRequest&,
                                      const NGConstraintSpace&) const;

  NGBoxStrut Borders() const {
    const NGPhysicalBoxFragment& physical_box_fragment =
        To<NGPhysicalBoxFragment>(physical_fragment_);
    return physical_box_fragment.Borders().ConvertToLogical(GetWritingMode(),
                                                            direction_);
  }
  NGBoxStrut Padding() const {
    const NGPhysicalBoxFragment& physical_box_fragment =
        To<NGPhysicalBoxFragment>(physical_fragment_);
    return physical_box_fragment.Padding().ConvertToLogical(GetWritingMode(),
                                                            direction_);
  }

  NGBorderEdges BorderEdges() const {
    const NGPhysicalBoxFragment& physical_box_fragment =
        To<NGPhysicalBoxFragment>(physical_fragment_);
    return NGBorderEdges::FromPhysical(physical_box_fragment.BorderEdges(),
                                       GetWritingMode());
  }

 protected:
  TextDirection direction_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_H_

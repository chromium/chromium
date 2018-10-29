// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBoxFragment_h
#define NGBoxFragment_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

struct NGBaselineRequest;
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
    const auto& physical_fragment = ToNGPhysicalBoxFragment(physical_fragment_);
    return physical_fragment.Borders().ConvertToLogical(GetWritingMode(),
                                                        direction_);
  }
  NGBoxStrut Padding() const {
    const auto& physical_fragment = ToNGPhysicalBoxFragment(physical_fragment_);
    return physical_fragment.Padding().ConvertToLogical(GetWritingMode(),
                                                        direction_);
  }

 protected:
  TextDirection direction_;
};

DEFINE_TYPE_CASTS(NGBoxFragment,
                  NGFragment,
                  fragment,
                  fragment->Type() == NGPhysicalFragment::kFragmentBox,
                  fragment.Type() == NGPhysicalFragment::kFragmentBox);

}  // namespace blink

#endif  // NGBoxFragment_h

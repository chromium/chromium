// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_frame_set_layout_algorithm.h"

#include "third_party/blink/renderer/core/html/html_frame_set_element.h"
#include "third_party/blink/renderer/core/layout/ng/frame_set_layout_data.h"

namespace blink {

NGFrameSetLayoutAlgorithm::NGFrameSetLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm<NGBlockNode, NGBoxFragmentBuilder, NGBlockBreakToken>(
          params) {}

const NGLayoutResult* NGFrameSetLayoutAlgorithm::Layout() {
  auto& frame_set = *To<HTMLFrameSetElement>(Node().GetDOMNode());
  auto layout_data = std::make_unique<FrameSetLayoutData>();
  layout_data->border_thickness = frame_set.Border(Style());
  layout_data->has_border_color = frame_set.HasBorderColor();
  layout_data->row_allow_border = frame_set.AllowBorderRows();
  layout_data->col_allow_border = frame_set.AllowBorderColumns();

  // TODO(crbug.com/1346221): Fill layout_data->col_sizes and
  // layout_data->row_sizes.

  // TODO(crbug.com/1346221): Layout children.

  container_builder_.TransferFrameSetLayoutData(std::move(layout_data));
  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGFrameSetLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  return MinMaxSizesResult(MinMaxSizes(), false);
}

}  // namespace blink

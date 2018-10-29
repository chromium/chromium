// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"

namespace blink {

NGLayoutResult::NGLayoutResult(
    scoped_refptr<const NGPhysicalFragment> physical_fragment,
    NGBoxFragmentBuilder* builder)
    : unpositioned_list_marker_(builder->unpositioned_list_marker_),
      exclusion_space_(std::move(builder->exclusion_space_)),
      bfc_line_offset_(builder->bfc_line_offset_),
      bfc_block_offset_(builder->bfc_block_offset_),
      end_margin_strut_(builder->end_margin_strut_),
      intrinsic_block_size_(builder->intrinsic_block_size_),
      minimal_space_shortage_(builder->minimal_space_shortage_),
      initial_break_before_(builder->initial_break_before_),
      final_break_after_(builder->previous_break_after_),
      has_forced_break_(builder->has_forced_break_),
      is_pushed_by_floats_(builder->is_pushed_by_floats_),
      adjoining_floats_(builder->adjoining_floats_),
      status_(kSuccess) {
  DCHECK(physical_fragment) << "Use the other constructor for aborting layout";
  root_fragment_.fragment_ = std::move(physical_fragment);
  oof_positioned_descendants_ = std::move(builder->oof_positioned_descendants_);
}

NGLayoutResult::NGLayoutResult(NGLayoutResultStatus status,
                               NGBoxFragmentBuilder* builder)
    : bfc_line_offset_(builder->bfc_line_offset_),
      bfc_block_offset_(builder->bfc_block_offset_),
      end_margin_strut_(builder->end_margin_strut_),
      initial_break_before_(EBreakBetween::kAuto),
      final_break_after_(EBreakBetween::kAuto),
      has_forced_break_(false),
      is_pushed_by_floats_(false),
      adjoining_floats_(kFloatTypeNone),
      status_(status) {
  DCHECK_NE(status, kSuccess)
      << "Use the other constructor for successful layout";
}

NGLayoutResult::NGLayoutResult(
    scoped_refptr<const NGPhysicalFragment> physical_fragment,
    NGLineBoxFragmentBuilder* builder)
    : unpositioned_list_marker_(builder->unpositioned_list_marker_),
      exclusion_space_(std::move(builder->exclusion_space_)),
      bfc_line_offset_(builder->bfc_line_offset_),
      bfc_block_offset_(builder->bfc_block_offset_),
      end_margin_strut_(builder->end_margin_strut_),
      minimal_space_shortage_(LayoutUnit::Max()),
      initial_break_before_(EBreakBetween::kAuto),
      final_break_after_(EBreakBetween::kAuto),
      has_forced_break_(false),
      is_pushed_by_floats_(builder->is_pushed_by_floats_),
      adjoining_floats_(builder->adjoining_floats_),
      status_(kSuccess) {
  root_fragment_.fragment_ = std::move(physical_fragment);
  oof_positioned_descendants_ = std::move(builder->oof_positioned_descendants_);
  positioned_floats_ = std::move(builder->positioned_floats_);
}

// We can't use =default here because RefCounted can't be copied.
NGLayoutResult::NGLayoutResult(const NGLayoutResult& other)
    : root_fragment_(other.root_fragment_),
      oof_positioned_descendants_(other.oof_positioned_descendants_),
      positioned_floats_(other.positioned_floats_),
      unpositioned_list_marker_(other.unpositioned_list_marker_),
      exclusion_space_(other.exclusion_space_),
      bfc_line_offset_(other.bfc_line_offset_),
      bfc_block_offset_(other.bfc_block_offset_),
      end_margin_strut_(other.end_margin_strut_),
      intrinsic_block_size_(other.intrinsic_block_size_),
      minimal_space_shortage_(other.minimal_space_shortage_),
      initial_break_before_(other.initial_break_before_),
      final_break_after_(other.final_break_after_),
      has_forced_break_(other.has_forced_break_),
      is_pushed_by_floats_(other.is_pushed_by_floats_),
      adjoining_floats_(other.adjoining_floats_),
      status_(other.status_) {}

// Define the destructor here, so that we can forward-declare more in the
// header.
NGLayoutResult::~NGLayoutResult() = default;

}  // namespace blink

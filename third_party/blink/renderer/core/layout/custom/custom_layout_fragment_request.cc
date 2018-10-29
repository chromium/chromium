// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/custom_layout_fragment_request.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_child.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_constraints_options.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_fragment.h"
#include "third_party/blink/renderer/core/layout/custom/layout_custom.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {

CustomLayoutFragmentRequest::CustomLayoutFragmentRequest(
    CustomLayoutChild* child,
    const CustomLayoutConstraintsOptions& options,
    scoped_refptr<SerializedScriptValue> constraint_data)
    : child_(child),
      options_(options),
      constraint_data_(std::move(constraint_data)) {}

CustomLayoutFragment* CustomLayoutFragmentRequest::PerformLayout(
    v8::Isolate* isolate) {
  // Abort if the child we are trying to perform layout upon doesn't exist.
  if (!IsValid())
    return nullptr;

  LayoutBox* box = child_->GetLayoutBox();
  const ComputedStyle& style = box->StyleRef();

  DCHECK(box->Parent());
  DCHECK(box->Parent()->IsLayoutCustom());
  DCHECK(box->Parent() == box->ContainingBlock());

  LayoutObject* parent = box->Parent();

  bool is_parallel_writing_mode = IsParallelWritingMode(
      parent->StyleRef().GetWritingMode(), style.GetWritingMode());

  if (options_.hasFixedInlineSize()) {
    if (is_parallel_writing_mode) {
      box->SetOverrideLogicalWidth(
          LayoutUnit::FromDoubleRound(options_.fixedInlineSize()));
    } else {
      box->SetOverrideLogicalHeight(
          LayoutUnit::FromDoubleRound(options_.fixedInlineSize()));
    }
  } else {
    box->SetOverrideContainingBlockContentLogicalWidth(
        options_.hasAvailableInlineSize() &&
                options_.availableInlineSize() >= 0.0
            ? LayoutUnit::FromDoubleRound(options_.availableInlineSize())
            : LayoutUnit());
  }

  if (options_.hasFixedBlockSize()) {
    if (is_parallel_writing_mode) {
      box->SetOverrideLogicalHeight(
          LayoutUnit::FromDoubleRound(options_.fixedBlockSize()));
    } else {
      box->SetOverrideLogicalWidth(
          LayoutUnit::FromDoubleRound(options_.fixedBlockSize()));
    }
  } else {
    box->SetOverrideContainingBlockContentLogicalHeight(
        options_.hasAvailableBlockSize() && options_.availableBlockSize() >= 0.0
            ? LayoutUnit::FromDoubleRound(options_.availableBlockSize())
            : LayoutUnit());
  }

  // We default the percentage resolution block-size to indefinite if nothing
  // is specified.
  LayoutUnit percentage_resolution_logical_height(-1);

  if (is_parallel_writing_mode) {
    if (options_.hasPercentageBlockSize() &&
        options_.percentageBlockSize() >= 0.0) {
      percentage_resolution_logical_height =
          LayoutUnit::FromDoubleRound(options_.percentageBlockSize());
    } else if (options_.hasAvailableBlockSize() &&
               options_.availableBlockSize() >= 0.0) {
      percentage_resolution_logical_height =
          LayoutUnit::FromDoubleRound(options_.availableBlockSize());
    }
  } else {
    if (options_.hasPercentageInlineSize() &&
        options_.percentageInlineSize() >= 0.0) {
      percentage_resolution_logical_height =
          LayoutUnit::FromDoubleRound(options_.percentageInlineSize());
    } else if (options_.hasAvailableInlineSize() &&
               options_.availableInlineSize() >= 0.0) {
      percentage_resolution_logical_height =
          LayoutUnit::FromDoubleRound(options_.availableInlineSize());
    }
  }

  box->SetOverrideContainingBlockPercentageResolutionLogicalHeight(
      percentage_resolution_logical_height);

  if (box->IsLayoutCustom())
    ToLayoutCustom(box)->SetConstraintData(constraint_data_);

  box->ForceLayout();

  box->ClearOverrideContainingBlockContentSize();
  box->ClearOverrideContainingBlockPercentageResolutionLogicalHeight();
  box->ClearOverrideSize();

  if (box->IsLayoutCustom())
    ToLayoutCustom(box)->ClearConstraintData();

  LayoutUnit fragment_inline_size =
      is_parallel_writing_mode ? box->LogicalWidth() : box->LogicalHeight();
  LayoutUnit fragment_block_size =
      is_parallel_writing_mode ? box->LogicalHeight() : box->LogicalWidth();

  return new CustomLayoutFragment(this, fragment_inline_size,
                                  fragment_block_size, isolate);
}

LayoutBox* CustomLayoutFragmentRequest::GetLayoutBox() const {
  return child_->GetLayoutBox();
}

bool CustomLayoutFragmentRequest::IsValid() const {
  return child_->IsValid();
}

void CustomLayoutFragmentRequest::Trace(blink::Visitor* visitor) {
  visitor->Trace(child_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

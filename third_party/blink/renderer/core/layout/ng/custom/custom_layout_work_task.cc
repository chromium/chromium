// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/custom/custom_layout_work_task.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/layout/ng/custom/custom_layout_child.h"
#include "third_party/blink/renderer/core/layout/ng/custom/custom_layout_constraints_options.h"
#include "third_party/blink/renderer/core/layout/ng/custom/custom_layout_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"

namespace blink {

CustomLayoutWorkTask::CustomLayoutWorkTask(
    CustomLayoutChild* child,
    CustomLayoutToken* token,
    ScriptPromiseResolver* resolver,
    const CustomLayoutConstraintsOptions* options,
    scoped_refptr<SerializedScriptValue> constraint_data)
    : child_(child),
      token_(token),
      resolver_(resolver),
      options_(options),
      constraint_data_(std::move(constraint_data)) {}

CustomLayoutWorkTask::~CustomLayoutWorkTask() = default;

void CustomLayoutWorkTask::Run(const NGConstraintSpace& parent_space,
                               const ComputedStyle& parent_style) {
  DCHECK(token_->IsValid());
  NGLayoutInputNode node = child_->GetLayoutNode();
  NGConstraintSpaceBuilder builder(parent_space, node.Style().GetWritingMode(),
                                   /* is_new_fc */ true);
  SetOrthogonalFallbackInlineSizeIfNeeded(parent_style, node, &builder);

  bool is_fixed_inline_size = false;
  bool is_fixed_block_size = false;
  LogicalSize available_size;
  LogicalSize percentage_size;

  if (options_->hasFixedInlineSize()) {
    is_fixed_inline_size = true;
    available_size.inline_size =
        LayoutUnit::FromDoubleRound(options_->fixedInlineSize());
  } else {
    available_size.inline_size =
        options_->hasAvailableInlineSize() &&
                options_->availableInlineSize() >= 0.0
            ? LayoutUnit::FromDoubleRound(options_->availableInlineSize())
            : LayoutUnit();
  }

  if (options_->hasFixedBlockSize()) {
    is_fixed_block_size = true;
    available_size.block_size =
        LayoutUnit::FromDoubleRound(options_->fixedBlockSize());
  } else {
    available_size.block_size =
        options_->hasAvailableBlockSize() &&
                options_->availableBlockSize() >= 0.0
            ? LayoutUnit::FromDoubleRound(options_->availableBlockSize())
            : LayoutUnit();
  }

  if (options_->hasPercentageInlineSize() &&
      options_->percentageInlineSize() >= 0.0) {
    percentage_size.inline_size =
        LayoutUnit::FromDoubleRound(options_->percentageInlineSize());
  } else if (options_->hasAvailableInlineSize() &&
             options_->availableInlineSize() >= 0.0) {
    percentage_size.inline_size =
        LayoutUnit::FromDoubleRound(options_->availableInlineSize());
  }

  if (options_->hasPercentageBlockSize() &&
      options_->percentageBlockSize() >= 0.0) {
    percentage_size.block_size =
        LayoutUnit::FromDoubleRound(options_->percentageBlockSize());
  } else if (options_->hasAvailableBlockSize() &&
             options_->availableBlockSize() >= 0.0) {
    percentage_size.block_size =
        LayoutUnit::FromDoubleRound(options_->availableBlockSize());
  } else {
    percentage_size.block_size = kIndefiniteSize;
  }

  builder.SetTextDirection(node.Style().Direction());
  builder.SetAvailableSize(available_size);
  builder.SetPercentageResolutionSize(percentage_size);
  builder.SetReplacedPercentageResolutionSize(percentage_size);
  builder.SetIsShrinkToFit(node.Style().LogicalWidth().IsAuto());
  builder.SetIsFixedInlineSize(is_fixed_inline_size);
  builder.SetIsFixedBlockSize(is_fixed_block_size);
  if (node.IsLayoutNGCustom())
    builder.SetCustomLayoutData(std::move(constraint_data_));
  auto space = builder.ToConstraintSpace();
  auto result = To<NGBlockNode>(node).Layout(space, nullptr /* break_token */);

  LogicalSize size = result->PhysicalFragment().Size().ConvertToLogical(
      parent_space.GetWritingMode());

  resolver_->Resolve(MakeGarbageCollected<CustomLayoutFragment>(
      child_, token_, std::move(result), size,
      resolver_->GetScriptState()->GetIsolate()));
}

}  // namespace blink

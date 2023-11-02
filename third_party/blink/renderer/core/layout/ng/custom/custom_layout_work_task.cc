// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/custom/custom_layout_work_task.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/layout/ng/custom/custom_intrinsic_sizes.h"
#include "third_party/blink/renderer/core/layout/ng/custom/custom_layout_child.h"
#include "third_party/blink/renderer/core/layout/ng/custom/custom_layout_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"

namespace blink {

CustomLayoutWorkTask::CustomLayoutWorkTask(CustomLayoutChild* child,
                                           CustomLayoutToken* token,
                                           ScriptPromiseResolver* resolver,
                                           const TaskType type)
    : CustomLayoutWorkTask(child, token, resolver, nullptr, nullptr, type) {}

CustomLayoutWorkTask::CustomLayoutWorkTask(
    CustomLayoutChild* child,
    CustomLayoutToken* token,
    ScriptPromiseResolver* resolver,
    const CustomLayoutConstraintsOptions* options,
    scoped_refptr<SerializedScriptValue> constraint_data,
    const TaskType type)
    : child_(child),
      token_(token),
      resolver_(resolver),
      options_(options),
      constraint_data_(std::move(constraint_data)),
      type_(type) {}

CustomLayoutWorkTask::~CustomLayoutWorkTask() = default;

void CustomLayoutWorkTask::Trace(Visitor* visitor) const {
  visitor->Trace(child_);
  visitor->Trace(token_);
  visitor->Trace(resolver_);
  visitor->Trace(options_);
}

void CustomLayoutWorkTask::Run(const NGConstraintSpace& parent_space,
                               const ComputedStyle& parent_style,
                               const LayoutUnit child_available_block_size,
                               bool* child_depends_on_block_constraints) {
  DCHECK(token_->IsValid());
  NGLayoutInputNode child = child_->GetLayoutNode();

  if (type_ == CustomLayoutWorkTask::TaskType::kIntrinsicSizes) {
    RunIntrinsicSizesTask(parent_space, parent_style,
                          child_available_block_size, child,
                          child_depends_on_block_constraints);
  } else {
    DCHECK_EQ(type_, CustomLayoutWorkTask::TaskType::kLayoutFragment);
    RunLayoutFragmentTask(parent_space, parent_style, child);
  }
}

void CustomLayoutWorkTask::RunLayoutFragmentTask(
    const NGConstraintSpace& parent_space,
    const ComputedStyle& parent_style,
    NGLayoutInputNode child) {
  DCHECK_EQ(type_, CustomLayoutWorkTask::TaskType::kLayoutFragment);
  DCHECK(options_ && resolver_);

  NGConstraintSpaceBuilder builder(parent_space,
                                   child.Style().GetWritingDirection(),
                                   /* is_new_fc */ true);
  SetOrthogonalFallbackInlineSizeIfNeeded(parent_style, child, &builder);

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

  builder.SetAvailableSize(available_size);
  builder.SetPercentageResolutionSize(percentage_size);
  builder.SetReplacedPercentageResolutionSize(percentage_size);
  builder.SetIsFixedInlineSize(is_fixed_inline_size);
  builder.SetIsFixedBlockSize(is_fixed_block_size);
  if (child.IsLayoutNGCustom())
    builder.SetCustomLayoutData(std::move(constraint_data_));
  auto space = builder.ToConstraintSpace();
  auto* result =
      To<NGBlockNode>(child).Layout(space, nullptr /* break_token */);

  NGBoxFragment fragment(parent_space.GetWritingDirection(),
                         To<NGPhysicalBoxFragment>(result->PhysicalFragment()));

  resolver_->Resolve(MakeGarbageCollected<CustomLayoutFragment>(
      child_, token_, std::move(result), fragment.Size(),
      fragment.FirstBaseline(), resolver_->GetScriptState()->GetIsolate()));
}

void CustomLayoutWorkTask::RunIntrinsicSizesTask(
    const NGConstraintSpace& parent_space,
    const ComputedStyle& parent_style,
    const LayoutUnit child_available_block_size,
    NGLayoutInputNode child,
    bool* child_depends_on_block_constraints) {
  DCHECK_EQ(type_, CustomLayoutWorkTask::TaskType::kIntrinsicSizes);
  DCHECK(resolver_);

  NGMinMaxConstraintSpaceBuilder builder(parent_space, parent_style, child,
                                         /* is_new_fc */ true);
  builder.SetAvailableBlockSize(child_available_block_size);
  const auto space = builder.ToConstraintSpace();

  MinMaxSizesResult result = ComputeMinAndMaxContentContribution(
      parent_style, To<NGBlockNode>(child), space);
  resolver_->Resolve(MakeGarbageCollected<CustomIntrinsicSizes>(
      child_, token_, result.sizes.min_size, result.sizes.max_size));

  if (child_depends_on_block_constraints)
    *child_depends_on_block_constraints |= result.depends_on_block_constraints;
}

}  // namespace blink

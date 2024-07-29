// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/custom_layout_algorithm.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_fragment_result_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_intrinsic_sizes_result_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_fragment.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_scope.h"
#include "third_party/blink/renderer/core/layout/custom/layout_worklet.h"
#include "third_party/blink/renderer/core/layout/custom/layout_worklet_global_scope_proxy.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"

namespace blink {

CustomLayoutAlgorithm::CustomLayoutAlgorithm(
    const LayoutAlgorithmParams& params)
    : LayoutAlgorithm(params), params_(params) {
  DCHECK(params.space.IsNewFormattingContext());
}

MinMaxSizesResult CustomLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput& input) {
  if (!Node().IsCustomLayoutLoaded())
    return FallbackMinMaxSizes(input);

  ScriptForbiddenScope::AllowUserAgentScript allow_script;
  CustomLayoutScope scope;

  const AtomicString& name = Style().DisplayLayoutCustomName();
  const Document& document = Node().GetDocument();
  LayoutWorklet* worklet = LayoutWorklet::From(*document.domWindow());
  CSSLayoutDefinition* definition = worklet->Proxy()->FindDefinition(name);

  // TODO(ikilpatrick): Cache the instance of the layout class.
  CSSLayoutDefinition::Instance* instance = definition->CreateInstance();

  if (!instance) {
    // TODO(ikilpatrick): Report this error to the developer.
    return FallbackMinMaxSizes(input);
  }

  bool depends_on_block_constraints = false;
  IntrinsicSizesResultOptions* intrinsic_sizes_result_options = nullptr;
  LogicalSize border_box_size{
      container_builder_.InlineSize(),
      ComputeBlockSizeForFragment(
          GetConstraintSpace(), Node(), BorderPadding(),
          CalculateDefaultBlockSize(GetConstraintSpace(), Node(),
                                    GetBreakToken(), BorderScrollbarPadding()),
          container_builder_.InlineSize())};
  if (!instance->IntrinsicSizes(
          GetConstraintSpace(), document, Node(), border_box_size,
          BorderScrollbarPadding(), ChildAvailableSize().block_size, &scope,
          &intrinsic_sizes_result_options, &depends_on_block_constraints)) {
    // TODO(ikilpatrick): Report this error to the developer.
    return FallbackMinMaxSizes(input);
  }

  MinMaxSizes sizes;
  sizes.max_size = LayoutUnit::FromDoubleRound(
      intrinsic_sizes_result_options->maxContentSize());
  sizes.min_size = std::min(
      sizes.max_size, LayoutUnit::FromDoubleRound(
                          intrinsic_sizes_result_options->minContentSize()));

  sizes.min_size.ClampNegativeToZero();
  sizes.max_size.ClampNegativeToZero();

  return MinMaxSizesResult(sizes, depends_on_block_constraints);
}

const LayoutResult* CustomLayoutAlgorithm::Layout() {
  DCHECK(!IsBreakInside(GetBreakToken()));

  if (!Node().IsCustomLayoutLoaded())
    return FallbackLayout();

  ScriptForbiddenScope::AllowUserAgentScript allow_script;
  CustomLayoutScope scope;

  // TODO(ikilpatrick): Scale inputs/outputs by effective-zoom.
  const float effective_zoom = Style().EffectiveZoom();
  const AtomicString& name = Style().DisplayLayoutCustomName();
  const Document& document = Node().GetDocument();
  LayoutWorklet* worklet = LayoutWorklet::From(*document.domWindow());
  CSSLayoutDefinition* definition = worklet->Proxy()->FindDefinition(name);

  // TODO(ikilpatrick): Cache the instance of the layout class.
  CSSLayoutDefinition::Instance* instance = definition->CreateInstance();

  if (!instance) {
    // TODO(ikilpatrick): Report this error to the developer.
    return FallbackLayout();
  }

  FragmentResultOptions* fragment_result_options = nullptr;
  scoped_refptr<SerializedScriptValue> fragment_result_data;
  LogicalSize border_box_size{
      container_builder_.InlineSize(),
      ComputeBlockSizeForFragment(
          GetConstraintSpace(), Node(), BorderPadding(),
          CalculateDefaultBlockSize(GetConstraintSpace(), Node(),
                                    GetBreakToken(), BorderScrollbarPadding()),
          container_builder_.InlineSize())};
  if (!instance->Layout(GetConstraintSpace(), document, Node(), border_box_size,
                        BorderScrollbarPadding(), &scope,
                        fragment_result_options, &fragment_result_data)) {
    // TODO(ikilpatrick): Report this error to the developer.
    return FallbackLayout();
  }

  const HeapVector<Member<CustomLayoutFragment>>& child_fragments =
      fragment_result_options->childFragments();

  LayoutInputNode child = Node().FirstChild();
  for (auto fragment : child_fragments) {
    if (!fragment->IsValid()) {
      // TODO(ikilpatrick): Report this error to the developer.
      return FallbackLayout();
    }

    AddAnyOutOfFlowPositionedChildren(&child);

    // TODO(ikilpatrick): Implement paint order. This should abort this loop,
    // and go into a "slow" loop which allows developers to control the paint
    // order of the children.
    if (!child || child != fragment->GetLayoutNode()) {
      // TODO(ikilpatrick): Report this error to the developer.
      return FallbackLayout();
    }

    // TODO(ikilpatrick): At this stage we may need to perform a re-layout on
    // the given child. (The LayoutFragment may have been produced from a
    // different LayoutFragmentRequest).

    LayoutUnit inline_offset =
        LayoutUnit::FromDoubleRound(fragment->inlineOffset());
    LayoutUnit block_offset =
        LayoutUnit::FromDoubleRound(fragment->blockOffset());
    container_builder_.AddResult(fragment->GetLayoutResult(),
                                 {inline_offset, block_offset});

    child = child.NextSibling();
  }

  // We've exhausted the inflow fragments list, but we may still have
  // OOF-positioned children to add to the fragment builder.
  AddAnyOutOfFlowPositionedChildren(&child);

  // Currently we only support exactly one LayoutFragment per LayoutChild.
  if (child) {
    // TODO(ikilpatrick): Report this error to the developer.
    return FallbackLayout();
  }

  // Compute the final block-size.
  LayoutUnit auto_block_size = std::max(
      BorderScrollbarPadding().BlockSum(),
      LayoutUnit::FromDoubleRound(fragment_result_options->autoBlockSize()));
  LayoutUnit block_size = ComputeBlockSizeForFragment(
      GetConstraintSpace(), Node(), BorderPadding(), auto_block_size,
      container_builder_.InitialBorderBoxSize().inline_size);

  // TODO(ikilpatrick): Allow setting both the first/last baseline instead of a
  // general baseline.
  if (fragment_result_options->hasBaseline()) {
    LayoutUnit baseline = LayoutUnit::FromDoubleRound(
        effective_zoom * fragment_result_options->baseline());
    container_builder_.SetBaselines(baseline);
  }

  container_builder_.SetCustomLayoutData(std::move(fragment_result_data));
  container_builder_.SetIntrinsicBlockSize(auto_block_size);
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  container_builder_.HandleOofsAndSpecialDescendants();

  return container_builder_.ToBoxFragment();
}

// Seeks forward through any children starting at |child|. If any children are
// OOF-positioned, adds them as a candidate, then proceeds to the next child.
//
// |child| will end up being the next inflow child, or empty.
void CustomLayoutAlgorithm::AddAnyOutOfFlowPositionedChildren(
    LayoutInputNode* child) {
  DCHECK(child);
  while (*child && child->IsOutOfFlowPositioned()) {
    container_builder_.AddOutOfFlowChildCandidate(
        To<BlockNode>(*child), BorderScrollbarPadding().StartOffset());
    *child = child->NextSibling();
  }
}

MinMaxSizesResult CustomLayoutAlgorithm::FallbackMinMaxSizes(
    const MinMaxSizesFloatInput& input) const {
  return BlockLayoutAlgorithm(params_).ComputeMinMaxSizes(input);
}

const LayoutResult* CustomLayoutAlgorithm::FallbackLayout() {
  return BlockLayoutAlgorithm(params_).Layout();
}

}  // namespace blink

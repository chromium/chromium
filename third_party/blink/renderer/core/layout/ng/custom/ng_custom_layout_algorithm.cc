// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/custom/ng_custom_layout_algorithm.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/ng/custom/custom_layout_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/custom/custom_layout_scope.h"
#include "third_party/blink/renderer/core/layout/ng/custom/fragment_result_options.h"
#include "third_party/blink/renderer/core/layout/ng/custom/layout_worklet.h"
#include "third_party/blink/renderer/core/layout/ng/custom/layout_worklet_global_scope_proxy.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"

namespace blink {

NGCustomLayoutAlgorithm::NGCustomLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params),
      params_(params),
      border_padding_(params.fragment_geometry.border +
                      params.fragment_geometry.padding),
      border_scrollbar_padding_(border_padding_ +
                                params.fragment_geometry.scrollbar) {
  DCHECK(params.space.IsNewFormattingContext());
  container_builder_.SetIsNewFormattingContext(
      params.space.IsNewFormattingContext());
  container_builder_.SetInitialFragmentGeometry(params.fragment_geometry);
}

base::Optional<MinMaxSize> NGCustomLayoutAlgorithm::ComputeMinMaxSize(
    const MinMaxSizeInput& input) const {
  // TODO(ikilpatrick): Invoke the web-developer defined "intrinsicSizes"
  // method.
  return FallbackMinMaxSize(input);
}

scoped_refptr<const NGLayoutResult> NGCustomLayoutAlgorithm::Layout() {
  DCHECK(!BreakToken());

  if (!Node().IsCustomLayoutLoaded())
    return FallbackLayout();

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
    return FallbackLayout();
  }

  FragmentResultOptions* fragment_result_options =
      FragmentResultOptions::Create();
  scoped_refptr<SerializedScriptValue> fragment_result_data;
  if (!instance->Layout(ConstraintSpace(), document, Node(),
                        container_builder_.InitialBorderBoxSize(),
                        border_scrollbar_padding_, &scope,
                        fragment_result_options, &fragment_result_data)) {
    // TODO(ikilpatrick): Report this error to the developer.
    return FallbackLayout();
  }

  const HeapVector<Member<CustomLayoutFragment>>& child_fragments =
      fragment_result_options->childFragments();

  NGLayoutInputNode child = Node().FirstChild();
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
      border_padding_.BlockSum(),
      LayoutUnit::FromDoubleRound(fragment_result_options->autoBlockSize()));
  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), border_padding_, auto_block_size);

  container_builder_.SetCustomLayoutData(std::move(fragment_result_data));
  container_builder_.SetIntrinsicBlockSize(auto_block_size);
  container_builder_.SetBlockSize(block_size);

  NGOutOfFlowLayoutPart(
      Node(), ConstraintSpace(),
      container_builder_.Borders() + container_builder_.Scrollbar(),
      &container_builder_)
      .Run();

  return container_builder_.ToBoxFragment();
}

// Seeks forward through any children starting at |child|. If any children are
// OOF-positioned, adds them as a candidate, then proceeds to the next child.
//
// |child| will end up being the next inflow child, or empty.
void NGCustomLayoutAlgorithm::AddAnyOutOfFlowPositionedChildren(
    NGLayoutInputNode* child) {
  DCHECK(child);
  while (*child && child->IsOutOfFlowPositioned()) {
    container_builder_.AddOutOfFlowChildCandidate(
        To<NGBlockNode>(*child), {border_scrollbar_padding_.inline_start,
                                  border_scrollbar_padding_.block_start});
    *child = child->NextSibling();
  }
}

base::Optional<MinMaxSize> NGCustomLayoutAlgorithm::FallbackMinMaxSize(
    const MinMaxSizeInput& input) const {
  NGBlockLayoutAlgorithm algorithm(params_);
  return algorithm.ComputeMinMaxSize(input);
}

scoped_refptr<const NGLayoutResult> NGCustomLayoutAlgorithm::FallbackLayout() {
  NGBlockLayoutAlgorithm algorithm(params_);
  return algorithm.Layout();
}

}  // namespace blink

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/fragment_repeater.h"

#include "third_party/blink/renderer/core/layout/inline/fragment_items.h"
#include "third_party/blink/renderer/core/layout/inline/physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_fragment_link.h"

namespace blink {

namespace {

// Remove all cloned results, but keep the first original one(s).
void RemoveClonedResults(LayoutBox& layout_box) {
  for (wtf_size_t idx = 0; idx < layout_box.PhysicalFragmentCount(); idx++) {
    const BlockBreakToken* break_token =
        layout_box.GetPhysicalFragment(idx)->GetBreakToken();
    if (!break_token || break_token->IsRepeated()) {
      layout_box.ShrinkLayoutResults(idx + 1);
      return;
    }
  }
  NOTREACHED_IN_MIGRATION();
}

void UpdateBreakTokens(LayoutBox& layout_box) {
  BlockNode node(&layout_box);
  wtf_size_t sequence_number = 0;
  wtf_size_t fragment_count = layout_box.PhysicalFragmentCount();

  // If this box is a fragmentation context root, we also need to update the
  // break tokens of the fragmentainers, since they aren't associated with a
  // layout object on their own.
  const PhysicalBoxFragment* last_fragmentainer = nullptr;
  wtf_size_t fragmentainer_sequence_number = 0;

  for (wtf_size_t idx = 0; idx < fragment_count; idx++, sequence_number++) {
    const auto& fragment = *layout_box.GetPhysicalFragment(idx);
    const BlockBreakToken* break_token = fragment.GetBreakToken();
    if (break_token && break_token->IsRepeated())
      break_token = nullptr;
    if (break_token) {
      // It may already have a break token, if there's another fragmentation
      // context inside the repeated root. But we need to update the sequence
      // number, unless we're inside the very first fragment generated for the
      // repeated root.
      if (break_token->SequenceNumber() != sequence_number) {
        break_token = BlockBreakToken::CreateForBreakInRepeatedFragment(
            node, sequence_number, break_token->ConsumedBlockSize(),
            break_token->IsAtBlockEnd());
      }
    } else if (idx + 1 < fragment_count) {
      // Unless it's the very last fragment, it needs a break token.
      break_token = BlockBreakToken::CreateRepeated(node, sequence_number);
    }
    fragment.GetMutableForCloning().SetBreakToken(break_token);

    // That's all we have to do, unless this is a fragmentation context root.

    if (!fragment.IsFragmentationContextRoot())
      continue;

    // If this is a fragmentation context root, we also need to update the
    // fragmentainers (which don't have a LayoutBox associated with them).

    for (const auto& child_link : fragment.Children()) {
      if (!child_link->IsFragmentainerBox())
        continue;
      const auto& fragmentainer =
          *To<PhysicalBoxFragment>(child_link.fragment.Get());
      const BlockBreakToken* fragmentainer_break_token =
          fragmentainer.GetBreakToken();
      if (fragmentainer_break_token && fragmentainer_break_token->IsRepeated())
        fragmentainer_break_token = nullptr;
      if (fragmentainer_break_token) {
        if (fragmentainer_break_token->SequenceNumber() !=
            fragmentainer_sequence_number) {
          fragmentainer_break_token =
              BlockBreakToken::CreateForBreakInRepeatedFragment(
                  node, fragmentainer_sequence_number,
                  fragmentainer_break_token->ConsumedBlockSize(),
                  /* is_at_block_end */ false);
          fragmentainer.GetMutableForCloning().SetBreakToken(
              fragmentainer_break_token);
        }
      } else {
        fragmentainer_break_token = BlockBreakToken::CreateRepeated(
            node, fragmentainer_sequence_number);
        fragmentainer.GetMutableForCloning().SetBreakToken(
            fragmentainer_break_token);

        // Since this fragmentainer didn't have a break token, it might be the
        // very last one, but it's not straight-forward to figure out whether
        // this is actually the case. So just keep track of what we're visiting.
        // It's been given a break token for now. If it turns out that this was
        // the last fragmentainer, we'll remove it again further below.
        last_fragmentainer = &fragmentainer;
      }
      fragmentainer_sequence_number++;
    }
  }

  // The last fragmentainer shouldn't have an outgoing break token, but it got
  // one above.
  if (last_fragmentainer)
    last_fragmentainer->GetMutableForCloning().SetBreakToken(nullptr);
}

}  // anonymous namespace

void FragmentRepeater::CloneChildFragments(
    const PhysicalBoxFragment& cloned_fragment) {
  if (cloned_fragment.HasItems()) {
    // Fragment items have already been cloned, but any atomic inlines were
    // shallowly cloned. Deep-clone them now, if any.
    for (auto& cloned_item : cloned_fragment.Items()->Items()) {
      const PhysicalBoxFragment* child_box_fragment = cloned_item.BoxFragment();
      if (!child_box_fragment)
        continue;
      const auto* child_layout_box =
          DynamicTo<LayoutBox>(child_box_fragment->GetLayoutObject());
      if (!child_layout_box) {
        // We don't need to clone non-atomic inlines.
        DCHECK(child_box_fragment->GetLayoutObject()->IsLayoutInline());
        continue;
      }
      const LayoutResult* child_result =
          GetClonableLayoutResult(*child_layout_box, *child_box_fragment);
      child_result = Repeat(*child_result);
      child_box_fragment =
          &To<PhysicalBoxFragment>(child_result->GetPhysicalFragment());
      cloned_item.GetMutableForCloning().ReplaceBoxFragment(
          *child_box_fragment);
    }
  }

  for (PhysicalFragmentLink& child :
       cloned_fragment.GetMutableForCloning().Children()) {
    if (const auto* child_box =
            DynamicTo<PhysicalBoxFragment>(child.fragment.Get())) {
      if (child_box->IsCSSBox()) {
        const auto* child_layout_box =
            To<LayoutBox>(child_box->GetLayoutObject());
        const LayoutResult* child_result =
            GetClonableLayoutResult(*child_layout_box, *child_box);
        child_result = Repeat(*child_result);
        child.fragment = &child_result->GetPhysicalFragment();
      } else if (child_box->IsFragmentainerBox()) {
        child_box = PhysicalBoxFragment::Clone(*child_box);
        CloneChildFragments(*child_box);
        child.fragment = child_box;
      }
    } else if (child->IsLineBox()) {
      child.fragment = PhysicalLineBoxFragment::Clone(
          To<PhysicalLineBoxFragment>(*child.fragment.Get()));
    }
  }
}

const LayoutResult* FragmentRepeater::Repeat(const LayoutResult& other) {
  const LayoutResult* cloned_result = LayoutResult::Clone(other);
  const auto& cloned_fragment =
      To<PhysicalBoxFragment>(cloned_result->GetPhysicalFragment());
  auto& layout_box = *To<LayoutBox>(cloned_fragment.GetMutableLayoutObject());

  if (is_first_clone_ && cloned_fragment.IsFirstForNode()) {
    // We're (re-)inserting cloned results, and we're at the first clone. Remove
    // the old results first.
    RemoveClonedResults(layout_box);
  }

  CloneChildFragments(cloned_fragment);

  // The first-for-node bit has also been cloned. But we're obviously not the
  // first anymore if we're repeated.
  cloned_fragment.GetMutableForCloning().ClearIsFirstForNode();

  layout_box.AppendLayoutResult(cloned_result);
  if (is_last_fragment_ && (!cloned_fragment.GetBreakToken() ||
                            cloned_fragment.GetBreakToken()->IsRepeated())) {
    // We've reached the end. We can finally add missing break tokens, and
    // update cloned sequence numbers.
    UpdateBreakTokens(layout_box);
    layout_box.ClearNeedsLayout();
    layout_box.FinalizeLayoutResults();
  }
  return cloned_result;
}

const LayoutResult* FragmentRepeater::GetClonableLayoutResult(
    const LayoutBox& layout_box,
    const PhysicalBoxFragment& fragment) const {
  if (const BlockBreakToken* break_token = fragment.GetBreakToken()) {
    if (!break_token->IsRepeated())
      return layout_box.GetLayoutResult(break_token->SequenceNumber());
  }
  // Cloned results may already have been added (so we can't just pick the last
  // one), but the break tokens have not yet been updated. Look for the first
  // result without a break token. Or look for the first result with a repeated
  // break token (unless the repeated break token is the result of an inner
  // fragmentation context), in case we've already been through this. This will
  // actually be the very first result, unless there's a fragmentation context
  // established inside the repeated root.
  for (const LayoutResult* result : layout_box.GetLayoutResults()) {
    const BlockBreakToken* break_token =
        To<PhysicalBoxFragment>(result->GetPhysicalFragment()).GetBreakToken();
    if (!break_token || break_token->IsRepeated())
      return result;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

}  // namespace blink

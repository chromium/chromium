// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_container_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_outline_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"

namespace blink {

NGPhysicalContainerFragment::NGPhysicalContainerFragment(
    NGContainerFragmentBuilder* builder,
    WritingMode block_or_line_writing_mode,
    NGLinkStorage* buffer,
    NGFragmentType type,
    unsigned sub_type)
    : NGPhysicalFragment(builder, type, sub_type),
      num_children_(builder->children_.size()) {
  DCHECK_EQ(builder->children_.size(), builder->offsets_.size());
  // Because flexible arrays need to be the last member in a class, we need to
  // have the buffer passed as a constructor argument and have the actual
  // storage be part of the subclass.
  wtf_size_t i = 0;
  for (auto& child : builder->children_) {
    buffer[i].fragment = child.get();
    buffer[i].fragment->AddRef();
    buffer[i].offset = builder->offsets_[i].ConvertToPhysical(
        block_or_line_writing_mode, builder->Direction(), Size(),
        child->Size());
    ++i;
  }
}

void NGPhysicalContainerFragment::AddOutlineRectsForNormalChildren(
    Vector<LayoutRect>* outline_rects,
    const LayoutPoint& additional_offset,
    NGOutlineType outline_type) const {
  for (const auto& child : Children()) {
    // Outlines of out-of-flow positioned descendants are handled in
    // NGPhysicalBoxFragment::AddSelfOutlineRects().
    if (child->IsOutOfFlowPositioned())
      continue;

    // Outline of an element continuation or anonymous block continuation is
    // added when we iterate the continuation chain.
    // See NGPhysicalBoxFragment::AddSelfOutlineRects().
    if (LayoutObject* child_layout_object = child->GetLayoutObject()) {
      if (child_layout_object->IsElementContinuation() ||
          (child_layout_object->IsLayoutBlockFlow() &&
           ToLayoutBlockFlow(child_layout_object)
               ->IsAnonymousBlockContinuation()))
        continue;
    }

    AddOutlineRectsForDescendant(child, outline_rects, additional_offset,
                                 outline_type);
  }
}

void NGPhysicalContainerFragment::AddOutlineRectsForDescendant(
    const NGLink& descendant,
    Vector<LayoutRect>* outline_rects,
    const LayoutPoint& additional_offset,
    NGOutlineType outline_type) const {
  if (descendant->IsText() || descendant->IsListMarker())
    return;

  if (const NGPhysicalBoxFragment* descendant_box =
          ToNGPhysicalBoxFragmentOrNull(descendant.get())) {
    LayoutObject* descendant_layout_object = descendant_box->GetLayoutObject();
    DCHECK(descendant_layout_object);

    if (descendant_box->HasLayer()) {
      Vector<LayoutRect> layer_outline_rects;
      descendant_box->AddSelfOutlineRects(&layer_outline_rects, LayoutPoint(),
                                          outline_type);
      descendant_layout_object->LocalToAncestorRects(
          layer_outline_rects, ToLayoutBoxModelObject(GetLayoutObject()),
          LayoutPoint(), additional_offset);
      outline_rects->AppendVector(layer_outline_rects);
      return;
    }

    if (descendant_layout_object->IsBox()) {
      descendant_box->AddSelfOutlineRects(
          outline_rects,
          additional_offset + descendant.Offset().ToLayoutPoint(),
          outline_type);
      return;
    }

    DCHECK(descendant_layout_object->IsLayoutInline());
    LayoutInline* descendant_layout_inline =
        ToLayoutInline(descendant_layout_object);
    // As an optimization, an ancestor has added rects for its line boxes
    // covering descendants' line boxes, so descendants don't need to add line
    // boxes again. For example, if the parent is a LayoutBlock, it adds rects
    // for its line box which cover the line boxes of this LayoutInline. So
    // the LayoutInline needs to add rects for children and continuations
    // only.
    if (!NGOutlineUtils::IsInlineOutlineNonpaintingFragment(*descendant)) {
      descendant_layout_inline->AddOutlineRectsForChildrenAndContinuations(
          *outline_rects, additional_offset, outline_type);
    }
    return;
  }

  if (const NGPhysicalLineBoxFragment* descendant_line_box =
          ToNGPhysicalLineBoxFragmentOrNull(descendant.get())) {
    descendant_line_box->AddOutlineRectsForNormalChildren(
        outline_rects, additional_offset + descendant.Offset().ToLayoutPoint(),
        outline_type);

    if (!descendant_line_box->Size().IsEmpty()) {
      outline_rects->emplace_back(additional_offset,
                                  descendant_line_box->Size().ToLayoutSize());
    } else if (descendant_line_box->Children().IsEmpty()) {
      // Special-case for when the first continuation does not generate
      // fragments. NGInlineLayoutAlgorithm suppresses box fragments when the
      // line is "empty". When there is a continuation from the LayoutInline,
      // the suppression makes such continuation not reachable. Check the
      // continuation from LayoutInline in such case.
      DCHECK(GetLayoutObject());
      if (LayoutInline* first_layout_inline =
              ToLayoutInlineOrNull(GetLayoutObject()->SlowFirstChild())) {
        if (!first_layout_inline->IsElementContinuation()) {
          first_layout_inline->AddOutlineRectsForChildrenAndContinuations(
              *outline_rects, additional_offset, outline_type);
        }
      }
    }
  }
}

}  // namespace blink

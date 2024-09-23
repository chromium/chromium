// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/style_adjuster.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_custom_scrollbar_part.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_spanner_placeholder.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_object_inl.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"

namespace blink {

void LayoutObject::Trace(Visitor* visitor) const {
  visitor->Trace(style_);
  visitor->Trace(node_);
  visitor->Trace(parent_);
  visitor->Trace(previous_);
  visitor->Trace(next_);
  visitor->Trace(fragment_);
  ImageResourceObserver::Trace(visitor);
  DisplayItemClient::Trace(visitor);
}

LayoutObject* LayoutObject::Container(AncestorSkipInfo* skip_info) const {
  NOT_DESTROYED();

#if DCHECK_IS_ON()
  if (skip_info)
    skip_info->AssertClean();
#endif

  if (IsTextOrSVGChild())
    return Parent();

  EPosition pos = style_->GetPosition();
  if (pos == EPosition::kFixed)
    return ContainerForFixedPosition(skip_info);

  if (pos == EPosition::kAbsolute) {
    return ContainerForAbsolutePosition(skip_info);
  }

  if (IsColumnSpanAll()) {
    LayoutObject* multicol_container = SpannerPlaceholder()->Container();
    if (skip_info) {
      // We jumped directly from the spanner to the multicol container. Need to
      // check if we skipped |ancestor| or filter/reflection on the way.
      for (LayoutObject* walker = Parent();
           walker && walker != multicol_container; walker = walker->Parent())
        skip_info->Update(*walker);
    }
    return multicol_container;
  }

  if (IsFloating() && !IsInLayoutNGInlineFormattingContext()) {
    // TODO(crbug.com/1229581): Remove this when removing support for legacy
    // layout.
    //
    // In the legacy engine, floats inside non-atomic inlines belong to their
    // nearest containing block, not the parent non-atomic inline (if any). Skip
    // past all non-atomic inlines. Note that the reason for not simply using
    // ContainingBlock() here is that we want to stop at any kind of LayoutBox,
    // such as LayoutVideo. Otherwise we won't mark the container chain
    // correctly when marking for re-layout.
    LayoutObject* walker = Parent();
    while (walker && walker->IsLayoutInline()) {
      if (skip_info)
        skip_info->Update(*walker);
      walker = walker->Parent();
    }
    return walker;
  }

  return Parent();
}

LayoutBox* LayoutObject::DeprecatedEnclosingScrollableBox() const {
  NOT_DESTROYED();
  DCHECK(!RuntimeEnabledFeatures::IntersectionOptimizationEnabled());
  for (LayoutObject* ancestor = Parent(); ancestor;
       ancestor = ancestor->Parent()) {
    if (!ancestor->IsBox())
      continue;

    auto* ancestor_box = To<LayoutBox>(ancestor);
    if (ancestor_box->IsUserScrollable()) {
      return ancestor_box;
    }
  }

  return nullptr;
}

void LayoutObject::SetNeedsOverflowRecalc(
    OverflowRecalcType overflow_recalc_type) {
  NOT_DESTROYED();
  if (IsLayoutFlowThread()) [[unlikely]] {
    // If we're a flow thread inside an NG multicol container, just redirect to
    // the multicol container, since the overflow recalculation walks down the
    // NG fragment tree, and the flow thread isn't represented there.
    if (auto* multicol_container = DynamicTo<LayoutBlockFlow>(Parent())) {
      multicol_container->SetNeedsOverflowRecalc(overflow_recalc_type);
      return;
    }
  }
  bool mark_container_chain_scrollable_overflow_recalc =
      !SelfNeedsScrollableOverflowRecalc();

  if (overflow_recalc_type ==
      OverflowRecalcType::kLayoutAndVisualOverflowRecalc) {
    SetSelfNeedsScrollableOverflowRecalc();
  }

  DCHECK(overflow_recalc_type ==
             OverflowRecalcType::kOnlyVisualOverflowRecalc ||
         overflow_recalc_type ==
             OverflowRecalcType::kLayoutAndVisualOverflowRecalc);
  SetShouldCheckForPaintInvalidation();
  MarkSelfPaintingLayerForVisualOverflowRecalc();

  if (mark_container_chain_scrollable_overflow_recalc) {
    MarkContainerChainForOverflowRecalcIfNeeded(
        overflow_recalc_type ==
        OverflowRecalcType::kLayoutAndVisualOverflowRecalc);
  }

#if 0  // TODO(crbug.com/1205708): This should pass, but it's not ready yet.
#if DCHECK_IS_ON()
  if (PaintLayer* layer = PaintingLayer())
    DCHECK(layer->NeedsVisualOverflowRecalc());
#endif
#endif
}

void LayoutObject::PropagateStyleToAnonymousChildren() {
  NOT_DESTROYED();
  // FIXME: We could save this call when the change only affected non-inherited
  // properties.
  for (LayoutObject* child = SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (!child->IsAnonymous() || child->StyleRef().StyleType() != kPseudoIdNone)
      continue;
    if (child->AnonymousHasStylePropagationOverride())
      continue;

    ComputedStyleBuilder new_style_builder =
        GetDocument().GetStyleResolver().CreateAnonymousStyleBuilderWithDisplay(
            StyleRef(), child->StyleRef().Display());

    if (IsA<LayoutTextCombine>(child)) [[unlikely]] {
      if (blink::IsHorizontalWritingMode(new_style_builder.GetWritingMode())) {
        // |LayoutTextCombine| will be removed when recalculating style for
        // <br> or <wbr>.
        // See StyleToHorizontalWritingModeWithWordBreak
        DCHECK(child->SlowFirstChild()->IsBR() ||
               To<LayoutText>(child->SlowFirstChild())->IsWordBreak() ||
               child->SlowFirstChild()->GetNode()->NeedsReattachLayoutTree());
      } else {
        // "text-combine-width-after-style-change.html" reaches here.
        StyleAdjuster::AdjustStyleForTextCombine(new_style_builder);
      }
    }

    UpdateAnonymousChildStyle(child, new_style_builder);

    child->SetStyle(new_style_builder.TakeStyle());
  }

  PseudoId pseudo_id = StyleRef().StyleType();
  if (pseudo_id == kPseudoIdNone)
    return;

  // Don't propagate style from markers with 'content: normal' because it's not
  // needed and it would be slow.
  if (pseudo_id == kPseudoIdMarker && StyleRef().ContentBehavesAsNormal())
    return;

  // Propagate style from pseudo elements to generated content. We skip children
  // with pseudo element StyleType() in the for-loop above and skip over
  // descendants which are not generated content in this subtree traversal.
  //
  // TODO(futhark): It's possible we could propagate anonymous style from pseudo
  // elements through anonymous table layout objects in the recursive
  // implementation above, but it would require propagating the StyleType()
  // somehow because there is code relying on generated content having a certain
  // StyleType().
  LayoutObject* child = NextInPreOrder(this);
  while (child) {
    if (!child->IsAnonymous()) {
      // Don't propagate into non-anonymous descendants of pseudo elements. This
      // can typically happen for ::first-letter inside ::before. The
      // ::first-letter will propagate to its anonymous children separately.
      child = child->NextInPreOrderAfterChildren(this);
      continue;
    }
    if (child->IsText() || child->IsQuote() || child->IsImage())
      child->SetPseudoElementStyle(*this);
    child = child->NextInPreOrder(this);
  }
}

void LayoutObject::UpdateImageObservers(const ComputedStyle* old_style,
                                        const ComputedStyle* new_style) {
  NOT_DESTROYED();
  DCHECK(old_style || new_style);
  DCHECK(!IsText());

  UpdateFillImages(old_style ? &old_style->BackgroundLayers() : nullptr,
                   new_style ? &new_style->BackgroundLayers() : nullptr);
  UpdateFillImages(old_style ? &old_style->MaskLayers() : nullptr,
                   new_style ? &new_style->MaskLayers() : nullptr);

  UpdateImage(old_style ? old_style->BorderImage().GetImage() : nullptr,
              new_style ? new_style->BorderImage().GetImage() : nullptr);
  UpdateImage(old_style ? old_style->MaskBoxImage().GetImage() : nullptr,
              new_style ? new_style->MaskBoxImage().GetImage() : nullptr);

  StyleImage* old_content_image =
      old_style && old_style->GetContentData() &&
              old_style->GetContentData()->IsImage()
          ? To<ImageContentData>(old_style->GetContentData())->GetImage()
          : nullptr;
  StyleImage* new_content_image =
      new_style && new_style->GetContentData() &&
              new_style->GetContentData()->IsImage()
          ? To<ImageContentData>(new_style->GetContentData())->GetImage()
          : nullptr;
  UpdateImage(old_content_image, new_content_image);

  StyleImage* old_box_reflect_mask_image =
      old_style && old_style->BoxReflect()
          ? old_style->BoxReflect()->Mask().GetImage()
          : nullptr;
  StyleImage* new_box_reflect_mask_image =
      new_style && new_style->BoxReflect()
          ? new_style->BoxReflect()->Mask().GetImage()
          : nullptr;
  UpdateImage(old_box_reflect_mask_image, new_box_reflect_mask_image);

  UpdateShapeImage(old_style ? old_style->ShapeOutside() : nullptr,
                   new_style ? new_style->ShapeOutside() : nullptr);
  UpdateCursorImages(old_style ? old_style->Cursors() : nullptr,
                     new_style ? new_style->Cursors() : nullptr);

  UpdateFirstLineImageObservers(new_style);
}

LayoutBlock* LayoutObject::ContainingBlock(AncestorSkipInfo* skip_info) const {
  NOT_DESTROYED();
  if (!IsTextOrSVGChild()) {
    if (style_->GetPosition() == EPosition::kFixed)
      return ContainingBlockForFixedPosition(skip_info);
    if (style_->GetPosition() == EPosition::kAbsolute)
      return ContainingBlockForAbsolutePosition(skip_info);
  }
  LayoutObject* object;
  if (IsColumnSpanAll()) {
    object = SpannerPlaceholder()->ContainingBlock();
  } else {
    object = Parent();
    if (!object && IsLayoutCustomScrollbarPart()) {
      object = To<LayoutCustomScrollbarPart>(this)
                   ->GetScrollableArea()
                   ->GetLayoutBox();
    }
    while (object && ((object->IsInline() && !object->IsAtomicInlineLevel()) ||
                      !object->IsLayoutBlock())) {
      if (skip_info)
        skip_info->Update(*object);
      object = object->Parent();
    }
  }

  return DynamicTo<LayoutBlock>(object);
}

}  // namespace blink

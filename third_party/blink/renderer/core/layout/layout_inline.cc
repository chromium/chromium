/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc.
 *               All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/layout/layout_inline.h"

#include "cc/base/region.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_box_model.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_outline_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/inline_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/outline_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "ui/gfx/geometry/quad_f.h"

namespace blink {

namespace {

// TODO(xiaochengh): Deduplicate with a similar function in ng_paint_fragment.cc
// ::before, ::after and ::first-letter can be hit test targets.
bool CanBeHitTestTargetPseudoNodeStyle(const ComputedStyle& style) {
  switch (style.StyleType()) {
    case kPseudoIdBefore:
    case kPseudoIdAfter:
    case kPseudoIdFirstLetter:
      return true;
    default:
      return false;
  }
}

}  // anonymous namespace

struct SameSizeAsLayoutInline : public LayoutBoxModelObject {
  ~SameSizeAsLayoutInline() override = default;
  LayoutObjectChildList children_;
  LineBoxList line_boxes_;
  wtf_size_t first_fragment_item_index_;
};

ASSERT_SIZE(LayoutInline, SameSizeAsLayoutInline);

LayoutInline::LayoutInline(Element* element)
    : LayoutBoxModelObject(element), line_boxes_() {
  SetChildrenInline(true);
}

void LayoutInline::Trace(Visitor* visitor) const {
  visitor->Trace(children_);
  visitor->Trace(line_boxes_);
  LayoutBoxModelObject::Trace(visitor);
}

LayoutInline* LayoutInline::CreateAnonymous(Document* document) {
  LayoutInline* layout_inline = MakeGarbageCollected<LayoutInline>(nullptr);
  layout_inline->SetDocumentForAnonymous(document);
  return layout_inline;
}

void LayoutInline::WillBeDestroyed() {
  NOT_DESTROYED();
  // Make sure to destroy anonymous children first while they are still
  // connected to the rest of the tree, so that they will properly dirty line
  // boxes that they are removed from. Effects that do :before/:after only on
  // hover could crash otherwise.
  Children()->DestroyLeftoverChildren();

  if (TextAutosizer* text_autosizer = GetDocument().GetTextAutosizer())
    text_autosizer->Destroy(this);

  if (!DocumentBeingDestroyed()) {
    if (FirstLineBox()) {
      // If line boxes are contained inside a root, that means we're an inline.
      // In that case, we need to remove all the line boxes so that the parent
      // lines aren't pointing to deleted children. If the first line box does
      // not have a parent that means they are either already disconnected or
      // root lines that can just be destroyed without disconnecting.
      if (FirstLineBox()->Parent()) {
        for (InlineFlowBox* box : *LineBoxes())
          box->Remove();
      }
    } else {
      if (Parent())
        Parent()->DirtyLinesFromChangedChild(this);
      if (FirstInlineFragmentItemIndex()) {
        NGFragmentItems::LayoutObjectWillBeDestroyed(*this);
        ClearFirstInlineFragmentItemIndex();
      }
    }
  }

  DeleteLineBoxes();

  LayoutBoxModelObject::WillBeDestroyed();

#if DCHECK_IS_ON()
  if (!IsInLayoutNGInlineFormattingContext())
    line_boxes_.AssertIsEmpty();
#endif
}

void LayoutInline::DeleteLineBoxes() {
  NOT_DESTROYED();
  if (!IsInLayoutNGInlineFormattingContext())
    MutableLineBoxes()->DeleteLineBoxes();
}

void LayoutInline::ClearFirstInlineFragmentItemIndex() {
  NOT_DESTROYED();
  CHECK(IsInLayoutNGInlineFormattingContext()) << *this;
  first_fragment_item_index_ = 0u;
}

void LayoutInline::SetFirstInlineFragmentItemIndex(wtf_size_t index) {
  NOT_DESTROYED();
  CHECK(IsInLayoutNGInlineFormattingContext()) << *this;
  DCHECK_NE(index, 0u);
  first_fragment_item_index_ = index;
}

bool LayoutInline::HasInlineFragments() const {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext())
    return first_fragment_item_index_;
  return FirstLineBox();
}

void LayoutInline::InLayoutNGInlineFormattingContextWillChange(bool new_value) {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext())
    ClearFirstInlineFragmentItemIndex();
  else
    DeleteLineBoxes();

  // Because |first_fragment_item_index_| and |line_boxes_| are union, when one
  // is deleted, the other should be initialized to nullptr.
  DCHECK(new_value ? !first_fragment_item_index_ : !line_boxes_.First());
}

void LayoutInline::UpdateFromStyle() {
  NOT_DESTROYED();
  LayoutBoxModelObject::UpdateFromStyle();

  // FIXME: Is this still needed. Was needed for run-ins, since run-in is
  // considered a block display type.
  SetInline(true);

  // FIXME: Support transforms and reflections on inline flows someday.
  SetHasTransformRelatedProperty(false);
  SetHasReflection(false);
}

void LayoutInline::StyleDidChange(StyleDifference diff,
                                  const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutBoxModelObject::StyleDidChange(diff, old_style);

  const ComputedStyle& new_style = StyleRef();
  if (!IsInLayoutNGInlineFormattingContext()) {
    if (!AlwaysCreateLineBoxes()) {
      bool always_create_line_boxes_new =
          HasSelfPaintingLayer() || HasBoxDecorationBackground() ||
          new_style.MayHavePadding() || new_style.MayHaveMargin() ||
          new_style.HasOutline();
      if (old_style && always_create_line_boxes_new) {
        DirtyLineBoxes(false);
        SetNeedsLayoutAndFullPaintInvalidation(
            layout_invalidation_reason::kStyleChange);
      }
      SetAlwaysCreateLineBoxes(always_create_line_boxes_new);
    }
  } else {
    if (!ShouldCreateBoxFragment()) {
      UpdateShouldCreateBoxFragment();
    }
    if (diff.NeedsReshape()) {
      SetNeedsCollectInlines();
    }
  }

  PropagateStyleToAnonymousChildren();
}

void LayoutInline::UpdateAlwaysCreateLineBoxes(bool full_layout) {
  NOT_DESTROYED();
  DCHECK(!IsInLayoutNGInlineFormattingContext());

  // Once we have been tainted once, just assume it will happen again. This way
  // effects like hover highlighting that change the background color will only
  // cause a layout on the first rollover.
  if (AlwaysCreateLineBoxes())
    return;

  const ComputedStyle& parent_style = Parent()->StyleRef();
  auto* parent_layout_inline = DynamicTo<LayoutInline>(Parent());
  bool check_fonts = GetDocument().InNoQuirksMode();
  bool always_create_line_boxes_new =
      (parent_layout_inline && parent_layout_inline->AlwaysCreateLineBoxes()) ||
      (parent_layout_inline &&
       parent_style.VerticalAlign() != EVerticalAlign::kBaseline) ||
      StyleRef().VerticalAlign() != EVerticalAlign::kBaseline ||
      StyleRef().GetTextEmphasisMark() != TextEmphasisMark::kNone ||
      (check_fonts &&
       (!StyleRef().HasIdenticalAscentDescentAndLineGap(parent_style) ||
        parent_style.LineHeight() != StyleRef().LineHeight()));

  if (!always_create_line_boxes_new && check_fonts &&
      GetDocument().GetStyleEngine().UsesFirstLineRules()) {
    // Have to check the first line style as well.
    const ComputedStyle& first_line_parent_style = Parent()->StyleRef(true);
    const ComputedStyle& child_style = StyleRef(true);
    always_create_line_boxes_new =
        !first_line_parent_style.HasIdenticalAscentDescentAndLineGap(
            child_style) ||
        child_style.VerticalAlign() != EVerticalAlign::kBaseline ||
        first_line_parent_style.LineHeight() != child_style.LineHeight();
  }

  if (always_create_line_boxes_new) {
    if (!full_layout)
      DirtyLineBoxes(false);
    SetAlwaysCreateLineBoxes();
  }
}

bool LayoutInline::ComputeInitialShouldCreateBoxFragment(
    const ComputedStyle& style) const {
  NOT_DESTROYED();

  // We'd like to use ScopedSVGPaintState in
  // NGInlineBoxFragmentPainter::Paint().
  // TODO(layout-dev): Improve the below condition so that we a create box
  // fragment only if this requires ScopedSVGPaintState, instead of
  // creating box fragments for all LayoutSVGInlines.
  if (IsSVGInline())
    return true;

  if (style.HasBoxDecorationBackground() || style.MayHavePadding() ||
      style.MayHaveMargin())
    return true;

  if (style.AnchorName())
    return true;

  if (const Element* element = DynamicTo<Element>(GetNode())) {
    if (element->HasImplicitlyAnchoredElement()) {
      return true;
    }
  }

  return ComputeIsAbsoluteContainer(&style) ||
         NGOutlineUtils::HasPaintedOutline(style, GetNode()) ||
         CanBeHitTestTargetPseudoNodeStyle(style);
}

bool LayoutInline::ComputeInitialShouldCreateBoxFragment() const {
  NOT_DESTROYED();
  const ComputedStyle& style = StyleRef();
  if (HasSelfPaintingLayer() || ComputeInitialShouldCreateBoxFragment(style) ||
      ShouldApplyPaintContainment() || ShouldApplyLayoutContainment())
    return true;

  const ComputedStyle& first_line_style = FirstLineStyleRef();
  if (UNLIKELY(&style != &first_line_style &&
               ComputeInitialShouldCreateBoxFragment(first_line_style)))
    return true;

  return false;
}

void LayoutInline::UpdateShouldCreateBoxFragment() {
  NOT_DESTROYED();
  // Once we have been tainted once, just assume it will happen again. This way
  // effects like hover highlighting that change the background color will only
  // cause a layout on the first rollover.
  if (IsInLayoutNGInlineFormattingContext()) {
    if (ShouldCreateBoxFragment())
      return;
  } else {
    SetIsInLayoutNGInlineFormattingContext(true);
    SetShouldCreateBoxFragment(false);
  }

  if (ComputeInitialShouldCreateBoxFragment()) {
    SetShouldCreateBoxFragment();
    SetNeedsLayoutAndFullPaintInvalidation(
        layout_invalidation_reason::kStyleChange);
  }
}

LayoutRect LayoutInline::LocalCaretRect(
    const InlineBox* inline_box,
    int,
    LayoutUnit* extra_width_to_end_of_line) const {
  NOT_DESTROYED();
  if (FirstChild()) {
    // This condition is possible if the LayoutInline is at an editing boundary,
    // i.e. the VisiblePosition is:
    //   <LayoutInline editingBoundary=true>|<LayoutText>
    //   </LayoutText></LayoutInline>
    // FIXME: need to figure out how to make this return a valid rect, note that
    // there are no line boxes created in the above case.
    return LayoutRect();
  }

  DCHECK(!inline_box);

  if (extra_width_to_end_of_line)
    *extra_width_to_end_of_line = LayoutUnit();

  LayoutRect caret_rect =
      LocalCaretRectForEmptyElement(BorderAndPaddingWidth(), LayoutUnit());

  if (InlineBox* first_box = FirstLineBox()) {
    caret_rect.MoveBy(first_box->Location());
  } else if (IsInLayoutNGInlineFormattingContext()) {
    NGInlineCursor cursor;
    cursor.MoveTo(*this);
    if (cursor) {
      caret_rect.MoveBy(
          cursor.Current().OffsetInContainerFragment().ToLayoutPoint());
    }
  }

  return caret_rect;
}

void LayoutInline::AddChild(LayoutObject* new_child,
                            LayoutObject* before_child) {
  NOT_DESTROYED();
  // Any table-part dom child of an inline element has anonymous wrappers in the
  // layout tree so we need to climb up to the enclosing anonymous table wrapper
  // and add the new child before that.
  // TODO(rhogan): If newChild is a table part we want to insert it into the
  // same table as beforeChild.
  while (before_child && before_child->IsTablePart())
    before_child = before_child->Parent();
  return AddChildIgnoringContinuation(new_child, before_child);
}

void LayoutInline::AddChildIgnoringContinuation(LayoutObject* new_child,
                                                LayoutObject* before_child) {
  NOT_DESTROYED();
  // Make sure we don't append things after :after-generated content if we have
  // it.
  if (!before_child && IsAfterContent(LastChild()))
    before_child = LastChild();

  if (!new_child->IsInline() && !new_child->IsFloatingOrOutOfFlowPositioned() &&
      // Table parts can be either inline or block. When creating its table
      // wrapper, |CreateAnonymousTableWithParent| creates an inline table if
      // the parent is |LayoutInline|.
      !new_child->IsTablePart()) {
    AddChildAsBlockInInline(new_child, before_child);
    return;
  }

  // If inserting an inline child before a block-in-inline, change
  // |before_child| to the anonymous block. The anonymous block may need to be
  // split if |before_child| is not the first child.
  if (before_child && before_child->Parent() != this) {
    DCHECK(!ForceLegacyLayout());
    DCHECK(before_child->Parent()->IsBlockInInline());
    DCHECK(IsA<LayoutBlockFlow>(before_child->Parent()));
    DCHECK_EQ(before_child->Parent()->Parent(), this);
    before_child = SplitAnonymousBoxesAroundChild(before_child);
  }

  LayoutBoxModelObject::AddChild(new_child, before_child);

  new_child->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kChildChanged);
}

void LayoutInline::AddChildAsBlockInInline(LayoutObject* new_child,
                                           LayoutObject* before_child) {
  DCHECK(!ForceLegacyLayout());
  DCHECK(!new_child->IsInline());
  LayoutBlockFlow* anonymous_box;
  if (!before_child) {
    anonymous_box = DynamicTo<LayoutBlockFlow>(LastChild());
  } else if (before_child->IsInline() ||
             before_child->IsFloatingOrOutOfFlowPositioned()) {
    anonymous_box = DynamicTo<LayoutBlockFlow>(before_child->PreviousSibling());
  } else {
    // If |before_child| is not inline, it should have been added to the
    // anonymous block.
    anonymous_box = DynamicTo<LayoutBlockFlow>(before_child->Parent());
    DCHECK(anonymous_box);
    DCHECK(anonymous_box->IsBlockInInline());
    anonymous_box->AddChild(new_child, before_child);
    return;
  }
  if (!anonymous_box || !anonymous_box->IsBlockInInline()) {
    anonymous_box = CreateAnonymousContainerForBlockChildren();
    LayoutBoxModelObject::AddChild(anonymous_box, before_child);
  }
  DCHECK(anonymous_box->IsBlockInInline());
  anonymous_box->AddChild(new_child);
}

LayoutBlockFlow* LayoutInline::CreateAnonymousContainerForBlockChildren()
    const {
  NOT_DESTROYED();
  // TODO(1229581): Determine if we actually need to set the direction for
  // block-in-inline.

  // We are placing a block inside an inline. We have to perform a split of this
  // inline into continuations. This involves creating an anonymous block box to
  // hold |newChild|. We then make that block box a continuation of this
  // inline. We take all of the children after |beforeChild| and put them in a
  // clone of this object.
  ComputedStyleBuilder new_style_builder =
      GetDocument().GetStyleResolver().CreateAnonymousStyleBuilderWithDisplay(
          StyleRef(), EDisplay::kBlock);
  const LayoutBlock* containing_block = ContainingBlock();
  // The anon block we create here doesn't exist in the CSS spec, so we need to
  // ensure that any blocks it contains inherit properly from its true
  // parent. This means they must use the direction set by the anon block's
  // containing block, so we need to prevent the anon block from inheriting
  // direction from the inline. If there are any other inheritable properties
  // that apply to block and inline elements but only affect the layout of
  // children we will want to special-case them here too. Writing-mode would be
  // one if it didn't create a formatting context of its own, removing the need
  // for continuations.
  new_style_builder.SetDirection(containing_block->StyleRef().Direction());

  LegacyLayout legacy = containing_block->ForceLegacyLayout()
                            ? LegacyLayout::kForce
                            : LegacyLayout::kAuto;

  return LayoutBlockFlow::CreateAnonymous(
      &GetDocument(), new_style_builder.TakeStyle(), legacy);
}

LayoutBox* LayoutInline::CreateAnonymousBoxToSplit(
    const LayoutBox* box_to_split) const {
  NOT_DESTROYED();
  DCHECK(box_to_split->IsBlockInInline());
  DCHECK(IsA<LayoutBlockFlow>(box_to_split));
  DCHECK(!ForceLegacyLayout());
  return CreateAnonymousContainerForBlockChildren();
}

void LayoutInline::Paint(const PaintInfo& paint_info) const {
  NOT_DESTROYED();
  InlinePainter(*this).Paint(paint_info);
}

template <typename PhysicalRectCollector>
void LayoutInline::CollectLineBoxRects(
    const PhysicalRectCollector& yield) const {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext()) {
    NGInlineCursor cursor;
    cursor.MoveToIncludingCulledInline(*this);
    for (; cursor; cursor.MoveToNextForSameLayoutObject())
      yield(cursor.CurrentRectInBlockFlow());
    return;
  }
  if (!AlwaysCreateLineBoxes()) {
    CollectCulledLineBoxRects(yield);
  } else {
    const LayoutBlock* block_for_flipping =
        UNLIKELY(HasFlippedBlocksWritingMode()) ? ContainingBlock() : nullptr;
    for (InlineFlowBox* curr : *LineBoxes()) {
      yield(FlipForWritingMode(LayoutRect(curr->Location(), curr->Size()),
                               block_for_flipping));
    }
  }
}

template <typename PhysicalRectCollector>
void LayoutInline::CollectCulledLineBoxRects(
    const PhysicalRectCollector& yield) const {
  NOT_DESTROYED();
  DCHECK(!IsInLayoutNGInlineFormattingContext());
  const LayoutBlock* block_for_flipping =
      UNLIKELY(HasFlippedBlocksWritingMode()) ? ContainingBlock() : nullptr;
  CollectCulledLineBoxRectsInFlippedBlocksDirection(
      [this, block_for_flipping, &yield](const LayoutRect& r) {
        PhysicalRect rect = FlipForWritingMode(r, block_for_flipping);
        yield(rect);
      },
      this);
}

static inline void ComputeItemTopHeight(const LayoutInline* container,
                                        const RootInlineBox& root_box,
                                        LayoutUnit* top,
                                        LayoutUnit* height) {
  bool first_line = root_box.IsFirstLineStyle();
  const SimpleFontData* font_data =
      root_box.GetLineLayoutItem().Style(first_line)->GetFont().PrimaryFont();
  const SimpleFontData* container_font_data =
      container->Style(first_line)->GetFont().PrimaryFont();
  DCHECK(font_data);
  DCHECK(container_font_data);
  if (!font_data || !container_font_data) {
    *top = LayoutUnit();
    *height = LayoutUnit();
    return;
  }
  auto metrics = font_data->GetFontMetrics();
  auto container_metrics = container_font_data->GetFontMetrics();
  *top =
      root_box.LogicalTop() + (metrics.Ascent() - container_metrics.Ascent());
  *height = LayoutUnit(container_metrics.Height());
}

template <typename FlippedRectCollector>
void LayoutInline::CollectCulledLineBoxRectsInFlippedBlocksDirection(
    const FlippedRectCollector& yield,
    const LayoutInline* container) const {
  NOT_DESTROYED();
  if (!CulledInlineFirstLineBox())
    return;

  bool is_horizontal = StyleRef().IsHorizontalWritingMode();

  LayoutUnit logical_top, logical_height;
  for (LayoutObject* curr = FirstChild(); curr; curr = curr->NextSibling()) {
    if (curr->IsFloatingOrOutOfFlowPositioned())
      continue;

    // We want to get the margin box in the inline direction, and then use our
    // font ascent/descent in the block direction (aligned to the root box's
    // baseline).
    if (curr->IsBox()) {
      auto* curr_box = To<LayoutBox>(curr);
      if (curr_box->InlineBoxWrapper()) {
        RootInlineBox& root_box = curr_box->InlineBoxWrapper()->Root();
        ComputeItemTopHeight(container, root_box, &logical_top,
                             &logical_height);
        if (is_horizontal) {
          yield(LayoutRect(
              curr_box->InlineBoxWrapper()->X() - curr_box->MarginLeft(),
              logical_top, curr_box->Size().Width() + curr_box->MarginWidth(),
              logical_height));
        } else {
          yield(LayoutRect(
              logical_top,
              curr_box->InlineBoxWrapper()->Y() - curr_box->MarginTop(),
              logical_height,
              curr_box->Size().Height() + curr_box->MarginHeight()));
        }
      }
    } else if (curr->IsLayoutInline()) {
      // If the child doesn't need line boxes either, then we can recur.
      auto* curr_inline = To<LayoutInline>(curr);
      if (!curr_inline->AlwaysCreateLineBoxes()) {
        curr_inline->CollectCulledLineBoxRectsInFlippedBlocksDirection(
            yield, container);
      } else {
        for (InlineFlowBox* child_line : *curr_inline->LineBoxes()) {
          RootInlineBox& root_box = child_line->Root();
          ComputeItemTopHeight(container, root_box, &logical_top,
                               &logical_height);
          LayoutUnit logical_width =
              child_line->LogicalWidth() + child_line->MarginLogicalWidth();
          if (is_horizontal) {
            yield(LayoutRect(
                LayoutUnit(child_line->X() - child_line->MarginLogicalLeft()),
                logical_top, logical_width, logical_height));
          } else {
            yield(LayoutRect(
                logical_top,
                LayoutUnit(child_line->Y() - child_line->MarginLogicalLeft()),
                logical_height, logical_width));
          }
        }
      }
    } else if (curr->IsText()) {
      auto* curr_text = To<LayoutText>(curr);
      for (InlineTextBox* child_text : curr_text->TextBoxes()) {
        RootInlineBox& root_box = child_text->Root();
        ComputeItemTopHeight(container, root_box, &logical_top,
                             &logical_height);
        if (is_horizontal)
          yield(LayoutRect(child_text->X(), logical_top,
                           child_text->LogicalWidth(), logical_height));
        else
          yield(LayoutRect(logical_top, child_text->Y(), logical_height,
                           child_text->LogicalWidth()));
      }
    }
  }
}

bool LayoutInline::AbsoluteTransformDependsOnPoint(
    const LayoutObject& object) const {
  const LayoutObject* current = &object;
  const LayoutObject* container = object.Container();
  while (container) {
    if (current->OffsetForContainerDependsOnPoint(container))
      return true;
    current = container;
    container = container->Container();
  }
  return false;
}

void LayoutInline::LocalQuadsForSelf(Vector<gfx::QuadF>& quads) const {
  QuadsForSelfInternal(quads, 0, false);
}

void LayoutInline::AbsoluteQuadsForSelf(Vector<gfx::QuadF>& quads,
                                        MapCoordinatesFlags mode) const {
  QuadsForSelfInternal(quads, mode, true);
}

void LayoutInline::QuadsForSelfInternal(Vector<gfx::QuadF>& quads,
                                        MapCoordinatesFlags mode,
                                        bool map_to_absolute) const {
  NOT_DESTROYED();
  absl::optional<gfx::Transform> mapping_to_absolute;
  // Set to true if the transform to absolute space depends on the point
  // being mapped (in which case we can't use LocalToAbsoluteTransform).
  bool transform_depends_on_point = false;
  bool transform_depends_on_point_computed = false;
  auto PushAbsoluteQuad = [&transform_depends_on_point,
                           &transform_depends_on_point_computed,
                           &mapping_to_absolute, &quads, mode,
                           this](const PhysicalRect& rect) {
    if (!transform_depends_on_point_computed) {
      transform_depends_on_point_computed = true;
      transform_depends_on_point = AbsoluteTransformDependsOnPoint(*this);
      if (!transform_depends_on_point)
        mapping_to_absolute.emplace(LocalToAbsoluteTransform(mode));
    }
    if (transform_depends_on_point) {
      quads.push_back(LocalToAbsoluteQuad(gfx::QuadF(gfx::RectF(rect)), mode));
    } else {
      quads.push_back(
          mapping_to_absolute->MapQuad(gfx::QuadF(gfx::RectF(rect))));
    }
  };

  CollectLineBoxRects(
      [&PushAbsoluteQuad, &map_to_absolute, &quads](const PhysicalRect& rect) {
        if (map_to_absolute)
          PushAbsoluteQuad(rect);
        else
          quads.push_back(gfx::QuadF(gfx::RectF(rect)));
      });
  if (quads.empty()) {
    if (map_to_absolute)
      PushAbsoluteQuad(PhysicalRect());
    else
      quads.push_back(gfx::QuadF());
  }
}

absl::optional<PhysicalOffset> LayoutInline::FirstLineBoxTopLeftInternal()
    const {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext()) {
    NGInlineCursor cursor;
    cursor.MoveToIncludingCulledInline(*this);
    if (!cursor)
      return absl::nullopt;
    return cursor.CurrentOffsetInBlockFlow();
  }
  if (const InlineBox* first_box = FirstLineBoxIncludingCulling()) {
    LayoutPoint location = first_box->Location();
    if (UNLIKELY(HasFlippedBlocksWritingMode())) {
      location.Move(first_box->Width(), LayoutUnit());
      return ContainingBlock()->FlipForWritingMode(location);
    }
    return PhysicalOffset(location);
  }
  return absl::nullopt;
}

PhysicalOffset LayoutInline::AnchorPhysicalLocation() const {
  NOT_DESTROYED();
  if (const auto& location = FirstLineBoxTopLeftInternal())
    return *location;
  // This object doesn't have fragment/line box, probably because it's an empty
  // and at the beginning/end of a line. Query sibling or parent.
  // TODO(crbug.com/953479): We won't need this if we always create line box
  // for empty inline elements. The following algorithm works in most cases for
  // anchor elements, though may be inaccurate in some corner cases (e.g. if the
  // sibling is not in the same line).
  if (const auto* sibling = NextSibling()) {
    if (sibling->IsLayoutInline())
      return To<LayoutInline>(sibling)->AnchorPhysicalLocation();
    if (sibling->IsText())
      return To<LayoutText>(sibling)->FirstLineBoxTopLeft();
    if (sibling->IsBox())
      return To<LayoutBox>(sibling)->PhysicalLocation();
  }
  if (Parent()->IsLayoutInline())
    return To<LayoutInline>(Parent())->AnchorPhysicalLocation();
  return PhysicalOffset();
}

PhysicalRect LayoutInline::AbsoluteBoundingBoxRectHandlingEmptyInline(
    MapCoordinatesFlags flags) const {
  NOT_DESTROYED();
  Vector<PhysicalRect> rects = OutlineRects(
      nullptr, PhysicalOffset(), NGOutlineType::kIncludeBlockVisualOverflow);
  PhysicalRect rect = UnionRect(rects);
  // When empty LayoutInline is not culled, |rect| is empty but |rects| is not.
  if (rect.IsEmpty())
    rect.offset = AnchorPhysicalLocation();
  return LocalToAbsoluteRect(rect);
}

LayoutUnit LayoutInline::OffsetLeft(const Element* parent) const {
  NOT_DESTROYED();
  return AdjustedPositionRelativeTo(FirstLineBoxTopLeft(), parent).left;
}

LayoutUnit LayoutInline::OffsetTop(const Element* parent) const {
  NOT_DESTROYED();
  return AdjustedPositionRelativeTo(FirstLineBoxTopLeft(), parent).top;
}

LayoutUnit LayoutInline::OffsetWidth() const {
  NOT_DESTROYED();
  return PhysicalLinesBoundingBox().Width();
}

LayoutUnit LayoutInline::OffsetHeight() const {
  NOT_DESTROYED();
  return PhysicalLinesBoundingBox().Height();
}

static LayoutUnit ComputeMargin(const LayoutInline* layout_object,
                                const Length& margin) {
  if (margin.IsFixed())
    return LayoutUnit(margin.Value());
  if (margin.IsPercentOrCalc())
    return MinimumValueForLength(
        margin,
        std::max(LayoutUnit(),
                 layout_object->ContainingBlock()->AvailableLogicalWidth()));
  return LayoutUnit();
}

LayoutUnit LayoutInline::MarginLeft() const {
  NOT_DESTROYED();
  return ComputeMargin(this, StyleRef().MarginLeft());
}

LayoutUnit LayoutInline::MarginRight() const {
  NOT_DESTROYED();
  return ComputeMargin(this, StyleRef().MarginRight());
}

LayoutUnit LayoutInline::MarginTop() const {
  NOT_DESTROYED();
  return ComputeMargin(this, StyleRef().MarginTop());
}

LayoutUnit LayoutInline::MarginBottom() const {
  NOT_DESTROYED();
  return ComputeMargin(this, StyleRef().MarginBottom());
}

bool LayoutInline::NodeAtPoint(HitTestResult& result,
                               const HitTestLocation& hit_test_location,
                               const PhysicalOffset& accumulated_offset,
                               HitTestPhase phase) {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext()) {
    // TODO(crbug.com/965976): We should fix the root cause of the missed
    // layout.
    if (UNLIKELY(NeedsLayout())) {
      NOTREACHED();
      return false;
    }

    // In LayoutNG, we reach here only when called from
    // PaintLayer::HitTestContents() without going through any ancestor, in
    // which case the element must have self painting layer.
    DCHECK(HasSelfPaintingLayer());
    NGInlineCursor cursor;
    cursor.MoveTo(*this);
    if (!cursor)
      return false;
    int target_fragment_idx = hit_test_location.FragmentIndex();
    // Fragment traversal requires a target fragment to be specified,
    // unless there's only one.
    DCHECK(!CanTraversePhysicalFragments() || target_fragment_idx >= 0 ||
           !FirstFragment().NextFragment());
    // Convert from inline fragment index to container fragment index, as the
    // inline may not start in the first fragment generated for the inline
    // formatting context.
    if (target_fragment_idx != -1)
      target_fragment_idx += cursor.ContainerFragmentIndex();

    for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
      if (target_fragment_idx != -1 &&
          wtf_size_t(target_fragment_idx) != cursor.ContainerFragmentIndex())
        continue;
      DCHECK(cursor.Current().Item());
      const NGFragmentItem& item = *cursor.Current().Item();
      const NGPhysicalBoxFragment* box_fragment = item.BoxFragment();
      DCHECK(box_fragment);
      // NGBoxFragmentPainter::NodeAtPoint() takes an offset that is accumulated
      // up to the fragment itself. Compute this offset.
      const PhysicalOffset child_offset =
          accumulated_offset + item.OffsetInContainerFragment();
      NGInlinePaintContext inline_context;
      if (NGBoxFragmentPainter(cursor, item, *box_fragment, &inline_context)
              .NodeAtPoint(result, hit_test_location, child_offset,
                           accumulated_offset, phase)) {
        return true;
      }
    }
    return false;
  }

  return LineBoxes()->HitTest(LineLayoutBoxModel(this), result,
                              hit_test_location, accumulated_offset, phase);
}

bool LayoutInline::HitTestCulledInline(HitTestResult& result,
                                       const HitTestLocation& hit_test_location,
                                       const PhysicalOffset& accumulated_offset,
                                       const NGInlineCursor* parent_cursor) {
  NOT_DESTROYED();
  DCHECK(parent_cursor || !AlwaysCreateLineBoxes());
  if (!VisibleToHitTestRequest(result.GetHitTestRequest()))
    return false;

  HitTestLocation adjusted_location(hit_test_location, -accumulated_offset);
  cc::Region region_result;
  bool intersected = false;
  auto yield = [&adjusted_location, &region_result,
                &intersected](const PhysicalRect& rect) {
    if (adjusted_location.Intersects(rect)) {
      intersected = true;
      region_result.Union(ToEnclosingRect(rect));
    }
  };

  // NG generates purely physical rectangles here, while legacy sets the block
  // offset on the rectangles relatively to the block-start. NG is doing the
  // right thing. Legacy is wrong.
  if (parent_cursor) {
    // Iterate fragments for |this|, including culled inline, but only that are
    // descendants of |parent_cursor|.
    DCHECK(IsDescendantOf(parent_cursor->GetLayoutBlockFlow()));
    NGInlineCursor cursor(*parent_cursor);
    cursor.MoveToIncludingCulledInline(*this);
    for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
      // Block-in-inline is inline in the box tree, and may appear as a child of
      // a culled inline, but it should be painted and hit-tested as block
      // painting-order-wise. Don't include it as part of the culled inline
      // region. https://www.w3.org/TR/CSS22/zindex.html#painting-order
      if (const NGPhysicalBoxFragment* fragment =
              cursor.Current().BoxFragment()) {
        if (UNLIKELY(fragment->IsOpaque()))
          continue;
      }
      yield(cursor.Current().RectInContainerFragment());
    }
  } else {
    DCHECK(!IsInLayoutNGInlineFormattingContext());
    CollectCulledLineBoxRects(yield);
  }

  if (intersected) {
    UpdateHitTestResult(result, adjusted_location.Point());
    if (result.AddNodeToListBasedTestResult(GetNode(), adjusted_location,
                                            region_result) == kStopHitTesting)
      return true;
  }
  return false;
}

PositionWithAffinity LayoutInline::PositionForPoint(
    const PhysicalOffset& point) const {
  NOT_DESTROYED();
  // FIXME: Does not deal with relative positioned inlines (should it?)

  if (const LayoutBlockFlow* ng_block_flow = FragmentItemsContainer())
    return ng_block_flow->PositionForPoint(point);

  DCHECK(CanUseInlineBox(*this));

  if (FirstLineBoxIncludingCulling()) {
    // This inline actually has a line box.  We must have clicked in the
    // border/padding of one of these boxes.  We
    // should try to find a result by asking our containing block.
    return ContainingBlock()->PositionForPoint(point);
  }

  return LayoutBoxModelObject::PositionForPoint(point);
}

PhysicalRect LayoutInline::PhysicalLinesBoundingBox() const {
  NOT_DESTROYED();

  if (IsInLayoutNGInlineFormattingContext()) {
    NGInlineCursor cursor;
    cursor.MoveToIncludingCulledInline(*this);
    PhysicalRect bounding_box;
    for (; cursor; cursor.MoveToNextForSameLayoutObject())
      bounding_box.UniteIfNonZero(cursor.Current().RectInContainerFragment());
    return bounding_box;
  }

  if (!AlwaysCreateLineBoxes()) {
    DCHECK(!FirstLineBox());
    PhysicalRect bounding_box;
    CollectLineBoxRects([&bounding_box](const PhysicalRect& rect) {
      bounding_box.UniteIfNonZero(rect);
    });
    return bounding_box;
  }

  LayoutRect result;

  // See <rdar://problem/5289721>, for an unknown reason the linked list here is
  // sometimes inconsistent, first is non-zero and last is zero.  We have been
  // unable to reproduce this at all (and consequently unable to figure ot why
  // this is happening).  The assert will hopefully catch the problem in debug
  // builds and help us someday figure out why.  We also put in a redundant
  // check of lastLineBox() to avoid the crash for now.
  DCHECK_EQ(!FirstLineBox(),
            !LastLineBox());  // Either both are null or both exist.
  if (FirstLineBox() && LastLineBox()) {
    // Return the width of the minimal left side and the maximal right side.
    LayoutUnit logical_left_side;
    LayoutUnit logical_right_side;
    for (InlineFlowBox* curr : *LineBoxes()) {
      if (curr == FirstLineBox() || curr->LogicalLeft() < logical_left_side)
        logical_left_side = curr->LogicalLeft();
      if (curr == FirstLineBox() || curr->LogicalRight() > logical_right_side)
        logical_right_side = curr->LogicalRight();
    }

    bool is_horizontal = StyleRef().IsHorizontalWritingMode();

    LayoutUnit x = is_horizontal ? logical_left_side : FirstLineBox()->X();
    LayoutUnit y = is_horizontal ? FirstLineBox()->Y() : logical_left_side;
    LayoutUnit width = is_horizontal ? logical_right_side - logical_left_side
                                     : LastLineBox()->LogicalBottom() - x;
    LayoutUnit height = is_horizontal ? LastLineBox()->LogicalBottom() - y
                                      : logical_right_side - logical_left_side;
    result = LayoutRect(x, y, width, height);
  }

  return FlipForWritingMode(result);
}

InlineBox* LayoutInline::CulledInlineFirstLineBox() const {
  NOT_DESTROYED();
  for (LayoutObject* curr = FirstChild(); curr; curr = curr->NextSibling()) {
    if (curr->IsFloatingOrOutOfFlowPositioned())
      continue;

    // We want to get the margin box in the inline direction, and then use our
    // font ascent/descent in the block direction (aligned to the root box's
    // baseline).
    if (curr->IsBox())
      return To<LayoutBox>(curr)->InlineBoxWrapper();
    if (curr->IsLayoutInline()) {
      auto* curr_inline = To<LayoutInline>(curr);
      InlineBox* result = curr_inline->FirstLineBoxIncludingCulling();
      if (result)
        return result;
    } else if (curr->IsText()) {
      auto* curr_text = To<LayoutText>(curr);
      if (curr_text->FirstTextBox())
        return curr_text->FirstTextBox();
    }
  }
  return nullptr;
}

InlineBox* LayoutInline::CulledInlineLastLineBox() const {
  NOT_DESTROYED();
  for (LayoutObject* curr = LastChild(); curr; curr = curr->PreviousSibling()) {
    if (curr->IsFloatingOrOutOfFlowPositioned())
      continue;

    // We want to get the margin box in the inline direction, and then use our
    // font ascent/descent in the block direction (aligned to the root box's
    // baseline).
    if (curr->IsBox())
      return To<LayoutBox>(curr)->InlineBoxWrapper();
    if (curr->IsLayoutInline()) {
      auto* curr_inline = To<LayoutInline>(curr);
      InlineBox* result = curr_inline->LastLineBoxIncludingCulling();
      if (result)
        return result;
    } else if (curr->IsText()) {
      auto* curr_text = To<LayoutText>(curr);
      if (curr_text->LastTextBox())
        return curr_text->LastTextBox();
    }
  }
  return nullptr;
}

PhysicalRect LayoutInline::CulledInlineVisualOverflowBoundingBox() const {
  NOT_DESTROYED();
  PhysicalRect result;
  CollectCulledLineBoxRects(
      [&result](const PhysicalRect& r) { result.UniteIfNonZero(r); });
  if (!FirstChild())
    return result;

  bool is_horizontal = StyleRef().IsHorizontalWritingMode();
  const LayoutBlock* block_for_flipping =
      UNLIKELY(HasFlippedBlocksWritingMode()) ? ContainingBlock() : nullptr;
  for (LayoutObject* curr = FirstChild(); curr; curr = curr->NextSibling()) {
    if (curr->IsFloatingOrOutOfFlowPositioned())
      continue;

    // For overflow we just have to propagate by hand and recompute it all.
    if (curr->IsBox()) {
      auto* curr_box = To<LayoutBox>(curr);
      if (!curr_box->HasSelfPaintingLayer() && curr_box->InlineBoxWrapper()) {
        LayoutRect logical_rect =
            curr_box->LogicalVisualOverflowRectForPropagation();
        if (is_horizontal) {
          logical_rect.MoveBy(curr_box->Location());
          result.UniteIfNonZero(PhysicalRect(logical_rect));
        } else {
          logical_rect.MoveBy(curr_box->Location());
          result.UniteIfNonZero(FlipForWritingMode(
              logical_rect.TransposedRect(), block_for_flipping));
        }
      }
    } else if (curr->IsLayoutInline()) {
      // If the child doesn't need line boxes either, then we can recur.
      auto* curr_inline = To<LayoutInline>(curr);
      if (!curr_inline->AlwaysCreateLineBoxes()) {
        result.UniteIfNonZero(
            curr_inline->CulledInlineVisualOverflowBoundingBox());
      } else if (!curr_inline->HasSelfPaintingLayer()) {
        result.UniteIfNonZero(curr_inline->PhysicalVisualOverflowRect());
      }
    } else if (curr->IsText()) {
      auto* curr_text = To<LayoutText>(curr);
      result.UniteIfNonZero(curr_text->PhysicalVisualOverflowRect());
    }
  }
  return result;
}

PhysicalRect LayoutInline::LinesVisualOverflowBoundingBox() const {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext()) {
    PhysicalRect result;
    NGInlineCursor cursor;
    cursor.MoveToIncludingCulledInline(*this);
    for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
      PhysicalRect child_rect = cursor.Current().InkOverflow();
      child_rect.offset += cursor.Current().OffsetInContainerFragment();
      result.Unite(child_rect);
    }
    return result;
  }

  if (!AlwaysCreateLineBoxes())
    return CulledInlineVisualOverflowBoundingBox();

  if (!FirstLineBox() || !LastLineBox())
    return PhysicalRect();

  // Return the width of the minimal left side and the maximal right side.
  LayoutUnit logical_left_side = LayoutUnit::Max();
  LayoutUnit logical_right_side = LayoutUnit::Min();
  for (InlineFlowBox* curr : *LineBoxes()) {
    logical_left_side =
        std::min(logical_left_side, curr->LogicalLeftVisualOverflow());
    logical_right_side =
        std::max(logical_right_side, curr->LogicalRightVisualOverflow());
  }

  RootInlineBox& first_root_box = FirstLineBox()->Root();
  RootInlineBox& last_root_box = LastLineBox()->Root();

  LayoutUnit logical_top =
      FirstLineBox()->LogicalTopVisualOverflow(first_root_box.LineTop());
  LayoutUnit logical_width = logical_right_side - logical_left_side;
  LayoutUnit logical_height =
      LastLineBox()->LogicalBottomVisualOverflow(last_root_box.LineBottom()) -
      logical_top;

  LayoutRect rect(logical_left_side, logical_top, logical_width,
                  logical_height);
  if (!StyleRef().IsHorizontalWritingMode())
    rect = rect.TransposedRect();
  return FlipForWritingMode(rect);
}

PhysicalRect LayoutInline::VisualRectInDocument(VisualRectFlags flags) const {
  NOT_DESTROYED();
  PhysicalRect rect = PhysicalVisualOverflowRect();
  MapToVisualRectInAncestorSpace(View(), rect, flags);
  return rect;
}

PhysicalRect LayoutInline::LocalVisualRectIgnoringVisibility() const {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext()) {
    return NGFragmentItem::LocalVisualRectFor(*this);
  }

  // If we don't create line boxes, we don't have any invalidations to do.
  if (!AlwaysCreateLineBoxes())
    return PhysicalRect();

  // VisualOverflowRect() is in "physical coordinates with flipped blocks
  // direction", while all "VisualRect"s are in pure physical coordinates.
  return PhysicalVisualOverflowRect();
}

PhysicalRect LayoutInline::PhysicalVisualOverflowRect() const {
  NOT_DESTROYED();
  PhysicalRect overflow_rect = LinesVisualOverflowBoundingBox();
  const ComputedStyle& style = StyleRef();
  LayoutUnit outline_outset(OutlinePainter::OutlineOutsetExtent(
      style, OutlineInfo::GetFromStyle(style)));
  if (outline_outset) {
    Vector<PhysicalRect> rects;
    if (GetDocument().InNoQuirksMode()) {
      // We have already included outline extents of line boxes in
      // linesVisualOverflowBoundingBox(), so the following just add outline
      // rects for children and continuations.
      AddOutlineRectsForNormalChildren(
          rects, PhysicalOffset(),
          style.OutlineRectsShouldIncludeBlockVisualOverflow());
    } else {
      // In non-standard mode, because the difference in
      // LayoutBlock::minLineHeightForReplacedObject(),
      // linesVisualOverflowBoundingBox() may not cover outline rects of lines
      // containing replaced objects.
      AddOutlineRects(rects, nullptr, PhysicalOffset(),
                      style.OutlineRectsShouldIncludeBlockVisualOverflow());
    }
    if (!rects.empty()) {
      PhysicalRect outline_rect = UnionRect(rects);
      outline_rect.Inflate(outline_outset);
      overflow_rect.Unite(outline_rect);
    }
  }
  // TODO(rendering-core): Add in Text Decoration overflow rect.
  return overflow_rect;
}

PhysicalRect LayoutInline::ReferenceBoxForClipPath() const {
  NOT_DESTROYED();
  // The spec just says to use the border box as clip-path reference box. It
  // doesn't say what to do if there are multiple lines. Gecko uses the first
  // fragment in that case. We'll do the same here (but correctly with respect
  // to writing-mode - Gecko has some issues there).
  // See crbug.com/641907
  if (IsInLayoutNGInlineFormattingContext()) {
    NGInlineCursor cursor;
    cursor.MoveTo(*this);
    if (cursor)
      return cursor.Current().RectInContainerFragment();
  }
  if (const InlineFlowBox* flow_box = FirstLineBox())
    return FlipForWritingMode(flow_box->FrameRect());
  return PhysicalRect();
}

bool LayoutInline::MapToVisualRectInAncestorSpaceInternal(
    const LayoutBoxModelObject* ancestor,
    TransformState& transform_state,
    VisualRectFlags visual_rect_flags) const {
  NOT_DESTROYED();
  if (ancestor == this)
    return true;

  LayoutObject* container = Container();
  DCHECK_EQ(container, Parent());
  if (!container)
    return true;

  bool preserve3d = container->StyleRef().Preserves3D();

  TransformState::TransformAccumulation accumulation =
      preserve3d ? TransformState::kAccumulateTransform
                 : TransformState::kFlattenTransform;

  if (StyleRef().HasInFlowPosition() && Layer()) {
    // Apply the in-flow position offset when invalidating a rectangle. The
    // layer is translated, but the layout box isn't, so we need to do this to
    // get the right dirty rect. Since this is called from LayoutObject::
    // setStyle, the relative position flag on the LayoutObject has been
    // cleared, so use the one on the style().
    transform_state.Move(Layer()->GetLayoutObject().OffsetForInFlowPosition(),
                         accumulation);
  }

  LayoutBox* container_box = DynamicTo<LayoutBox>(container);
  if (container_box && container != ancestor &&
      !container_box->MapContentsRectToBoxSpace(transform_state, accumulation,
                                                *this, visual_rect_flags))
    return false;

  return container->MapToVisualRectInAncestorSpaceInternal(
      ancestor, transform_state, visual_rect_flags);
}

PhysicalOffset LayoutInline::OffsetFromContainerInternal(
    const LayoutObject* container,
    bool ignore_scroll_offset) const {
  NOT_DESTROYED();
  DCHECK_EQ(container, Container());

  PhysicalOffset offset;
  if (IsInFlowPositioned())
    offset += OffsetForInFlowPosition();

  if (container->IsScrollContainer())
    offset += OffsetFromScrollableContainer(container, ignore_scroll_offset);

  return offset;
}

PaintLayerType LayoutInline::LayerTypeRequired() const {
  NOT_DESTROYED();
  return IsInFlowPositioned() || CreatesGroup() ||
                 StyleRef().ShouldCompositeForCurrentAnimations() ||
                 ShouldApplyPaintContainment()
             ? kNormalPaintLayer
             : kNoPaintLayer;
}

void LayoutInline::ChildBecameNonInline(LayoutObject* child) {
  NOT_DESTROYED();
  DCHECK(!child->IsInline());
  // Following tests reach here.
  //  * external/wpt/css/CSS2/positioning/toogle-abspos-on-relpos-inline-child.html
  //  * fast/block/float/float-originating-line-deleted-crash.html
  //  * paint/stacking/layer-stacking-change-under-inline.html
  auto* const anonymous_box = CreateAnonymousContainerForBlockChildren();
  LayoutBoxModelObject::AddChild(anonymous_box, child);
  Children()->RemoveChildNode(this, child);
  anonymous_box->AddChild(child);
}

void LayoutInline::UpdateHitTestResult(HitTestResult& result,
                                       const PhysicalOffset& point) const {
  NOT_DESTROYED();
  if (result.InnerNode())
    return;

  PhysicalOffset local_point = point;
  if (Node* n = GetNode()) {
    result.SetNodeAndPosition(n, local_point);
  }
}

void LayoutInline::DirtyLineBoxes(bool full_layout) {
  NOT_DESTROYED();
  if (full_layout) {
    DeleteLineBoxes();
    return;
  }

  if (!AlwaysCreateLineBoxes()) {
    // We have to grovel into our children in order to dirty the appropriate
    // lines.
    for (LayoutObject* curr = FirstChild(); curr; curr = curr->NextSibling()) {
      if (curr->IsFloatingOrOutOfFlowPositioned())
        continue;
      if (curr->IsBox() && !curr->NeedsLayout()) {
        auto* curr_box = To<LayoutBox>(curr);
        if (curr_box->InlineBoxWrapper())
          curr_box->InlineBoxWrapper()->Root().MarkDirty();
      } else if (!curr->SelfNeedsLayout()) {
        if (curr->IsLayoutInline()) {
          auto* curr_inline = To<LayoutInline>(curr);
          for (InlineFlowBox* child_line : *curr_inline->LineBoxes())
            child_line->Root().MarkDirty();
        } else if (curr->IsText()) {
          auto* curr_text = To<LayoutText>(curr);
          for (InlineTextBox* child_text : curr_text->TextBoxes())
            child_text->Root().MarkDirty();
        }
      }
    }
  } else {
    MutableLineBoxes()->DirtyLineBoxes();
  }
}

InlineFlowBox* LayoutInline::CreateInlineFlowBox() {
  NOT_DESTROYED();
  return MakeGarbageCollected<InlineFlowBox>(LineLayoutItem(this));
}

InlineFlowBox* LayoutInline::CreateAndAppendInlineFlowBox() {
  NOT_DESTROYED();
  SetAlwaysCreateLineBoxes();
  InlineFlowBox* flow_box = CreateInlineFlowBox();
  MutableLineBoxes()->AppendLineBox(flow_box);
  return flow_box;
}

void LayoutInline::DirtyLinesFromChangedChild(
    LayoutObject* child,
    MarkingBehavior marking_behavior) {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext()) {
    if (const LayoutBlockFlow* container = FragmentItemsContainer())
      NGFragmentItems::DirtyLinesFromChangedChild(*child, *container);
    return;
  }
  MutableLineBoxes()->DirtyLinesFromChangedChild(
      LineLayoutItem(this), LineLayoutItem(child),
      marking_behavior == kMarkContainerChain);
}

LayoutUnit LayoutInline::LineHeight(
    bool first_line,
    LineDirectionMode /*direction*/,
    LinePositionMode /*linePositionMode*/) const {
  if (first_line && GetDocument().GetStyleEngine().UsesFirstLineRules()) {
    const ComputedStyle* s = Style(first_line);
    if (s != Style())
      return LayoutUnit(s->ComputedLineHeight());
  }

  return LayoutUnit(StyleRef().ComputedLineHeight());
}

LayoutUnit LayoutInline::BaselinePosition(
    FontBaseline baseline_type,
    bool first_line,
    LineDirectionMode direction,
    LinePositionMode line_position_mode) const {
  NOT_DESTROYED();
  DCHECK_EQ(line_position_mode, kPositionOnContainingLine);
  const SimpleFontData* font_data = Style(first_line)->GetFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return LayoutUnit(-1);
  const FontMetrics& font_metrics = font_data->GetFontMetrics();
  return LayoutUnit((font_metrics.Ascent(baseline_type) +
                     (LineHeight(first_line, direction, line_position_mode) -
                      font_metrics.Height()) /
                         2)
                        .ToInt());
}

PhysicalOffset LayoutInline::OffsetForInFlowPositionedInline(
    const LayoutBox& child) const {
  NOT_DESTROYED();
  // TODO(layout-dev): This function isn't right with mixed writing modes,
  // but LayoutNG has fixed the issue. This function seems to always return
  // zero in LayoutNG. We should probably remove this function for LayoutNG.

  DCHECK(IsInFlowPositioned() || StyleRef().HasNonInitialFilter() ||
         StyleRef().HasNonInitialBackdropFilter());
  if (!IsInFlowPositioned() && !StyleRef().HasNonInitialFilter() &&
      !StyleRef().HasNonInitialBackdropFilter()) {
    DCHECK(CreatesGroup())
        << "Inlines with filters or backdrop-filters should create a group";
    return PhysicalOffset();
  }

  // When we have an enclosing relpositioned inline, we need to add in the
  // offset of the first line box from the rest of the content, but only in the
  // cases where we know we're positioned relative to the inline itself.

  LayoutSize logical_offset;
  LayoutUnit inline_position;
  LayoutUnit block_position;
  if (FirstLineBox()) {
    inline_position = FirstLineBox()->LogicalLeft();
    block_position = FirstLineBox()->LogicalTop();
  } else {
    DCHECK(Layer());
    inline_position = Layer()->StaticInlinePosition();
    block_position = Layer()->StaticBlockPosition();
  }

  // Per http://www.w3.org/TR/CSS2/visudet.html#abs-non-replaced-width an
  // absolute positioned box with a static position should locate itself as
  // though it is a normal flow box in relation to its containing block.
  if (!child.StyleRef().HasStaticInlinePosition(
          StyleRef().IsHorizontalWritingMode()))
    logical_offset.SetWidth(inline_position);

  if (!child.StyleRef().HasStaticBlockPosition(
          StyleRef().IsHorizontalWritingMode()))
    logical_offset.SetHeight(block_position);

  return PhysicalOffset(StyleRef().IsHorizontalWritingMode()
                            ? logical_offset
                            : logical_offset.TransposedSize());
}

void LayoutInline::ImageChanged(WrappedImagePtr, CanDeferInvalidation) {
  NOT_DESTROYED();
  if (!Parent())
    return;

  SetShouldDoFullPaintInvalidationWithoutLayoutChange(
      PaintInvalidationReason::kImage);
}

void LayoutInline::AddOutlineRects(
    Vector<PhysicalRect>& rects,
    OutlineInfo* info,
    const PhysicalOffset& additional_offset,
    NGOutlineType include_block_overflows) const {
  NOT_DESTROYED();
#if DCHECK_IS_ON()
  // TODO(crbug.com/987836): enable this DCHECK universally.
  Page* page = GetDocument().GetPage();
  if (page && !page->GetSettings().GetSpatialNavigationEnabled()) {
    DCHECK_GE(GetDocument().Lifecycle().GetState(),
              DocumentLifecycle::kAfterPerformLayout);
  }
#endif  // DCHECK_IS_ON()

  CollectLineBoxRects([&rects, &additional_offset](const PhysicalRect& r) {
    auto rect = r;
    rect.Move(additional_offset);
    rects.push_back(rect);
  });
  AddOutlineRectsForNormalChildren(rects, additional_offset,
                                   include_block_overflows);
  if (info) {
    *info = OutlineInfo::GetFromStyle(StyleRef());
  }
}

gfx::RectF LayoutInline::LocalBoundingBoxRectForAccessibility() const {
  NOT_DESTROYED();
  Vector<PhysicalRect> rects = OutlineRects(
      nullptr, PhysicalOffset(), NGOutlineType::kIncludeBlockVisualOverflow);
  return gfx::RectF(FlipForWritingMode(UnionRect(rects).ToLayoutRect()));
}

void LayoutInline::AddAnnotatedRegions(Vector<AnnotatedRegionValue>& regions) {
  NOT_DESTROYED();
  // Convert the style regions to absolute coordinates.
  if (StyleRef().Visibility() != EVisibility::kVisible)
    return;

  if (StyleRef().DraggableRegionMode() == EDraggableRegionMode::kNone)
    return;

  AnnotatedRegionValue region;
  region.draggable =
      StyleRef().DraggableRegionMode() == EDraggableRegionMode::kDrag;
  region.bounds = PhysicalLinesBoundingBox();
  // TODO(crbug.com/966048): We probably want to also cover continuations.

  LayoutObject* container = ContainingBlock();
  if (!container)
    container = this;

  // TODO(crbug.com/966048): The kIgnoreTransforms seems incorrect. We probably
  // want to map visual rect (with clips applied).
  region.bounds.offset +=
      container->LocalToAbsolutePoint(PhysicalOffset(), kIgnoreTransforms);
  regions.push_back(region);
}

void LayoutInline::InvalidateDisplayItemClients(
    PaintInvalidationReason invalidation_reason) const {
  NOT_DESTROYED();
  ObjectPaintInvalidator paint_invalidator(*this);

  if (IsInLayoutNGInlineFormattingContext()) {
    if (!ShouldCreateBoxFragment())
      return;
#if DCHECK_IS_ON()
    NGInlineCursor cursor;
    for (cursor.MoveTo(*this); cursor; cursor.MoveToNextForSameLayoutObject())
      DCHECK_EQ(cursor.Current().GetDisplayItemClient(), this);
#endif
    paint_invalidator.InvalidateDisplayItemClient(*this, invalidation_reason);
    return;
  }

  paint_invalidator.InvalidateDisplayItemClient(*this, invalidation_reason);

  for (InlineFlowBox* box : *LineBoxes())
    paint_invalidator.InvalidateDisplayItemClient(*box, invalidation_reason);
}

PhysicalRect LayoutInline::DebugRect() const {
  NOT_DESTROYED();
  return PhysicalRect(ToEnclosingRect(PhysicalLinesBoundingBox()));
}

}  // namespace blink

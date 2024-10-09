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
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/outline_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
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

bool IsInChildRubyText(const LayoutInline& start_object,
                       const LayoutObject* target) {
  if (!target || !start_object.IsInlineRuby() || &start_object == target) {
    return false;
  }
  const LayoutObject* start_child = target;
  while (start_child->Parent() != &start_object) {
    start_child = start_child->Parent();
  }
  return start_child->IsInlineRubyText();
}

}  // anonymous namespace

struct SameSizeAsLayoutInline : public LayoutBoxModelObject {
  ~SameSizeAsLayoutInline() override = default;
  LayoutObjectChildList children_;
  wtf_size_t first_fragment_item_index_;
};

ASSERT_SIZE(LayoutInline, SameSizeAsLayoutInline);

LayoutInline::LayoutInline(Element* element) : LayoutBoxModelObject(element) {
  SetChildrenInline(true);
}

void LayoutInline::Trace(Visitor* visitor) const {
  visitor->Trace(children_);
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
    if (Parent()) {
      Parent()->DirtyLinesFromChangedChild(this);
    }
    if (FirstInlineFragmentItemIndex()) {
      FragmentItems::LayoutObjectWillBeDestroyed(*this);
      ClearFirstInlineFragmentItemIndex();
    }
  }

  LayoutBoxModelObject::WillBeDestroyed();
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
  return first_fragment_item_index_;
}

void LayoutInline::InLayoutNGInlineFormattingContextWillChange(bool new_value) {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext())
    ClearFirstInlineFragmentItemIndex();
}

void LayoutInline::UpdateFromStyle() {
  NOT_DESTROYED();
  LayoutBoxModelObject::UpdateFromStyle();

  // This is needed (at a minimum) for LayoutSVGInline, which (including
  // subclasses) is constructed for svg:a, svg:textPath, and svg:tspan,
  // regardless of CSS 'display'.
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

bool LayoutInline::ComputeInitialShouldCreateBoxFragment(
    const ComputedStyle& style) const {
  NOT_DESTROYED();

  // We'd like to use ScopedSVGPaintState in
  // InlineBoxFragmentPainter::Paint().
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
         HasPaintedOutline(style, GetNode()) ||
         CanBeHitTestTargetPseudoNodeStyle(style);
}

bool LayoutInline::ComputeInitialShouldCreateBoxFragment() const {
  NOT_DESTROYED();
  const ComputedStyle& style = StyleRef();
  if (HasSelfPaintingLayer() || ComputeInitialShouldCreateBoxFragment(style) ||
      ShouldApplyPaintContainment() || ShouldApplyLayoutContainment())
    return true;

  const ComputedStyle& first_line_style = FirstLineStyleRef();
  if (&style != &first_line_style &&
      ComputeInitialShouldCreateBoxFragment(first_line_style)) [[unlikely]] {
    return true;
  }

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

PhysicalRect LayoutInline::LocalCaretRect(int) const {
  NOT_DESTROYED();
  if (FirstChild()) {
    // This condition is possible if the LayoutInline is at an editing boundary,
    // i.e. the VisiblePosition is:
    //   <LayoutInline editingBoundary=true>|<LayoutText>
    //   </LayoutText></LayoutInline>
    // FIXME: need to figure out how to make this return a valid rect, note that
    // there are no line boxes created in the above case.
    return PhysicalRect();
  }

  LogicalRect logical_caret_rect =
      LocalCaretRectForEmptyElement(BorderAndPaddingInlineSize(), LayoutUnit());

  if (IsInLayoutNGInlineFormattingContext()) {
    InlineCursor cursor;
    cursor.MoveTo(*this);
    if (cursor) {
      PhysicalRect caret_rect =
          WritingModeConverter(
              {StyleRef().GetWritingMode(), TextDirection::kLtr},
              cursor.CurrentItem()->Size())
              .ToPhysical(logical_caret_rect);
      caret_rect.Move(cursor.Current().OffsetInContainerFragment());
      return caret_rect;
    }
  }

  return PhysicalRect(logical_caret_rect.offset.inline_offset,
                      logical_caret_rect.offset.block_offset,
                      logical_caret_rect.size.inline_size,
                      logical_caret_rect.size.block_size);
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

void LayoutInline::BlockInInlineBecameFloatingOrOutOfFlow(
    LayoutBlockFlow* anonymous_block_child) {
  NOT_DESTROYED();
  // Look for in-flow children. Any in-flow child will prevent the wrapper from
  // being deleted.
  for (const LayoutObject* grandchild = anonymous_block_child->FirstChild();
       grandchild; grandchild = grandchild->NextSibling()) {
    if (!grandchild->IsFloating() && !grandchild->IsOutOfFlowPositioned()) {
      return;
    }
  }
  // There are no longer any in-flow children inside the anonymous block wrapper
  // child. Get rid of it.
  anonymous_block_child->MoveAllChildrenTo(this, anonymous_block_child);
  anonymous_block_child->Destroy();
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

  return LayoutBlockFlow::CreateAnonymous(&GetDocument(),
                                          new_style_builder.TakeStyle());
}

LayoutBox* LayoutInline::CreateAnonymousBoxToSplit(
    const LayoutBox* box_to_split) const {
  NOT_DESTROYED();
  DCHECK(box_to_split->IsBlockInInline());
  DCHECK(IsA<LayoutBlockFlow>(box_to_split));
  return CreateAnonymousContainerForBlockChildren();
}

void LayoutInline::Paint(const PaintInfo& paint_info) const {
  NOT_DESTROYED();
  NOTREACHED_IN_MIGRATION();
}

template <typename PhysicalRectCollector>
void LayoutInline::CollectLineBoxRects(
    const PhysicalRectCollector& yield) const {
  NOT_DESTROYED();
  if (!IsInLayoutNGInlineFormattingContext()) {
    // InlineCursor::MoveToIncludingCulledInline() below would fail DCHECKs in
    // this situation, so just bail. This is most likely not a good situation to
    // be in, though. See crbug.com/1448357
    return;
  }
  InlineCursor cursor;
  cursor.MoveToIncludingCulledInline(*this);
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    if (!IsInChildRubyText(*this, cursor.Current().GetLayoutObject())) {
      yield(cursor.CurrentRectInBlockFlow());
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

void LayoutInline::QuadsInAncestorInternal(Vector<gfx::QuadF>& quads,
                                           const LayoutBoxModelObject* ancestor,
                                           MapCoordinatesFlags mode) const {
  QuadsForSelfInternal(quads, ancestor, mode, true);
}

void LayoutInline::QuadsForSelfInternal(Vector<gfx::QuadF>& quads,
                                        const LayoutBoxModelObject* ancestor,
                                        MapCoordinatesFlags mode,
                                        bool map_to_ancestor) const {
  NOT_DESTROYED();
  std::optional<gfx::Transform> mapping_to_ancestor;
  // Set to true if the transform to absolute space depends on the point
  // being mapped (in which case we can't use LocalToAncestorTransform).
  bool transform_depends_on_point = false;
  bool transform_depends_on_point_computed = false;
  auto PushAncestorQuad = [&transform_depends_on_point,
                           &transform_depends_on_point_computed,
                           &mapping_to_ancestor, &quads, ancestor, mode,
                           this](const PhysicalRect& rect) {
    if (!transform_depends_on_point_computed) {
      transform_depends_on_point_computed = true;
      transform_depends_on_point = AbsoluteTransformDependsOnPoint(*this);
      if (!transform_depends_on_point)
        mapping_to_ancestor.emplace(LocalToAncestorTransform(ancestor, mode));
    }
    if (transform_depends_on_point) {
      quads.push_back(
          LocalToAncestorQuad(gfx::QuadF(gfx::RectF(rect)), ancestor, mode));
    } else {
      quads.push_back(
          mapping_to_ancestor->MapQuad(gfx::QuadF(gfx::RectF(rect))));
    }
  };

  CollectLineBoxRects(
      [&PushAncestorQuad, &map_to_ancestor, &quads](const PhysicalRect& rect) {
        if (map_to_ancestor) {
          PushAncestorQuad(rect);
        } else {
          quads.push_back(gfx::QuadF(gfx::RectF(rect)));
        }
      });
  if (quads.empty()) {
    if (map_to_ancestor) {
      PushAncestorQuad(PhysicalRect());
    } else {
      quads.push_back(gfx::QuadF());
    }
  }
}

std::optional<PhysicalOffset> LayoutInline::FirstLineBoxTopLeftInternal()
    const {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext()) {
    InlineCursor cursor;
    cursor.MoveToIncludingCulledInline(*this);
    if (!cursor)
      return std::nullopt;
    return cursor.CurrentOffsetInBlockFlow();
  }
  return std::nullopt;
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
      nullptr, PhysicalOffset(), OutlineType::kIncludeBlockInkOverflow);
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
  if (margin.IsPercent() || margin.IsCalculated()) {
    return MinimumValueForLength(
        margin,
        std::max(LayoutUnit(),
                 layout_object->ContainingBlock()->AvailableLogicalWidth()));
  }
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
    if (NeedsLayout()) [[unlikely]] {
      DUMP_WILL_BE_NOTREACHED();
      return false;
    }

    // In LayoutNG, we reach here only when called from
    // PaintLayer::HitTestContents() without going through any ancestor, in
    // which case the element must have self painting layer.
    DCHECK(HasSelfPaintingLayer());
    InlineCursor cursor;
    cursor.MoveTo(*this);
    if (!cursor)
      return false;
    int target_fragment_idx = hit_test_location.FragmentIndex();
    // Fragment traversal requires a target fragment to be specified,
    // unless there's only one.
    DCHECK(!CanTraversePhysicalFragments() || target_fragment_idx >= 0 ||
           !IsFragmented());
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
      const FragmentItem& item = *cursor.Current().Item();
      const PhysicalBoxFragment* box_fragment = item.BoxFragment();
      DCHECK(box_fragment);
      // BoxFragmentPainter::NodeAtPoint() takes an offset that is accumulated
      // up to the fragment itself. Compute this offset.
      const PhysicalOffset child_offset =
          accumulated_offset + item.OffsetInContainerFragment();
      InlinePaintContext inline_context;
      if (BoxFragmentPainter(cursor, item, *box_fragment, &inline_context)
              .NodeAtPoint(result, hit_test_location, child_offset,
                           accumulated_offset, phase)) {
        return true;
      }
    }
    return false;
  }

  NOTREACHED();
}

bool LayoutInline::HitTestCulledInline(HitTestResult& result,
                                       const HitTestLocation& hit_test_location,
                                       const PhysicalOffset& accumulated_offset,
                                       const InlineCursor& parent_cursor) {
  NOT_DESTROYED();
  if (!VisibleToHitTestRequest(result.GetHitTestRequest()))
    return false;

  HitTestLocation adjusted_location(hit_test_location, -accumulated_offset);
  cc::Region region_result;
  bool intersected = false;

  // NG generates purely physical rectangles here.

  // Iterate fragments for |this|, including culled inline, but only that are
  // descendants of |parent_cursor|.
  DCHECK(IsDescendantOf(parent_cursor.GetLayoutBlockFlow()));
  InlineCursor cursor(parent_cursor);
  cursor.MoveToIncludingCulledInline(*this);
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    // Block-in-inline is inline in the box tree, and may appear as a child of
    // a culled inline, but it should be painted and hit-tested as block
    // painting-order-wise. Don't include it as part of the culled inline
    // region. https://www.w3.org/TR/CSS22/zindex.html#painting-order
    if (const auto* fragment = cursor.Current().BoxFragment()) {
      if (fragment->IsOpaque()) [[unlikely]] {
        continue;
      }
    }
    PhysicalRect rect = cursor.Current().RectInContainerFragment();
    if (adjusted_location.Intersects(rect)) {
      intersected = true;
      region_result.Union(ToEnclosingRect(rect));
    }
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

  return LayoutBoxModelObject::PositionForPoint(point);
}

PhysicalRect LayoutInline::PhysicalLinesBoundingBox() const {
  NOT_DESTROYED();

  if (IsInLayoutNGInlineFormattingContext()) {
    InlineCursor cursor;
    cursor.MoveToIncludingCulledInline(*this);
    PhysicalRect bounding_box;
    for (; cursor; cursor.MoveToNextForSameLayoutObject())
      bounding_box.UniteIfNonZero(cursor.Current().RectInContainerFragment());
    return bounding_box;
  }
  return PhysicalRect();
}

PhysicalRect LayoutInline::LinesVisualOverflowBoundingBox() const {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext()) {
    PhysicalRect result;
    InlineCursor cursor;
    cursor.MoveToIncludingCulledInline(*this);
    for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
      PhysicalRect child_rect = cursor.Current().InkOverflowRect();
      child_rect.offset += cursor.Current().OffsetInContainerFragment();
      result.Unite(child_rect);
    }
    return result;
  }
  return PhysicalRect();
}

PhysicalRect LayoutInline::VisualOverflowRect() const {
  NOT_DESTROYED();
  PhysicalRect overflow_rect = LinesVisualOverflowBoundingBox();
  const ComputedStyle& style = StyleRef();
  LayoutUnit outline_outset(OutlinePainter::OutlineOutsetExtent(
      style, OutlineInfo::GetFromStyle(style)));
  if (outline_outset) {
    UnionOutlineRectCollector collector;
    if (GetDocument().InNoQuirksMode()) {
      // We have already included outline extents of line boxes in
      // linesVisualOverflowBoundingBox(), so the following just add outline
      // rects for children and continuations.
      AddOutlineRectsForNormalChildren(
          collector, PhysicalOffset(),
          style.OutlineRectsShouldIncludeBlockInkOverflow());
    } else {
      // In non-standard mode, because the difference in
      // LayoutBlock::minLineHeightForReplacedObject(),
      // linesVisualOverflowBoundingBox() may not cover outline rects of lines
      // containing replaced objects.
      AddOutlineRects(collector, nullptr, PhysicalOffset(),
                      style.OutlineRectsShouldIncludeBlockInkOverflow());
    }
    if (!collector.Rect().IsEmpty()) {
      PhysicalRect outline_rect = collector.Rect();
      outline_rect.Inflate(outline_outset);
      overflow_rect.Unite(outline_rect);
    }
  }
  // TODO(rendering-core): Add in Text Decoration overflow rect.
  return overflow_rect;
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

  if (IsStickyPositioned()) {
    transform_state.Move(StickyPositionOffset(), accumulation);
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
    MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  DCHECK_EQ(container, Container());

  PhysicalOffset offset;
  if (IsStickyPositioned() && !(mode & kIgnoreStickyOffset)) {
    offset += StickyPositionOffset();
  }

  if (container->IsScrollContainer()) {
    offset +=
        OffsetFromScrollableContainer(container, mode & kIgnoreScrollOffset);
  }

  return offset;
}

PaintLayerType LayoutInline::LayerTypeRequired() const {
  NOT_DESTROYED();
  return IsRelPositioned() || IsStickyPositioned() || CreatesGroup() ||
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

void LayoutInline::DirtyLinesFromChangedChild(LayoutObject* child) {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext()) {
    if (const LayoutBlockFlow* container = FragmentItemsContainer())
      FragmentItems::DirtyLinesFromChangedChild(*child, *container);
  }
}

LayoutUnit LayoutInline::FirstLineHeight() const {
  return LayoutUnit(FirstLineStyle()->ComputedLineHeight());
}

void LayoutInline::ImageChanged(WrappedImagePtr, CanDeferInvalidation) {
  NOT_DESTROYED();
  if (!Parent())
    return;

  SetShouldDoFullPaintInvalidationWithoutLayoutChange(
      PaintInvalidationReason::kImage);
}

void LayoutInline::AddOutlineRects(OutlineRectCollector& collector,
                                   OutlineInfo* info,
                                   const PhysicalOffset& additional_offset,
                                   OutlineType include_block_overflows) const {
  NOT_DESTROYED();
#if DCHECK_IS_ON()
  // TODO(crbug.com/987836): enable this DCHECK universally.
  Page* page = GetDocument().GetPage();
  if (page && !page->GetSettings().GetSpatialNavigationEnabled()) {
    DCHECK_GE(GetDocument().Lifecycle().GetState(),
              DocumentLifecycle::kAfterPerformLayout);
  }
#endif  // DCHECK_IS_ON()

  CollectLineBoxRects([&collector, &additional_offset](const PhysicalRect& r) {
    auto rect = r;
    rect.Move(additional_offset);
    collector.AddRect(rect);
  });
  AddOutlineRectsForNormalChildren(collector, additional_offset,
                                   include_block_overflows);
  if (info) {
    *info = OutlineInfo::GetFromStyle(StyleRef());
  }
}

gfx::RectF LayoutInline::LocalBoundingBoxRectF() const {
  NOT_DESTROYED();
  Vector<gfx::QuadF> quads;
  QuadsForSelfInternal(quads, /*ancestor=*/nullptr, 0, false);

  wtf_size_t n = quads.size();
  if (n == 0) {
    return gfx::RectF();
  }

  gfx::RectF result = quads[0].BoundingBox();
  for (wtf_size_t i = 1; i < n; ++i) {
    result.Union(quads[i].BoundingBox());
  }
  return result;
}

gfx::RectF LayoutInline::LocalBoundingBoxRectForAccessibility() const {
  NOT_DESTROYED();
  UnionOutlineRectCollector collector;
  AddOutlineRects(collector, nullptr, PhysicalOffset(),
                  OutlineType::kIncludeBlockInkOverflow);
  return gfx::RectF(collector.Rect());
}

void LayoutInline::AddDraggableRegions(Vector<DraggableRegionValue>& regions) {
  NOT_DESTROYED();
  // Convert the style regions to absolute coordinates.
  if (StyleRef().UsedVisibility() != EVisibility::kVisible) {
    return;
  }

  if (StyleRef().DraggableRegionMode() == EDraggableRegionMode::kNone)
    return;

  DraggableRegionValue region;
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
  LayoutBoxModelObject::InvalidateDisplayItemClients(invalidation_reason);

#if DCHECK_IS_ON()
  if (IsInLayoutNGInlineFormattingContext()) {
    InlineCursor cursor;
    for (cursor.MoveTo(*this); cursor; cursor.MoveToNextForSameLayoutObject()) {
      DCHECK_EQ(cursor.Current().GetDisplayItemClient(), this);
    }
  }
#endif
}

PhysicalRect LayoutInline::DebugRect() const {
  NOT_DESTROYED();
  return PhysicalRect(ToEnclosingRect(PhysicalLinesBoundingBox()));
}

}  // namespace blink

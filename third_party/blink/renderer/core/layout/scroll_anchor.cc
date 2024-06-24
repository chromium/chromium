// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/scroll_anchor.h"

#include <algorithm>
#include <memory>

#include "base/trace_event/typed_macros.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/nth_index_cache.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/bloom_filter.h"

namespace blink {

namespace {

bool IsNGBlockFragmentationRoot(const LayoutBlockFlow* block_flow) {
  return block_flow && block_flow->IsFragmentationContextRoot() &&
         block_flow->IsLayoutNGObject();
}

}  // anonymous namespace

// With 100 unique strings, a 2^12 slot table has a false positive rate of ~2%.
using ClassnameFilter = CountingBloomFilter<12>;
using Corner = ScrollAnchor::Corner;

ScrollAnchor::ScrollAnchor()
    : anchor_object_(nullptr),
      corner_(Corner::kTopLeft),
      scroll_anchor_disabling_style_changed_(false),
      queued_(false) {}

ScrollAnchor::ScrollAnchor(ScrollableArea* scroller) : ScrollAnchor() {
  SetScroller(scroller);
}

ScrollAnchor::~ScrollAnchor() = default;

void ScrollAnchor::Trace(Visitor* visitor) const {
  visitor->Trace(scroller_);
  visitor->Trace(anchor_object_);
}

void ScrollAnchor::SetScroller(ScrollableArea* scroller) {
  DCHECK_NE(scroller_, scroller);
  DCHECK(scroller);
  DCHECK(scroller->IsRootFrameViewport() ||
         scroller->IsPaintLayerScrollableArea());
  scroller_ = scroller;
  ClearSelf();
}

static LayoutBox* ScrollerLayoutBox(const ScrollableArea* scroller) {
  LayoutBox* box = scroller->GetLayoutBox();
  DCHECK(box);
  return box;
}

// TODO(skobes): Storing a "corner" doesn't make much sense anymore since we
// adjust only on the block flow axis.  This could probably be refactored to
// simply measure the movement of the block-start edge.
static Corner CornerToAnchor(const ScrollableArea* scroller) {
  const ComputedStyle* style = ScrollerLayoutBox(scroller)->Style();
  if (style->IsFlippedBlocksWritingMode())
    return Corner::kTopRight;
  return Corner::kTopLeft;
}

static PhysicalOffset CornerPointOfRect(const PhysicalRect& rect,
                                        Corner which_corner) {
  switch (which_corner) {
    case Corner::kTopLeft:
      return rect.MinXMinYCorner();
    case Corner::kTopRight:
      return rect.MaxXMinYCorner();
  }
  NOTREACHED_IN_MIGRATION();
  return PhysicalOffset();
}

// Bounds of the LayoutObject relative to the scroller's visible content rect.
static PhysicalRect RelativeBounds(const LayoutObject* layout_object,
                                   const ScrollableArea* scroller) {
  PhysicalRect local_bounds;
  if (const auto* box = DynamicTo<LayoutBox>(layout_object)) {
    local_bounds = box->PhysicalBorderBoxRect();
    // If we clip overflow then we can use the `PhysicalBorderBoxRect()`
    // as our bounds. If not, we expand the bounds by the scrollable overflow.
    if (!layout_object->ShouldClipOverflowAlongEitherAxis()) {
      // BorderBoxRect doesn't include overflow content and floats.
      LayoutUnit max_y = std::max(local_bounds.Bottom(),
                                  box->ScrollableOverflowRect().Bottom());
      local_bounds.ShiftBottomEdgeTo(max_y);
    }
  } else if (layout_object->IsText()) {
    const auto* text = To<LayoutText>(layout_object);
    // TODO(kojii): |PhysicalLinesBoundingBox()| cannot compute, and thus
    // returns (0, 0) when changes are made that |DeleteLineBoxes()| or clear
    // |SetPaintFragment()|, e.g., |SplitFlow()|. crbug.com/965352
    local_bounds.Unite(text->PhysicalLinesBoundingBox());
  } else {
    // Only LayoutBox and LayoutText are supported.
    NOTREACHED_IN_MIGRATION();
  }

  gfx::RectF relative_bounds =
      scroller
          ->LocalToVisibleContentQuad(gfx::QuadF(gfx::RectF(local_bounds)),
                                      layout_object)
          .BoundingBox();

  return PhysicalRect::FastAndLossyFromRectF(relative_bounds);
}

static LayoutPoint ComputeRelativeOffset(const LayoutObject* layout_object,
                                         const ScrollableArea* scroller,
                                         Corner corner) {
  PhysicalOffset offset =
      CornerPointOfRect(RelativeBounds(layout_object, scroller), corner);
  const LayoutBox* scroller_box = ScrollerLayoutBox(scroller);
  return scroller_box->FlipForWritingMode(offset);
}

static bool CandidateMayMoveWithScroller(const LayoutObject* candidate,
                                         const ScrollableArea* scroller) {
  if (candidate->IsFixedPositioned() ||
      candidate->StyleRef().HasStickyConstrainedPosition())
    return false;

  LayoutObject::AncestorSkipInfo skip_info(ScrollerLayoutBox(scroller));
  candidate->Container(&skip_info);
  return !skip_info.AncestorSkipped();
}

static bool IsOnlySiblingWithTagName(Element* element) {
  DCHECK(element);
  return (1U == NthIndexCache::NthOfTypeIndex(*element)) &&
         (1U == NthIndexCache::NthLastOfTypeIndex(*element));
}

static const AtomicString UniqueClassnameAmongSiblings(Element* element) {
  DCHECK(element);

  auto classname_filter = std::make_unique<ClassnameFilter>();

  Element* parent_element = ElementTraversal::FirstAncestor(*element);
  Element* sibling_element =
      parent_element ? ElementTraversal::FirstChild(*parent_element) : element;
  // Add every classname of every sibling to our bloom filter, starting from the
  // leftmost sibling, but skipping |element|.
  for (; sibling_element;
       sibling_element = ElementTraversal::NextSibling(*sibling_element)) {
    if (sibling_element->HasClass() && sibling_element != element) {
      const SpaceSplitString& class_names = sibling_element->ClassNames();
      for (wtf_size_t i = 0; i < class_names.size(); ++i) {
        classname_filter->Add(class_names[i].Hash());
      }
    }
  }

  const SpaceSplitString& class_names = element->ClassNames();
  for (wtf_size_t i = 0; i < class_names.size(); ++i) {
    // MayContain allows for false positives, but a false positive is relatively
    // harmless; it just means we have to choose a different classname, or in
    // the worst case a different selector.
    if (!classname_filter->MayContain(class_names[i].Hash())) {
      return class_names[i];
    }
  }

  return AtomicString();
}

// Calculate a simple selector for |element| that uniquely identifies it among
// its siblings. If present, the element's id will be used; otherwise, less
// specific selectors are preferred to more specific ones. The ordering of
// selector preference is:
// 1. ID
// 2. Tag name
// 3. Class name
// 4. nth-child
static const String UniqueSimpleSelectorAmongSiblings(Element* element) {
  DCHECK(element);

  if (element->HasID() &&
      !element->GetDocument().ContainsMultipleElementsWithId(
          element->GetIdAttribute())) {
    StringBuilder builder;
    builder.Append("#");
    SerializeIdentifier(element->GetIdAttribute(), builder);
    return builder.ToAtomicString();
  }

  if (IsOnlySiblingWithTagName(element)) {
    StringBuilder builder;
    SerializeIdentifier(element->TagQName().ToString(), builder);
    return builder.ToAtomicString();
  }

  if (element->HasClass()) {
    AtomicString unique_classname = UniqueClassnameAmongSiblings(element);
    if (!unique_classname.empty()) {
      return AtomicString(".") + unique_classname;
    }
  }

  return ":nth-child(" +
         String::Number(NthIndexCache::NthChildIndex(
             *element, /*filter=*/nullptr, /*selector_checker=*/nullptr,
             /*context=*/nullptr)) +
         ")";
}

// Computes a selector that uniquely identifies |anchor_node|. This is done
// by computing a selector that uniquely identifies each ancestor among its
// sibling elements, terminating at a definitively unique ancestor. The
// definitively unique ancestor is either the first ancestor with an id or
// the root of the document. The computed selectors are chained together with
// the child combinator(>) to produce a compound selector that is
// effectively a path through the DOM tree to |anchor_node|.
static const String ComputeUniqueSelector(Node* anchor_node) {
  DCHECK(anchor_node);
  // The scroll anchor can be a pseudo element, but pseudo elements aren't part
  // of the DOM and can't be used as part of a selector. We fail in this case;
  // success isn't possible.
  if (anchor_node->IsPseudoElement()) {
    return String();
  }

  // When the scroll anchor is a shadow DOM element, the selector may be applied
  // to the top document. We fail in this case.
  if (anchor_node->IsInShadowTree()) {
    return String();
  }

  TRACE_EVENT0("blink", "ScrollAnchor::SerializeAnchor");

  Vector<String> selector_list;
  for (Element* element = ElementTraversal::FirstAncestorOrSelf(*anchor_node);
       element; element = ElementTraversal::FirstAncestor(*element)) {
    selector_list.push_back(UniqueSimpleSelectorAmongSiblings(element));
    if (element->HasID() &&
        !element->GetDocument().ContainsMultipleElementsWithId(
            element->GetIdAttribute())) {
      break;
    }
  }

  StringBuilder builder;
  size_t i = 0;
  // We added the selectors tree-upward order from left to right, but css
  // selectors are written tree-downward from left to right. We reverse the
  // order of iteration to get a properly ordered compound selector.
  for (auto reverse_iterator = selector_list.rbegin();
       reverse_iterator != selector_list.rend(); ++reverse_iterator, ++i) {
    if (i)
      builder.Append(">");
    builder.Append(*reverse_iterator);
  }

  if (builder.length() > kMaxSerializedSelectorLength) {
    return String();
  }

  return builder.ToString();
}

static PhysicalRect GetVisibleRect(ScrollableArea* scroller) {
  auto visible_rect =
      ScrollerLayoutBox(scroller)->OverflowClipRect(PhysicalOffset());

  const ComputedStyle* style = ScrollerLayoutBox(scroller)->Style();
  visible_rect.ContractEdges(
      MinimumValueForLength(style->ScrollPaddingTop(), visible_rect.Height()),
      MinimumValueForLength(style->ScrollPaddingRight(), visible_rect.Width()),
      MinimumValueForLength(style->ScrollPaddingBottom(),
                            visible_rect.Height()),
      MinimumValueForLength(style->ScrollPaddingLeft(), visible_rect.Width()));
  return visible_rect;
}

ScrollAnchor::ExamineResult ScrollAnchor::Examine(
    const LayoutObject* candidate) const {
  if (candidate == ScrollerLayoutBox(scroller_))
    return ExamineResult(kContinue);

  if (candidate->StyleRef().OverflowAnchor() == EOverflowAnchor::kNone)
    return ExamineResult(kSkip);

  if (candidate->IsLayoutInline())
    return ExamineResult(kContinue);

  // Anonymous blocks are not in the DOM tree and it may be hard for
  // developers to reason about the anchor node.
  if (candidate->IsAnonymous())
    return ExamineResult(kContinue);

  if (!candidate->IsText() && !candidate->IsBox())
    return ExamineResult(kSkip);

  if (!CandidateMayMoveWithScroller(candidate, scroller_))
    return ExamineResult(kSkip);

  PhysicalRect candidate_rect = RelativeBounds(candidate, scroller_);
  PhysicalRect visible_rect = GetVisibleRect(scroller_);

  bool occupies_space =
      candidate_rect.Width() > 0 && candidate_rect.Height() > 0;
  if (occupies_space && visible_rect.Intersects(candidate_rect)) {
    return ExamineResult(
        visible_rect.Contains(candidate_rect) ? kReturn : kConstrain,
        CornerToAnchor(scroller_));
  } else {
    return ExamineResult(kSkip);
  }
}

void ScrollAnchor::FindAnchor() {
  TRACE_EVENT0("blink", "ScrollAnchor::FindAnchor");

  bool found_priority_anchor = FindAnchorInPriorityCandidates();
  if (!found_priority_anchor)
    FindAnchorRecursive(ScrollerLayoutBox(scroller_));

  if (anchor_object_) {
    anchor_object_->SetIsScrollAnchorObject();
    saved_relative_offset_ =
        ComputeRelativeOffset(anchor_object_, scroller_, corner_);
    TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("blink.debug"), "FindAnchor",
                        "anchor_object_", anchor_object_->DebugName());
    TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("blink.debug"), "FindAnchor",
                        "saved_relative_offset_",
                        saved_relative_offset_.ToString());
    anchor_is_cv_auto_without_layout_ =
        DisplayLockUtilities::IsAutoWithoutLayout(*anchor_object_);
  }
}

bool ScrollAnchor::FindAnchorInPriorityCandidates() {
  auto* scroller_box = ScrollerLayoutBox(scroller_);
  if (!scroller_box)
    return false;

  auto& document = scroller_box->GetDocument();

  // Focused area.
  LayoutObject* candidate = nullptr;
  ExamineResult result{kSkip};
  auto* focused_element = document.FocusedElement();
  if (focused_element && IsEditable(*focused_element)) {
    candidate = PriorityCandidateFromNode(focused_element);
    if (candidate) {
      result = ExaminePriorityCandidate(candidate);
      if (IsViable(result.status)) {
        anchor_object_ = candidate;
        corner_ = result.corner;
        return true;
      }
    }
  }

  // Active find-in-page match.
  candidate =
      PriorityCandidateFromNode(document.GetFindInPageActiveMatchNode());
  result = ExaminePriorityCandidate(candidate);
  if (IsViable(result.status)) {
    anchor_object_ = candidate;
    corner_ = result.corner;
    return true;
  }
  return false;
}

LayoutObject* ScrollAnchor::PriorityCandidateFromNode(const Node* node) const {
  while (node) {
    if (auto* layout_object = node->GetLayoutObject()) {
      if (!layout_object->IsAnonymous() &&
          (!layout_object->IsInline() ||
           layout_object->IsAtomicInlineLevel())) {
        return layout_object;
      }
    }
    node = FlatTreeTraversal::Parent(*node);
  }
  return nullptr;
}

ScrollAnchor::ExamineResult ScrollAnchor::ExaminePriorityCandidate(
    const LayoutObject* candidate) const {
  auto* ancestor = candidate;
  auto* scroller_box = ScrollerLayoutBox(scroller_);
  while (ancestor && ancestor != scroller_box) {
    if (ancestor->StyleRef().OverflowAnchor() == EOverflowAnchor::kNone)
      return ExamineResult(kSkip);

    if (!CandidateMayMoveWithScroller(ancestor, scroller_))
      return ExamineResult(kSkip);

    ancestor = ancestor->Parent();
  }
  return ancestor ? Examine(candidate) : ExamineResult(kSkip);
}

ScrollAnchor::WalkStatus ScrollAnchor::FindAnchorRecursive(
    LayoutObject* candidate) {
  if (!candidate->EverHadLayout()) {
    return kSkip;
  }
  ExamineResult result = Examine(candidate);
  WalkStatus status = result.status;
  if (IsViable(status)) {
    anchor_object_ = candidate;
    corner_ = result.corner;
  }

  if (status == kReturn || status == kSkip)
    return status;

  bool is_block_fragmentation_context_root =
      IsNGBlockFragmentationRoot(DynamicTo<LayoutBlockFlow>(candidate));

  for (LayoutObject* child = candidate->SlowFirstChild(); child;
       child = child->NextSibling()) {
    WalkStatus child_status = FindAnchorRecursive(child);
    if (child_status == kReturn)
      return child_status;
    if (child_status == kConstrain) {
      // We have found an anchor, but it's not fully contained within the
      // viewport. If this is an NG block fragmentation context root, break now
      // to search for OOFs inside the fragmentainers, which may provide a
      // better anchor.
      if (is_block_fragmentation_context_root) {
        status = child_status;
        break;
      }
      return child_status;
    }
  }

  // Make a separate pass to catch positioned descendants with a static DOM
  // parent that we skipped over (crbug.com/692701).
  WalkStatus oof_status = FindAnchorInOOFs(candidate);
  if (IsViable(oof_status))
    return oof_status;

  return status;
}

ScrollAnchor::WalkStatus ScrollAnchor::FindAnchorInOOFs(
    LayoutObject* candidate) {
  auto* layout_block = DynamicTo<LayoutBlock>(candidate);
  if (!layout_block)
    return kSkip;

  // Look for OOF child fragments. If we're at a fragmentation context root,
  // this means that we need to look for them inside the fragmentainers (which
  // are children of fragmentation context root fragments), because then an OOF
  // is normally a direct child of a fragmentainer, not its actual containing
  // block.
  //
  // Be aware that the scroll anchor machinery often operates on a dirty layout
  // tree, which means that the LayoutObject that once generated the fragment
  // may have been deleted (but the fragment may still be around). In such cases
  // the LayoutObject associated with the fragment will be set to nullptr, so we
  // need to check for that.
  bool is_block_fragmentation_context_root =
      IsNGBlockFragmentationRoot(DynamicTo<LayoutBlockFlow>(layout_block));
  for (const PhysicalBoxFragment& fragment :
       layout_block->PhysicalFragments()) {
    if (!fragment.HasOutOfFlowFragmentChild() &&
        !is_block_fragmentation_context_root)
      continue;

    for (const PhysicalFragmentLink& child : fragment.Children()) {
      if (child->IsOutOfFlowPositioned()) {
        LayoutObject* layout_object = child->GetMutableLayoutObject();
        if (layout_object && layout_object->Parent() != candidate) {
          WalkStatus status = FindAnchorRecursive(layout_object);
          if (IsViable(status))
            return status;
        }
        continue;
      }
      if (!is_block_fragmentation_context_root ||
          !child->IsFragmentainerBox() || !child->HasOutOfFlowFragmentChild())
        continue;

      // Look for OOFs inside a fragmentainer.
      for (const PhysicalFragmentLink& grandchild : child->Children()) {
        if (!grandchild->IsOutOfFlowPositioned())
          continue;
        LayoutObject* layout_object = grandchild->GetMutableLayoutObject();
        if (layout_object) {
          WalkStatus status = FindAnchorRecursive(layout_object);
          if (IsViable(status))
            return status;
        }
      }
    }
  }

  return kSkip;
}

bool ScrollAnchor::ComputeScrollAnchorDisablingStyleChanged() {
  LayoutObject* current = AnchorObject();
  if (!current)
    return false;

  LayoutObject* scroller_box = ScrollerLayoutBox(scroller_);
  while (true) {
    DCHECK(current);
    if (current->ScrollAnchorDisablingStyleChanged())
      return true;
    if (current == scroller_box)
      return false;
    current = current->Parent();
  }
}

void ScrollAnchor::NotifyBeforeLayout() {
  if (queued_) {
    scroll_anchor_disabling_style_changed_ |=
        ComputeScrollAnchorDisablingStyleChanged();
    return;
  }
  DCHECK(scroller_);
  ScrollOffset scroll_offset = scroller_->GetScrollOffset();
  float block_direction_scroll_offset =
      ScrollerLayoutBox(scroller_)->IsHorizontalWritingMode()
          ? scroll_offset.y()
          : scroll_offset.x();
  if (block_direction_scroll_offset == 0) {
    ClearSelf();
    return;
  }

  if (!anchor_object_) {
    // FindAnchor() and ComputeRelativeOffset() query a box's borders as part of
    // its geometry. But when collapsed, table borders can depend on internal
    // parts, which get sorted during a layout pass. When a table with dirty
    // internal structure is checked as an anchor candidate, a DCHECK was hit.
    FindAnchor();
    if (!anchor_object_)
      return;
  }

  scroll_anchor_disabling_style_changed_ =
      ComputeScrollAnchorDisablingStyleChanged();

  LocalFrameView* frame_view = ScrollerLayoutBox(scroller_)->GetFrameView();
  auto* root_frame_viewport = DynamicTo<RootFrameViewport>(scroller_.Get());
  ScrollableArea* owning_scroller = root_frame_viewport
                                        ? &root_frame_viewport->LayoutViewport()
                                        : scroller_.Get();
  frame_view->EnqueueScrollAnchoringAdjustment(owning_scroller);
  queued_ = true;
}

gfx::Vector2d ScrollAnchor::ComputeAdjustment() const {
  // The anchor node can report fractional positions, but it is DIP-snapped when
  // painting (crbug.com/610805), so we must round the offsets to determine the
  // visual delta. If we scroll by the delta in LayoutUnits, the snapping of the
  // anchor node may round differently from the snapping of the scroll position.
  // (For example, anchor moving from 2.4px -> 2.6px is really 2px -> 3px, so we
  // should scroll by 1px instead of 0.2px.) This is true regardless of whether
  // the ScrollableArea actually uses fractional scroll positions.
  gfx::Vector2d delta = ToRoundedVector2d(ComputeRelativeOffset(
                            anchor_object_, scroller_, corner_)) -
                        ToRoundedVector2d(saved_relative_offset_);

  PhysicalRect anchor_rect = RelativeBounds(anchor_object_, scroller_);
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("blink.debug"),
                      "ComputeAdjustment", "anchor_object_",
                      anchor_object_->DebugName());
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("blink.debug"),
                      "ComputeAdjustment", "delta", delta.ToString());

  // Only adjust on the block layout axis.
  const LayoutBox* scroller_box = ScrollerLayoutBox(scroller_);
  if (scroller_box->IsHorizontalWritingMode())
    delta.set_x(0);
  else
    delta.set_y(0);

  if (anchor_is_cv_auto_without_layout_) {
    // See the effect delta would have on the anchor rect.
    // If the anchor is now off-screen (in block direction) then make sure it's
    // just at the edge.
    anchor_rect.Move(-PhysicalOffset(delta));
    if (scroller_box->IsHorizontalWritingMode()) {
      if (anchor_rect.Bottom() < 0) {
        delta.set_y(delta.y() + anchor_rect.Bottom().ToInt());
      }
    } else {
      // For the flipped blocks writing mode, we need to adjust the offset to
      // align the opposite edge of the block (MaxX edge instead of X edge).
      if (scroller_box->HasFlippedBlocksWritingMode()) {
        auto visible_rect = GetVisibleRect(scroller_);
        if (anchor_rect.X() > visible_rect.Right()) {
          delta.set_x(delta.x() -
                      (anchor_rect.X().ToInt() - visible_rect.Right().ToInt()));
        }
      } else if (anchor_rect.Right() < 0) {
        delta.set_x(delta.x() + anchor_rect.Right().ToInt());
      }
    }
  }

  // If block direction is flipped, delta is a logical value, so flip it to
  // make it physical.
  if (!scroller_box->IsHorizontalWritingMode() &&
      scroller_box->HasFlippedBlocksWritingMode()) {
    delta.set_x(-delta.x());
  }
  return delta;
}

void ScrollAnchor::Adjust() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("blink.debug"),
               "ScrollAnchor::Adjust");
  if (!queued_)
    return;
  queued_ = false;
  DCHECK(scroller_);
  if (!anchor_object_)
    return;
  gfx::Vector2d adjustment = ComputeAdjustment();
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("blink.debug"), "Adjust",
                      "adjustment", adjustment.ToString());

  // We should pick a new anchor if we had an unlaid-out content-visibility
  // auto. It should have been laid out, so if it is still the best candidate,
  // we will select it without this boolean set.
  if (anchor_is_cv_auto_without_layout_)
    ClearSelf();

  if (adjustment.IsZero())
    return;

  if (scroll_anchor_disabling_style_changed_) {
    // Note that we only clear if the adjustment would have been non-zero.
    // This minimizes redundant calls to findAnchor.
    ClearSelf();
    return;
  }

  ScrollOffset new_offset =
      scroller_->GetScrollOffset() + ScrollOffset(adjustment);

  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("blink.debug"), "Adjust",
                      "new_offset", new_offset.ToString());

  scroller_->SetScrollOffset(new_offset, mojom::blink::ScrollType::kAnchoring);

  UseCounter::Count(ScrollerLayoutBox(scroller_)->GetDocument(),
                    WebFeature::kScrollAnchored);
}

bool ScrollAnchor::RestoreAnchor(const SerializedAnchor& serialized_anchor) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("blink.debug"),
               "ScrollAnchor::RestoreAnchor");
  if (!scroller_ || !serialized_anchor.IsValid()) {
    return false;
  }

  if (anchor_object_ && serialized_anchor.selector == saved_selector_) {
    return true;
  }

  if (anchor_object_) {
    return false;
  }

  Document* document = &(ScrollerLayoutBox(scroller_)->GetDocument());

  // This is a considered and deliberate usage of DummyExceptionStateForTesting.
  // We really do want to always swallow it. Here's why:
  // 1) We have no one to propagate an exception to.
  // 2) We don't want to rely on having an isolate(which normal ExceptionState
  // does), as this requires setting up and using javascript/v8. This is
  // undesirable since it needlessly prevents us from running when javascript is
  // disabled, and causes proxy objects to be prematurely
  // initialized(crbug.com/810897).
  DummyExceptionStateForTesting exception_state;
  StaticElementList* found_elements = document->QuerySelectorAll(
      AtomicString(serialized_anchor.selector), exception_state);

  if (exception_state.HadException()) {
    return false;
  }

  if (found_elements->length() < 1) {
    return false;
  }

  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("blink.debug"), "RestoreAnchor",
                      "found_elements_length", found_elements->length());

  for (unsigned index = 0; index < found_elements->length(); index++) {
    Element* anchor_element = found_elements->item(index);
    LayoutObject* anchor_object = anchor_element->GetLayoutObject();

    if (!anchor_object) {
      continue;
    }

    // There are scenarios where the layout object we find is non-box and
    // non-text; this can happen, e.g., if the original anchor object was a text
    // element of a non-box element like <code>. The generated selector can't
    // directly locate the text object, resulting in a loss of precision.
    // Instead we scroll the object we do find into the same relative position
    // and attempt to re-find the anchor. The user-visible effect should end up
    // roughly the same.
    ScrollOffset current_offset = scroller_->GetScrollOffset();
    gfx::RectF bounding_box = anchor_object->AbsoluteBoundingBoxRectF();
    gfx::PointF location_point =
        anchor_object->Style()->IsFlippedBlocksWritingMode()
            ? bounding_box.top_right()
            : bounding_box.origin();
    gfx::PointF desired_point = location_point + current_offset;

    ScrollOffset desired_offset = desired_point.OffsetFromOrigin();
    ScrollOffset delta =
        ScrollOffset(serialized_anchor.relative_offset.X().ToFloat(),
                     serialized_anchor.relative_offset.Y().ToFloat());
    desired_offset -= delta;
    TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("blink.debug"),
                        "RestoreAnchor", "anchor_object",
                        anchor_object->DebugName());
    scroller_->SetScrollOffset(desired_offset,
                               mojom::blink::ScrollType::kAnchoring);
    FindAnchor();

    // If the above FindAnchor call failed, reset the scroll position and try
    // again with the next found element.
    if (!anchor_object_) {
      scroller_->SetScrollOffset(current_offset,
                                 mojom::blink::ScrollType::kAnchoring);
      continue;
    }

    saved_selector_ = serialized_anchor.selector;
    return true;
  }

  return false;
}

const SerializedAnchor ScrollAnchor::GetSerializedAnchor() {
  if (auto* scroller_box = ScrollerLayoutBox(scroller_)) {
    // This method may be called to find a serialized anchor on a document which
    // needs a lifecycle update. Computing offsets below may currently compute
    // style for ::first-line. If that is done with dirty active stylesheets, we
    // may have null pointer crash as style computation assumes active sheets
    // are up to date. Update active style if necessary here.
    scroller_box->GetDocument().GetStyleEngine().UpdateActiveStyle();
  }

  // It's safe to return saved_selector_ before checking anchor_object_, since
  // clearing anchor_object_ also clears saved_selector_.
  if (!saved_selector_.empty()) {
    DCHECK(anchor_object_);
    return SerializedAnchor(
        saved_selector_,
        ComputeRelativeOffset(anchor_object_, scroller_, corner_));
  }

  if (!anchor_object_) {
    FindAnchor();
    if (!anchor_object_)
      return SerializedAnchor();
  }

  DCHECK(anchor_object_->GetNode());
  SerializedAnchor new_anchor(
      ComputeUniqueSelector(anchor_object_->GetNode()),
      ComputeRelativeOffset(anchor_object_, scroller_, corner_));

  if (new_anchor.IsValid()) {
    saved_selector_ = new_anchor.selector;
  }

  return new_anchor;
}

void ScrollAnchor::ClearSelf() {
  LayoutObject* anchor_object = anchor_object_;
  anchor_object_ = nullptr;
  saved_selector_ = String();

  if (anchor_object)
    anchor_object->MaybeClearIsScrollAnchorObject();
}

void ScrollAnchor::Dispose() {
  if (scroller_) {
    LocalFrameView* frame_view = ScrollerLayoutBox(scroller_)->GetFrameView();
    auto* root_frame_viewport = DynamicTo<RootFrameViewport>(scroller_.Get());
    ScrollableArea* owning_scroller =
        root_frame_viewport ? &root_frame_viewport->LayoutViewport()
                            : scroller_.Get();
    frame_view->DequeueScrollAnchoringAdjustment(owning_scroller);
    scroller_.Clear();
  }
  anchor_object_ = nullptr;
  saved_selector_ = String();
}

void ScrollAnchor::Clear() {
  LayoutObject* layout_object =
      anchor_object_ ? anchor_object_ : ScrollerLayoutBox(scroller_);
  PaintLayer* layer = nullptr;
  if (LayoutObject* parent = layout_object->Parent())
    layer = parent->EnclosingLayer();

  // Walk up the layer tree to clear any scroll anchors.
  while (layer) {
    if (PaintLayerScrollableArea* scrollable_area =
            layer->GetScrollableArea()) {
      ScrollAnchor* anchor = scrollable_area->GetScrollAnchor();
      DCHECK(anchor);
      anchor->ClearSelf();
    }
    layer = layer->Parent();
  }
}

bool ScrollAnchor::RefersTo(const LayoutObject* layout_object) const {
  return anchor_object_ == layout_object;
}

void ScrollAnchor::NotifyRemoved(LayoutObject* layout_object) {
  if (anchor_object_ == layout_object)
    ClearSelf();
}

}  // namespace blink

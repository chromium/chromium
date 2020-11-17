// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/scroll_anchor.h"

#include <algorithm>
#include <memory>

#include "third_party/blink/renderer/core/css/css_markup.h"
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
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

// With 100 unique strings, a 2^12 slot table has a false positive rate of ~2%.
using ClassnameFilter = BloomFilter<12>;
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

static LayoutPoint CornerPointOfRect(LayoutRect rect, Corner which_corner) {
  switch (which_corner) {
    case Corner::kTopLeft:
      return rect.MinXMinYCorner();
    case Corner::kTopRight:
      return rect.MaxXMinYCorner();
  }
  NOTREACHED();
  return LayoutPoint();
}

// Bounds of the LayoutObject relative to the scroller's visible content rect.
static LayoutRect RelativeBounds(const LayoutObject* layout_object,
                                 const ScrollableArea* scroller) {
  PhysicalRect local_bounds;
  if (const auto* box = DynamicTo<LayoutBox>(layout_object)) {
    local_bounds = box->PhysicalBorderBoxRect();
    // If we clip overflow then we can use the `PhysicalBorderBoxRect()`
    // as our bounds. If not, we expand the bounds by the layout overflow and
    // lowest floating object.
    if (!layout_object->ShouldClipOverflowAlongEitherAxis()) {
      // BorderBoxRect doesn't include overflow content and floats.
      LayoutUnit max_y =
          std::max(local_bounds.Bottom(), box->LayoutOverflowRect().MaxY());
      auto* layout_block_flow = DynamicTo<LayoutBlockFlow>(layout_object);
      if (layout_block_flow && layout_block_flow->ContainsFloats()) {
        // Note that lowestFloatLogicalBottom doesn't include floating
        // grandchildren.
        max_y = std::max(max_y, layout_block_flow->LowestFloatLogicalBottom());
      }
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
    NOTREACHED();
  }

  LayoutRect relative_bounds = LayoutRect(
      scroller
          ->LocalToVisibleContentQuad(FloatRect(local_bounds), layout_object)
          .BoundingBox());

  return relative_bounds;
}

static LayoutPoint ComputeRelativeOffset(const LayoutObject* layout_object,
                                         const ScrollableArea* scroller,
                                         Corner corner) {
  LayoutPoint offset =
      CornerPointOfRect(RelativeBounds(layout_object, scroller), corner);
  const LayoutBox* scroller_box = ScrollerLayoutBox(scroller);
  return scroller_box->FlipForWritingMode(PhysicalOffset(offset));
}

static bool CandidateMayMoveWithScroller(const LayoutObject* candidate,
                                         const ScrollableArea* scroller) {
  if (const ComputedStyle* style = candidate->Style()) {
    if (style->HasViewportConstrainedPosition() ||
        style->HasStickyConstrainedPosition())
      return false;
  }

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
        classname_filter->Add(class_names[i].Impl()->ExistingHash());
      }
    }
  }

  const SpaceSplitString& class_names = element->ClassNames();
  for (wtf_size_t i = 0; i < class_names.size(); ++i) {
    // MayContain allows for false positives, but a false positive is relatively
    // harmless; it just means we have to choose a different classname, or in
    // the worst case a different selector.
    if (!classname_filter->MayContain(class_names[i].Impl()->ExistingHash())) {
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
    if (!unique_classname.IsEmpty()) {
      return AtomicString(".") + unique_classname;
    }
  }

  return ":nth-child(" +
         String::Number(NthIndexCache::NthChildIndex(*element)) + ")";
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

  TRACE_EVENT0("blink", "ScrollAnchor::SerializeAnchor");
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER(
      "Layout.ScrollAnchor.TimeToComputeAnchorNodeSelector");

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

  DEFINE_STATIC_LOCAL(CustomCountHistogram, selector_length_histogram,
                      ("Layout.ScrollAnchor.SerializedAnchorSelectorLength", 1,
                       kMaxSerializedSelectorLength, 50));
  selector_length_histogram.Count(builder.length());

  if (builder.length() > kMaxSerializedSelectorLength) {
    return String();
  }

  return builder.ToString();
}

static LayoutRect GetVisibleRect(ScrollableArea* scroller) {
  auto visible_rect =
      ScrollerLayoutBox(scroller)->OverflowClipRect(LayoutPoint());

  const ComputedStyle* style = ScrollerLayoutBox(scroller)->Style();
  LayoutRectOutsets scroll_padding(
      MinimumValueForLength(style->ScrollPaddingTop(), visible_rect.Height()),
      MinimumValueForLength(style->ScrollPaddingRight(), visible_rect.Width()),
      MinimumValueForLength(style->ScrollPaddingBottom(),
                            visible_rect.Height()),
      MinimumValueForLength(style->ScrollPaddingLeft(), visible_rect.Width()));
  visible_rect.Contract(scroll_padding);
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

  LayoutRect candidate_rect = RelativeBounds(candidate, scroller_);
  LayoutRect visible_rect = GetVisibleRect(scroller_);

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
  TRACE_EVENT0("blink", "ScrollAnchor::findAnchor");
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER("Layout.ScrollAnchor.TimeToFindAnchor");

  bool found_priority_anchor = FindAnchorInPriorityCandidates();
  if (!found_priority_anchor)
    FindAnchorRecursive(ScrollerLayoutBox(scroller_));

  if (anchor_object_) {
    anchor_object_->SetIsScrollAnchorObject();
    saved_relative_offset_ =
        ComputeRelativeOffset(anchor_object_, scroller_, corner_);
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
  if (focused_element && HasEditableStyle(*focused_element)) {
    candidate = PriorityCandidateFromNode(focused_element);
    if (candidate) {
      result = ExaminePriorityCandidate(candidate);
      if (result.viable) {
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
  if (result.viable) {
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

bool ScrollAnchor::FindAnchorRecursive(LayoutObject* candidate) {
  ExamineResult result = Examine(candidate);
  if (result.viable) {
    anchor_object_ = candidate;
    corner_ = result.corner;
  }

  if (result.status == kReturn)
    return true;

  if (result.status == kSkip)
    return false;

  for (LayoutObject* child = candidate->SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (FindAnchorRecursive(child))
      return true;
  }

  // Make a separate pass to catch positioned descendants with a static DOM
  // parent that we skipped over (crbug.com/692701).
  if (auto* layouy_block = DynamicTo<LayoutBlock>(candidate)) {
    if (TrackedLayoutBoxListHashSet* positioned_descendants =
            layouy_block->PositionedObjects()) {
      for (LayoutBox* descendant : *positioned_descendants) {
        if (descendant->Parent() != candidate) {
          if (FindAnchorRecursive(descendant))
            return true;
        }
      }
    }
  }

  if (result.status == kConstrain)
    return true;

  DCHECK_EQ(result.status, kContinue);
  return false;
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
          ? scroll_offset.Height()
          : scroll_offset.Width();
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

IntSize ScrollAnchor::ComputeAdjustment() const {
  // The anchor node can report fractional positions, but it is DIP-snapped when
  // painting (crbug.com/610805), so we must round the offsets to determine the
  // visual delta. If we scroll by the delta in LayoutUnits, the snapping of the
  // anchor node may round differently from the snapping of the scroll position.
  // (For example, anchor moving from 2.4px -> 2.6px is really 2px -> 3px, so we
  // should scroll by 1px instead of 0.2px.) This is true regardless of whether
  // the ScrollableArea actually uses fractional scroll positions.
  IntSize delta = RoundedIntSize(ComputeRelativeOffset(anchor_object_,
                                                       scroller_, corner_)) -
                  RoundedIntSize(saved_relative_offset_);

  LayoutRect anchor_rect = RelativeBounds(anchor_object_, scroller_);

  // Only adjust on the block layout axis.
  const LayoutBox* scroller_box = ScrollerLayoutBox(scroller_);
  if (scroller_box->IsHorizontalWritingMode())
    delta.SetWidth(0);
  else
    delta.SetHeight(0);

  if (anchor_is_cv_auto_without_layout_) {
    // See the effect delta would have on the anchor rect.
    // If the anchor is now off-screen (in block direction) then make sure it's
    // just at the edge.
    anchor_rect.Move(-delta);
    if (scroller_box->IsHorizontalWritingMode()) {
      if (anchor_rect.MaxY() < 0)
        delta.SetHeight(delta.Height() + anchor_rect.MaxY().ToInt());
    } else {
      // For the flipped blocks writing mode, we need to adjust the offset to
      // align the opposite edge of the block (MaxX edge instead of X edge).
      if (scroller_box->HasFlippedBlocksWritingMode()) {
        auto visible_rect = GetVisibleRect(scroller_);
        if (anchor_rect.X() > visible_rect.MaxX()) {
          delta.SetWidth(delta.Width() - (anchor_rect.X().ToInt() -
                                          visible_rect.MaxX().ToInt()));
        }
      } else if (anchor_rect.MaxX() < 0) {
        delta.SetWidth(delta.Width() + anchor_rect.MaxX().ToInt());
      }
    }
  }

  // If block direction is flipped, delta is a logical value, so flip it to
  // make it physical.
  if (!scroller_box->IsHorizontalWritingMode() &&
      scroller_box->HasFlippedBlocksWritingMode()) {
    delta.SetWidth(-delta.Width());
  }
  return delta;
}

void ScrollAnchor::Adjust() {
  if (!queued_)
    return;
  queued_ = false;
  DCHECK(scroller_);
  if (!anchor_object_)
    return;
  IntSize adjustment = ComputeAdjustment();

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
    // TODO(skobes): add UMA metric for this.
    ClearSelf();
    return;
  }

  scroller_->SetScrollOffset(
      scroller_->GetScrollOffset() + FloatSize(adjustment),
      mojom::blink::ScrollType::kAnchoring);

  UseCounter::Count(ScrollerLayoutBox(scroller_)->GetDocument(),
                    WebFeature::kScrollAnchored);
}

bool ScrollAnchor::RestoreAnchor(const SerializedAnchor& serialized_anchor) {
  if (!scroller_ || !serialized_anchor.IsValid()) {
    return false;
  }

  SCOPED_BLINK_UMA_HISTOGRAM_TIMER("Layout.ScrollAnchor.TimeToRestoreAnchor");

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
    FloatRect bounding_box = anchor_object->AbsoluteBoundingBoxFloatRect();
    FloatPoint location_point =
        anchor_object->Style()->IsFlippedBlocksWritingMode()
            ? bounding_box.MaxXMinYCorner()
            : bounding_box.Location();
    FloatPoint desired_point = location_point + current_offset;

    ScrollOffset desired_offset =
        ScrollOffset(desired_point.X(), desired_point.Y());
    ScrollOffset delta =
        ScrollOffset(RoundedIntSize(serialized_anchor.relative_offset));
    desired_offset -= delta;
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
  // It's safe to return saved_selector_ before checking anchor_object_, since
  // clearing anchor_object_ also clears saved_selector_.
  if (!saved_selector_.IsEmpty()) {
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

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/annotation/annotation_agent_impl.h"

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/typed_macros.h"
#include "cc/base/features.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_container_impl.h"
#include "third_party/blink/renderer/core/annotation/annotation_selector.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/text_fragment_marker.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/range_in_flat_tree.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/highlight/highlight_style_utils.h"
#include "third_party/blink/renderer/core/html/html_details_element.h"
#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "ui/display/screen_info.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

namespace {
bool IsValidRange(const RangeInFlatTree* range) {
  // An attached range may have !IsCollapsed but converting to EphemeralRange
  // results in IsCollapsed. For an example, see
  // AnnotationAgentImplTest.ScrollIntoViewCollapsedRange.
  bool is_valid = range && range->IsConnected() && !range->IsCollapsed() &&
                  !range->ToEphemeralRange().IsCollapsed();

  if (is_valid) {
    // TODO(crbug.com/410033683): Temporary to work around a crash.
    // DocumentMarkers work on EphemeralRange (i.e. not FlatTree) so when we try
    // to add a marker in ProcessAttachmentFinished, a well-ordered range in a
    // flat tree may become invalid due to slotted elements. DocumentMarkers
    // should maybe work on FlatTree types but for now just invalidate this
    // case.
    Position start = ToPositionInDOMTree(range->StartPosition());
    Position end = ToPositionInDOMTree(range->EndPosition());
    if (start > end) {
      return false;
    }
  }

  return is_valid;
}

// There are several cases where text isn't visible/presented to the user but
// does appear findable to FindBuffer. Some use cases (see
// `ShouldUseIsValidRangeAndMarkable` below) want to prevent offering scrolls to
// these sections as its confusing (in fact, document Markers will avoid
// creating a highlight for these, despite the fact we can scroll to it). We
// probably want to do this for general SharedHighlights as well but that will
// require some more thought and spec changes but we can experiment with this to
// see how it works.
bool IsValidRangeAndMarkable(const RangeInFlatTree* range) {
  if (!IsValidRange(range)) {
    return false;
  }

  EphemeralRangeInFlatTree ephemeral_range = range->ToEphemeralRange();

  // Technically, the text could span multiple Elements, each of which could
  // hide overflow. However, that doesn't seem to be common so do the more
  // performant thing and check the common ancestor.
  Node* common_node = ephemeral_range.CommonAncestorContainer();

  LayoutObject* object = common_node->GetLayoutObject();
  while (!object) {
    common_node = FlatTreeTraversal::Parent(*common_node);
    // We should exit this loop before this is false (i.e. there must be at
    // least one ancestor node with a LayoutObject).
    CHECK(common_node);
    object = common_node->GetLayoutObject();
  }

  PhysicalRect absolute_bounding_box =
      object->AbsoluteBoundingBoxRectForScrollIntoView();

  if (LocalFrameView* view = ephemeral_range.GetDocument().View()) {
    PhysicalRect bounding_box_in_document =
        view->FrameToDocument(absolute_bounding_box);
    // If the box is positioned out of the document bounds (eg: something like
    // position: absolute; top: -9999em), it cannot be highlighted.
    if (bounding_box_in_document.Bottom() <= 0 ||
        bounding_box_in_document.Right() <= 0) {
      return false;
    }
  }

  for (; !object->IsLayoutView(); object = object->Parent()) {
    LayoutBox* box = DynamicTo<LayoutBox>(object);
    if (!box) {
      continue;
    }

    // It's common for collapsible sections to be implemented by hiding
    // collapsed text within a `height:0; overflow: hidden` box. However,
    // FindBuffer does find this text (as typically overflow: hidden can still
    // be programmatically scrolled).
    if (box->HasNonVisibleOverflow()) {
      PhysicalSize size = box->StitchedSize();
      if (box->StyleRef().OverflowX() != EOverflow::kVisible &&
          size.width.RawValue() <= 0) {
        return false;
      }

      if (box->StyleRef().OverflowY() != EOverflow::kVisible &&
          size.height.RawValue() <= 0) {
        return false;
      }
    }

    // If an ancestor is set to opacity 0, consider the target invisible.
    if (box->StyleRef().Opacity() == 0) {
      return false;
    }

    // If the range is in a fixed subtree, scrolling the view won't change its
    // viewport-relative location so report the range as unfindable if its
    // currently offscreen.
    if (box->StyleRef().GetPosition() == EPosition::kFixed) {
      PhysicalRect view_rect =
          PhysicalRect::EnclosingRect(box->View()->AbsoluteBoundingBoxRectF());
      if (!view_rect.Intersects(absolute_bounding_box)) {
        return false;
      }
    }
  }

  return true;
}

bool ShouldUseIsValidRangeAndMarkable(mojom::blink::AnnotationType type) {
  switch (type) {
    case mojom::blink::AnnotationType::kTextFinder:
    case mojom::blink::AnnotationType::kGlic:
      return true;
    case mojom::blink::AnnotationType::kSharedHighlight:
    case mojom::blink::AnnotationType::kUserNote:
      return false;
  }
}

// The maximum scroll distance for which an AnnotationAgent of type kGlic should
// use a smooth (animated) scroll. For longer distances, the scroll will be
// instant.
int GetGlicSmoothScrollThresholdInDIPs() {
  const base::FeatureParam<int> glic_smooth_scroll_threshold_in_dips{
      &features::kProgrammaticScrollAnimationOverride,
      "glic_smooth_scroll_threshold_in_dips", 15000};
  return glic_smooth_scroll_threshold_in_dips.Get();
}

std::optional<DocumentMarker::MarkerTypes> GetMarkerTypesForAnnotationType(
    mojom::blink::AnnotationType annotation_type) {
  switch (annotation_type) {
    case mojom::blink::AnnotationType::kSharedHighlight:
    case mojom::blink::AnnotationType::kUserNote:
      return DocumentMarker::MarkerTypes::TextFragment();
    case mojom::blink::AnnotationType::kGlic:
      return DocumentMarker::MarkerTypes::Glic();
    case mojom::blink::AnnotationType::kTextFinder:
      return std::nullopt;
  }
}

bool HasMarkerAroundPosition(const HitTestResult& result,
                             DocumentMarker::MarkerType marker_type) {
  // Tree should be clean before accessing the position.
  // |HitTestResult::GetPosition| calls |PositionForPoint()| which requires
  // |kPrePaintClean|.
  DCHECK_GE(result.InnerNodeFrame()->GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);

  DocumentMarkerController& marker_controller =
      result.InnerNodeFrame()->GetDocument()->Markers();
  PositionWithAffinity pos_with_affinity = result.GetPosition();
  const Position marker_position = pos_with_affinity.GetPosition();

  auto markers = marker_controller.MarkersAroundPosition(
      ToPositionInFlatTree(marker_position),
      DocumentMarker::MarkerTypes(marker_type));
  return !markers.empty();
}

float CalculateMaxScrollOffsetPx(
    LocalFrameView* view,
    const PhysicalRect& bounding_box,
    const mojom::blink::ScrollIntoViewParams& params) {
  CHECK(view);
  CHECK(view->GetScrollableArea());
  ScrollOffset scroll_offset_px =
      scroll_into_view_util::GetScrollOffsetToExpose(
          *view->GetScrollableArea(), bounding_box, PhysicalBoxStrut(),
          *params.align_x, *params.align_y);
  // Removes any potential negative offset from the
  // `ScrollAlignment::CenterAlways()`.
  scroll_offset_px =
      view->GetScrollableArea()->ClampScrollOffset(scroll_offset_px);
  ScrollOffset scroll_distance_px =
      scroll_offset_px - view->GetScrollableArea()->GetScrollOffset();

  return std::max(std::abs(scroll_distance_px.x()),
                  std::abs(scroll_distance_px.y()));
}

}  // namespace

AnnotationAgentImpl::AnnotationAgentImpl(
    AnnotationAgentContainerImpl& owning_container,
    mojom::blink::AnnotationType annotation_type,
    AnnotationSelector& selector,
    std::optional<DOMNodeId> search_range_start_node_id,
    AnnotationAgentContainerImpl::PassKey)
    : agent_host_(owning_container.GetSupplementable()->GetExecutionContext()),
      receiver_(this,
                owning_container.GetSupplementable()->GetExecutionContext()),
      owning_container_(&owning_container),
      selector_(&selector),
      search_range_start_node_id_(search_range_start_node_id),
      type_(annotation_type) {
  DCHECK(!IsAttached());
  DCHECK(!IsRemoved());
}

void AnnotationAgentImpl::Trace(Visitor* visitor) const {
  visitor->Trace(agent_host_);
  visitor->Trace(receiver_);
  visitor->Trace(owning_container_);
  visitor->Trace(selector_);
  visitor->Trace(attached_range_);
  visitor->Trace(pending_range_);
}

void AnnotationAgentImpl::Bind(
    mojo::PendingRemote<mojom::blink::AnnotationAgentHost> host_remote,
    mojo::PendingReceiver<mojom::blink::AnnotationAgent> agent_receiver) {
  DCHECK(!IsRemoved());

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      owning_container_->GetSupplementable()->GetTaskRunner(
          TaskType::kInternalDefault);

  agent_host_.Bind(std::move(host_remote), task_runner);
  receiver_.Bind(std::move(agent_receiver), task_runner);

  // Breaking the mojo connection will cause this agent to remove itself from
  // the container.
  receiver_.set_disconnect_handler(BindOnce(
      [](WeakPersistent<AnnotationAgentImpl> agent) {
        if (!agent || !agent->OwningContainer()) {
          return;
        }
        agent->OwningContainer()->RemoveAgent(*agent);
      },
      WrapWeakPersistent(this)));
}

void AnnotationAgentImpl::Attach(AnnotationAgentContainerImpl::PassKey) {
  TRACE_EVENT("blink", "AnnotationAgentImpl::Attach");
  CHECK(!IsRemoved());
  CHECK(!IsAttached());
  CHECK(!pending_range_);
  CHECK(owning_container_->IsLifecycleCleanForAttachment());

  // We may still have an old range despite the CHECK above if the range become
  // collapsed due to DOM changes.
  attached_range_.Clear();

  // Create the search range within which the agent will attempt to match the
  // selector in.
  Document* document = owning_container_->GetSupplementable();
  Range* search_range = document->createRange();
  if (document->body()) {
    search_range->selectNode(document->body());
  }

  if (search_range_start_node_id_.has_value()) {
    Node* search_range_start_node =
        Node::FromDomNodeId(search_range_start_node_id_.value());
    // Check if the search range start node is null, which is not in the
    // document.
    if (!search_range_start_node || search_range->collapsed()) {
      needs_attachment_ = false;
      agent_host_->DidFinishAttachment(
          gfx::Rect(), mojom::blink::AttachmentResult::kRangeInvalid);
      return;
    }
    search_range->setStart(search_range_start_node, 0);
  }

  needs_attachment_ = false;
  selector_->FindRange(*search_range, AnnotationSelector::kSynchronous,
                       BindOnce(&AnnotationAgentImpl::DidFinishFindRange,
                                WrapWeakPersistent(this)));
}

bool AnnotationAgentImpl::IsAttached() const {
  return IsValidRange(attached_range_);
}

bool AnnotationAgentImpl::IsAttachmentPending() const {
  // This can be an invalid range but still returns true because the attachment
  // is still in progress until the DomMutation task runs in the next rAF.
  return pending_range_ != nullptr;
}

bool AnnotationAgentImpl::IsBoundForTesting() const {
  DCHECK_EQ(agent_host_.is_bound(), receiver_.is_bound());
  return receiver_.is_bound();
}

void AnnotationAgentImpl::Reset(base::PassKey<AnnotationAgentContainerImpl>) {
  DCHECK(!IsRemoved());

  if (IsAttached()) {
    EphemeralRange dom_range =
        EphemeralRange(ToPositionInDOMTree(attached_range_->StartPosition()),
                       ToPositionInDOMTree(attached_range_->EndPosition()));
    Document* document = attached_range_->StartPosition().GetDocument();
    DCHECK(document);

    if (LocalFrame* frame = document->GetFrame()) {
      // Markers require that layout is up to date if we're making any
      // modifications.
      frame->GetDocument()->UpdateStyleAndLayout(
          DocumentUpdateReason::kFindInPage);

      std::optional<DocumentMarker::MarkerTypes> marker_types =
          GetMarkerTypesForAnnotationType(type_);
      if (marker_types.has_value()) {
        document->Markers().RemoveMarkersInRange(dom_range, *marker_types);
      }
    }
  }

  attached_range_.Clear();
  pending_range_.Clear();

  agent_host_.reset();
  receiver_.reset();

  selector_.Clear();
  owning_container_.Clear();
}

void AnnotationAgentImpl::ScrollIntoView(bool applies_focus) const {
  DCHECK(!IsRemoved());

  if (!IsAttached())
    return;

  EphemeralRangeInFlatTree range = attached_range_->ToEphemeralRange();
  CHECK(range.Nodes().begin() != range.Nodes().end());
  Node& first_node = *range.Nodes().begin();

  Document& document = *owning_container_->GetSupplementable();
  document.EnsurePaintLocationDataValidForNode(
      &first_node, DocumentUpdateReason::kFindInPage);

  Node* first_node_with_layout_object = nullptr;
  for (Node& node : range.Nodes()) {
    if (node.GetLayoutObject()) {
      first_node_with_layout_object = &node;
    }
  }

  // TODO(bokan): Text can be attached without having a LayoutObject since it
  // may be inside an unexpanded <details> element or inside a
  // `content-visibility: auto` subtree. In those cases we should make sure we
  // expand/make-visible the node. This is implemented in TextFragmentAnchor
  // but that doesn't cover all cases we can get here so we should migrate that
  // code here.
  if (!first_node_with_layout_object) {
    return;
  }

  // Set the bounding box height to zero because we want to center the top of
  // the text range.
  PhysicalRect bounding_box(ComputeTextRect(range));
  bounding_box.SetHeight(LayoutUnit());

  mojom::blink::ScrollIntoViewParamsPtr params =
      scroll_into_view_util::CreateScrollIntoViewParams(
          ScrollAlignment::CenterAlways(), ScrollAlignment::CenterAlways(),
          mojom::blink::ScrollType::kProgrammatic);
  params->cross_origin_boundaries = false;
  params->behavior = ComputeScrollIntoViewBehavior(bounding_box, *params);

  if (applies_focus) {
    // If the first node accepts keyboard focus, move focus there to aid users
    // relying on keyboard navigation. If the node is not focusable, clear focus
    // so the next "Tab" press will start the search to find the next focusable
    // element from this element.
    auto* element = first_node_with_layout_object->parentElement();
    if (element && element->IsFocusable()) {
      document.SetFocusedElement(
          element, FocusParams(SelectionBehaviorOnFocus::kNone,
                               mojom::blink::FocusType::kNone, nullptr));
    } else {
      document.ClearFocusedElement();
    }
  }

  // Set the sequential focus navigation to the start of selection.
  // Even if this element isn't focusable, "Tab" press will
  // start the search to find the next focusable element from this element.
  document.SetSequentialFocusNavigationStartingPoint(
      first_node_with_layout_object);

  if (type_ == mojom::blink::AnnotationType::kGlic) {
    float max_distance_px = CalculateMaxScrollOffsetPx(
        first_node_with_layout_object->GetLayoutObject()->GetFrameView(),
        bounding_box, *params);
    if (max_distance_px <= 1.f) {
      document.Markers().StartGlicMarkerAnimationIfNeeded();
    } else {
      // Scroll is guaranteed to happen. `ScrollableArea::OnScrollFinished()`
      // will call `StartGlicMarkerAnimation()`. This is a near-term solution
      // due to the re-arch work in crbug.com/41406914. It means in the nested
      // multiple scollers case, the first ever `OnScrollFinished()` starts the
      // animation, regardless if the actual scroll has finished or not.
      //
      // TODO(https://crbug.com/41406914): Migrate from `OnScrollFinished()` to
      // the scroll-promises.
    }
  }

  scroll_into_view_util::ScrollRectToVisible(
      *first_node_with_layout_object->GetLayoutObject(), bounding_box,
      std::move(params));
}

std::optional<mojom::blink::AnnotationType>
AnnotationAgentImpl::IsOverAnnotation(const HitTestResult& result) {
  if (!result.InnerNode() || !result.InnerNodeFrame()) {
    return std::nullopt;
  }

  if (HasMarkerAroundPosition(result, DocumentMarker::MarkerType::kGlic)) {
    // Note: We could also have a marker of type kTextFragment around the
    // position as well, but we treat kGlic as topmost.
    return mojom::blink::AnnotationType::kGlic;
  }

  if (HasMarkerAroundPosition(result,
                              DocumentMarker::MarkerType::kTextFragment)) {
    return mojom::blink::AnnotationType::kSharedHighlight;
  }

  return std::nullopt;
}

void AnnotationAgentImpl::DidFinishFindRange(const RangeInFlatTree* range) {
  TRACE_EVENT("blink", "AnnotationAgentImpl::DidFinishFindRange",
              "bound_to_host", agent_host_.is_bound());
  if (IsRemoved()) {
    TRACE_EVENT_INSTANT("blink", "Removed");
    return;
  }

  pending_range_ = range;

  // In some cases, attaching to text can lead to DOM mutation. For example,
  // expanding <details> elements or unhiding an hidden=until-found element.
  // That needs to be done before processing the attachment (i.e. adding a
  // highlight). However, DOM/layout may not be safe to do here so we'll post a
  // task in that case.  However, if we don't need to perform those actions we
  // can avoid the extra post and just process the attachment now.
  if (!NeedsDOMMutationToAttach()) {
    ProcessAttachmentFinished();
  } else {
    // TODO(bokan): We may need to force an animation frame e.g. if we're in a
    // throttled iframe.
    Document& document = *owning_container_->GetSupplementable();
    document.EnqueueAnimationFrameTask(
        BindOnce(&AnnotationAgentImpl::PerformPreAttachDOMMutation,
                 WrapPersistent(this)));
  }
}

bool AnnotationAgentImpl::NeedsDOMMutationToAttach() const {
  if (!IsValidRange(pending_range_)) {
    return false;
  }

  // TextFinder type is used only to determine whether a given text can be
  // found in the page, it should have no side-effects.
  if (type_ == mojom::blink::AnnotationType::kTextFinder) {
    return false;
  }

  EphemeralRangeInFlatTree range = pending_range_->ToEphemeralRange();

  // TODO(crbug.com/1252872): Only |first_node| is considered in the range, but
  // we should be considering the entire range of selected text for ancestor
  // unlocking as well.
  if (DisplayLockUtilities::NeedsActivationForFindInPage(range)) {
    return true;
  }

  return false;
}

void AnnotationAgentImpl::PerformPreAttachDOMMutation() {
  if (IsValidRange(pending_range_)) {
    // TODO(crbug.com/1252872): Only |first_node| is considered for the below
    // ancestor expanding code, but we should be considering the entire range
    // of selected text for ancestor unlocking as well.
    Node& first_node = *pending_range_->ToEphemeralRange().Nodes().begin();

    // Activate content-visibility:auto subtrees if needed.
    DisplayLockUtilities::ActivateFindInPageMatchRangeIfNeeded(
        pending_range_->ToEphemeralRange());

    // If the active match is hidden inside a <details> element or a
    // hidden=until-found element, then we should expand it so we can scroll to
    // it.
    if (DisplayLockUtilities::RevealAutoExpandableAncestors(first_node)
            .revealed_details) {
      UseCounter::Count(
          first_node.GetDocument(),
          WebFeature::kAutoExpandedDetailsForScrollToTextFragment);
    }

    // Ensure we leave clean layout since we'll be applying markers after this.
    first_node.GetDocument().UpdateStyleAndLayout(
        DocumentUpdateReason::kFindInPage);
  }

  ProcessAttachmentFinished();
}

void AnnotationAgentImpl::ProcessAttachmentFinished() {
  CHECK(!attached_range_);

  // See IsValidRangeAndMarkable for why we treat some types differently here.
  bool pending_range_valid = ShouldUseIsValidRangeAndMarkable(type_)
                                 ? IsValidRangeAndMarkable(pending_range_)
                                 : IsValidRange(pending_range_);

  if (pending_range_valid) {
    attached_range_ = pending_range_;

    TRACE_EVENT_INSTANT("blink", "IsAttached");

    EphemeralRange dom_range =
        EphemeralRange(ToPositionInDOMTree(attached_range_->StartPosition()),
                       ToPositionInDOMTree(attached_range_->EndPosition()));
    Document* document = attached_range_->StartPosition().GetDocument();
    DCHECK(document);

    switch (type_) {
      case mojom::blink::AnnotationType::kUserNote:
      case mojom::blink::AnnotationType::kSharedHighlight: {
        document->Markers().AddTextFragmentMarker(dom_range);
        document->Markers().MergeOverlappingMarkers(
            DocumentMarker::kTextFragment);
        break;
      }
      case mojom::blink::AnnotationType::kGlic: {
        document->Markers().AddGlicMarker(dom_range);
        break;
      }
      case mojom::blink::AnnotationType::kTextFinder: {
        // TextFinder type is used only to determine whether a given text can be
        // found in the page, it should have no side-effects.
        break;
      }
    }

    if (type_ != mojom::blink::AnnotationType::kUserNote) {
      Node* anchor_node = attached_range_->StartPosition().AnchorNode();
      CHECK(anchor_node);
      if (anchor_node->IsInShadowTree()) {
        UseCounter::Count(document, WebFeature::kTextDirectiveInShadowDOM);
      }
    }
  } else {
    TRACE_EVENT_INSTANT("blink", "NotAttached");
  }

  pending_range_.Clear();

  // If we're bound to one, let the host know we've finished attempting to
  // attach.
  // TODO(bokan): Perhaps we should keep track of whether we've called
  // DidFinishFindRange and, if set, call the host method when binding.
  if (agent_host_.is_bound()) {
    gfx::Rect range_rect_in_document;
    if (IsAttached()) {
      gfx::Rect rect_in_frame =
          ComputeTextRect(attached_range_->ToEphemeralRange());

      Document* document = attached_range_->StartPosition().GetDocument();
      DCHECK(document);

      LocalFrameView* view = document->View();
      DCHECK(view);

      range_rect_in_document = view->FrameToDocument(rect_in_frame);
    }

    mojom::blink::AttachmentResult attachment_result =
        IsAttached() ? mojom::blink::AttachmentResult::kSuccess
                     : mojom::blink::AttachmentResult::kSelectorNotMatched;
    agent_host_->DidFinishAttachment(range_rect_in_document, attachment_result);
  }
}

bool AnnotationAgentImpl::IsRemoved() const {
  // selector_ and owning_container_ should only ever be null if the agent was
  // removed.
  DCHECK_EQ(!owning_container_, !selector_);

  // If the agent is removed, all its state should be cleared.
  DCHECK(owning_container_ || !attached_range_);
  DCHECK(owning_container_ || !agent_host_.is_bound());
  DCHECK(owning_container_ || !receiver_.is_bound());
  return !owning_container_;
}

mojom::blink::ScrollBehavior AnnotationAgentImpl::ComputeScrollIntoViewBehavior(
    const PhysicalRect& bounding_box,
    const mojom::blink::ScrollIntoViewParams& params) const {
  using mojom::blink::AnnotationType;
  using mojom::blink::ScrollBehavior;

  CHECK(owning_container_->GetSupplementable());
  Document* document = owning_container_->GetSupplementable();
  if (document->GetSettings() &&
      document->GetSettings()->GetPrefersReducedMotion()) {
    return ScrollBehavior::kInstant;
  }

  switch (type_) {
    case AnnotationType::kSharedHighlight:
    case AnnotationType::kTextFinder:
    case AnnotationType::kUserNote:
      return ScrollBehavior::kAuto;
    case AnnotationType::kGlic:
      // Use kInstant for long scroll distances, kSmooth otherwise.
      if (LocalFrameView* view = document->GetFrame()->View()) {
        float max_distance_in_dips =
            CalculateMaxScrollOffsetPx(view, bounding_box, params);
        if (ChromeClient* client = view->GetChromeClient()) {
          // Note: We explicitly don't use `LocalFrame::DevicePixelRatio` or
          // `LocalFrame::LayoutZoomFactor` as both are affected by browser
          // zoom, and we don't want to allow longer scrolls (in physical
          // pixels) when content is zoomed.
          const float device_scale_factor =
              client->GetScreenInfo(view->GetFrame()).device_scale_factor;
          max_distance_in_dips = max_distance_in_dips / device_scale_factor;
        }
        base::UmaHistogramCustomCounts("Glic.ScrollTo.ScrollDistance",
                                       max_distance_in_dips, 1, 500000, 50);
        if (max_distance_in_dips < GetGlicSmoothScrollThresholdInDIPs()) {
          return ScrollBehavior::kSmooth;
        }
      }
      return ScrollBehavior::kInstant;
  }
}

}  // namespace blink

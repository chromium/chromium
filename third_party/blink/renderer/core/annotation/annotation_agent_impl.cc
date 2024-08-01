// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/annotation/annotation_agent_impl.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_container_impl.h"
#include "third_party/blink/renderer/core/annotation/annotation_selector.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/text_fragment_marker.h"
#include "third_party/blink/renderer/core/editing/range_in_flat_tree.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/highlight/highlight_style_utils.h"
#include "third_party/blink/renderer/core/html/html_details_element.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {
bool IsValidRange(const RangeInFlatTree* range) {
  // An attached range may have !IsCollapsed but converting to EphemeralRange
  // results in IsCollapsed. For an example, see
  // AnnotationAgentImplTest.ScrollIntoViewCollapsedRange.
  return range && range->IsConnected() && !range->IsCollapsed() &&
         !range->ToEphemeralRange().IsCollapsed();
}

// There are several cases where text isn't visible/presented to the user but
// does appear findable to FindBuffer. The TextFinder use case wants to prevent
// offering scrolls to these sections as its confusing (in fact, document
// Markers will avoid creating a highlight for these, despite the fact we can
// scroll to it). We probably want to do this for general SharedHighlights as
// well but that will require some more thought and spec changes but we can
// experiment with this for TextFinder to see how it works.
bool IsValidRangeForTextFinder(const RangeInFlatTree* range) {
  if (!IsValidRange(range)) {
    return false;
  }

  EphemeralRangeInFlatTree ephemeral_range = range->ToEphemeralRange();

  // Technically, the text could span multiple Elements, each of which could
  // hide overflow. However, that doesn't seem to be common so do the more
  // performant thing and check the common ancestor.
  Node* common_node = ephemeral_range.CommonAncestorContainer();

  LayoutObject* object = common_node->GetLayoutObject();
  CHECK(object);

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
      if (box->StyleRef().OverflowX() != EOverflow::kVisible &&
          box->Size().width.RawValue() <= 0) {
        return false;
      }

      if (box->StyleRef().OverflowY() != EOverflow::kVisible &&
          box->Size().height.RawValue() <= 0) {
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
      if (!view_rect.Intersects(
              common_node->GetLayoutObject()
                  ->AbsoluteBoundingBoxRectForScrollIntoView())) {
        return false;
      }
    }
  }

  return true;
}
}  // namespace

AnnotationAgentImpl::AnnotationAgentImpl(
    AnnotationAgentContainerImpl& owning_container,
    mojom::blink::AnnotationType annotation_type,
    AnnotationSelector& selector,
    AnnotationAgentContainerImpl::PassKey)
    : agent_host_(owning_container.GetSupplementable()->GetExecutionContext()),
      receiver_(this,
                owning_container.GetSupplementable()->GetExecutionContext()),
      owning_container_(&owning_container),
      selector_(&selector),
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
  receiver_.set_disconnect_handler(
      WTF::BindOnce(&AnnotationAgentImpl::Remove, WrapWeakPersistent(this)));
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

  needs_attachment_ = false;
  Document& document = *owning_container_->GetSupplementable();
  selector_->FindRange(document, AnnotationSelector::kSynchronous,
                       WTF::BindOnce(&AnnotationAgentImpl::DidFinishFindRange,
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

void AnnotationAgentImpl::Remove() {
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

      document->Markers().RemoveMarkersInRange(
          dom_range, DocumentMarker::MarkerTypes::TextFragment());
    }
  }

  attached_range_.Clear();
  pending_range_.Clear();

  agent_host_.reset();
  receiver_.reset();
  owning_container_->RemoveAgent(*this, PassKey());

  selector_.Clear();
  owning_container_.Clear();
}

void AnnotationAgentImpl::ScrollIntoView() const {
  DCHECK(!IsRemoved());

  if (!IsAttached())
    return;

  EphemeralRangeInFlatTree range = attached_range_->ToEphemeralRange();
  CHECK(range.Nodes().begin() != range.Nodes().end());
  Node& first_node = *range.Nodes().begin();

  Document& document = *owning_container_->GetSupplementable();
  document.EnsurePaintLocationDataValidForNode(
      &first_node, DocumentUpdateReason::kFindInPage);

  // TODO(bokan): Text can be attached without having a LayoutObject since it
  // may be inside an unexpanded <details> element or inside a
  // `content-visibility: auto` subtree. In those cases we should make sure we
  // expand/make-visible the node. This is implemented in TextFragmentAnchor
  // but that doesn't cover all cases we can get here so we should migrate that
  // code here.
  if (!first_node.GetLayoutObject()) {
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

  scroll_into_view_util::ScrollRectToVisible(*first_node.GetLayoutObject(),
                                             bounding_box, std::move(params));
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
        WTF::BindOnce(&AnnotationAgentImpl::PerformPreAttachDOMMutation,
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

    // If the active match is hidden inside a <details> element, then we should
    // expand it so we can scroll to it.
    if (HTMLDetailsElement::ExpandDetailsAncestors(first_node)) {
      UseCounter::Count(
          first_node.GetDocument(),
          WebFeature::kAutoExpandedDetailsForScrollToTextFragment);
    }

    // If the active match is hidden inside a hidden=until-found element, then
    // we should reveal it so we can scroll to it.
    DisplayLockUtilities::RevealHiddenUntilFoundAncestors(first_node);

    // Ensure we leave clean layout since we'll be applying markers after this.
    first_node.GetDocument().UpdateStyleAndLayout(
        DocumentUpdateReason::kFindInPage);
  }

  ProcessAttachmentFinished();
}

void AnnotationAgentImpl::ProcessAttachmentFinished() {
  CHECK(!attached_range_);

  // See IsValidRangeForTextFinder for why we treat kTextFinder differently
  // here.
  bool pending_range_valid = type_ == mojom::blink::AnnotationType::kTextFinder
                                 ? IsValidRangeForTextFinder(pending_range_)
                                 : IsValidRange(pending_range_);

  if (pending_range_valid) {
    attached_range_ = pending_range_;

    TRACE_EVENT_INSTANT("blink", "IsAttached");

    EphemeralRange dom_range =
        EphemeralRange(ToPositionInDOMTree(attached_range_->StartPosition()),
                       ToPositionInDOMTree(attached_range_->EndPosition()));
    Document* document = attached_range_->StartPosition().GetDocument();
    DCHECK(document);

    // TextFinder type is used only to determine whether a given text can be
    // found in the page, it should have no side-effects.
    if (type_ != mojom::blink::AnnotationType::kTextFinder) {
      document->Markers().AddTextFragmentMarker(dom_range);
      document->Markers().MergeOverlappingMarkers(
          DocumentMarker::kTextFragment);
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

    // Empty rect means the selector didn't find its content.
    agent_host_->DidFinishAttachment(range_rect_in_document);
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

}  // namespace blink

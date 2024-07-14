// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/text_fragment_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/shared_highlighting/core/common/disabled_sites.h"
#include "components/shared_highlighting/core/common/fragment_directives_utils.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_impl.h"
#include "third_party/blink/renderer/core/annotation/annotation_selector.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/range_in_flat_tree.h"
#include "third_party/blink/renderer/core/editing/selection_editor.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/fragment_directive/fragment_directive_utils.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_anchor.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_selector_generator.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"

namespace blink {

TextFragmentHandler::TextFragmentHandler(LocalFrame* frame) : frame_(frame) {}

// TODO(http://crbug/1262142): lazily bind once and not re-bind each time.
void TextFragmentHandler::BindTextFragmentReceiver(
    mojo::PendingReceiver<mojom::blink::TextFragmentReceiver> producer) {
  selector_producer_.reset();
  selector_producer_.Bind(
      std::move(producer),
      GetFrame()->GetTaskRunner(blink::TaskType::kInternalDefault));
}

void TextFragmentHandler::Cancel() {
  // TODO(crbug.com/1303881): This shouldn't happen, but sometimes browser
  // side requests link to text when generation was never started.
  // See crash in crbug.com/1301794.
  if (!GetTextFragmentSelectorGenerator())
    return;

  GetTextFragmentSelectorGenerator()->Reset();
}

void TextFragmentHandler::RequestSelector(RequestSelectorCallback callback) {
  DCHECK(shared_highlighting::ShouldOfferLinkToText(
      GURL(GetFrame()->GetDocument()->Url())));

  response_callback_ = std::move(callback);
  selector_ready_status_ =
      preemptive_generation_result_.has_value()
          ? shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady
          : shared_highlighting::LinkGenerationReadyStatus::
                kRequestedBeforeReady;

  if (!GetTextFragmentSelectorGenerator()) {
    // TODO(crbug.com/1303881): This shouldn't happen, but sometimes browser
    // side requests link to text when generation was never started.
    // See crash in crbug.com/1301794.
    error_ = shared_highlighting::LinkGenerationError::kNotGenerated;
    InvokeReplyCallback(
        TextFragmentSelector(TextFragmentSelector::SelectorType::kInvalid),
        error_);
    return;
  }

  GetTextFragmentSelectorGenerator()->RecordSelectorStateUma();

  // If generation finished simply respond with the result. Otherwise, the
  // response callback is stored so that we reply on completion.
  if (selector_ready_status_.value() ==
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady)
    InvokeReplyCallback(preemptive_generation_result_.value(), error_);
}

void TextFragmentHandler::GetExistingSelectors(
    GetExistingSelectorsCallback callback) {
  Vector<String> text_fragment_selectors;

  for (auto& annotation : annotation_agents_) {
    if (annotation->IsAttached())
      text_fragment_selectors.push_back(annotation->GetSelector()->Serialize());
  }

  std::move(callback).Run(text_fragment_selectors);
}

void TextFragmentHandler::RemoveFragments() {
  // DismissFragmentAnchor normally runs the URL update steps to remove the
  // selectors from the URL. However, even if the outermost main frame doesn't
  // have a text fragment anchor, the selectors still need to be removed from
  // the URL. This is because dismissing the text fragment anchors is a
  // page-wide operation, and the URL might have selectors for a subframe.
  FragmentDirectiveUtils::RemoveSelectorsFromUrl(GetFrame());
  for (auto& annotation : annotation_agents_)
    annotation->Remove();

  annotation_agents_.clear();

  GetFrame()->View()->ClearFragmentAnchor();
}

// static
bool TextFragmentHandler::IsOverTextFragment(const HitTestResult& result) {
  if (!result.InnerNode() || !result.InnerNodeFrame()) {
    return false;
  }

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
      DocumentMarker::MarkerTypes::TextFragment());
  return !markers.empty();
}

void TextFragmentHandler::ExtractTextFragmentsMatches(
    ExtractTextFragmentsMatchesCallback callback) {
  Vector<String> text_fragment_matches;

  for (auto& annotation : annotation_agents_) {
    if (annotation->IsAttached()) {
      text_fragment_matches.push_back(
          PlainText(annotation->GetAttachedRange().ToEphemeralRange()));
    }
  }

  std::move(callback).Run(text_fragment_matches);
}

void TextFragmentHandler::ExtractFirstFragmentRect(
    ExtractFirstFragmentRectCallback callback) {
  gfx::Rect rect_in_viewport;

  if (annotation_agents_.empty()) {
    std::move(callback).Run(gfx::Rect());
    return;
  }

  for (auto& annotation : annotation_agents_) {
    if (!annotation->IsAttached())
      continue;

    PhysicalRect bounding_box(
        ComputeTextRect(annotation->GetAttachedRange().ToEphemeralRange()));
    rect_in_viewport =
        GetFrame()->View()->FrameToViewport(ToEnclosingRect(bounding_box));
    break;
  }

  std::move(callback).Run(rect_in_viewport);
}

void TextFragmentHandler::DidFinishSelectorGeneration(
    const TextFragmentSelector& selector,
    shared_highlighting::LinkGenerationError error) {
  DCHECK(!preemptive_generation_result_.has_value());

  if (response_callback_) {
    InvokeReplyCallback(selector, error);
  } else {
    // If we don't have a callback yet, it's because we started generating
    // preemptively. We'll store the result so that when the selector actually
    // is requested we can simply use the stored result.
    preemptive_generation_result_.emplace(selector);
    error_ = error;
  }
}

void TextFragmentHandler::StartGeneratingForCurrentSelection() {
  preemptive_generation_result_.reset();
  error_ = shared_highlighting::LinkGenerationError::kNone;
  selector_ready_status_.reset();

  // It is possible we have unserved callback, but if we are starting a new
  // generation, then we have a new selection, in which case it is safe to
  // assume that the client is not waiting for the callback return.
  response_callback_.Reset();

  VisibleSelectionInFlatTree selection =
      GetFrame()->Selection().ComputeVisibleSelectionInFlatTree();
  EphemeralRangeInFlatTree selection_range(selection.Start(), selection.End());
  RangeInFlatTree* current_selection_range =
      MakeGarbageCollected<RangeInFlatTree>(selection_range.StartPosition(),
                                            selection_range.EndPosition());
  if (!GetTextFragmentSelectorGenerator()) {
    text_fragment_selector_generator_ =
        MakeGarbageCollected<TextFragmentSelectorGenerator>(GetFrame());
  }
  GetTextFragmentSelectorGenerator()->Generate(
      *current_selection_range,
      WTF::BindOnce(&TextFragmentHandler::DidFinishSelectorGeneration,
                    WrapWeakPersistent(this)));
}

void TextFragmentHandler::Trace(Visitor* visitor) const {
  visitor->Trace(annotation_agents_);
  visitor->Trace(text_fragment_selector_generator_);
  visitor->Trace(selector_producer_);
  visitor->Trace(frame_);
}

void TextFragmentHandler::DidDetachDocumentOrFrame() {
  // Clear out any state in the generator and cancel pending tasks so they
  // don't run after frame detachment.
  if (GetTextFragmentSelectorGenerator()) {
    GetTextFragmentSelectorGenerator()->Reset();
    // The generator is preserved since that's used in RequestSelector to
    // determine whether to respond with kNotGenerated.
  }

  annotation_agents_.clear();
}

void TextFragmentHandler::InvokeReplyCallback(
    const TextFragmentSelector& selector,
    shared_highlighting::LinkGenerationError error) {
  DCHECK(response_callback_);
  DCHECK(selector_ready_status_.has_value());

  std::move(response_callback_)
      .Run(selector.ToString(), error, selector_ready_status_.value());

  // After reply is sent it is safe to reset the generator.
  if (GetTextFragmentSelectorGenerator())
    GetTextFragmentSelectorGenerator()->Reset();
}

TextFragmentAnchor* TextFragmentHandler::GetTextFragmentAnchor() {
  if (!GetFrame() || !GetFrame()->View()) {
    return nullptr;
  }
  FragmentAnchor* fragmentAnchor = GetFrame()->View()->GetFragmentAnchor();
  if (!fragmentAnchor || !fragmentAnchor->IsTextFragmentAnchor()) {
    return nullptr;
  }
  return static_cast<TextFragmentAnchor*>(fragmentAnchor);
}

// static
bool TextFragmentHandler::ShouldPreemptivelyGenerateFor(LocalFrame* frame) {
  if (frame->GetTextFragmentHandler())
    return true;

  // Always preemptively generate for outermost main frame.
  if (frame->IsOutermostMainFrame())
    return true;

  // Only generate for iframe urls if they are supported
  return shared_highlighting::SupportsLinkGenerationInIframe(
      GURL(frame->GetDocument()->Url()));
}

// static
void TextFragmentHandler::OpenedContextMenuOverSelection(LocalFrame* frame) {
  if (!TextFragmentHandler::ShouldPreemptivelyGenerateFor(frame))
    return;

  if (!shared_highlighting::ShouldOfferLinkToText(
          GURL(frame->GetDocument()->Url()))) {
    return;
  }

  if (frame->Selection().SelectedText().empty())
    return;

  if (!frame->GetTextFragmentHandler())
    frame->CreateTextFragmentHandler();

  frame->GetTextFragmentHandler()->StartGeneratingForCurrentSelection();
}

// static
void TextFragmentHandler::DidCreateTextFragment(AnnotationAgentImpl& agent,
                                                Document& owning_document) {
  LocalFrame* frame = owning_document.GetFrame();
  DCHECK(frame);

  if (!frame->GetTextFragmentHandler())
    frame->CreateTextFragmentHandler();

  frame->GetTextFragmentHandler()->annotation_agents_.push_back(&agent);
}

}  // namespace blink

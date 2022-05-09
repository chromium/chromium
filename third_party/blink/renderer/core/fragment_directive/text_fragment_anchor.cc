// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/text_fragment_anchor.h"

#include "base/auto_reset.h"
#include "base/trace_event/typed_macros.h"
#include "components/shared_highlighting/core/common/fragment_directives_utils.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/fragment_directive/fragment_directive.h"
#include "third_party/blink/renderer/core/fragment_directive/fragment_directive_utils.h"
#include "third_party/blink/renderer/core/fragment_directive/text_directive.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_handler.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_selector.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_details_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/search_engine_utils.h"

namespace blink {

namespace {

bool CheckSecurityRestrictions(LocalFrame& frame) {
  // This algorithm checks the security restrictions detailed in
  // https://wicg.github.io/ScrollToTextFragment/#should-allow-a-text-fragment
  // TODO(bokan): These are really only relevant for observable actions like
  // scrolling. We should consider allowing highlighting regardless of these
  // conditions. See the TODO in the relevant spec section:
  // https://wicg.github.io/ScrollToTextFragment/#restricting-the-text-fragment

  if (!frame.Loader().GetDocumentLoader()->ConsumeTextFragmentToken()) {
    TRACE_EVENT_INSTANT("blink", "CheckSecurityRestrictions", "Result",
                        "No Token");
    return false;
  }

  if (frame.GetDocument()->contentType() != "text/html") {
    TRACE_EVENT_INSTANT("blink", "CheckSecurityRestrictions", "Result",
                        "Invalid ContentType");
    return false;
  }

  // For cross origin initiated navigations, we only allow text
  // fragments if the frame is not script accessible by another frame, i.e. no
  // cross origin iframes or window.open.
  if (!frame.Loader()
           .GetDocumentLoader()
           ->LastNavigationHadTrustedInitiator()) {
    if (frame.Tree().Parent()) {
      TRACE_EVENT_INSTANT("blink", "CheckSecurityRestrictions", "Result",
                          "Cross-Origin Subframe");
      return false;
    }

    if (frame.GetPage()->RelatedPages().size()) {
      TRACE_EVENT_INSTANT("blink", "CheckSecurityRestrictions", "Result",
                          "Non-Empty Browsing Context Group");
      return false;
    }
  }

  TRACE_EVENT_INSTANT("blink", "CheckSecurityRestrictions", "Result", "Pass");
  return true;
}

}  // namespace

// static
bool TextFragmentAnchor::GenerateNewToken(const DocumentLoader& loader) {
  // Avoid invoking the text fragment for history, reload as they'll be
  // clobbered by scroll restoration anyway. In particular, history navigation
  // is considered browser initiated even if performed via non-activated script
  // so we don't want this case to produce a token. See
  // https://crbug.com/1042986 for details. Note: this also blocks form
  // navigations.
  if (loader.GetNavigationType() != kWebNavigationTypeLinkClicked &&
      loader.GetNavigationType() != kWebNavigationTypeOther) {
    return false;
  }

  // A new permission to invoke should only be granted if the navigation had a
  // transient user activation attached to it. Browser initiated navigations
  // (e.g. typed address in the omnibox) don't carry the transient user
  // activation bit so we have to check that separately but we consider that
  // user initiated as well.
  return loader.LastNavigationHadTransientUserActivation() ||
         loader.IsBrowserInitiated();
}

// static
bool TextFragmentAnchor::GenerateNewTokenForSameDocument(
    const DocumentLoader& loader,
    WebFrameLoadType load_type,
    mojom::blink::SameDocumentNavigationType same_document_navigation_type) {
  if ((load_type != WebFrameLoadType::kStandard &&
       load_type != WebFrameLoadType::kReplaceCurrentItem) ||
      same_document_navigation_type !=
          mojom::blink::SameDocumentNavigationType::kFragment)
    return false;

  // Same-document text fragment navigations are allowed only when initiated
  // from the browser process (e.g. typing in the omnibox) or a same-origin
  // document. This is restricted by the spec:
  // https://wicg.github.io/scroll-to-text-fragment/#restricting-the-text-fragment.
  if (!loader.LastNavigationHadTrustedInitiator()) {
    return false;
  }

  // Only generate a token if it's going to be consumed (i.e. the new fragment
  // has a text fragment in it).
  FragmentDirective& fragment_directive =
      loader.GetFrame()->GetDocument()->fragmentDirective();
  if (!fragment_directive.LastNavigationHadFragmentDirective() ||
      fragment_directive.GetDirectives<TextDirective>().IsEmpty()) {
    return false;
  }

  return true;
}

// static
TextFragmentAnchor* TextFragmentAnchor::TryCreate(const KURL& url,
                                                  LocalFrame& frame,
                                                  bool should_scroll) {
  DCHECK(RuntimeEnabledFeatures::TextFragmentIdentifiersEnabled(
      frame.DomWindow()));

  HeapVector<Member<TextDirective>> text_directives =
      frame.GetDocument()->fragmentDirective().GetDirectives<TextDirective>();
  if (text_directives.IsEmpty()) {
    if (frame.GetDocument()
            ->fragmentDirective()
            .LastNavigationHadFragmentDirective()) {
      UseCounter::Count(frame.GetDocument(),
                        WebFeature::kInvalidFragmentDirective);
    }
    return nullptr;
  }

  TRACE_EVENT("blink", "TextFragmentAnchor::TryCreate", "url", url,
              "should_scroll", should_scroll);

  if (!CheckSecurityRestrictions(frame)) {
    return nullptr;
  } else if (!should_scroll) {
    if (frame.Loader().GetDocumentLoader() &&
        !frame.Loader().GetDocumentLoader()->NavigationScrollAllowed()) {
      // We want to record a use counter whenever a text-fragment is blocked by
      // ForceLoadAtTop.  If we passed security checks but |should_scroll| was
      // passed in false, we must have calculated |block_fragment_scroll| in
      // FragmentLoader::ProcessFragment. This can happen in one of two cases:
      //   1) Blocked by ForceLoadAtTop - what we want to measure
      //   2) Blocked because we're restoring from history. However, in this
      //      case we'd not pass security restrictions because we filter out
      //      history navigations.
      UseCounter::Count(frame.GetDocument(),
                        WebFeature::kTextFragmentBlockedByForceLoadAtTop);
    }
  }

  return MakeGarbageCollected<TextFragmentAnchor>(text_directives, frame,
                                                  should_scroll);
}

TextFragmentAnchor::TextFragmentAnchor(
    HeapVector<Member<TextDirective>>& text_directives,
    LocalFrame& frame,
    bool should_scroll)
    : SelectorFragmentAnchor(frame, should_scroll),
      metrics_(MakeGarbageCollected<TextFragmentAnchorMetrics>(
          frame_->GetDocument())) {
  TRACE_EVENT("blink", "TextFragmentAnchor::TextFragmentAnchor");
  DCHECK(!text_directives.IsEmpty());
  DCHECK(frame_->View());

  metrics_->DidCreateAnchor(text_directives.size());

  directive_finder_pairs_.ReserveCapacity(text_directives.size());
  for (Member<TextDirective>& directive : text_directives) {
    directive_finder_pairs_.push_back(std::make_pair(
        directive,
        MakeGarbageCollected<TextFragmentFinder>(
            *this, directive->GetSelector(), frame_->GetDocument(),
            TextFragmentFinder::FindBufferRunnerType::kSynchronous)));
  }
}

bool TextFragmentAnchor::InvokeSelector() {
  // We need to keep this TextFragmentAnchor alive if we're proxying an
  // element fragment anchor.
  if (element_fragment_anchor_) {
    DCHECK(search_finished_);
    return true;
  }

  // Only invoke once, and then a second time once the document is loaded.
  // Otherwise page load performance could be significantly
  // degraded, since TextFragmentFinder has O(n) performance. The reason
  // for invoking twice is to give client-side rendered sites more opportunity
  // to add text that can participate in text fragment invocation.
  if (!frame_->GetDocument()->IsLoadCompleted()) {
    // When parsing is complete the following sequence happens:
    // 1. Invoke with beforematch_state_ == kNoMatchFound. This runs a match and
    //    causes beforematch_state_ to be set to kEventQueued, and queues
    //    a task to set beforematch_state_ to be set to kFiredEvent.
    // 2. (maybe) Invoke with beforematch_state_ == kEventQueued.
    // 3. Invoke with beforematch_state_ == kFiredEvent. This runs a match and
    //    causes text_searched_after_parsing_finished_ to become true.
    // 4. Any future calls to Invoke before loading are ignored.
    //
    // TODO(chrishtr): if layout is not dirtied, we don't need to re-run
    // the text finding again and again for each of the above steps.
    if (has_performed_first_text_search_ && beforematch_state_ != kEventQueued)
      return true;
  }

  // If we're done searching, return true if this hasn't been dismissed yet so
  // that this is kept alive.
  if (search_finished_)
    return !dismissed_ || needs_perform_pre_raf_actions_;

  frame_->GetDocument()->Markers().RemoveMarkersOfTypes(
      DocumentMarker::MarkerTypes::TextFragment());

  if (!did_find_match_) {
    metrics_->DidStartSearch();
  }

  first_match_needs_scroll_ = should_scroll_ && !user_scrolled_;

  {
    // FindMatch might cause scrolling and set user_scrolled_ so reset it when
    // it's done.
    base::AutoReset<bool> reset_user_scrolled(&user_scrolled_, user_scrolled_);

    metrics_->ResetMatchCount();
    for (auto& directive_finder_pair : directive_finder_pairs_)
      directive_finder_pair.second->FindMatch();
  }

  if (beforematch_state_ != kEventQueued)
    has_performed_first_text_search_ = true;

  // Stop searching for matching text once the load event has fired. This may
  // cause ScrollToTextFragment to not work on pages which dynamically load
  // content: http://crbug.com/963045
  if (frame_->GetDocument()->IsLoadCompleted() &&
      beforematch_state_ != kEventQueued)
    DidFinishSearch();

  // We return true to keep this anchor alive as long as we need another invoke,
  // are waiting to be dismissed, or are proxying an element fragment anchor.
  // TODO(bokan): There's a lot of implicit state here, lets clean this up into
  // a more explicit state machine.
  return !search_finished_ || !dismissed_ || needs_perform_pre_raf_actions_ ||
         beforematch_state_ == kEventQueued;
}

void TextFragmentAnchor::Installed() {}

void TextFragmentAnchor::PerformPreRafActions() {
  if (!needs_perform_pre_raf_actions_)
    return;

  needs_perform_pre_raf_actions_ = false;

  if (element_fragment_anchor_) {
    element_fragment_anchor_->Installed();
    element_fragment_anchor_->Invoke();
    element_fragment_anchor_->PerformPreRafActions();
    element_fragment_anchor_ = nullptr;
  }

  // Notify the DOM object exposed to JavaScript that we've completed the
  // search and pass it the range we found.
  for (DirectiveFinderPair& directive_finder_pair : directive_finder_pairs_) {
    TextDirective* text_directive = directive_finder_pair.first.Get();
    TextFragmentFinder* finder = directive_finder_pair.second.Get();
    text_directive->DidFinishMatching(finder->FirstMatch());
  }
}

void TextFragmentAnchor::Trace(Visitor* visitor) const {
  visitor->Trace(element_fragment_anchor_);
  visitor->Trace(metrics_);
  visitor->Trace(directive_finder_pairs_);
  SelectorFragmentAnchor::Trace(visitor);
}

void TextFragmentAnchor::DidFindMatch(const RangeInFlatTree& range,
                                      bool is_unique) {
  // TODO(bokan): Can this happen or should this be a DCHECK?
  if (search_finished_)
    return;

  if (!is_unique)
    metrics_->DidFindAmbiguousMatch();

  // TODO(nburris): Determine what we should do with overlapping text matches.
  // This implementation drops a match if it overlaps a previous match, since
  // overlapping ranges are likely unintentional by the URL creator and could
  // therefore indicate that the page text has changed.
  if (!frame_->GetDocument()
           ->Markers()
           .MarkersIntersectingRange(
               range.ToEphemeralRange(),
               DocumentMarker::MarkerTypes::TextFragment())
           .IsEmpty()) {
    return;
  }

  if (beforematch_state_ == kNoMatchFound) {
    Element* enclosing_block =
        EnclosingBlock(range.StartPosition(), kCannotCrossEditingBoundary);
    DCHECK(enclosing_block);
    frame_->GetDocument()->EnqueueAnimationFrameTask(
        WTF::Bind(&TextFragmentAnchor::FireBeforeMatchEvent,
                  WrapPersistent(this), WrapPersistent(&range)));
    beforematch_state_ = kEventQueued;
    return;
  }
  if (beforematch_state_ == kEventQueued)
    return;
  // TODO(jarhar): Consider what to do based on DOM/style modifications made by
  // the beforematch event here and write tests for it once we decide on a
  // behavior here: https://github.com/WICG/display-locking/issues/150

  // Apply :target to the first match
  if (!did_find_match_) {
    ApplyTargetToCommonAncestor(range.ToEphemeralRange());
    frame_->GetDocument()->UpdateStyleAndLayout(
        DocumentUpdateReason::kFindInPage);
  }

  Node& first_node = *range.ToEphemeralRange().Nodes().begin();

  metrics_->DidFindMatch();
  did_find_match_ = true;

  if (first_match_needs_scroll_) {
    first_match_needs_scroll_ = false;

    PhysicalRect bounding_box(ComputeTextRect(range.ToEphemeralRange()));

    // Set the bounding box height to zero because we want to center the top of
    // the text range.
    bounding_box.SetHeight(LayoutUnit());

    DCHECK(range.ToEphemeralRange().Nodes().begin() !=
           range.ToEphemeralRange().Nodes().end());

    DCHECK(first_node.GetLayoutObject());

    // TODO(bokan): Refactor this to use the common
    // FragmentAnchor::ScrollElementIntoViewWithOptions.
    mojom::blink::ScrollIntoViewParamsPtr params =
        ScrollAlignment::CreateScrollIntoViewParams(
            ScrollAlignment::CenterAlways(), ScrollAlignment::CenterAlways(),
            mojom::blink::ScrollType::kProgrammatic);
    params->cross_origin_boundaries = false;
    scroll_into_view_util::ScrollRectToVisible(*first_node.GetLayoutObject(),
                                               bounding_box, std::move(params));
    did_scroll_into_view_ = true;

    if (AXObjectCache* cache = frame_->GetDocument()->ExistingAXObjectCache())
      cache->HandleScrolledToAnchor(&first_node);

    metrics_->DidInvokeScrollIntoView();
  }
  EphemeralRange dom_range =
      EphemeralRange(ToPositionInDOMTree(range.StartPosition()),
                     ToPositionInDOMTree(range.EndPosition()));
  frame_->GetDocument()->Markers().AddTextFragmentMarker(dom_range);

  // Set the sequential focus navigation to the start of selection.
  // Even if this element isn't focusable, "Tab" press will
  // start the search to find the next focusable element from this element.
  frame_->GetDocument()->SetSequentialFocusNavigationStartingPoint(
      range.StartPosition().NodeAsRangeFirstNode());
}

void TextFragmentAnchor::DidFinishSearch() {
  DCHECK(!search_finished_);
  search_finished_ = true;
  needs_perform_pre_raf_actions_ = true;

  metrics_->SetSearchEngineSource(HasSearchEngineSource());
  metrics_->ReportMetrics();

  if (!did_find_match_) {
    dismissed_ = true;

    DCHECK(!element_fragment_anchor_);
    // ElementFragmentAnchor needs to be invoked from PerformPreRafActions
    // since it can cause script to run and we may be in a ScriptForbiddenScope
    // here.
    element_fragment_anchor_ = ElementFragmentAnchor::TryCreate(
        frame_->GetDocument()->Url(), *frame_, should_scroll_);
  }

  frame_->GetPage()->GetChromeClient().ScheduleAnimation(frame_->View());
}

bool TextFragmentAnchor::Dismiss() {
  // To decrease the likelihood of the user dismissing the highlight before
  // seeing it, we only dismiss the anchor after search_finished_, at which
  // point we've scrolled it into view or the user has started scrolling the
  // page.
  if (!search_finished_)
    return false;

  if (!did_find_match_ || dismissed_)
    return true;

  DCHECK(!should_scroll_ || did_scroll_into_view_ || user_scrolled_);

  frame_->GetDocument()->Markers().RemoveMarkersOfTypes(
      DocumentMarker::MarkerTypes::TextFragment());

  return SelectorFragmentAnchor::Dismiss();
}

void TextFragmentAnchor::ApplyTargetToCommonAncestor(
    const EphemeralRangeInFlatTree& range) {
  Node* common_node = range.CommonAncestorContainer();
  while (common_node && common_node->getNodeType() != Node::kElementNode) {
    common_node = common_node->parentNode();
  }

  DCHECK(common_node);
  if (common_node) {
    auto* target = DynamicTo<Element>(common_node);
    frame_->GetDocument()->SetCSSTarget(target);
  }
}

void TextFragmentAnchor::FireBeforeMatchEvent(const RangeInFlatTree* range) {
  // TODO(crbug.com/1252872): Only |first_node| is considered for the below
  // ancestor expanding code, but we should be considering the entire range
  // of selected text for ancestor unlocking as well.
  Node& first_node = *range->ToEphemeralRange().Nodes().begin();

  // Activate content-visibility:auto subtrees if needed.
  DisplayLockUtilities::ActivateFindInPageMatchRangeIfNeeded(
      range->ToEphemeralRange());

  // If the active match is hidden inside a <details> element, then we should
  // expand it so we can scroll to it.
  if (RuntimeEnabledFeatures::AutoExpandDetailsElementEnabled() &&
      HTMLDetailsElement::ExpandDetailsAncestors(first_node)) {
    UseCounter::Count(first_node.GetDocument(),
                      WebFeature::kAutoExpandedDetailsForScrollToTextFragment);
  }

  // If the active match is hidden inside a hidden=until-found element, then we
  // should reveal it so we can scroll to it.
  if (RuntimeEnabledFeatures::BeforeMatchEventEnabled(
          first_node.GetExecutionContext())) {
    DisplayLockUtilities::RevealHiddenUntilFoundAncestors(first_node);
  }

  beforematch_state_ = kFiredEvent;
}

void TextFragmentAnchor::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  metrics_->SetTickClockForTesting(tick_clock);
}

bool TextFragmentAnchor::HasSearchEngineSource() {
  if (!frame_->GetDocument() || !frame_->GetDocument()->Loader())
    return false;

  // Client side redirects should not happen for links opened from search
  // engines. If a redirect occurred, we can't rely on the requestorOrigin as
  // it won't point to the original requestor anymore.
  if (frame_->GetDocument()->Loader()->IsClientRedirect())
    return false;

  // TODO(crbug.com/1133823): Add test case for valid referrer.
  if (!frame_->GetDocument()->Loader()->GetRequestorOrigin())
    return false;

  return IsKnownSearchEngine(
      frame_->GetDocument()->Loader()->GetRequestorOrigin()->ToString());
}

}  // namespace blink

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/text_fragment_anchor.h"

#include "base/auto_reset.h"
#include "base/trace_event/typed_macros.h"
#include "components/shared_highlighting/core/common/fragment_directives_utils.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_container_impl.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_impl.h"
#include "third_party/blink/renderer/core/annotation/text_annotation_selector.h"
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

  AnnotationAgentContainerImpl* annotation_container =
      AnnotationAgentContainerImpl::From(*frame_->GetDocument());
  DCHECK(annotation_container);

  directive_annotation_pairs_.ReserveCapacity(text_directives.size());
  for (Member<TextDirective>& directive : text_directives) {
    auto* selector =
        MakeGarbageCollected<TextAnnotationSelector>(directive->GetSelector());
    AnnotationAgentImpl* agent = annotation_container->CreateUnboundAgent(
        mojom::blink::AnnotationType::kSharedHighlight, *selector);

    directive_annotation_pairs_.push_back(std::make_pair(directive, agent));
  }
}

bool TextFragmentAnchor::InvokeSelector() {
  if (state_ == kDone) {
    return false;
  } else if (state_ == kScriptableActions) {
    // We need to keep this TextFragmentAnchor alive if we're proxying an
    // element fragment anchor or we're still waiting for a rAF to perform
    // post-search actions, but in that case InvokeSelector() should be a no-op.
    return true;
  }

  // Only invoke once, and then a second time once the document is loaded.
  // Otherwise page load performance could be significantly
  // degraded, since TextFragmentFinder has O(n) performance. The reason
  // for invoking twice is to give client-side rendered sites more opportunity
  // to add text that can participate in text fragment invocation.
  if (!frame_->GetDocument()->IsLoadCompleted()) {
    // When parsing is complete the following sequence happens:
    // 1. Invoke with `state_` == kSearching. This runs a match and
    //    causes `state_` to be set to kEventQueued, and queues
    //    a task to set `state_` to be set to kFiredEvent.
    // 2. (maybe) Invoke with `state_` == kEventQueued.
    // 3. Invoke with `state_` == kFiredEvent. This runs a match and
    //    causes `suppress_text_search_until_load_event_` to become true.
    // 4. Any future calls to Invoke before loading are ignored.
    //
    // TODO(chrishtr): if layout is not dirtied, we don't need to re-run
    // the text finding again and again for each of the above steps.
    // TODO(bokan): I'm not sure why we proceed if we're queued? It seems like
    // we could simply return here as well?
    if (suppress_text_search_until_load_event_ &&
        (state_ == kSearching || state_ == kBeforeMatchEventFired)) {
      return true;
    }
  }

  // TODO(bokan): This is needed since we re-search the text of the document
  // each time Invoke is called so after the first Invoke creates text markers
  // subsequent calls will try to create a marker that overlaps and trips our
  // "no overlapping text fragment" DCHECKs. A followup CL will change this
  // class to only perform the text search once since further Invoke calls are
  // used only to continue after a BeforeMatch event or to ensure previously
  // found matches aren't shifted out of view by layout.
  // https://crbug.com/1303887.
  frame_->GetDocument()->Markers().RemoveMarkersOfTypes(
      DocumentMarker::MarkerTypes::TextFragment());

  if (!did_find_match_) {
    metrics_->DidStartSearch();
  }

  // TODO(bokan): Performing attachment is expensive. InvokeSelector is
  // currently called on each BeginMainFrame to push the state machine forward
  // but we're performing attachment each time. This is really inefficient and
  // wasteful. Now that this is all based on AnnotationAgent it should be
  // straight forward to perform attachment only if a given directive hasn't
  // yet been matched.
  // https://crbug.com/1303887.
  {
    // DidFinishAttach might cause scrolling and set user_scrolled_ so reset it
    // when it's done.
    base::AutoReset<bool> reset_user_scrolled(&user_scrolled_, user_scrolled_);

    metrics_->ResetMatchCount();
    for (auto& directive_annotation_pair : directive_annotation_pairs_) {
      AnnotationAgentImpl* annotation = directive_annotation_pair.second;
      annotation->Attach();
      bool did_match = DidFinishAttach(*annotation, did_find_match_);

      if (did_match) {
        metrics_->DidFindMatch();
        did_find_match_ = true;
      }
    }
  }

  // If we found a match, we need to wait for it to fire before doing anything
  // else.
  if (state_ == kBeforeMatchEventQueued)
    return true;

  // Either no matches were found or we've fired a BeforeMatch event and we
  // just finished applying the effects to the matched text snippets.
  DCHECK(state_ == kSearching || state_ == kBeforeMatchEventFired);

  // Stop searching for matching text once the load event has fired. This may
  // cause ScrollToTextFragment to not work on pages which dynamically load
  // content: http://crbug.com/963045
  if (frame_->GetDocument()->IsLoadCompleted())
    DidFinishSearch();
  else
    suppress_text_search_until_load_event_ = true;

  // We return true to keep this anchor alive as long as we need another invoke
  // or have to finish up at the next rAF.
  return state_ != kDone;
}

void TextFragmentAnchor::Installed() {}

void TextFragmentAnchor::PerformPreRafActions() {
  if (state_ != kScriptableActions)
    return;

  if (element_fragment_anchor_) {
    element_fragment_anchor_->Installed();
    element_fragment_anchor_->Invoke();
    element_fragment_anchor_->PerformPreRafActions();
    element_fragment_anchor_ = nullptr;
  }

  // Notify the DOM object exposed to JavaScript that we've completed the
  // search and pass it the range we found.
  for (DirectiveAnnotationPair& directive_annotation_pair :
       directive_annotation_pairs_) {
    TextDirective* text_directive = directive_annotation_pair.first.Get();
    AnnotationAgentImpl* annotation = directive_annotation_pair.second.Get();
    const RangeInFlatTree* attached_range =
        annotation->IsAttached() ? &annotation->GetAttachedRange() : nullptr;
    text_directive->DidFinishMatching(attached_range);
  }

  state_ = kDone;
}

void TextFragmentAnchor::Trace(Visitor* visitor) const {
  visitor->Trace(element_fragment_anchor_);
  visitor->Trace(metrics_);
  visitor->Trace(directive_annotation_pairs_);
  SelectorFragmentAnchor::Trace(visitor);
}

bool TextFragmentAnchor::DidFinishAttach(const AnnotationAgentImpl& annotation,
                                         bool first_match_found) {
  if (!annotation.IsAttached())
    return false;

  DCHECK_LE(state_, kBeforeMatchEventFired);

  if (!static_cast<const TextAnnotationSelector*>(annotation.GetSelector())
           ->WasMatchUnique()) {
    metrics_->DidFindAmbiguousMatch();
  }

  // Everything below is applied only to the first match.
  if (first_match_found)
    return true;

  const RangeInFlatTree& range = annotation.GetAttachedRange();

  // TODO(bokan): This fires an event and reveals only at the first match - it
  // seems like something we may want to do for all highlights on a page?
  // https://crbug.com/1327379.
  if (state_ == kSearching) {
    Element* enclosing_block =
        EnclosingBlock(range.StartPosition(), kCannotCrossEditingBoundary);
    DCHECK(enclosing_block);
    frame_->GetDocument()->EnqueueAnimationFrameTask(
        WTF::Bind(&TextFragmentAnchor::FireBeforeMatchEvent,
                  WrapPersistent(this), WrapPersistent(&range)));
    state_ = kBeforeMatchEventQueued;
    return false;
  }
  if (state_ == kBeforeMatchEventQueued)
    return false;
  // TODO(jarhar): Consider what to do based on DOM/style modifications made by
  // the beforematch event here and write tests for it once we decide on a
  // behavior here: https://github.com/WICG/display-locking/issues/150

  // Apply :target pseudo class.
  ApplyTargetToCommonAncestor(range.ToEphemeralRange());
  frame_->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kFindInPage);

  // Perform scroll and related actions.
  if (should_scroll_ && !user_scrolled_) {
    DCHECK(range.ToEphemeralRange().Nodes().begin() !=
           range.ToEphemeralRange().Nodes().end());

    annotation.ScrollIntoView();

    if (AXObjectCache* cache = frame_->GetDocument()->ExistingAXObjectCache()) {
      Node& first_node = *range.ToEphemeralRange().Nodes().begin();
      cache->HandleScrolledToAnchor(&first_node);
    }

    metrics_->DidInvokeScrollIntoView();

    // Set the sequential focus navigation to the start of selection.
    // Even if this element isn't focusable, "Tab" press will
    // start the search to find the next focusable element from this element.
    frame_->GetDocument()->SetSequentialFocusNavigationStartingPoint(
        range.StartPosition().NodeAsRangeFirstNode());
  }

  return true;
}

void TextFragmentAnchor::DidFinishSearch() {
  DCHECK_LE(state_, kBeforeMatchEventFired);
  state_ = kScriptableActions;

  metrics_->SetSearchEngineSource(HasSearchEngineSource());
  metrics_->ReportMetrics();

  if (!did_find_match_) {
    DCHECK(!element_fragment_anchor_);
    // ElementFragmentAnchor needs to be invoked from PerformPreRafActions
    // since it can cause script to run and we may be in a ScriptForbiddenScope
    // here.
    element_fragment_anchor_ = ElementFragmentAnchor::TryCreate(
        frame_->GetDocument()->Url(), *frame_, should_scroll_);
  }

  // There are actions resulting from matching text fragment that can lead to
  // executing script. These need to happen when script is allowed so schedule
  // a new frame to perform these final actions.
  frame_->GetPage()->GetChromeClient().ScheduleAnimation(frame_->View());
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

  state_ = kBeforeMatchEventFired;
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

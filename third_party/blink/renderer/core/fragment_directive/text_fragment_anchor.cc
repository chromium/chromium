// Copyright 2019 The Chromium Authors
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

  // TODO(bokan): Reevaluate whether it's safe to allow text fragments inside a
  // fenced frame. https://crbug.com/1334788.
  if (frame.IsFencedFrameRoot()) {
    TRACE_EVENT_INSTANT("blink", "CheckSecurityRestrictions", "Result",
                        "Fenced Frame");
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
      fragment_directive.GetDirectives<TextDirective>().empty()) {
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
  if (text_directives.empty()) {
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
  DCHECK(!text_directives.empty());
  DCHECK(frame_->View());

  metrics_->DidCreateAnchor(text_directives.size());

  AnnotationAgentContainerImpl* annotation_container =
      AnnotationAgentContainerImpl::From(*frame_->GetDocument());
  DCHECK(annotation_container);

  directive_annotation_pairs_.reserve(text_directives.size());
  for (Member<TextDirective>& directive : text_directives) {
    auto* selector =
        MakeGarbageCollected<TextAnnotationSelector>(directive->GetSelector());
    AnnotationAgentImpl* agent = annotation_container->CreateUnboundAgent(
        mojom::blink::AnnotationType::kSharedHighlight, *selector);

    directive_annotation_pairs_.push_back(std::make_pair(directive, agent));
  }
}

bool TextFragmentAnchor::InvokeSelector() {
  // InvokeSelector is called repeatedly during the Blink lifecycle, however,
  // attachment (i.e. text searching DOM) is an expensive operation.  Perform
  // it once on the first invoke (after parsing completes) and once again for
  // any unattached directives the first time InvokeSelector is called after
  // the load event in case more content was loaded.
  if (!did_perform_initial_attachment_ ||
      (!did_perform_post_load_attachment_ &&
       frame_->GetDocument()->IsLoadCompleted())) {
    // If this successfully attaches the first directive it will move the anchor
    // into kBeforeMatchEventQueued state.
    TryAttachingUnattachedDirectives();

    did_perform_initial_attachment_ = true;

    if (frame_->GetDocument()->IsLoadCompleted())
      did_perform_post_load_attachment_ = true;
  }

  switch (state_) {
    case kSearching:
      if (frame_->GetDocument()->IsLoadCompleted())
        DidFinishSearch();
      break;
    case kBeforeMatchEventQueued:
      // If a match was found, we need to wait to fire and process the
      // BeforeMatch event before doing anything else so don't try to finish
      // the search yet.
      break;
    case kBeforeMatchEventFired:
      // Now that the event has been processed, apply the necessary effects to
      // the matching DOM nodes.
      ApplyEffectsToFirstMatch();

      // A second text-search pass will occur after the load event has been
      // fired so don't perform any finalization until after that.
      if (frame_->GetDocument()->IsLoadCompleted())
        DidFinishSearch();
      else
        state_ = kEffectsAppliedKeepInView;
      break;
    case kEffectsAppliedKeepInView:
      // Until the load event ensure the matched text is kept in view in the
      // face of layout changes.
      EnsureFirstMatchInViewIfNeeded();
      if (frame_->GetDocument()->IsLoadCompleted())
        DidFinishSearch();
      break;
    case kScriptableActions:
      // The search has finished but we're waiting to apply some effects in a
      // script-safe section. Like above, ensure the match is kept in view.
      if (first_match_)
        EnsureFirstMatchInViewIfNeeded();
      break;
    case kDone:
      break;
  }

  // We return true to keep this anchor alive as long as we need another invoke
  // or have to finish up at the next rAF.
  return state_ != kDone;
}

void TextFragmentAnchor::Installed() {}

void TextFragmentAnchor::PerformScriptableActions() {
  // This is called at the start of each BeginMainFrame regardless of the state
  // is needed only when waiting to invoke actions that need a script-safe
  // section.
  if (state_ != kScriptableActions)
    return;

  DCHECK(frame_->GetDocument()->IsLoadCompleted());

  if (element_fragment_anchor_) {
    element_fragment_anchor_->Installed();
    element_fragment_anchor_->Invoke();
    element_fragment_anchor_->PerformScriptableActions();
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
  visitor->Trace(first_match_);
  SelectorFragmentAnchor::Trace(visitor);
}

void TextFragmentAnchor::TryAttachingUnattachedDirectives() {
  // TODO(bokan): This sets the start time that's used to report
  // TimeToScrollIntoView. Using `!first_match` means the start time will
  // differ based on whether or not we had a match in the first attachment. The
  // TimeToScrollIntoView means to report how long from parsing until the user
  // sees the scroll change so the user-invisible timing shouldn't matter.
  // DidStartSearch should be called once and preferably from
  // TextFragmentAnchor creation (which is what the histogram's description
  // says happens...). https://crbug.com/1327734.
  if (!first_match_)
    metrics_->DidStartSearch();

  for (auto& directive_annotation_pair : directive_annotation_pairs_) {
    AnnotationAgentImpl* annotation = directive_annotation_pair.second;
    if (annotation->IsAttached())
      continue;

    annotation->Attach();
    if (annotation->IsAttached()) {
      if (!first_match_)
        DidFindFirstMatch(*annotation);

      metrics_->DidFindMatch();
      if (!static_cast<const TextAnnotationSelector*>(annotation->GetSelector())
               ->WasMatchUnique()) {
        metrics_->DidFindAmbiguousMatch();
      }
    }
  }
}

void TextFragmentAnchor::DidFindFirstMatch(
    const AnnotationAgentImpl& annotation) {
  DCHECK(annotation.IsAttached());
  DCHECK_EQ(state_, kSearching);
  DCHECK(!first_match_);

  first_match_ = &annotation;

  const RangeInFlatTree& range = annotation.GetAttachedRange();

  // TODO(bokan): This fires an event and reveals only at the first match - it
  // seems like something we may want to do for all highlights on a page?
  // https://crbug.com/1327379.
  Element* enclosing_block =
      EnclosingBlock(range.StartPosition(), kCannotCrossEditingBoundary);
  DCHECK(enclosing_block);
  frame_->GetDocument()->EnqueueAnimationFrameTask(
      WTF::BindOnce(&TextFragmentAnchor::FireBeforeMatchEvent,
                    WrapPersistent(this), WrapPersistent(&range)));

  state_ = kBeforeMatchEventQueued;
}

void TextFragmentAnchor::ApplyEffectsToFirstMatch() {
  DCHECK(first_match_);
  DCHECK_EQ(state_, kBeforeMatchEventFired);

  // TODO(jarhar): Consider what to do based on DOM/style modifications made by
  // the beforematch event here and write tests for it once we decide on a
  // behavior here: https://github.com/WICG/display-locking/issues/150

  // It's possible the DOM the match was attached to was removed by this time.
  if (!first_match_->IsAttached())
    return;

  const RangeInFlatTree& range = first_match_->GetAttachedRange();

  // Apply :target pseudo class.
  ApplyTargetToCommonAncestor(range.ToEphemeralRange());
  frame_->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kFindInPage);

  // Scroll the match into view.
  if (!EnsureFirstMatchInViewIfNeeded())
    return;

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

bool TextFragmentAnchor::EnsureFirstMatchInViewIfNeeded() {
  DCHECK(first_match_);
  DCHECK_GE(state_, kBeforeMatchEventFired);

  if (!should_scroll_ || user_scrolled_)
    return false;

  // It's possible the DOM the match was attached to was removed by this time.
  if (!first_match_->IsAttached())
    return false;

  // Ensure we don't treat the text fragment ScrollIntoView as a user scroll
  // so reset user_scrolled_ when it's done.
  base::AutoReset<bool> reset_user_scrolled(&user_scrolled_, user_scrolled_);
  first_match_->ScrollIntoView();

  return true;
}

void TextFragmentAnchor::DidFinishSearch() {
  DCHECK(frame_->GetDocument()->IsLoadCompleted());
  DCHECK_LE(state_, kEffectsAppliedKeepInView);

  metrics_->SetSearchEngineSource(HasSearchEngineSource());
  metrics_->ReportMetrics();

  bool did_find_any_matches = first_match_;

  if (!did_find_any_matches) {
    DCHECK(!element_fragment_anchor_);
    // ElementFragmentAnchor needs to be invoked from PerformScriptableActions
    // since it can cause script to run and we may be in a ScriptForbiddenScope
    // here.
    element_fragment_anchor_ = ElementFragmentAnchor::TryCreate(
        frame_->GetDocument()->Url(), *frame_, should_scroll_);
  }

  DCHECK(!did_find_any_matches || !element_fragment_anchor_);
  state_ = did_find_any_matches || element_fragment_anchor_ ? kScriptableActions
                                                            : kDone;

  if (state_ == kScriptableActions) {
    // There are actions resulting from matching text fragment that can lead to
    // executing script. These need to happen when script is allowed so schedule
    // a new frame to perform these final actions.
    frame_->GetPage()->GetChromeClient().ScheduleAnimation(frame_->View());
  }
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
  if (!range->IsCollapsed() && range->IsConnected()) {
    // TODO(crbug.com/1252872): Only |first_node| is considered for the below
    // ancestor expanding code, but we should be considering the entire range
    // of selected text for ancestor unlocking as well.
    Node& first_node = *range->ToEphemeralRange().Nodes().begin();

    // Activate content-visibility:auto subtrees if needed.
    DisplayLockUtilities::ActivateFindInPageMatchRangeIfNeeded(
        range->ToEphemeralRange());

    // If the active match is hidden inside a <details> element, then we should
    // expand it so we can scroll to it.
    if (HTMLDetailsElement::ExpandDetailsAncestors(first_node)) {
      UseCounter::Count(
          first_node.GetDocument(),
          WebFeature::kAutoExpandedDetailsForScrollToTextFragment);
    }

    // If the active match is hidden inside a hidden=until-found element, then
    // we should reveal it so we can scroll to it.
    if (RuntimeEnabledFeatures::BeforeMatchEventEnabled(
            first_node.GetExecutionContext())) {
      DisplayLockUtilities::RevealHiddenUntilFoundAncestors(first_node);
    }
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

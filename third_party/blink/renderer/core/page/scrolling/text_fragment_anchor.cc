// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_anchor.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/search_engine_utils.h"

namespace blink {

namespace {

bool ParseTextDirective(const String& fragment_directive,
                        Vector<TextFragmentSelector>* out_selectors) {
  DCHECK(out_selectors);

  size_t start_pos = 0;
  size_t end_pos = 0;
  while (end_pos != kNotFound) {
    if (fragment_directive.Find(kTextFragmentIdentifierPrefix, start_pos) !=
        start_pos) {
      // If this is not a text directive, continue to the next directive
      end_pos = fragment_directive.find('&', start_pos + 1);
      start_pos = end_pos + 1;
      continue;
    }

    start_pos += kTextFragmentIdentifierPrefixStringLength;
    end_pos = fragment_directive.find('&', start_pos);

    String target_text;
    if (end_pos == kNotFound) {
      target_text = fragment_directive.Substring(start_pos);
    } else {
      target_text =
          fragment_directive.Substring(start_pos, end_pos - start_pos);
      start_pos = end_pos + 1;
    }

    TextFragmentSelector selector = TextFragmentSelector::Create(target_text);
    if (selector.Type() != TextFragmentSelector::kInvalid)
      out_selectors->push_back(selector);
  }

  return out_selectors->size() > 0;
}

bool CheckSecurityRestrictions(LocalFrame& frame) {
  // This algorithm checks the security restrictions detailed in
  // https://wicg.github.io/ScrollToTextFragment/#should-allow-a-text-fragment
  // TODO(bokan): These are really only relevant for observable actions like
  // scrolling. We should consider allowing highlighting regardless of these
  // conditions. See the TODO in the relevant spec section:
  // https://wicg.github.io/ScrollToTextFragment/#restricting-the-text-fragment

  // We only allow text fragment anchors for user navigations, e.g. link
  // clicks, omnibox navigations, no script navigations.
  if (!frame.Loader().GetDocumentLoader()->ConsumeTextFragmentToken())
    return false;

  // Allow text fragments on same-origin initiated navigations.
  if (frame.Loader().GetDocumentLoader()->IsSameOriginNavigation())
    return true;

  // Otherwise, for cross origin initiated navigations, we only allow text
  // fragments if the frame is not script accessible by another frame, i.e. no
  // cross origin iframes or window.open.
  if (frame.Tree().Parent() || frame.GetPage()->RelatedPages().size())
    return false;

  return true;
}

}  // namespace

// static
bool TextFragmentAnchor::GenerateNewToken(const DocumentLoader& loader) {
  // Avoid invoking the text fragment for history, reload as they'll be
  // clobbered by scroll restoration anyway. In particular, history navigation
  // is considered browser initiated even if performed via non-activated script
  // so we don't want this case to produce a token. See
  // https://crbug.com/1042986 for details. This will also block form
  // navigations but that's fine since the intent is to generate a token in
  // real cross-page navigations only.
  if (loader.GetNavigationType() != kWebNavigationTypeLinkClicked &&
      loader.GetNavigationType() != kWebNavigationTypeOther) {
    return false;
  }

  // A new permission to invoke should only be granted if the navigation had a
  // user gesture attached to it. Browser initiated navigations (e.g. typed
  // address in the omnibox) don't carry the |had_transient_activation_| bit so
  // we have to check that separately but we consider that user initiated as
  // well.
  return loader.HadTransientActivation() || loader.IsBrowserInitiated();
}

// static
bool TextFragmentAnchor::GenerateNewTokenForSameDocument(
    const String& fragment,
    WebFrameLoadType load_type,
    bool is_content_initiated,
    SameDocumentNavigationSource source) {
  if (load_type != WebFrameLoadType::kStandard ||
      source != kSameDocumentNavigationDefault)
    return false;

  // Only allow browser-initiated navigations are allowed for same-document
  // navigations (e.g. typing in the omnibox). This is restricted by the spec:
  // https://wicg.github.io/scroll-to-text-fragment/#restricting-the-text-fragment.
  // Note: this could change in the future but we should ensure in that case we
  // look for the user gesture on the LocalFrame, rather than DocumentLoader,
  // since the latter's state isn't updated by same document navigations (and
  // hence why we pass individual properties to this method rather than a
  // DocumentLoader reference).
  if (is_content_initiated)
    return false;

  // Only generate a token if it's going to be consumed (i.e. the new fragment
  // has a text fragment in it).
  {
    wtf_size_t start_pos = fragment.Find(kFragmentDirectivePrefix);
    if (start_pos == kNotFound)
      return false;

    String fragment_directive =
        fragment.Substring(start_pos + kFragmentDirectivePrefixStringLength);
    Vector<TextFragmentSelector> selectors;
    if (!ParseTextDirective(fragment_directive, &selectors))
      return false;
  }

  return true;
}

TextFragmentAnchor* TextFragmentAnchor::TryCreateFragmentDirective(
    const KURL& url,
    LocalFrame& frame,
    bool should_scroll) {
  DCHECK(RuntimeEnabledFeatures::TextFragmentIdentifiersEnabled(
      frame.DomWindow()));

  if (!frame.GetDocument()->GetFragmentDirective())
    return nullptr;

  if (!CheckSecurityRestrictions(frame))
    return nullptr;

  Vector<TextFragmentSelector> selectors;

  if (!ParseTextDirective(frame.GetDocument()->GetFragmentDirective(),
                          &selectors)) {
    UseCounter::Count(frame.GetDocument(),
                      WebFeature::kInvalidFragmentDirective);
    return nullptr;
  }

  return MakeGarbageCollected<TextFragmentAnchor>(selectors, frame,
                                                  should_scroll);
}

TextFragmentAnchor::TextFragmentAnchor(
    const Vector<TextFragmentSelector>& text_fragment_selectors,
    LocalFrame& frame,
    bool should_scroll)
    : frame_(&frame),
      should_scroll_(should_scroll),
      metrics_(MakeGarbageCollected<TextFragmentAnchorMetrics>(
          frame_->GetDocument())) {
  DCHECK(!text_fragment_selectors.IsEmpty());
  DCHECK(frame_->View());

  metrics_->DidCreateAnchor(
      text_fragment_selectors.size(),
      frame.GetDocument()->GetFragmentDirective().length());

  text_fragment_finders_.ReserveCapacity(text_fragment_selectors.size());
  for (TextFragmentSelector selector : text_fragment_selectors)
    text_fragment_finders_.emplace_back(*this, selector);
}

bool TextFragmentAnchor::Invoke() {
  // Wait until the page has been made visible before searching.
  if (!frame_->GetPage()->IsPageVisible() && !page_has_been_visible_)
    return true;
  else
    page_has_been_visible_ = true;

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
    return !dismissed_;

  frame_->GetDocument()->Markers().RemoveMarkersOfTypes(
      DocumentMarker::MarkerTypes::TextFragment());

  // TODO(bokan): Once BlockHTMLParserOnStyleSheets is launched, there won't be
  // a way for the user to scroll before we invoke and scroll the anchor. We
  // should confirm if we can remove tracking this after that point or if we
  // need a replacement metric.
  if (user_scrolled_ && !did_scroll_into_view_)
    metrics_->ScrollCancelled();

  if (!did_find_match_) {
    metrics_->DidStartSearch();
  }

  first_match_needs_scroll_ = should_scroll_ && !user_scrolled_;

  {
    // FindMatch might cause scrolling and set user_scrolled_ so reset it when
    // it's done.
    base::AutoReset<bool> reset_user_scrolled(&user_scrolled_, user_scrolled_);

    metrics_->ResetMatchCount();
    for (auto& finder : text_fragment_finders_)
      finder.FindMatch(*frame_->GetDocument());
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
  return !search_finished_ || !dismissed_ || element_fragment_anchor_ ||
         beforematch_state_ == kEventQueued;
}

void TextFragmentAnchor::Installed() {}

void TextFragmentAnchor::DidScroll(mojom::blink::ScrollType type) {
  if (type != mojom::blink::ScrollType::kUser &&
      type != mojom::blink::ScrollType::kCompositor) {
    return;
  }

  Dismiss();
  user_scrolled_ = true;

  if (did_non_zero_scroll_ &&
      frame_->View()->GetScrollableArea()->GetScrollOffset().IsZero()) {
    metrics_->DidScrollToTop();
  }
}

void TextFragmentAnchor::PerformPreRafActions() {
  if (element_fragment_anchor_) {
    element_fragment_anchor_->Installed();
    element_fragment_anchor_->Invoke();
    element_fragment_anchor_->PerformPreRafActions();
    element_fragment_anchor_ = nullptr;
  }
}

void TextFragmentAnchor::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(element_fragment_anchor_);
  visitor->Trace(metrics_);
  FragmentAnchor::Trace(visitor);
}

void TextFragmentAnchor::DidFindMatch(
    const EphemeralRangeInFlatTree& range,
    const TextFragmentAnchorMetrics::Match match_metrics,
    bool is_unique) {
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
               range, DocumentMarker::MarkerTypes::TextFragment())
           .IsEmpty()) {
    return;
  }

  if (beforematch_state_ == kNoMatchFound) {
    Element* enclosing_block =
        EnclosingBlock(range.StartPosition(), kCannotCrossEditingBoundary);
    DCHECK(enclosing_block);
    frame_->GetDocument()->EnqueueAnimationFrameTask(
        WTF::Bind(&TextFragmentAnchor::FireBeforeMatchEvent,
                  WrapPersistent(this), WrapWeakPersistent(enclosing_block)));
    beforematch_state_ = kEventQueued;
    return;
  }
  if (beforematch_state_ == kEventQueued)
    return;
  // TODO(jarhar): Consider what to do based on DOM/style modifications made by
  // the beforematch event here and write tests for it once we decide on a
  // behavior here: https://github.com/WICG/display-locking/issues/150

  bool needs_style_and_layout = false;

  // Apply :target to the first match
  if (!did_find_match_) {
    ApplyTargetToCommonAncestor(range);
    needs_style_and_layout = true;
  }

  // Activate any find-in-page activatable display-locks in the ancestor
  // chain.
  if (DisplayLockUtilities::ActivateFindInPageMatchRangeIfNeeded(range)) {
    // Since activating a lock dirties layout, we need to make sure it's clean
    // before computing the text rect below.
    needs_style_and_layout = true;
    // TODO(crbug.com/1041942): It is possible and likely that activation
    // signal causes script to resize something on the page. This code here
    // should really yield until the next frame to give script an opportunity
    // to run.
  }

  if (needs_style_and_layout) {
    frame_->GetDocument()->UpdateStyleAndLayout(
        DocumentUpdateReason::kFindInPage);
  }

  metrics_->DidFindMatch(match_metrics);
  did_find_match_ = true;

  if (first_match_needs_scroll_) {
    metrics_->SetSearchEngineSource(HasSearchEngineSource());
    first_match_needs_scroll_ = false;

    PhysicalRect bounding_box(ComputeTextRect(range));

    // Set the bounding box height to zero because we want to center the top of
    // the text range.
    bounding_box.SetHeight(LayoutUnit());

    DCHECK(range.Nodes().begin() != range.Nodes().end());

    Node& node = *range.Nodes().begin();

    DCHECK(node.GetLayoutObject());

    PhysicalRect scrolled_bounding_box =
        node.GetLayoutObject()->ScrollRectToVisible(
            bounding_box, ScrollAlignment::CreateScrollIntoViewParams(
                              ScrollAlignment::CenterAlways(),
                              ScrollAlignment::CenterAlways(),
                              mojom::blink::ScrollType::kProgrammatic));
    did_scroll_into_view_ = true;

    if (AXObjectCache* cache = frame_->GetDocument()->ExistingAXObjectCache())
      cache->HandleScrolledToAnchor(&node);

    metrics_->DidScroll();

    // We scrolled the text into view if the main document scrolled or the text
    // bounding box changed, i.e. if it was scrolled in a nested scroller.
    // TODO(nburris): The rect returned by ScrollRectToVisible,
    // scrolled_bounding_box, should be in frame coordinates in which case
    // just checking its location would suffice, but there is a bug where it is
    // actually in document coordinates and therefore does not change with a
    // main document scroll.
    if (!frame_->View()->GetScrollableArea()->GetScrollOffset().IsZero() ||
        scrolled_bounding_box.offset != bounding_box.offset) {
      did_non_zero_scroll_ = true;
      metrics_->DidNonZeroScroll();
    }
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

  metrics_->ReportMetrics();

  if (!did_find_match_) {
    dismissed_ = true;

    DCHECK(!element_fragment_anchor_);
    element_fragment_anchor_ = ElementFragmentAnchor::TryCreate(
        frame_->GetDocument()->Url(), *frame_, should_scroll_);
    if (element_fragment_anchor_) {
      // Schedule a frame so we can invoke the element anchor in
      // PerformPreRafActions.
      frame_->GetPage()->GetChromeClient().ScheduleAnimation(frame_->View());
    }
  }
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
  dismissed_ = true;
  metrics_->Dismissed();

  return dismissed_;
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

void TextFragmentAnchor::FireBeforeMatchEvent(Element* element) {
  if (RuntimeEnabledFeatures::BeforeMatchEventEnabled(
          frame_->GetDocument()->GetExecutionContext())) {
    element->DispatchEvent(
        *Event::CreateBubble(event_type_names::kBeforematch));
  }
  beforematch_state_ = kFiredEvent;
}

void TextFragmentAnchor::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  metrics_->SetTickClockForTesting(tick_clock);
}

bool TextFragmentAnchor::HasSearchEngineSource() {
  AtomicString referrer = frame_->GetDocument()->referrer();
  // TODO(crbug.com/1133823): Add test case for valid referrer.
  if (!referrer)
    return false;

  return IsKnownSearchEngine(referrer);
}

}  // namespace blink

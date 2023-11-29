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

  AtomicString content_type = frame.GetDocument()->contentType();
  if (content_type != "text/html" && content_type != "text/plain") {
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
base::TimeDelta TextFragmentAnchor::PostLoadTaskDelay() {
  // The amount of time to wait after load without a DOM mutation before
  // invoking the text search. Each time a DOM mutation occurs the text search
  // is pushed back by this delta. Experimentally determined.
  return base::Milliseconds(500);
}

// static
base::TimeDelta TextFragmentAnchor::PostLoadTaskTimeout() {
  // The maximum amount of time to wait after load before performing the text
  // search. Experimentally determined.
  return base::Milliseconds(3000);
}

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
      post_load_timer_(frame.GetTaskRunner(TaskType::kInternalFindInPage),
                       this,
                       &TextFragmentAnchor::PostLoadTask),
      post_load_timeout_timer_(
          frame.GetTaskRunner(TaskType::kInternalFindInPage),
          this,
          &TextFragmentAnchor::PostLoadTask),
      metrics_(MakeGarbageCollected<TextFragmentAnchorMetrics>(
          frame_->GetDocument())) {
  TRACE_EVENT("blink", "TextFragmentAnchor::TextFragmentAnchor");
  DCHECK(!text_directives.empty());
  DCHECK(frame_->View());

  metrics_->DidCreateAnchor(text_directives.size());

  AnnotationAgentContainerImpl* annotation_container =
      AnnotationAgentContainerImpl::CreateIfNeeded(*frame_->GetDocument());
  DCHECK(annotation_container);

  directive_annotation_pairs_.reserve(text_directives.size());
  for (Member<TextDirective>& directive : text_directives) {
    auto* selector =
        MakeGarbageCollected<TextAnnotationSelector>(directive->GetSelector());
    AnnotationAgentImpl* agent = annotation_container->CreateUnboundAgent(
        mojom::blink::AnnotationType::kSharedHighlight, *selector);

    // TODO(bokan): This is a stepping stone in refactoring the
    // TextFragmentHandler. When we replace it with a browser-side manager it
    // may make for a better API to have components register a handler for an
    // annotation type with AnnotationAgentContainer.
    // https://crbug.com/1303887.
    TextFragmentHandler::DidCreateTextFragment(*agent, *frame_->GetDocument());

    directive_annotation_pairs_.push_back(std::make_pair(directive, agent));
  }
}

bool TextFragmentAnchor::InvokeSelector() {
  UpdateCurrentState();

  switch (state_) {
    case kSearching:
      if (iteration_ == kDone) {
        DidFinishSearch();
      }
      break;
    case kWaitingForDOMMutations:
      // A match was found but requires some kind of DOM mutation to make it
      // visible and ready so don't try to finish the search yet.
      CHECK(first_match_);
      if (first_match_->IsAttachmentPending()) {
        // Still waiting.
        break;
      }

      // Move to ApplyEffects immediately.
      state_ = kApplyEffects;
      [[fallthrough]];
    case kApplyEffects:
      // Now that the event - if needed - has been processed, apply the
      // necessary effects to the matching DOM nodes.
      ApplyEffectsToFirstMatch();
      state_ = kKeepInView;
      [[fallthrough]];
    case kKeepInView:
      // Until the load event ensure the matched text is kept in view in the
      // face of layout changes.
      EnsureFirstMatchInViewIfNeeded();
      if (iteration_ == kDone) {
        DidFinishSearch();
      }
      break;
    case kFinalized:
      break;
  }

  // We return true to keep this anchor alive as long as we need another invoke
  // or have to finish up at the next rAF.
  return !(state_ == kFinalized && iteration_ == kDone);
}

void TextFragmentAnchor::Installed() {
  AnnotationAgentContainerImpl* container =
      Supplement<Document>::From<AnnotationAgentContainerImpl>(
          frame_->GetDocument());
  CHECK(container);
  container->AddObserver(this);
}

void TextFragmentAnchor::NewContentMayBeAvailable() {
  // The post load task will only be invoked once so don't restart an inactive
  // timer (if it's inactive it's because it's already been invoked).
  if (iteration_ != kPostLoad || !post_load_timer_.IsActive()) {
    return;
  }

  // Restart the timer.
  post_load_timer_.StartOneShot(PostLoadTaskDelay(), FROM_HERE);
}

void TextFragmentAnchor::FinalizeAnchor() {
  CHECK_EQ(iteration_, kDone);
  CHECK_LT(state_, kFinalized);

  if (element_fragment_anchor_) {
    element_fragment_anchor_->Installed();
    element_fragment_anchor_->Invoke();
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
  state_ = kFinalized;
}

void TextFragmentAnchor::Trace(Visitor* visitor) const {
  visitor->Trace(element_fragment_anchor_);
  visitor->Trace(metrics_);
  visitor->Trace(directive_annotation_pairs_);
  visitor->Trace(first_match_);
  visitor->Trace(matched_annotations_);
  visitor->Trace(post_load_timer_);
  visitor->Trace(post_load_timeout_timer_);
  SelectorFragmentAnchor::Trace(visitor);
}

void TextFragmentAnchor::WillPerformAttach() {
  if (iteration_ == kParsing && frame_->GetDocument()->IsLoadCompleted()) {
    iteration_ = kLoad;
    MarkFailedAttachmentsForRetry();
  }
}

void TextFragmentAnchor::UpdateCurrentState() {
  bool all_found = true;
  bool any_needs_attachment = false;
  for (auto& directive_annotation_pair : directive_annotation_pairs_) {
    AnnotationAgentImpl* annotation = directive_annotation_pair.second;

    // This method is called right after AnnotationAgentContainerImpl calls
    // PerformInitialAttachments. However, it may have avoided attachment if
    // the page is hidden. If that's the case, avoid moving to kPostLoad so
    // that we don't finish the search until the page becomes visible.
    if (annotation->NeedsAttachment()) {
      any_needs_attachment = true;
    }

    bool found_match =
        annotation->IsAttachmentPending() || annotation->IsAttached();
    if (!found_match) {
      all_found = false;
      continue;
    }

    // Text fragments apply effects (scroll, focus) only to the first
    // *matching* directive into view so that's the directive that reflects the
    // `state_`. The Attach() call matches synchronously (but may
    // ansynchronously perform DOMMutations) so the first such matching agent
    // will be set to first_match_.
    if (!first_match_) {
      CHECK_EQ(state_, kSearching);
      state_ = annotation->IsAttachmentPending() ? kWaitingForDOMMutations
                                                 : kApplyEffects;
      first_match_ = annotation;
    }

    if (matched_annotations_.insert(annotation).is_new_entry) {
      metrics_->DidFindMatch();
      const AnnotationSelector* selector = annotation->GetSelector();
      // Selector must be a TextAnnotationSelector since this is the
      // *Text*FragmentAnchor.
      if (selector && !To<TextAnnotationSelector>(selector)->WasMatchUnique()) {
        metrics_->DidFindAmbiguousMatch();
      }
    }
  }

  if (all_found) {
    iteration_ = kDone;
  } else if (iteration_ == kLoad && !any_needs_attachment) {
    iteration_ = kPostLoad;
    post_load_timer_.StartOneShot(PostLoadTaskDelay(), FROM_HERE);
    post_load_timeout_timer_.StartOneShot(PostLoadTaskTimeout(), FROM_HERE);
  }
}

void TextFragmentAnchor::ApplyEffectsToFirstMatch() {
  DCHECK(first_match_);
  DCHECK_EQ(state_, kApplyEffects);

  // TODO(jarhar): Consider what to do based on DOM/style modifications made by
  // the beforematch event here and write tests for it once we decide on a
  // behavior here: https://github.com/WICG/display-locking/issues/150

  // It's possible the DOM the match was attached to was removed by this time.
  if (!first_match_->IsAttached())
    return;

  // If we're attached, we must have already waited for DOM mutations.
  CHECK(!first_match_->IsAttachmentPending());

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
  CHECK_GE(state_, kApplyEffects);
  CHECK(first_match_);

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
  CHECK_EQ(iteration_, kDone);
  CHECK_LT(state_, kFinalized);

  if (finalize_pending_) {
    return;
  }

  AnnotationAgentContainerImpl* container =
      Supplement<Document>::From<AnnotationAgentContainerImpl>(
          frame_->GetDocument());
  CHECK(container);
  container->RemoveObserver(this);

  metrics_->SetSearchEngineSource(HasSearchEngineSource());
  metrics_->ReportMetrics();

  bool did_find_any_matches = first_match_ != nullptr;

  if (!did_find_any_matches) {
    DCHECK(!element_fragment_anchor_);
    // ElementFragmentAnchor needs to be invoked from FinalizeAnchor
    // since it can cause script to run and we may be in a ScriptForbiddenScope
    // here.
    element_fragment_anchor_ = ElementFragmentAnchor::TryCreate(
        frame_->GetDocument()->Url(), *frame_, should_scroll_);
  }

  DCHECK(!did_find_any_matches || !element_fragment_anchor_);

  // Finalizing the anchor may cause script execution so schedule a new frame
  // to perform finalization.
  frame_->GetDocument()->EnqueueAnimationFrameTask(WTF::BindOnce(
      &TextFragmentAnchor::FinalizeAnchor, WrapWeakPersistent(this)));
  finalize_pending_ = true;
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

bool TextFragmentAnchor::MarkFailedAttachmentsForRetry() {
  bool did_mark = false;
  for (auto& directive_annotation_pair : directive_annotation_pairs_) {
    AnnotationAgentImpl* annotation = directive_annotation_pair.second;
    if (!annotation->IsAttached() && !annotation->IsAttachmentPending()) {
      annotation->SetNeedsAttachment();
      did_mark = true;
    }
  }

  return did_mark;
}

void TextFragmentAnchor::PostLoadTask(TimerBase*) {
  CHECK_NE(iteration_, kDone);

  // Stop both timers - the post load task is run just once.
  post_load_timer_.Stop();
  post_load_timeout_timer_.Stop();
  if (!frame_->IsDetached() && MarkFailedAttachmentsForRetry()) {
    frame_->GetPage()->GetChromeClient().ScheduleAnimation(frame_->View());
  }

  iteration_ = kDone;
}

}  // namespace blink

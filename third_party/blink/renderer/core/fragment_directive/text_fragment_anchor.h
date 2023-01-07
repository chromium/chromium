// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAGMENT_DIRECTIVE_TEXT_FRAGMENT_ANCHOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAGMENT_DIRECTIVE_TEXT_FRAGMENT_ANCHOR_H_

#include "third_party/blink/public/mojom/loader/same_document_navigation_type.mojom-blink.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/fragment_directive/selector_fragment_anchor.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_anchor_metrics.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_finder.h"
#include "third_party/blink/renderer/core/page/scrolling/element_fragment_anchor.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class AnnotationAgentImpl;
class DocumentLoader;
class LocalFrame;
class KURL;
class TextDirective;

// TextFragmentAnchor is the coordinator class for applying text directives
// from the URL (also known as "scroll-to-text") to a document. This class'
// purpose is to integrate with Blink's loading and lifecycle states. The
// actual logic of performing the text search and applying highlights is
// delegated out to the core annotation API.
//
// A frame will try to create a TextFragmentAnchor when parsing in a document
// completes. If the URL has a valid text directive an instance of
// TextFragmentAnchor will be created and stored on the LocalFrameView.
//
// The anchor performs its operations via the InvokeSelector method which is
// invoked repeatedly, each time layout finishes in the document. Thus, the
// anchor is guaranteed that layout is clean in InvokeSelector; however,
// end-of-layout is a script-forbidden section so no actions that can result in
// script being run can be invoked from there. Scriptable actions will instead
// cause a BeginMainFrame to be scheduled and run in that frame before the
// lifecycle, where script is allowed.
//
// TextFragmentAnchor is a state machine that transitions state via
// InvokeSelector (and some external events). Here are the state transitions:
//
//           ┌──────┐
//           │   ┌──┴────────────────────────┐
//           └───►       kSearching          ├────────────┐
//               └─────────────┬─────────────┘            │
//                             │                          │
//         ┌─────┬─────────────▼─────────────┐            │
//         └────►│ kBeforeMatchEventQueued   │            │
//               └─────────────┬─────────────┘            │
//                             │                          │
//               ┌─────────────▼─────────────┐            │
//               │  kBeforeMatchEventFired   ├────────────┤
//               └─────────────┬─────────────┘            │
//                             │                          │
//         ┌─────┬─────────────▼─────────────┐            │
//         └────►│ kEffectsAppliedKeepInView │            │
//               └─────────────┬─────────────┘            │
//                             │                          │
//               ┌------------─▼-------------┐            │
//         ┌─────┤     [[SearchFinished]]    |◄───────────┘
//         │     └-------------┬-------------┘
//         │                   │
//         │     ┌─────────────▼─────────────┬─────┐
//         │     │    kScriptableActions     │◄────┘
//         │     └───────────────────────────┘
//         │                   │
//         │     ┌─────────────▼─────────────┐
//         └─────►           kDone           │
//               └───────────────────────────┘
class CORE_EXPORT TextFragmentAnchor final : public SelectorFragmentAnchor {
 public:
  // When a document is loaded, this method will be called to see if it meets
  // the criteria to generate a new permission token (at a high level it means:
  // did the user initiate the navigation?). This token can then be used to
  // invoke the text fragment anchor later during loading or propagated across
  // a redirect so that the real destination can invoke a text fragment.
  static bool GenerateNewToken(const DocumentLoader&);

  // Same as above but for same-document navigations. These require a bit of
  // care since DocumentLoader's state is based on the initial document load.
  // In this case, we also avoid generating the token unless the new URL has a
  // text fragment in it (and thus it'll be consumed immediately).
  static bool GenerateNewTokenForSameDocument(
      const DocumentLoader&,
      WebFrameLoadType load_type,
      mojom::blink::SameDocumentNavigationType same_document_navigation_type);

  static TextFragmentAnchor* TryCreate(const KURL& url,
                                       LocalFrame& frame,
                                       bool should_scroll);

  TextFragmentAnchor(HeapVector<Member<TextDirective>>& text_directives,
                     LocalFrame& frame,
                     bool should_scroll);
  TextFragmentAnchor(const TextFragmentAnchor&) = delete;
  TextFragmentAnchor& operator=(const TextFragmentAnchor&) = delete;
  ~TextFragmentAnchor() override = default;

  bool InvokeSelector() override;

  void Installed() override;

  void PerformScriptableActions() override;

  void SetTickClockForTesting(const base::TickClock* tick_clock);

  void Trace(Visitor*) const override;

  bool IsTextFragmentAnchor() override { return true; }

 private:
  // Performs attachment (text search for each directive) for each directive
  // that hasn't yet found a match.
  void TryAttachingUnattachedDirectives();

  // Called on the first directive to be successfully matched. This will queue
  // a BeforeMatch event to be fired.
  void DidFindFirstMatch(const AnnotationAgentImpl& annotation);

  // Called once the BeforeMatch event from above has been processed. CSS
  // style and scroll into view can now be performed.
  void ApplyEffectsToFirstMatch();

  // Performs ScrollIntoView so that the first match is visible in the viewport.
  // Returns true if scrolling was performed, false otherwise.
  bool EnsureFirstMatchInViewIfNeeded();

  // Called when the search is finished. Reports metrics and activates the
  // element fragment anchor if we didn't find a match. If a match was found or
  // an element fragment is used, moves the anchor into kScriptableActions.
  // Otherwise, moves to kDone.
  void DidFinishSearch();

  void ApplyTargetToCommonAncestor(const EphemeralRangeInFlatTree& range);

  void FireBeforeMatchEvent(const RangeInFlatTree* range);

  bool HasSearchEngineSource();

  // This keeps track of each TextDirective and its associated
  // AnnotationAgentImpl. The directive is the DOM object exposed to JS that's
  // parsed from the URL while the AnnotationAgent is the object responsible
  // for performing the search for the specified text in the Document.
  using DirectiveAnnotationPair =
      std::pair<Member<TextDirective>, Member<AnnotationAgentImpl>>;
  HeapVector<DirectiveAnnotationPair> directive_annotation_pairs_;

  // Once the first directive (in specified order) has been successfully
  // attached to DOM its annotation will be stored in `first_match_`.
  Member<const AnnotationAgentImpl> first_match_;

  // If the text fragment anchor is defined as a fragment directive and we don't
  // find a match, we fall back to the element anchor if it is present.
  Member<ElementFragmentAnchor> element_fragment_anchor_;

  // Set to true after the first time InvokeSelector is called.
  bool did_perform_initial_attachment_ = false;

  // Set to true once InvokeSelector has been called after IsLoadCompleted.
  bool did_perform_post_load_attachment_ = false;

  enum AnchorState {
    // We're still waiting to find any matches. Either a search hasn't yet been
    // performed or no matches were found. If no matches were found in the
    // initial attachment, a second try will occur only after the load event
    // has fired; InvokeSelector calls before then will be a no-op.
    kSearching,
    // At least one match has been found. The BeforeMatch event was queued and
    // InvokeSelector will now be a no-op until BeforeMatch fires and is
    // processed.
    kBeforeMatchEventQueued,
    // The BeforeMatch event has been processed, InvokeSelector will now apply
    // effects like CSS :target, scrollIntoView, etc.
    kBeforeMatchEventFired,
    // The first match has been processed and scrolled into view. The anchor is
    // kept alive in this state until the load event fires. All that's done
    // during this state is to keep the match centered in the viewport.
    kEffectsAppliedKeepInView,
    // Effects have been applied but there's still something left to do in a
    // script-allowed section. Entered only after the load event has fired.
    kScriptableActions,
    // All actions completed - the anchor can be disposed.
    kDone
  } state_ = kSearching;

  Member<TextFragmentAnchorMetrics> metrics_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAGMENT_DIRECTIVE_TEXT_FRAGMENT_ANCHOR_H_

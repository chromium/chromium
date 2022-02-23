// Copyright 2019 The Chromium Authors. All rights reserved.
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

class DocumentLoader;
class LocalFrame;
class KURL;
class TextDirective;

class CORE_EXPORT TextFragmentAnchor final : public SelectorFragmentAnchor,
                                             public TextFragmentFinder::Client {
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

  void DidScroll(mojom::blink::ScrollType type) override;

  void PerformPreRafActions() override;

  // Removes text match highlights if any highlight is in view.
  bool Dismiss() override;

  void SetTickClockForTesting(const base::TickClock* tick_clock);

  void Trace(Visitor*) const override;

  // TextFragmentFinder::Client interface
  void DidFindMatch(const RangeInFlatTree& range,
                    const TextFragmentAnchorMetrics::Match match_metrics,
                    bool is_unique) override;

  void NoMatchFound() override {}

  using DirectiveFinderPair =
      std::pair<Member<TextDirective>, Member<TextFragmentFinder>>;
  const HeapVector<DirectiveFinderPair>& DirectiveFinderPairs() const {
    return directive_finder_pairs_;
  }

  bool IsTextFragmentAnchor() override { return true; }

 private:
  // Called when the search is finished. Reports metrics and activates the
  // element fragment anchor if we didn't find a match.
  void DidFinishSearch();

  void ApplyTargetToCommonAncestor(const EphemeralRangeInFlatTree& range);

  void FireBeforeMatchEvent(Element* element);

  bool HasSearchEngineSource();

  // This keeps track of each TextDirective and its associated
  // TextFragmentFinder. The directive is the DOM object exposed to JS that's
  // parsed from the URL while the finder is the object responsible for
  // performing the search for the specified text in the Document.
  HeapVector<DirectiveFinderPair> directive_finder_pairs_;

  bool search_finished_ = false;
  // Indicates that we should scroll into view the first match that we find, set
  // to true each time the anchor is invoked if the user hasn't scrolled.
  bool first_match_needs_scroll_ = false;
  // Whether we successfully scrolled into view a match at least once, used for
  // metrics reporting.
  bool did_scroll_into_view_ = false;
  // Whether we found a match. Used to determine if we should activate the
  // element fragment anchor at the end of searching.
  bool did_find_match_ = false;
  // If the text fragment anchor is defined as a fragment directive and we don't
  // find a match, we fall back to the element anchor if it is present.
  Member<ElementFragmentAnchor> element_fragment_anchor_;
  // Whether we performed a non-zero scroll to scroll a match into view. Used
  // to determine whether the user subsequently scrolls back to the top.
  bool did_non_zero_scroll_ = false;
  // Whether PerformPreRafActions should run at the next rAF.
  bool needs_perform_pre_raf_actions_ = false;

  // Whether a text fragment finder was run.
  bool has_performed_first_text_search_ = false;

  enum BeforematchState {
    kNoMatchFound,  // DidFindMatch has not been called.
    kEventQueued,   // Beforematch event has been queued, but not fired yet.
    kFiredEvent     // Beforematch event has been fired.
  } beforematch_state_ = kNoMatchFound;

  Member<TextFragmentAnchorMetrics> metrics_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAGMENT_DIRECTIVE_TEXT_FRAGMENT_ANCHOR_H_

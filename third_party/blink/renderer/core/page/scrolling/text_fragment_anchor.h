// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_ANCHOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_ANCHOR_H_

#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/core/page/scrolling/element_fragment_anchor.h"
#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_anchor_metrics.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_finder.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DocumentLoader;
class LocalFrame;
class KURL;

constexpr char kFragmentDirectivePrefix[] = ":~:";
// Subtract 1 because base::size includes the \0 string terminator.
constexpr size_t kFragmentDirectivePrefixStringLength =
    base::size(kFragmentDirectivePrefix) - 1;

constexpr char kTextFragmentIdentifierPrefix[] = "text=";
// Subtract 1 because base::size includes the \0 string terminator.
constexpr size_t kTextFragmentIdentifierPrefixStringLength =
    base::size(kTextFragmentIdentifierPrefix) - 1;

class CORE_EXPORT TextFragmentAnchor final : public FragmentAnchor,
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
      const String& fragment,
      WebFrameLoadType load_type,
      bool is_content_initiated,
      SameDocumentNavigationSource source);

  static TextFragmentAnchor* TryCreateFragmentDirective(
      const KURL& url,
      LocalFrame& frame,
      bool should_scroll);

  TextFragmentAnchor(
      const Vector<TextFragmentSelector>& text_fragment_selectors,
      LocalFrame& frame,
      bool should_scroll);
  ~TextFragmentAnchor() override = default;

  bool Invoke() override;

  void Installed() override;

  void DidScroll(mojom::blink::ScrollType type) override;

  void PerformPreRafActions() override;

  // Removes text match highlights if any highlight is in view.
  bool Dismiss() override;

  void SetTickClockForTesting(const base::TickClock* tick_clock);

  void Trace(Visitor*) const override;

  // TextFragmentFinder::Client interface
  void DidFindMatch(const EphemeralRangeInFlatTree& range,
                    const TextFragmentAnchorMetrics::Match match_metrics,
                    bool is_unique) override;

  void NoMatchFound() override {}

 private:
  // Called when the search is finished. Reports metrics and activates the
  // element fragment anchor if we didn't find a match.
  void DidFinishSearch();

  void ApplyTargetToCommonAncestor(const EphemeralRangeInFlatTree& range);

  void FireBeforeMatchEvent(Element* element);

  Vector<TextFragmentFinder> text_fragment_finders_;

  Member<LocalFrame> frame_;

  bool search_finished_ = false;
  // Whether the user has scrolled the page.
  bool user_scrolled_ = false;
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
  // Whether the text fragment anchor has been dismissed yet. This should be
  // kept alive until dismissed so we can remove text highlighting.
  bool dismissed_ = false;
  // Whether we should scroll the anchor into view. This will be false for
  // history navigations and reloads, where we want to restore the highlight but
  // not scroll into view again.
  bool should_scroll_ = false;
  // Whether the page has been made visible. Used to ensure we wait until the
  // page has been made visible to start matching, to help prevent brute force
  // search attacks.
  bool page_has_been_visible_ = false;
  // Whether we performed a non-zero scroll to scroll a match into view. Used
  // to determine whether the user subsequently scrolls back to the top.
  bool did_non_zero_scroll_ = false;

  // Whether a text fragment finder was run.
  bool has_performed_first_text_search_ = false;

  enum BeforematchState {
    kNoMatchFound,  // DidFindMatch has not been called.
    kEventQueued,   // Beforematch event has been queued, but not fired yet.
    kFiredEvent     // Beforematch event has been fired.
  } beforematch_state_ = kNoMatchFound;

  Member<TextFragmentAnchorMetrics> metrics_;

  DISALLOW_COPY_AND_ASSIGN(TextFragmentAnchor);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_ANCHOR_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_ANCHOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_ANCHOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/page/scrolling/element_fragment_anchor.h"
#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_anchor_metrics.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_finder.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

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
  static TextFragmentAnchor* TryCreateFragmentDirective(
      const KURL& url,
      LocalFrame& frame,
      bool same_document_navigation,
      bool should_scroll);

  TextFragmentAnchor(
      const Vector<TextFragmentSelector>& text_fragment_selectors,
      LocalFrame& frame,
      bool should_scroll);
  ~TextFragmentAnchor() override = default;

  bool Invoke() override;

  void Installed() override;

  void DidScroll(ScrollType type) override;

  void PerformPreRafActions() override;

  void DidCompleteLoad() override;

  // Removes text match highlights if any highlight is in view.
  bool Dismiss() override;

  void Trace(blink::Visitor*) override;

  // TextFragmentFinder::Client interface
  void DidFindMatch(const EphemeralRangeInFlatTree& range) override;
  void DidFindAmbiguousMatch() override;

 private:
  // Called when the search is finished. Reports metrics and activates the
  // element fragment anchor if we didn't find a match.
  void DidFinishSearch();

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

  Member<TextFragmentAnchorMetrics> metrics_;

  DISALLOW_COPY_AND_ASSIGN(TextFragmentAnchor);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_ANCHOR_H_

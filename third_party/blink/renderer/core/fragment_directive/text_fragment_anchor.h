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

class AnnotationAgentImpl;
class DocumentLoader;
class LocalFrame;
class KURL;
class TextDirective;

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

  void PerformPreRafActions() override;

  void SetTickClockForTesting(const base::TickClock* tick_clock);

  void Trace(Visitor*) const override;

  // Returns true if this was a match and it was processed.
  // `first_match_found` - true if a prior annotation had a match already.
  bool DidFinishAttach(const AnnotationAgentImpl& annotation,
                       bool first_match_found);

  bool IsTextFragmentAnchor() override { return true; }

 private:
  // Called when the search is finished. Reports metrics and activates the
  // element fragment anchor if we didn't find a match.
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

  // Whether any directives have found a match.
  bool did_find_match_ = false;

  // If the text fragment anchor is defined as a fragment directive and we don't
  // find a match, we fall back to the element anchor if it is present.
  Member<ElementFragmentAnchor> element_fragment_anchor_;

  // Set to true after the first matching pass (i.e. no matches found or the
  // first InvokeSelector after the BeforeMatch event was fired). Used to
  // prevent repeated searches until the load event for performance reasons.
  bool suppress_text_search_until_load_event_ = false;

  enum AnchorState {
    // We're still waiting to find any matches. Either a search hasn't yet been
    // performed or no matches were found.
    kSearching,
    // At least one match has been found. The BeforeMatch event was queued and
    // we need to wait until it fires before we apply highlighting,
    // scrollIntoView, etc.
    kBeforeMatchEventQueued,
    // The BeforeMatch event has been processed, we can now apply highlighting,
    // scrollIntoView, etc.
    kBeforeMatchEventFired,
    // Effects have been applied but there's still something left to do in a
    // script-allowed section.
    kScriptableActions,
    // All actions completed - the anchor can be disposed.
    kDone
  } state_ = kSearching;

  Member<TextFragmentAnchorMetrics> metrics_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAGMENT_DIRECTIVE_TEXT_FRAGMENT_ANCHOR_H_

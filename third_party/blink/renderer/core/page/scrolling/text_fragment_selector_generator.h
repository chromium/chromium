// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_SELECTOR_GENERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_SELECTOR_GENERATOR_H_

#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_finder.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector.h"

namespace blink {

class LocalFrame;

// TextFragmentSelectorGenerator is responsible for generating text fragment
// selectors for the user selected text according to spec in
// https://github.com/WICG/scroll-to-text-fragment#proposed-solution.
// Generated selectors would be later used to highlight the same
// text if successfully parsed by |TextFragmentAnchor |. Generation will be
// triggered when users request "link to text" for the selected text.
class CORE_EXPORT TextFragmentSelectorGenerator final
    : public GarbageCollected<TextFragmentSelectorGenerator>,
      public TextFragmentFinder::Client {
 public:
  explicit TextFragmentSelectorGenerator() = default;

  // Sets the frame and range of the current selection.
  void UpdateSelection(LocalFrame* selection_frame,
                       const EphemeralRangeInFlatTree& selection_range);

  // Generates selector for current selection.
  void GenerateSelector();

  // TextFragmentFinder::Client interface
  void DidFindMatch(const EphemeralRangeInFlatTree& match,
                    const TextFragmentAnchorMetrics::Match match_metrics,
                    bool is_unique) override;

  // Sets the callback used for notifying test results of |GenerateSelector|.
  void SetCallbackForTesting(
      base::OnceCallback<void(const TextFragmentSelector&)> callback);

  // Notifies the results of |GenerateSelector|.
  void NotifySelectorReady(const TextFragmentSelector& selector);

  void DocumentDetached(Document* document);

  void Trace(Visitor*) const;

 private:
  Member<LocalFrame> selection_frame_;
  Member<Range> selection_range_;
  std::unique_ptr<TextFragmentSelector> selector_;

  base::OnceCallback<void(const TextFragmentSelector&)> callback_for_tests_;

  DISALLOW_COPY_AND_ASSIGN(TextFragmentSelectorGenerator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_SELECTOR_GENERATOR_H_

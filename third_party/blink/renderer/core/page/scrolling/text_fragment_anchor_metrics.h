// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_ANCHOR_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_ANCHOR_METRICS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

// Helper class for TextFragmentAnchor that provides hooks for tracking and
// reporting usage and performance metrics to UMA.
class CORE_EXPORT TextFragmentAnchorMetrics final
    : public GarbageCollected<TextFragmentAnchorMetrics> {
 public:
  TextFragmentAnchorMetrics(Document* document);

  void DidCreateAnchor(int selector_count);

  void DidFindMatch(const String text);
  void ResetMatchCount();

  void DidFindAmbiguousMatch();

  void ScrollCancelled();

  void DidScroll();

  void DidNonZeroScroll();

  void ReportMetrics();

  void Dismissed();

  void Trace(blink::Visitor*);

 private:
  Member<Document> document_;

#ifndef NDEBUG
  bool metrics_reported_ = false;
#endif

  wtf_size_t selector_count_ = 0;
  Vector<String> matches_;
  bool ambiguous_match_ = false;
  bool scroll_cancelled_ = false;
  base::TimeTicks create_time_;
  base::TimeTicks first_scroll_into_view_time_;
  bool did_non_zero_scroll_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_ANCHOR_METRICS_H_

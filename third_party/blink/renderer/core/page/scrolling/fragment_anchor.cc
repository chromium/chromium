// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/page/scrolling/element_fragment_anchor.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_anchor.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

FragmentAnchor* FragmentAnchor::TryCreate(const KURL& url,
                                          LocalFrame& frame,
                                          bool should_scroll) {
  DCHECK(frame.GetDocument());

  FragmentAnchor* anchor = nullptr;
  const bool text_fragment_identifiers_enabled =
      RuntimeEnabledFeatures::TextFragmentIdentifiersEnabled(frame.DomWindow());

  // The text fragment anchor will be created if we successfully parsed the
  // text directive but we only do the text matching later on.
  bool text_fragment_anchor_created = false;
  if (text_fragment_identifiers_enabled) {
    anchor = TextFragmentAnchor::TryCreateFragmentDirective(url, frame,
                                                            should_scroll);
    text_fragment_anchor_created = anchor;
  }

  bool element_id_anchor_found = false;
  if (!anchor) {
    anchor = ElementFragmentAnchor::TryCreate(url, frame, should_scroll);
    element_id_anchor_found = anchor;
  }

  // Track how often we have a element fragment that we can't find. Only track
  // if we didn't match a text fragment since we expect those would inflate the
  // "failed" case.
  if (IsA<HTMLDocument>(frame.GetDocument()) && url.HasFragmentIdentifier() &&
      !text_fragment_anchor_created) {
    UMA_HISTOGRAM_BOOLEAN("TextFragmentAnchor.ElementIdFragmentFound",
                          element_id_anchor_found);
  }

  return anchor;
}

}  // namespace blink

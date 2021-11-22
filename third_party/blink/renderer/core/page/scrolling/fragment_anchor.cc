// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/fragment_directive/css_selector_fragment_anchor.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_anchor.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_details_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/page/scrolling/element_fragment_anchor.h"
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
  bool selector_fragment_anchor_created = false;
  if (text_fragment_identifiers_enabled) {
    anchor = TextFragmentAnchor::TryCreate(url, frame, should_scroll);
    selector_fragment_anchor_created = anchor;
  }

  // TODO(crbug.com/1265726): Do highlighting related to all fragment
  // directives and scroll the first one into view
  if (!anchor && RuntimeEnabledFeatures::CSSSelectorFragmentAnchorEnabled()) {
    anchor = CssSelectorFragmentAnchor::TryCreate(url, frame, should_scroll);
    selector_fragment_anchor_created = anchor;
  }

  bool element_id_anchor_found = false;
  if (!anchor) {
    anchor = ElementFragmentAnchor::TryCreate(url, frame, should_scroll);
    element_id_anchor_found = anchor;
  }

  // Track how often we have an element fragment that we can't find. Only track
  // if we didn't match a selector fragment since we expect those would inflate
  // the "failed" case.
  if (IsA<HTMLDocument>(frame.GetDocument()) && url.HasFragmentIdentifier() &&
      !selector_fragment_anchor_created) {
    UMA_HISTOGRAM_BOOLEAN("TextFragmentAnchor.ElementIdFragmentFound",
                          element_id_anchor_found);
  }

  return anchor;
}

void FragmentAnchor::ScrollElementIntoViewWithOptions(
    Element* element_to_scroll,
    ScrollIntoViewOptions* options) {
  // Expand <details> elements so we can make |element_to_scroll| visible.
  bool needs_style_and_layout =
      RuntimeEnabledFeatures::AutoExpandDetailsElementEnabled() &&
      HTMLDetailsElement::ExpandDetailsAncestors(*element_to_scroll);

  // Reveal hidden=until-found ancestors so we can make |element_to_scroll|
  // visible.
  needs_style_and_layout |=
      RuntimeEnabledFeatures::BeforeMatchEventEnabled(
          element_to_scroll->GetExecutionContext()) &&
      DisplayLockUtilities::RevealHiddenUntilFoundAncestors(*element_to_scroll);

  if (needs_style_and_layout) {
    // If we opened any details elements, we need to update style and layout
    // to account for the new content to render inside the now-expanded
    // details element before we scroll to it. The added open attribute may
    // also affect style.
    frame_->GetDocument()->UpdateStyleAndLayoutForNode(
        element_to_scroll, DocumentUpdateReason::kFindInPage);
  }

  element_to_scroll->ScrollIntoViewNoVisualUpdate(options);
}

void FragmentAnchor::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
}

}  // namespace blink

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/route_matching/route.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/route_matching/navigation_state.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

namespace {

bool MatchesPatterns(const KURL& url,
                     const HeapVector<Member<URLPattern>>& patterns) {
  if (url.IsEmpty()) {
    return false;
  }
  for (const URLPattern* pattern : patterns) {
    if (pattern->Match(url)) {
      return true;
    }
  }
  return false;
}

}  // anonymous namespace

void Route::Trace(Visitor* v) const {
  v->Trace(document_);
  v->Trace(patterns_);
}

URLPattern* Route::pattern() const {
  if (!patterns_.empty()) {
    // TODO(crbug.com/436805487): Should multiple patterns be allowed, or not?
    DCHECK_EQ(patterns_.size(), 1u);
    return patterns_[0];
  }
  return nullptr;
}

bool Route::MatchesUrl(const KURL& url) const {
  return MatchesPatterns(url, patterns_);
}

void Route::AddPattern(URLPattern* pattern) {
  DCHECK(pattern);
  patterns_.push_back(pattern);
}

void Route::UpdateMatchStatus(const NavigationState* navigation_state) {
  if (navigation_state) {
    const KURL& old_url = navigation_state->GetOldURL();
    const KURL& new_url = navigation_state->GetNewURL();
    bool committed =
        navigation_state->GetPhase() == NavigationPhase::kCommitted;

    matches_at_ = MatchesPatterns(committed ? new_url : old_url, patterns_);
    matches_from_ = MatchesPatterns(old_url, patterns_);
    matches_to_ = MatchesPatterns(new_url, patterns_);
  } else {
    matches_at_ = false;
    matches_to_ = false;
    matches_from_ = false;
  }
}

}  // namespace blink

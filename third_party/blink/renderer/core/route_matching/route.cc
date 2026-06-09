// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/route_matching/route.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_target_names.h"
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

bool MatchesAllPatternParams(const Vector<std::pair<String, String>>& params1,
                             const Vector<std::pair<String, String>>& params2) {
  if (params1.size() != params2.size()) {
    return false;
  }
  for (wtf_size_t idx = 0; idx < params1.size(); idx++) {
    if (params1[idx].first != params2[idx].first) {
      return false;
    }
    if (params1[idx].first == "0") {
      // Not a :param, but rather a wildcard etc.
      continue;
    }
    if (params1[idx].second != params2[idx].second) {
      return false;
    }
  }
  return true;
}

}  // anonymous namespace

void Route::Trace(Visitor* v) const {
  v->Trace(document_);
  v->Trace(patterns_);
  EventTarget::Trace(v);
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

bool Route::UpdateMatchStatus(const NavigationState* navigation_state) {
  bool matches_at = false;
  bool matches_to = false;
  bool matches_from = false;
  bool matches_with = false;

  if (navigation_state) {
    const KURL& old_url = navigation_state->GetOldURL();
    const KURL& new_url = navigation_state->GetNewURL();
    bool committed =
        navigation_state->GetPhase() == NavigationPhase::kCommitted;

    matches_at = MatchesPatterns(committed ? new_url : old_url, patterns_);
    matches_from = MatchesPatterns(old_url, patterns_);
    matches_to = MatchesPatterns(new_url, patterns_);
    matches_with = MatchesPatterns(committed ? old_url : new_url, patterns_);
  }

  bool at_changed = matches_at_ != matches_at;
  bool from_changed = matches_from_ != matches_from;
  bool to_changed = matches_to_ != matches_to;
  bool with_changed = matches_with_ != matches_with;

  matches_at_ = matches_at;
  matches_from_ = matches_from;
  matches_to_ = matches_to;
  matches_with_ = matches_with;

  return at_changed || from_changed || to_changed || with_changed;
}

bool Route::URLPatternMatchesURLAndHref(const KURL& active_navigation_url,
                                        const KURL& href_url) const {
  const URLPattern* url_pattern = pattern();
  if (!url_pattern) {
    return false;
  }

  URLPattern::MatchResult r1;
  if (!url_pattern->Match(active_navigation_url, &r1)) {
    return false;
  }

  URLPattern::MatchResult r2;
  if (!url_pattern->Match(href_url, &r2)) {
    return false;
  }

  // Certain components are deliberately omitted here.
  //
  // See https://drafts.csswg.org/css-navigation-1/#typedef-init-descriptor-name
  return MatchesAllPatternParams(r1.protocol, r2.protocol) &&
         MatchesAllPatternParams(r1.hostname, r2.hostname) &&
         MatchesAllPatternParams(r1.port, r2.port) &&
         MatchesAllPatternParams(r1.pathname, r2.pathname) &&
         MatchesAllPatternParams(r1.search, r2.search) &&
         MatchesAllPatternParams(r1.hash, r2.hash);
}

const AtomicString& Route::InterfaceName() const {
  return event_target_names::kRoute;
}

ExecutionContext* Route::GetExecutionContext() const {
  return document_->GetExecutionContext();
}

}  // namespace blink

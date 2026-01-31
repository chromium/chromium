// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/route_matching/route.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

namespace {

bool MatchesPatterns(Document& document,
                     const KURL& url,
                     const HeapVector<Member<URLPattern>>& patterns) {
  for (const URLPattern* pattern : patterns) {
    if (pattern->Match(url)) {
      return true;
    }
  }
  return false;
}

bool GetParamValueFromComponent(
    const Vector<std::pair<String, String>>& component,
    const AtomicString& key,
    String& value) {
  for (const auto& param : component) {
    if (param.first == key) {
      value = param.second;
      return true;
    }
  }
  return false;
}

bool GetParamValue(const URLPattern::MatchResult& result,
                   const AtomicString& key,
                   String& value) {
  return GetParamValueFromComponent(result.protocol, key, value) ||
         GetParamValueFromComponent(result.hostname, key, value) ||
         GetParamValueFromComponent(result.port, key, value) ||
         GetParamValueFromComponent(result.pathname, key, value) ||
         GetParamValueFromComponent(result.search, key, value) ||
         GetParamValueFromComponent(result.hash, key, value);
}

bool IsParamEqualTo(const URLPattern& url_pattern,
                    const KURL& url,
                    const AtomicString& key,
                    const String& expected_value) {
  if (url.IsNull()) {
    return false;
  }

  URLPattern::MatchResult result;
  if (!url_pattern.Match(url, &result)) {
    return false;
  }

  String value;
  if (!GetParamValue(result, key, value)) {
    return false;
  }

  return value == expected_value;
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
  return MatchesPatterns(*document_, url, patterns_);
}

void Route::AddPattern(URLPattern* pattern) {
  DCHECK(pattern);
  patterns_.push_back(pattern);
}

bool Route::UpdateMatchStatus(const KURL& previous_url, const KURL& next_url) {
  bool matches_at = MatchesPatterns(*document_, document_->Url(), patterns_);

  // If a previous/next URL are set, we're moving from one route to another.
  // Both need to be set, or none of them should be set.
  DCHECK_EQ(previous_url.IsNull(), next_url.IsNull());

  bool matches_from = !previous_url.IsNull() &&
                      MatchesPatterns(*document_, previous_url, patterns_);
  bool matches_to =
      !next_url.IsNull() && MatchesPatterns(*document_, next_url, patterns_);
  bool from_changed = matches_from_ != matches_from;
  bool to_changed = matches_to_ != matches_to;

  matches_from_ = matches_from;
  matches_to_ = matches_to;
  if (matches_at_ == matches_at) {
    return from_changed || to_changed;
  }

  matches_at_ = matches_at;
  return true;
}

bool Route::FromOrToMatchesParamInHref(const KURL& from,
                                       const KURL& to,
                                       const AtomicString& key,
                                       const KURL& href) const {
  const URLPattern* url_pattern = pattern();
  if (!url_pattern) {
    return false;
  }

  URLPattern::MatchResult result;
  if (!url_pattern->Match(href, &result)) {
    return false;
  }

  String expected_value;
  if (!GetParamValue(result, key, expected_value)) {
    return false;
  }

  return IsParamEqualTo(*url_pattern, from, key, expected_value) ||
         IsParamEqualTo(*url_pattern, to, key, expected_value);
}

bool Route::HrefMatchesParam(const KURL& href,
                             const AtomicString& key,
                             const AtomicString& expected_value) const {
  const URLPattern* url_pattern = pattern();
  if (!url_pattern) {
    return false;
  }

  URLPattern::MatchResult result;
  if (!url_pattern->Match(href, &result)) {
    return false;
  }

  String value;
  return GetParamValue(result, key, value) && value == expected_value;
}

const AtomicString& Route::InterfaceName() const {
  return event_target_names::kRoute;
}

ExecutionContext* Route::GetExecutionContext() const {
  return document_->GetExecutionContext();
}

}  // namespace blink

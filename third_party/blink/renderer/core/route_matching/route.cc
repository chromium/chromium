// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/route_matching/route.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/route_matching/route_event.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

bool MatchesPatterns(Document& document,
                     const KURL& url,
                     const HeapVector<Member<URLPattern>>& patterns) {
  V8URLPatternInput* url_pattern_input =
      MakeGarbageCollected<V8URLPatternInput>(url.GetString());
  v8::Isolate* isolate = document.GetExecutionContext()->GetIsolate();
  for (const URLPattern* pattern : patterns) {
    if (pattern->test(isolate, url_pattern_input, IGNORE_EXCEPTION)) {
      return true;
    }
  }
  return false;
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
  AtomicString type(matches_at_ ? "activate" : "deactivate");
  auto* event = MakeGarbageCollected<RouteEvent>(type);
  event->SetTarget(this);
  DispatchEvent(*event);
  return true;
}

const AtomicString& Route::InterfaceName() const {
  return event_target_names::kRoute;
}

ExecutionContext* Route::GetExecutionContext() const {
  return document_->GetExecutionContext();
}

}  // namespace blink

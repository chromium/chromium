// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/route_matching/route.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

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

void Route::AddPattern(URLPattern* pattern) {
  DCHECK(pattern);
  patterns_.push_back(pattern);
}

bool Route::UpdateMatchStatus() {
  V8URLPatternInput* url_pattern_input =
      MakeGarbageCollected<V8URLPatternInput>(document_->Url().GetString());
  v8::Isolate* isolate = document_->GetExecutionContext()->GetIsolate();
  bool matches_now = false;
  for (const URLPattern* pattern : patterns_) {
    if (pattern->test(isolate, url_pattern_input, IGNORE_EXCEPTION)) {
      matches_now = true;
      break;
    }
  }
  if (matches_ == matches_now) {
    return false;
  }

  matches_ = matches_now;
  return true;
}

}  // namespace blink

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/has_matched_cache_scope.h"

#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

HasMatchedCacheScope::HasMatchedCacheScope(Document* document)
    : document_(document) {
  DCHECK(document_);

  if (document_->GetHasMatchedCacheScope())
    return;

  document_->SetHasMatchedCacheScope(this);
}

HasMatchedCacheScope::~HasMatchedCacheScope() {
  if (document_->GetHasMatchedCacheScope() != this)
    return;

  document_->SetHasMatchedCacheScope(nullptr);
}

// static
ElementHasMatchedMap& HasMatchedCacheScope::GetCacheForSelector(
    const Document* document,
    const CSSSelector* selector) {
  // To increase the cache hit ratio, we need to have a same cache key
  // for multiple selector instances those are actually has a same selector.
  // TODO(blee@igalia.com) Find a way to get hash key without serialization.
  String selector_text = selector->SelectorText();

  DCHECK(document);
  DCHECK(document->GetHasMatchedCacheScope());

  HasMatchedCache& cache =
      document->GetHasMatchedCacheScope()->has_matched_cache_;

  auto element_has_matched_map = cache.find(selector_text);

  if (element_has_matched_map == cache.end()) {
    return *cache
                .Set(selector_text,
                     MakeGarbageCollected<ElementHasMatchedMap>())
                .stored_value->value;
  }

  return *element_has_matched_map->value;
}

}  // namespace blink

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/route_matching/route_map.h"

#include "base/check_is_test.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

RouteMap::RouteMap(Document& document) : Supplement<Document>(document) {
  DCHECK(RuntimeEnabledFeatures::RouteMatchingEnabled());
}

RouteMap::RouteMap() : Supplement<Document>(nullptr) {
  CHECK_IS_TEST();
}

void RouteMap::Trace(Visitor* v) const {
  v->Trace(locations_);
  Supplement<Document>::Trace(v);
}

// BEGIN Supplement support:

const char RouteMap::kSupplementName[] = "RouteMap";

const RouteMap* RouteMap::Get(const Document* document) {
  if (!document) {
    return nullptr;
  }
  return Supplement<Document>::From<RouteMap>(*document);
}

RouteMap* RouteMap::Get(Document* document) {
  if (!document) {
    return nullptr;
  }
  return Supplement<Document>::From<RouteMap>(*document);
}

RouteMap& RouteMap::Ensure(Document& document) {
  RouteMap* route_map = Get(&document);
  if (!route_map) {
    route_map = MakeGarbageCollected<RouteMap>(document);
    Supplement<Document>::ProvideTo<RouteMap>(document, route_map);
  }
  return *route_map;
}

// END Supplement support

void RouteMap::AddURLPatternFromLocation(const AtomicString& dashed_ident,
                                         URLPattern* url_pattern) {
  DCHECK(dashed_ident.starts_with("--"));
  if (locations_.find(dashed_ident) != locations_.end()) {
    // TODO(crbug.com/436805487): Handle route modificiation and removal.
    return;
  }
  locations_.insert(dashed_ident, url_pattern);
}

const URLPattern* RouteMap::FindURLPatternByLocation(
    const AtomicString& location_name) const {
  const auto it = locations_.find(location_name);
  return it == locations_.end() ? nullptr : it->value;
}

}  // namespace blink

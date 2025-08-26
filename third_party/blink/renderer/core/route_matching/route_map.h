// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MAP_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Document;
class KURL;

// TODO(crbug.com/436805487): Document this when we know more.
//
// See;
// https://github.com/WICG/declarative-partial-updates?tab=readme-ov-file#part-2-route-matching
class CORE_EXPORT RouteMap final : public GarbageCollected<RouteMap>,
                                   public Supplement<Document> {
 public:
  static const char kSupplementName[];

  struct Route final : public GarbageCollected<Route> {
    HeapVector<String> patterns_;

    void Trace(Visitor* v) const { v->Trace(patterns_); }
  };

  struct ParseResult final {
    // TODO(crbug.com/436805487): Error reporting needs to be specced.
    enum Status {
      kSuccess,
      kSyntaxError,
      kTypeError,
    };

    Status status;
    String message;

    // For `kSuccess` cases (which don't have messages).
    explicit ParseResult(Status status) : status(status) {
      CHECK_EQ(status, kSuccess);
    }

    // For error cases.
    ParseResult(Status status, String message)
        : status(status), message(message) {
      CHECK(status != kSuccess);
    }
  };

  explicit RouteMap(Document&);

  // For testing only.
  RouteMap();

  // Supplement support:
  static const RouteMap* Get(const Document&);
  static RouteMap* Get(Document&);
  static RouteMap& Ensure(Document&);

  ParseResult ParseAndApplyRoutes(const String& route_map_text);

  ParseResult ParseRoutes(const String& route_map_text);

  bool MatchesRoute(const KURL&, const String& route) const;

  HashSet<String> GetActiveRoutes(const KURL&) const;

  void Trace(Visitor*) const final;

 private:
  HeapHashMap<String, Member<Route>> routes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MAP_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MAP_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Document;
class Route;

// TODO(crbug.com/436805487): Document this when we know more.
//
// See;
// https://github.com/WICG/declarative-partial-updates?tab=readme-ov-file#part-2-route-matching
class CORE_EXPORT RouteMap final : public GarbageCollected<RouteMap>,
                                   public Supplement<Document> {
 public:
  static const char kSupplementName[];

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

    bool IsSuccess() const { return status == kSuccess; }
  };

  explicit RouteMap(Document&);

  // For testing only.
  RouteMap();

  void Trace(Visitor*) const final;

  // Supplement support. Document pointers may be null (in which case null will
  // be returned).
  static const RouteMap* Get(const Document*);
  static RouteMap* Get(Document*);
  static RouteMap& Ensure(Document&);

  Document& GetDocument() const {
    Document* document = GetSupplementable();
    DCHECK(document);
    return *document;
  }

  ParseResult ParseAndApplyRoutes(const String& route_map_text);

  ParseResult ParseRoutes(const String& route_map_text);

  bool MatchesRoute(const String& route) const;

  // Re-match all routes. Return true if any route changed state.
  bool UpdateActiveRoutes();

  HashSet<String> GetActiveRoutes() const;

 private:
  HeapHashMap<String, Member<Route>> routes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MAP_H_

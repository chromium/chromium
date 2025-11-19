// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MAP_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/route_matching/route_match_state.h"
#include "third_party/blink/renderer/core/route_matching/route_preposition.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Document;
class Route;
class URLPattern;

// TODO(crbug.com/436805487): Document this when we know more.
//
// See;
// https://github.com/WICG/declarative-partial-updates?tab=readme-ov-file#part-2-route-matching
class CORE_EXPORT RouteMap final : public ScriptWrappable,
                                   public GarbageCollectedMixin {
  DEFINE_WRAPPERTYPEINFO();

 public:
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

  Route* get(const String& route_name);

  // Document pointers may be null (in which case null will be returned).
  static const RouteMap* Get(const Document*);
  static RouteMap* Get(Document*);
  static RouteMap& Ensure(Document&);

  Document& GetDocument() const {
    DCHECK(document_);
    return *document_;
  }

  ParseResult ParseAndApplyRoutes(const String& route_map_text);

  ParseResult ParseRoutes(const String& route_map_text);

  void AddAnonymousRoute(URLPattern*);

  const Route* FindRoute(const String& route_name) const;
  const Route* FindRoute(const URLPattern*) const;

  // Re-match all routes. Schedule for re-evaluation of CSS rules if something
  // changed.
  void UpdateActiveRoutes();

  void GetActiveRoutes(RoutePreposition,
                       RouteMatchState::MatchCollection*) const;

  // Set the URLs that we're navigating between at the start of navigation. This
  // is used to match @route "from" (and "to") rules.
  void OnNavigationStart(const KURL& previous_url, const KURL& next_url) {
    previous_url_ = previous_url;
    next_url_ = next_url;
    UpdateActiveRoutes();
  }

  // Clear the URL that we're navigating between when the navigation is
  // complete.
  void OnNavigationDone() {
    previous_url_ = KURL();
    next_url_ = KURL();
    UpdateActiveRoutes();
  }

 private:
  Member<Document> document_;

  HeapHashMap<String, Member<Route>> routes_;
  HeapHashMap<String, Member<Route>> anonymous_routes_;

  // Only set while navigating from one URL to another one.
  KURL previous_url_;
  KURL next_url_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MAP_H_

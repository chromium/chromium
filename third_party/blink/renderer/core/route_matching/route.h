// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_H_

#include "third_party/blink/renderer/core/route_matching/navigation_phase.h"
#include "third_party/blink/renderer/core/route_matching/navigation_preposition.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Document;
class KURL;
class NavigationState;
class URLPattern;

class Route : public GarbageCollected<Route> {
 public:
  Route(Document& document) : document_(&document) {}
  void Trace(Visitor* v) const;

  URLPattern* pattern() const;
  bool matches() const { return matches_at_; }

  bool Matches(NavigationPreposition preposition) const {
    switch (preposition) {
      case NavigationPreposition::kAt:
        return matches_at_;
      case NavigationPreposition::kFrom:
        return matches_from_;
      case NavigationPreposition::kTo:
        return matches_to_;
    }
  }

  bool MatchesUrl(const KURL&) const;

  void AddPattern(URLPattern*);

  // Check and update whether or not this route matches anything. Store the
  // current state.
  void UpdateMatchStatus(const NavigationState*);

 private:
  Member<Document> document_;
  HeapVector<Member<URLPattern>> patterns_;
  bool matches_at_ = false;
  bool matches_from_ = false;
  bool matches_to_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_H_

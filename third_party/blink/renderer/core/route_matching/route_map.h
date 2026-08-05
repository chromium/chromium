// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MAP_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/route_matching/navigation_preposition.h"
#include "third_party/blink/renderer/core/route_matching/navigation_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"
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
class CORE_EXPORT RouteMap final : public GarbageCollected<RouteMap>,
                                   public Supplement<Document> {
 public:
  static const char kSupplementName[];

  using MatchCollection = HeapHashSet<WeakMember<Route>>;

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

  void SetHasHistoryRules() {
    has_history_rules_ = true;
    SetNeedsStyleUpdateOnNavigation();
  }
  bool HasHistoryRules() const { return has_history_rules_; }

  void SetNeedsStyleUpdateOnNavigation() {
    needs_style_update_on_navigation_ = true;
  }

  void AddRouteFromRule(const String& dashed_ident, URLPattern*);
  void AddAnonymousRoute(const AtomicString& url_pattern_string);

  const Route* FindRoute(const AtomicString& route_name) const;
  const Route* FindAnonymousRoute(const AtomicString& url_pattern_string) const;

  // Re-match all routes. Schedule for re-evaluation of CSS rules if something
  // changed. NavigationState may be nullptr.
  void UpdateActiveRoutes(NavigationState*);

  // TODO(crbug.com/436805487): We probably don't need to keep this.
  void GetActiveRoutesForTesting(NavigationPreposition, MatchCollection*) const;

  // When the new document in a cross-document navigation is ready, this
  // function is called, in order to establish an active navigation. For
  // same-document navigations, this is instead handled directly by the
  // Navigation API.
  void EstablishNavigationStateFromActivation();

  // Set the navigation as started, based on the current NavigationState. This
  // is used to match @route "from" (and "to") rules.
  void SetNavigationStarted();

  void SetTraverseType(NavigationState::HistoryTraverseType type);

  // The current URL has changed. This is used to match @route "at" rules.
  void SetCommitted();

  // Finish the navigation if allowed, and update routes. Calling this if
  // there's no active navigation is allowed, and has no effect.
  //
  // Returns false if we cannot finish yet, e.g. due to an active view
  // transition.
  bool AttemptSetNavigationFinished();

  void OnPreviewStart();
  void OnPreviewFinished();

  bool IsActiveNavigation() const {
    return !!NavigationState::Get(&GetDocument());
  }

  // Get the "active navigation URL", given the specified preposition.
  //
  // https://drafts.csswg.org/css-navigation-1/#active-navigation-url
  KURL GetActiveNavigationURL(NavigationPreposition) const;

 private:
  void NotifyStyleEngineIfNeeded();

  HeapHashMap<String, Member<Route>> routes_;
  HeapHashMap<String, Member<Route>> anonymous_routes_;

  bool has_history_rules_ = false;

  bool needs_style_update_on_navigation_ = false;

#if DCHECK_IS_ON()
  bool is_updating_active_routes_ = false;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MAP_H_

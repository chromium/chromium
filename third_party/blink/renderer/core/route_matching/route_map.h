// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MAP_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/route_matching/navigation_preposition.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class Document;
class URLPattern;

// TODO(crbug.com/436805487): Document this when we know more.
//
// See;
// https://github.com/WICG/declarative-partial-updates?tab=readme-ov-file#part-2-route-matching
class CORE_EXPORT RouteMap final : public GarbageCollected<RouteMap>,
                                   public Supplement<Document> {
 public:
  static const char kSupplementName[];

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

  void SetNeedsStyleUpdateOnNavigation() {
    needs_style_update_on_navigation_ = true;
  }

  void AddURLPatternFromLocation(const AtomicString& dashed_ident, URLPattern*);

  const URLPattern* FindURLPatternByLocation(
      const AtomicString& location_name) const;

  // When the new document in a cross-document navigation is ready, this
  // function is called, in order to establish an active navigation. For
  // same-document navigations, this is instead handled directly by the
  // Navigation API.
  void EstablishNavigationStateFromActivation();

  // Set the navigation as started, based on the current NavigationState. This
  // is used to match @navigation rules.
  void SetNavigationStarted();

  // The current URL has changed. This is used to match @navigation "at" rules.
  void SetCommitted();

  // Finish the navigation if allowed. Calling this if there's no active
  // navigation is allowed, and has no effect.
  //
  // Returns false if we cannot finish yet, e.g. due to an active view
  // transition.
  bool AttemptSetNavigationFinished();

  void OnPreviewStart();
  void OnPreviewFinished();

  // Return true if the URLPattern matches the current NavigationState, with the
  // preposition given.
  bool MatchesCurrentNavigation(NavigationPreposition, const URLPattern&) const;

 private:
  void NotifyStyleEngineIfNeeded();

  // URLPattern entries defined by @location rules.
  HeapHashMap<AtomicString, Member<URLPattern>> locations_;

  bool needs_style_update_on_navigation_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MAP_H_

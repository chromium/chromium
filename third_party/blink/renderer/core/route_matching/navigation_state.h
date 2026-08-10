// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_NAVIGATION_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_NAVIGATION_STATE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/route_matching/navigation_phase.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class Document;
class Element;

// Based on "navigation state":
// https://drafts.csswg.org/css-navigation-1/#processing-model
class CORE_EXPORT NavigationState final
    : public GarbageCollected<NavigationState>,
      public Supplement<Document> {
 public:
  static const char kSupplementName[];

  enum HistoryTraverseType {
    kNotTraversing,
    kBack,
    kForward,
    kReload,
  };

  NavigationState(Document& document,
                  const KURL& old_url,
                  const KURL& new_url,
                  Element* source_element)
      : Supplement<Document>(document),
        old_url_(old_url),
        new_url_(new_url),
        source_element_(source_element) {
    DCHECK(RuntimeEnabledFeatures::NavigationStateEnabled());
  }

  static const NavigationState* Get(const Document* document) {
    if (!document) {
      return nullptr;
    }
    return Supplement<Document>::From<NavigationState>(*document);
  }
  static NavigationState* Get(Document* document) {
    if (!document) {
      return nullptr;
    }
    return Supplement<Document>::From<NavigationState>(*document);
  }

  // Create a new NavigationState object and associate with the specified
  // Document.
  static NavigationState& Create(Document&,
                                 const KURL& old_url,
                                 const KURL& new_url,
                                 Element* source_element);

  // Attempt to finish any ongoing navigation, based on the current
  // NavigationState associated (if any) with the specified Document. Will
  // destroy that NavigationState if it was possible to finish the navigation.
  static void AttemptFinishNavigationAndDestroy(Document*);

  void Trace(Visitor*) const final;

  bool Equal(const NavigationState& other) const {
    return old_url_ == other.old_url_ && new_url_ == other.new_url_ &&
           source_element_ == other.source_element_ &&
           traverse_type_ == other.traverse_type_ && phase_ == other.phase_ &&
           is_in_preview_ == other.is_in_preview_;
  }
  bool operator==(const NavigationState& other) const { return Equal(other); }
  bool operator!=(const NavigationState& other) const { return !Equal(other); }

  KURL GetOldURL() const { return old_url_; }
  KURL GetNewURL() const { return new_url_; }

  Element* GetSourceElement() { return source_element_; }
  const Element* GetSourceElement() const { return source_element_; }

  void SetTraverseType(HistoryTraverseType type) { traverse_type_ = type; }
  HistoryTraverseType GetTraverseType() const { return traverse_type_; }

  void SetPhase(NavigationPhase phase) { phase_ = phase; }
  NavigationPhase GetPhase() const { return phase_; }

  void SetIsInPreview(bool b) { is_in_preview_ = b; }
  bool IsInPreview() const { return is_in_preview_; }

 private:
  KURL old_url_;
  KURL new_url_;

  Member<Element> source_element_;

  HistoryTraverseType traverse_type_ = kNotTraversing;
  NavigationPhase phase_ = NavigationPhase::kLoading;
  bool is_in_preview_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_NAVIGATION_STATE_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_NAVIGATION_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_NAVIGATION_STATE_H_

#include "third_party/blink/renderer/core/route_matching/navigation_phase.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class Document;

// Based on "navigation state":
// https://drafts.csswg.org/css-navigation-1/#processing-model
class NavigationState : public GarbageCollected<NavigationState> {
 public:
  enum HistoryTraverseType {
    kNotTraversing,
    kBack,
    kForward,
  };

  NavigationState(const KURL& old_url, const KURL& new_url)
      : old_url_(old_url), new_url_(new_url) {}

  static const NavigationState* Get(const Document*);

  void Trace(Visitor*) const;

  bool Equal(const NavigationState& other) const {
    return old_url_ == other.old_url_ && new_url_ == other.new_url_ &&
           traverse_type_ == other.traverse_type_ && phase_ == other.phase_ &&
           is_in_preview_ == other.is_in_preview_;
  }
  bool operator==(const NavigationState& other) const { return Equal(other); }
  bool operator!=(const NavigationState& other) const { return !Equal(other); }

  KURL GetOldURL() const { return old_url_; }
  KURL GetNewURL() const { return new_url_; }

  void SetTraverseType(HistoryTraverseType type) { traverse_type_ = type; }
  HistoryTraverseType GetTraverseType() const { return traverse_type_; }

  void SetPhase(NavigationPhase phase) { phase_ = phase; }
  NavigationPhase GetPhase() const { return phase_; }

  void SetIsInPreview(bool b) { is_in_preview_ = b; }
  bool IsInPreview() const { return is_in_preview_; }

 private:
  KURL old_url_;
  KURL new_url_;

  HistoryTraverseType traverse_type_ = kNotTraversing;
  NavigationPhase phase_ = NavigationPhase::kLoading;
  bool is_in_preview_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_NAVIGATION_STATE_H_

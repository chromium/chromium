// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SCROLL_MARKER_GROUP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SCROLL_MARKER_GROUP_H_

#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ScrollMarkerGroup : public GarbageCollected<ScrollMarkerGroup> {
 public:
  enum class ScrollMarkerPosition { kAfter, kBefore };
  enum class ScrollMarkerMode { kTabs, kLinks };

  explicit ScrollMarkerGroup(ScrollMarkerPosition position,
                             ScrollMarkerMode mode = ScrollMarkerMode::kLinks)
      : mode_(mode), position_(position) {}

  ScrollMarkerGroup(const ScrollMarkerGroup&) = delete;
  ScrollMarkerGroup& operator=(const ScrollMarkerGroup&) = delete;

  bool operator==(const ScrollMarkerGroup& other) const {
    return position_ == other.position_ && mode_ == other.mode_;
  }

  ScrollMarkerMode Mode() const { return mode_; }
  ScrollMarkerPosition Position() const { return position_; }

  bool IsInTabsMode() const { return mode_ == ScrollMarkerMode::kTabs; }
  bool IsInLinksMode() const { return mode_ == ScrollMarkerMode::kLinks; }
  bool PositionAfter() const {
    return position_ == ScrollMarkerPosition::kAfter;
  }
  bool PositionBefore() const {
    return position_ == ScrollMarkerPosition::kBefore;
  }

  virtual void Trace(Visitor* v) const {}

 private:
  ScrollMarkerMode mode_;
  ScrollMarkerPosition position_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SCROLL_MARKER_GROUP_H_

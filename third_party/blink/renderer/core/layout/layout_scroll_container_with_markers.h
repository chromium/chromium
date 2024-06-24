// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_SCROLL_CONTAINER_WITH_MARKERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_SCROLL_CONTAINER_WITH_MARKERS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

namespace blink {

// Layout wrapper for scroll container and ::scroll-marker-group.
// Largely inspired by LayoutFieldset.
class CORE_EXPORT LayoutScrollContainerWithMarkers final
    : public LayoutBlockFlow {
 public:
  explicit LayoutScrollContainerWithMarkers(Element*);

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutScrollContainerWithMarkers";
  }

  void InsertedIntoTree() override;
  void UpdateAnonymousChildStyle(
      const LayoutObject* child,
      ComputedStyleBuilder& child_style_builder) const override;
  void AddChild(LayoutObject* new_child, LayoutObject* before_child) override;

  LayoutBox* FindScrollMarkerGroup() const;
  bool CreatesNewFormattingContext() const final {
    NOT_DESTROYED();
    return true;
  }

 protected:
  bool IsScrollContainerWithMarkers() const final {
    NOT_DESTROYED();
    return true;
  }

  bool RespectsCSSOverflow() const override {
    NOT_DESTROYED();
    return false;
  }
  LayoutUnit ScrollWidth() const override;
  LayoutUnit ScrollHeight() const override;
};

template <>
struct DowncastTraits<LayoutScrollContainerWithMarkers> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsScrollContainerWithMarkers();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_SCROLL_CONTAINER_WITH_MARKERS_H_

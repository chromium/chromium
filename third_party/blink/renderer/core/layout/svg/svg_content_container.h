// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_CONTENT_CONTAINER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_CONTENT_CONTAINER_H_

#include "third_party/blink/renderer/core/layout/api/hit_test_action.h"
#include "third_party/blink/renderer/core/layout/layout_object_child_list.h"

namespace blink {

class FloatRect;
class HitTestLocation;
class HitTestResult;

// Content representation for an SVG container. Wraps a LayoutObjectChildList
// with additional state related to the children of the container. Used by
// <svg>, <g> etc.
class SVGContentContainer {
 public:
  void Layout(bool force_layout,
              bool screen_scaling_factor_changed,
              bool layout_size_changed);
  bool HitTest(HitTestResult&, const HitTestLocation&, HitTestAction) const;

  void ComputeBoundingBoxes(FloatRect& object_bounding_box,
                            bool& object_bounding_box_valid,
                            FloatRect& stroke_bounding_box) const;
  bool ComputeHasNonIsolatedBlendingDescendants() const;

  LayoutObjectChildList& Children() { return children_; }
  const LayoutObjectChildList& Children() const { return children_; }

 private:
  LayoutObjectChildList children_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_CONTENT_CONTAINER_H_

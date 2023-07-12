// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_CONTENT_CONTAINER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_CONTENT_CONTAINER_H_

#include "third_party/blink/renderer/core/layout/hit_test_phase.h"
#include "third_party/blink/renderer/core/layout/layout_object_child_list.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

class HitTestLocation;
class HitTestResult;

struct SVGContainerLayoutInfo {
  bool force_layout = false;
  bool scale_factor_changed = false;
  bool viewport_changed = false;
};

// Content representation for an SVG container. Wraps a LayoutObjectChildList
// with additional state related to the children of the container. Used by
// <svg>, <g> etc.
class SVGContentContainer {
  DISALLOW_NEW();

 public:
  static bool IsChildAllowed(const LayoutObject& child);
  void Layout(const SVGContainerLayoutInfo&);
  bool HitTest(HitTestResult&, const HitTestLocation&, HitTestPhase) const;

  bool UpdateBoundingBoxes(bool& object_bounding_box_valid);
  const gfx::RectF& ObjectBoundingBox() const { return object_bounding_box_; }
  const gfx::RectF& DecoratedBoundingBox() const {
    return decorated_bounding_box_;
  }

  bool ComputeHasNonIsolatedBlendingDescendants() const;
  gfx::RectF ComputeStrokeBoundingBox() const;

  LayoutObjectChildList& Children() { return children_; }
  const LayoutObjectChildList& Children() const { return children_; }

  void Trace(Visitor* visitor) const { visitor->Trace(children_); }

 private:
  LayoutObjectChildList children_;

  gfx::RectF object_bounding_box_;
  gfx::RectF decorated_bounding_box_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_CONTENT_CONTAINER_H_

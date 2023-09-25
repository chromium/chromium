// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_GEOMETRY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

class Element;
class LayoutObject;
class Node;

// Computes the intersection between an ancestor (root) node and a
// descendant (target) element, with overflow and CSS clipping applied.
// Optionally also checks whether the target is occluded or has visual
// effects applied.
//
// If the root argument to the constructor is null, computes the intersection
// of the target with the top-level frame viewport (AKA the "implicit root").
class CORE_EXPORT IntersectionGeometry {
 public:
  // See comment of IntersectionObserver::kMinimumThreshold.
  static constexpr float kMinimumThreshold = std::numeric_limits<float>::min();

  static constexpr gfx::Vector2dF kInfiniteScrollDelta{
      std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};

  enum Flags {
    // These flags should passed to the constructor
    kShouldReportRootBounds = 1 << 0,
    kShouldComputeVisibility = 1 << 1,
    kShouldTrackFractionOfRoot = 1 << 2,
    kForFrameViewportIntersection = 1 << 3,
    kShouldConvertToCSSPixels = 1 << 4,
    // Applies to boxes. If true, OverflowClipRect() is used if necessary
    // instead of BorderBoundingBox().
    kUseOverflowClipEdge = 1 << 5,

    // These flags will be computed
    kShouldUseCachedRects = 1 << 6,
    kRootIsImplicit = 1 << 7,
    kDidComputeGeometry = 1 << 8,
    kIsVisible = 1 << 9
  };

  struct RootGeometry {
    STACK_ALLOCATED();

   public:
    RootGeometry(const LayoutObject* root, const Vector<Length>& margin);

    float zoom;
    // The root object's content rect in the root object's own coordinate system
    PhysicalRect local_root_rect;
    gfx::Transform root_to_document_transform;
  };

  struct CachedRects {
    // Target's bounding rect in the target's coordinate space
    PhysicalRect local_target_rect;
    // Target rect mapped up to the root's space, with intermediate clips
    // applied, but without applying the root's clip or scroll offset.
    PhysicalRect unscrolled_unclipped_intersection_rect;
    // We only need to update intersection geometry on future scroll if
    // the scroll delta >= this value in either direction.
    gfx::Vector2dF min_scroll_delta_to_update;
    // True iff unscrolled_unclipped_intersection_rect actually intersects the
    // root, as defined by edge-inclusive intersection rules.
    bool does_intersect = false;
    // True iff the target rect before any margins were applied was empty
    bool pre_margin_target_rect_is_empty = false;
    // Invalidation flag
    bool valid = false;
  };

  static const LayoutObject* GetExplicitRootLayoutObject(const Node& root_node);

  IntersectionGeometry(const Node* root,
                       const Element& target,
                       const Vector<Length>& root_margin,
                       const Vector<float>& thresholds,
                       const Vector<Length>& target_margin,
                       const Vector<Length>& scroll_margin,
                       unsigned flags,
                       CachedRects* cached_rects = nullptr);

  IntersectionGeometry(const RootGeometry& root_geometry,
                       const Node& explicit_root,
                       const Element& target,
                       const Vector<float>& thresholds,
                       const Vector<Length>& target_margin,
                       const Vector<Length>& scroll_margin,
                       unsigned flags,
                       CachedRects* cached_rects = nullptr);

  IntersectionGeometry(const IntersectionGeometry&) = default;

  bool ShouldReportRootBounds() const {
    return flags_ & kShouldReportRootBounds;
  }
  bool ShouldComputeVisibility() const {
    return flags_ & kShouldComputeVisibility;
  }
  bool ShouldTrackFractionOfRoot() const {
    return flags_ & kShouldTrackFractionOfRoot;
  }

  PhysicalRect TargetRect() const { return target_rect_; }
  PhysicalRect IntersectionRect() const { return intersection_rect_; }

  // The intersection rect without applying viewport clipping.
  PhysicalRect UnclippedIntersectionRect() const {
    return unclipped_intersection_rect_;
  }

  PhysicalRect RootRect() const { return root_rect_; }

  gfx::Rect IntersectionIntRect() const {
    return ToPixelSnappedRect(intersection_rect_);
  }
  gfx::Rect TargetIntRect() const { return ToPixelSnappedRect(target_rect_); }
  gfx::Rect RootIntRect() const { return ToPixelSnappedRect(root_rect_); }

  double IntersectionRatio() const { return intersection_ratio_; }
  unsigned ThresholdIndex() const { return threshold_index_; }

  bool DidComputeGeometry() const { return flags_ & kDidComputeGeometry; }
  bool IsIntersecting() const { return threshold_index_ > 0; }
  bool IsVisible() const { return flags_ & kIsVisible; }

  gfx::Vector2dF MinScrollDeltaToUpdate() const {
    return min_scroll_delta_to_update_;
  }

  bool CanUseCachedRectsForTesting() const { return ShouldUseCachedRects(); }

 private:
  bool RootIsImplicit() const { return flags_ & kRootIsImplicit; }
  bool ShouldUseCachedRects() const { return flags_ & kShouldUseCachedRects; }
  bool IsForFrameViewportIntersection() const {
    return flags_ & kForFrameViewportIntersection;
  }

  struct RootAndTarget {
    STACK_ALLOCATED();

   public:
    RootAndTarget(const Node* root_node, const Element& target_element);
    const LayoutObject* target;
    const LayoutObject* root;
    enum Relationship {
      kInvalid,
      // The target is in a sub-frame of the implicit root.
      kTargetInSubFrame,
      // The target can't be scrolled in the root by any scroller.
      kNotScrollable,
      // The target can be scrolled in the root by the root only, without any
      // intermediate clippers (scroll containers or not).
      kScrollableByRootOnly,
      // The target can be scrolled in the root, with intermediate clippers
      // (scroll containers or not).
      kScrollableWithIntermediateClippers,
    };
    Relationship relationship = kInvalid;
    // This is used only when relationship is kScrollable*.
    bool has_filter = false;

   private:
    static const LayoutObject* GetTargetLayoutObject(
        const Element& target_element);
    const LayoutObject* GetRootLayoutObject(const Node* root_node) const;
    void ComputeRelationship(bool root_is_implicit);
  };
  RootAndTarget PrepareComputeGeometry(const Node* root_node,
                                       const Element& target_element,
                                       CachedRects* cached_rects);

  void ComputeGeometry(const RootGeometry& root_geometry,
                       const RootAndTarget& root_and_target,
                       const Vector<float>& thresholds,
                       const Vector<Length>& target_margin,
                       const Vector<Length>& scroll_margin,
                       CachedRects* cached_rects);

  // Map intersection_rect from the coordinate system of the target to the
  // coordinate system of the root, applying intervening clips.
  bool ClipToRoot(const LayoutObject* root,
                  const LayoutObject* target,
                  const PhysicalRect& root_rect,
                  PhysicalRect& unclipped_intersection_rect,
                  PhysicalRect& intersection_rect,
                  const Vector<Length>& scroll_margin,
                  CachedRects* cached_rects = nullptr);
  unsigned FirstThresholdGreaterThan(float ratio,
                                     const Vector<float>& thresholds) const;

  gfx::Vector2dF ComputeMinScrollDeltaToUpdate(
      const RootAndTarget& root_and_target,
      const gfx::Transform& target_to_document_transform,
      const gfx::Transform& root_to_document_transform,
      const Vector<float>& thresholds,
      const Vector<Length>& scroll_margin) const;

  PhysicalRect target_rect_;
  PhysicalRect intersection_rect_;
  PhysicalRect unclipped_intersection_rect_;
  PhysicalRect root_rect_;
  gfx::Vector2dF min_scroll_delta_to_update_;
  unsigned flags_;
  double intersection_ratio_ = 0;
  unsigned threshold_index_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_GEOMETRY_H_

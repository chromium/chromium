// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_GEOMETRY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

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
  enum Flags {
    // These flags should passed to the constructor
    kShouldReportRootBounds = 1 << 0,
    kShouldComputeVisibility = 1 << 1,
    kShouldTrackFractionOfRoot = 1 << 2,
    kShouldUseReplacedContentRect = 1 << 3,
    kShouldConvertToCSSPixels = 1 << 4,
    kShouldUseCachedRects = 1 << 5,

    // These flags will be computed
    kRootIsImplicit = 1 << 6,
    kIsVisible = 1 << 7
  };

  struct RootGeometry {
    STACK_ALLOCATED();

   public:
    RootGeometry(const LayoutObject* root, const Vector<Length>& margin);

    float zoom;
    // The root object's content rect in the root object's own coordinate system
    PhysicalRect local_root_rect;
    TransformationMatrix root_to_document_transform;
  };

  struct CachedRects {
    // Target's bounding rect in the target's coordinate space
    PhysicalRect local_target_rect;
    // Target rect mapped up to the root's space, with intermediate clips
    // applied, but without applying the root's clip or scroll offset.
    PhysicalRect unscrolled_unclipped_intersection_rect;
    // True iff unscrolled_unclipped_intersection_rect actually intersects the
    // root, as defined by edge-inclusive intersection rules.
    bool does_intersect;
    // Invalidation flag
    bool valid;
  };

  static const LayoutObject* GetRootLayoutObjectForTarget(
      const Node* root_node,
      LayoutObject* target,
      bool check_containing_block_chain);

  IntersectionGeometry(const Node* root,
                       const Element& target,
                       const Vector<Length>& root_margin,
                       const Vector<float>& thresholds,
                       const Vector<Length>& target_margin,
                       unsigned flags,
                       CachedRects* cached_rects = nullptr);

  IntersectionGeometry(const RootGeometry& root_geometry,
                       const Node& explicit_root,
                       const Element& target,
                       const Vector<float>& thresholds,
                       const Vector<Length>& target_margin,
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

  IntRect IntersectionIntRect() const {
    return PixelSnappedIntRect(intersection_rect_);
  }
  IntRect TargetIntRect() const { return PixelSnappedIntRect(target_rect_); }
  IntRect RootIntRect() const { return PixelSnappedIntRect(root_rect_); }

  double IntersectionRatio() const { return intersection_ratio_; }
  unsigned ThresholdIndex() const { return threshold_index_; }

  bool RootIsImplicit() const { return flags_ & kRootIsImplicit; }
  bool IsIntersecting() const { return threshold_index_ > 0; }
  bool IsVisible() const { return flags_ & kIsVisible; }

 private:
  bool ShouldUseCachedRects() const { return flags_ & kShouldUseCachedRects; }
  void ComputeGeometry(const RootGeometry& root_geometry,
                       const LayoutObject* root,
                       const LayoutObject* target,
                       const Vector<float>& thresholds,
                       const Vector<Length>& target_margin,
                       CachedRects* cached_rects);
  // Map intersection_rect from the coordinate system of the target to the
  // coordinate system of the root, applying intervening clips.
  bool ClipToRoot(const LayoutObject* root,
                  const LayoutObject* target,
                  const PhysicalRect& root_rect,
                  PhysicalRect& unclipped_intersection_rect,
                  PhysicalRect& intersection_rect,
                  CachedRects* cached_rects = nullptr);
  unsigned FirstThresholdGreaterThan(float ratio,
                                     const Vector<float>& thresholds) const;

  PhysicalRect target_rect_;
  PhysicalRect intersection_rect_;
  PhysicalRect unclipped_intersection_rect_;
  PhysicalRect root_rect_;
  unsigned flags_;
  double intersection_ratio_;
  unsigned threshold_index_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_GEOMETRY_H_

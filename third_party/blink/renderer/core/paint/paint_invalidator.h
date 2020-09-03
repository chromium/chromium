// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_INVALIDATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_INVALIDATOR_H_

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_builder.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint_invalidation_reason.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class PrePaintTreeWalk;

struct CORE_EXPORT PaintInvalidatorContext {
  DISALLOW_NEW();

 public:
  class ParentContextAccessor {
   public:
    ParentContextAccessor() = default;
    ParentContextAccessor(PrePaintTreeWalk* tree_walk,
                          wtf_size_t parent_context_index)
        : tree_walk_(tree_walk), parent_context_index_(parent_context_index) {}
    const PaintInvalidatorContext* ParentContext() const;

   private:
    PrePaintTreeWalk* tree_walk_ = nullptr;
    wtf_size_t parent_context_index_ = 0u;
  };

  PaintInvalidatorContext() = default;

  PaintInvalidatorContext(const ParentContextAccessor& parent_context_accessor)
      : parent_context_accessor_(parent_context_accessor),
        subtree_flags(ParentContext()->subtree_flags),
        directly_composited_container(
            ParentContext()->directly_composited_container),
        directly_composited_container_for_stacked_contents(
            ParentContext()
                ->directly_composited_container_for_stacked_contents),
        painting_layer(ParentContext()->painting_layer) {}

  bool NeedsSubtreeWalk() const {
    return subtree_flags &
           (kSubtreeInvalidationChecking | kSubtreeFullInvalidation |
            kSubtreeFullInvalidationForStackedContents);
  }

  const PaintInvalidatorContext* ParentContext() const {
    return parent_context_accessor_.ParentContext();
  }

 private:
  // Parent context accessor has to be initialized first, so inject the private
  // access block here for that reason.
  ParentContextAccessor parent_context_accessor_;

 public:
  // When adding new subtree flags, ensure |NeedsSubtreeWalk| is updated.
  enum SubtreeFlag {
    kSubtreeInvalidationChecking = 1 << 0,
    kSubtreeFullInvalidation = 1 << 1,
    kSubtreeFullInvalidationForStackedContents = 1 << 2,

    // When this flag is set, no paint or raster invalidation will be issued
    // for the subtree.
    //
    // Context: some objects in this paint walk, for example SVG resource
    // container subtrees, always paint onto temporary PaintControllers which
    // don't have cache, and don't actually have any raster regions, so they
    // don't need any invalidation. They are used as "painting subroutines"
    // for one or more other locations in SVG.
    kSubtreeNoInvalidation = 1 << 6,
  };
  unsigned subtree_flags = 0;

  // The following fields can be null only before
  // PaintInvalidator::updateContext().

  // The current directly composited  container for normal flow objects.
  // It is the enclosing composited object.
  const LayoutBoxModelObject* directly_composited_container = nullptr;

  // The current directly composited container for stacked contents (stacking
  // contexts or positioned objects). It is the nearest ancestor composited
  // object which establishes a stacking context. For more information, see:
  // https://chromium.googlesource.com/chromium/src.git/+/master/third_party/blink/renderer/core/paint/README.md#Stacked-elements-and-stacking-contexts
  const LayoutBoxModelObject*
      directly_composited_container_for_stacked_contents = nullptr;

  PaintLayer* painting_layer = nullptr;

  // The previous PaintOffset of FragmentData.
  PhysicalOffset old_paint_offset;

  const FragmentData* fragment_data = nullptr;

 private:
  friend class PaintInvalidator;

  // Not using Optional because we need to keep the pointer stable when the
  // vector containing this PaintInvalidatorContext reallocates.
  std::unique_ptr<LayoutShiftTracker::ContainingBlockScope>
      containing_block_scope_;
  const TransformPaintPropertyNodeOrAlias* transform_ = nullptr;
};

class PaintInvalidator {
  DISALLOW_NEW();

 public:
  // Returns true if the object is invalidated.
  bool InvalidatePaint(const LayoutObject&,
                       const NGPrePaintInfo*,
                       const PaintPropertyTreeBuilderContext*,
                       PaintInvalidatorContext&);

  // Process objects needing paint invalidation on the next frame.
  // See the definition of PaintInvalidationDelayedFull for more details.
  void ProcessPendingDelayedPaintInvalidations();

 private:
  friend struct PaintInvalidatorContext;
  friend class PrePaintTreeWalk;

  ALWAYS_INLINE void UpdatePaintingLayer(const LayoutObject&,
                                         PaintInvalidatorContext&,
                                         bool is_ng_painting);
  ALWAYS_INLINE void UpdateDirectlyCompositedContainer(const LayoutObject&,
                                                       PaintInvalidatorContext&,
                                                       bool is_ng_painting);
  ALWAYS_INLINE void UpdateFromTreeBuilderContext(
      const PaintPropertyTreeBuilderFragmentContext&,
      PaintInvalidatorContext&);
  ALWAYS_INLINE void UpdateLayoutShiftTracking(
      const LayoutObject&,
      const PaintPropertyTreeBuilderFragmentContext&,
      PaintInvalidatorContext&);

  Vector<const LayoutObject*> pending_delayed_paint_invalidations_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_INVALIDATOR_H_

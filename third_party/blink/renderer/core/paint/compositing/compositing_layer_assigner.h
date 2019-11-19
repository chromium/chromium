/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_LAYER_ASSIGNER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_LAYER_ASSIGNER_H_

#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/graphics/squashing_disallowed_reasons.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CompositedLayerMapping;
class PaintLayer;

class CompositingLayerAssigner {
  STACK_ALLOCATED();

 public:
  explicit CompositingLayerAssigner(PaintLayerCompositor*);
  ~CompositingLayerAssigner();

  void Assign(PaintLayer* update_root,
              Vector<PaintLayer*>& layers_needing_paint_invalidation);

  bool LayersChanged() const { return layers_changed_; }

  // FIXME: This function should be private. We should remove the one caller
  // once we've fixed the compositing chicken/egg issues.
  CompositingStateTransitionType ComputeCompositedLayerUpdate(PaintLayer*);

 private:
  struct SquashingState {
    SquashingState()
        : most_recent_mapping(nullptr),
          has_most_recent_mapping(false),
          have_assigned_backings_to_entire_squashing_layer_subtree(false),
          next_squashed_layer_index(0),
          total_area_of_squashed_rects(0) {}

    void UpdateSquashingStateForNewMapping(
        CompositedLayerMapping*,
        bool has_new_composited_paint_layer_mapping,
        Vector<PaintLayer*>& layers_needing_paint_invalidation);

    // The most recent composited backing that the layer should squash onto if
    // needed.
    CompositedLayerMapping* most_recent_mapping;
    bool has_most_recent_mapping;

    // Whether all Layers in the stacking subtree rooted at the most recent
    // mapping's owning layer have had CompositedLayerMappings assigned. Layers
    // cannot squash into a CompositedLayerMapping owned by a stacking ancestor,
    // since this changes paint order.
    bool have_assigned_backings_to_entire_squashing_layer_subtree;

    // Counter that tracks what index the next Layer would be if it gets
    // squashed to the current squashing layer.
    wtf_size_t next_squashed_layer_index;

    // The absolute bounding rect of all the squashed layers.
    IntRect bounding_rect;

    // This is simply the sum of the areas of the squashed rects. This can be
    // very skewed if the rects overlap, but should be close enough to drive a
    // heuristic.
    uint64_t total_area_of_squashed_rects;
  };

  void AssignLayersToBackingsInternal(
      PaintLayer*,
      SquashingState&,
      Vector<PaintLayer*>& layers_needing_paint_invalidation);
  SquashingDisallowedReasons GetReasonsPreventingSquashing(
      const PaintLayer*,
      const SquashingState&);
  bool SquashingWouldExceedSparsityTolerance(const PaintLayer* candidate,
                                             const SquashingState&);
  void UpdateSquashingAssignment(
      PaintLayer*,
      SquashingState&,
      CompositingStateTransitionType,
      Vector<PaintLayer*>& layers_needing_paint_invalidation);
  bool NeedsOwnBacking(const PaintLayer*) const;

  PaintLayerCompositor* compositor_;
  bool layers_changed_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_LAYER_ASSIGNER_H_

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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_GRAPHICS_LAYER_UPDATER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_GRAPHICS_LAYER_UPDATER_H_

#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class PaintLayer;

class GraphicsLayerUpdater {
  STACK_ALLOCATED();

 public:
  GraphicsLayerUpdater();

  enum UpdateType {
    kDoNotForceUpdate,
    kForceUpdate,
  };

  void Update(PaintLayer&,
              Vector<PaintLayer*>& layers_needing_paint_invalidation);

  bool NeedsRebuildTree() const { return needs_rebuild_tree_; }

#if DCHECK_IS_ON()
  static void AssertNeedsToUpdateGraphicsLayerBitsCleared(PaintLayer&);
#endif

  class UpdateContext {
   public:
    UpdateContext();
    UpdateContext(const UpdateContext& other, const PaintLayer& layer);
    const PaintLayer* CompositingContainer(const PaintLayer& layer) const;
    const PaintLayer* CompositingStackingContext() const;

    // Offset of this PaintLayer's LayoutObject relative to the position of its
    // main GraphicsLayer.
    IntSize object_offset_delta;

    // The object_offset_delta of the compositing ancestor.
    IntSize parent_object_offset_delta;

   private:
    const PaintLayer* compositing_stacking_context_;
    const PaintLayer* compositing_ancestor_;
    bool use_slow_path_;
  };

 private:
  void UpdateRecursive(PaintLayer&,
                       UpdateType,
                       UpdateContext&,
                       Vector<PaintLayer*>& layers_needing_paint_invalidation);

  bool needs_rebuild_tree_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_GRAPHICS_LAYER_UPDATER_H_

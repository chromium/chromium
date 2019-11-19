// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_LAYER_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_LAYER_PAINTER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painting_info.h"
#include "third_party/blink/renderer/core/paint/paint_result.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CullRect;
class ClipRect;
class ComputedStyle;
class DisplayItemClient;
class GraphicsContext;
struct PhysicalOffset;

// This class is responsible for painting self-painting PaintLayer.
//
// See PainterLayer SELF-PAINTING LAYER section about what 'self-painting'
// means and how it impacts this class.
class CORE_EXPORT PaintLayerPainter {
  STACK_ALLOCATED();

 public:
  PaintLayerPainter(PaintLayer& paint_layer) : paint_layer_(paint_layer) {}

  // The Paint() method paints the layers that intersect the cull rect from
  // back to front.  paint() assumes that the caller will clip to the bounds of
  // damageRect if necessary.
  void Paint(GraphicsContext&,
             const CullRect&,
             const GlobalPaintFlags = kGlobalPaintNormalPhase,
             PaintLayerFlags = 0);
  // Paint() assumes that the caller will clip to the bounds of the painting
  // dirty if necessary.
  PaintResult Paint(GraphicsContext&,
                    const PaintLayerPaintingInfo&,
                    PaintLayerFlags);
  // PaintLayerContents() assumes that the caller will clip to the bounds of the
  // painting dirty rect if necessary.
  PaintResult PaintLayerContents(GraphicsContext&,
                                 const PaintLayerPaintingInfo&,
                                 PaintLayerFlags);

  void PaintOverlayOverflowControls(GraphicsContext&,
                                    const CullRect&,
                                    const GlobalPaintFlags);

  // Returns true if the painted output of this PaintLayer and its children is
  // invisible and therefore can't impact painted output.
  static bool PaintedOutputInvisible(const ComputedStyle&);

 private:
  friend class PaintLayerPainterTest;

  PaintResult PaintChildren(PaintLayerIteration children_to_visit,
                            GraphicsContext&,
                            const PaintLayerPaintingInfo&,
                            PaintLayerFlags);
  bool AtLeastOneFragmentIntersectsDamageRect(
      PaintLayerFragments&,
      const PaintLayerPaintingInfo&,
      PaintLayerFlags,
      const PhysicalOffset& offset_from_root);
  void PaintFragmentWithPhase(PaintPhase,
                              const PaintLayerFragment&,
                              GraphicsContext&,
                              const ClipRect&,
                              const PaintLayerPaintingInfo&,
                              PaintLayerFlags);
  void PaintBackgroundForFragments(const PaintLayerFragments&,
                                   GraphicsContext&,
                                   const PaintLayerPaintingInfo&,
                                   PaintLayerFlags);
  void PaintForegroundForFragments(const PaintLayerFragments&,
                                   GraphicsContext&,
                                   const PaintLayerPaintingInfo&,
                                   bool selection_only,
                                   bool force_paint_chunks,
                                   PaintLayerFlags);
  void PaintForegroundForFragmentsWithPhase(PaintPhase,
                                            const PaintLayerFragments&,
                                            GraphicsContext&,
                                            const PaintLayerPaintingInfo&,
                                            PaintLayerFlags);
  void PaintSelfOutlineForFragments(const PaintLayerFragments&,
                                    GraphicsContext&,
                                    const PaintLayerPaintingInfo&,
                                    PaintLayerFlags);
  void PaintOverlayOverflowControlsForFragments(const PaintLayerFragments&,
                                                GraphicsContext&,
                                                const PaintLayerPaintingInfo&,
                                                PaintLayerFlags);
  void PaintMaskForFragments(const PaintLayerFragments&,
                             GraphicsContext&,
                             const PaintLayerPaintingInfo&,
                             PaintLayerFlags);

  void FillMaskingFragment(GraphicsContext&,
                           const ClipRect&,
                           const DisplayItemClient&);

  void PaintEmptyContentForFilters(GraphicsContext&);

  void AdjustForPaintProperties(const GraphicsContext&,
                                PaintLayerPaintingInfo&,
                                PaintLayerFlags&);

  PaintLayer& paint_layer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_LAYER_PAINTER_H_

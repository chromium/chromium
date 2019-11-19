/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_PAINT_LAYER_COMPOSITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_PAINT_LAYER_COMPOSITOR_H_

#include <memory>
#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_inputs_root.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"

namespace blink {

class PaintLayer;
class GraphicsLayer;
class LayoutEmbeddedContent;
class Page;
class Scrollbar;
class ScrollingCoordinator;

enum CompositingUpdateType {
  kCompositingUpdateNone,
  kCompositingUpdateAfterGeometryChange,
  kCompositingUpdateAfterCompositingInputChange,
  kCompositingUpdateRebuildTree,
};

enum CompositingStateTransitionType {
  kNoCompositingStateChange,
  kAllocateOwnCompositedLayerMapping,
  kRemoveOwnCompositedLayerMapping,
  kPutInSquashingLayer,
  kRemoveFromSquashingLayer
};

// PaintLayerCompositor maintains document-level compositing state and is the
// entry point of the "compositing update" lifecycle stage.  There is one PLC
// per LayoutView.
//
// The compositing update, implemented by PaintLayerCompositor and friends,
// decides for each PaintLayer whether it should get a CompositedLayerMapping,
// and asks each CLM to set up its GraphicsLayers.
//
// With CompositeAfterPaint, PaintLayerCompositor will be eventually replaced by
// PaintArtifactCompositor.

class CORE_EXPORT PaintLayerCompositor {
  USING_FAST_MALLOC(PaintLayerCompositor);

 public:
  explicit PaintLayerCompositor(LayoutView&);
  ~PaintLayerCompositor();

  // Called while the LocalFrame behind the LayoutView is being detached.
  // Pointers in other objects should be cleaned up at this point, before
  // pointers out of this object become invalid.
  void CleanUp();

  // Called after layout is performed on the LocalFrame holding the LayoutView,
  // during the document lifecycle update.
  void DidLayout();

  void UpdateIfNeededRecursive(DocumentLifecycle::LifecycleState target_state);

  // Return true if this LayoutView is in "compositing mode" (i.e. has one or
  // more composited Layers)
  bool InCompositingMode() const;
  // FIXME: Replace all callers with inCompositingMode and remove this function.
  bool StaleInCompositingMode() const;
  // This will make a compositing layer at the root automatically, and hook up
  // to the native view/window system.
  void SetCompositingModeEnabled(bool);

  // Returns true if the accelerated compositing is enabled
  bool HasAcceleratedCompositing() const {
    return has_accelerated_compositing_;
  }

  bool PreferCompositingToLCDTextEnabled() const;

  bool RootShouldAlwaysComposite() const;

  // Notifies about changes to PreferCompositingToLCDText or
  // AcceleratedCompositing.
  void UpdateAcceleratedCompositingSettings();

  // Used to indicate that a compositing update will be needed for the next
  // frame that gets drawn.
  void SetNeedsCompositingUpdate(CompositingUpdateType);

  // Whether the given layer needs an extra 'contents' layer.
  bool NeedsContentsCompositingLayer(const PaintLayer*) const;

  // Issue paint invalidations of the appropriate layers when the given Layer
  // starts or stops being composited.
  void PaintInvalidationOnCompositingChange(PaintLayer*);

  void FullyInvalidatePaint();

  PaintLayer* RootLayer() const;

  // The LayoutView's main GraphicsLayer.
  GraphicsLayer* RootGraphicsLayer() const;

  // Returns the GraphicsLayer we should start painting from. This can differ
  // from above in some cases, e.g.  when the RootGraphicsLayer is detached and
  // swapped out for an overlay video or immersive-ar DOM overlay layer.
  GraphicsLayer* PaintRootGraphicsLayer() const;

  static PaintLayerCompositor* FrameContentsCompositor(LayoutEmbeddedContent&);
  // Return true if the layers changed.
  static bool AttachFrameContentLayersToIframeLayer(LayoutEmbeddedContent&);

  void UpdateTrackingRasterInvalidations();

  DocumentLifecycle& Lifecycle() const;

  void UpdatePotentialCompositingReasonsFromStyle(PaintLayer&);

  // Whether the layer could ever be composited.
  bool CanBeComposited(const PaintLayer*) const;

  // FIXME: Move allocateOrClearCompositedLayerMapping to
  // CompositingLayerAssigner once we've fixed the compositing chicken/egg
  // issues.
  bool AllocateOrClearCompositedLayerMapping(
      PaintLayer*,
      CompositingStateTransitionType composited_layer_update);

  PaintLayer* GetCompositingInputsRoot() {
    return compositing_inputs_root_.Get();
  }

  void ClearCompositingInputsRoot() { compositing_inputs_root_.Clear(); }

  void UpdateCompositingInputsRoot(PaintLayer* layer) {
    compositing_inputs_root_.Update(layer);
  }

  void ForceRecomputeVisualRectsIncludingNonCompositingDescendants(
      LayoutObject&);

 private:
#if DCHECK_IS_ON()
  void AssertNoUnresolvedDirtyBits();
#endif

  void UpdateIfNeededRecursiveInternal(
      DocumentLifecycle::LifecycleState target_state,
      CompositingReasonsStats&);

  void UpdateWithoutAcceleratedCompositing(CompositingUpdateType);
  void UpdateIfNeeded(DocumentLifecycle::LifecycleState target_state,
                      CompositingReasonsStats&);

  void AttachRootLayer();
  void DetachRootLayer();

  Page* GetPage() const;

  ScrollingCoordinator* GetScrollingCoordinator() const;

  void EnableCompositingModeIfNeeded();

  void AdjustOverlayFullscreenVideoPosition(GraphicsLayer*);
  GraphicsLayer* OverlayFullscreenVideoGraphicsLayer() const;

  // Checks the given graphics layer against the compositor's horizontal and
  // vertical scrollbar graphics layers, returning the associated Scrollbar
  // instance if any, else nullptr.
  Scrollbar* GraphicsLayerToScrollbar(const GraphicsLayer*) const;

  bool IsMainFrame() const;

  GraphicsLayer* GetXrImmersiveDomOverlayLayer() const;

  LayoutView& layout_view_;
  const bool has_accelerated_compositing_ = true;

  bool compositing_ = false;

  // The root layer doesn't composite if it's a non-scrollable frame.
  // So, after a layout we set this dirty bit to know that we need
  // to recompute whether the root layer should composite even if
  // none of its descendants composite.
  // FIXME: Get rid of all the callers of SetCompositingModeEnabled()
  // except the one in UpdateIfNeeded(), then rename this to
  // compositing_dirty_.
  bool root_should_always_composite_dirty_ = true;

  // After initialization, compositing updates must be done, so start dirty.
  CompositingUpdateType pending_update_type_ =
      kCompositingUpdateAfterCompositingInputChange;

  enum RootLayerAttachment {
    kRootLayerUnattached,
    kRootLayerAttachedViaEnclosingFrame,
    kRootLayerOfLocalFrameRoot  // which doesn't need to attach to anything.
  };
  RootLayerAttachment root_layer_attachment_ = kRootLayerUnattached;

  CompositingInputsRoot compositing_inputs_root_;

  FRIEND_TEST_ALL_PREFIXES(FrameThrottlingTest,
                           IntersectionObservationOverridesThrottling);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_PAINT_LAYER_COMPOSITOR_H_

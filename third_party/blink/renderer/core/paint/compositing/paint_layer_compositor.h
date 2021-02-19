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
#include "third_party/blink/renderer/core/paint/compositing/compositing_update_type.h"

namespace blink {

class PaintLayer;
class GraphicsLayer;
class LayoutEmbeddedContent;
class LayoutView;
class Page;
class Scrollbar;

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

  void UpdateInputsIfNeededRecursive(
      DocumentLifecycle::LifecycleState target_state);
  void UpdateAssignmentsIfNeededRecursive(
      DocumentLifecycle::LifecycleState target_state);

  // Return true if this LayoutView is in "compositing mode" (i.e. has one or
  // more composited Layers)
  bool InCompositingMode() const;
  // FIXME: Replace all callers with inCompositingMode and remove this function.
  bool StaleInCompositingMode() const;
  // This will make a compositing layer at the root automatically, and hook up
  // to the native view/window system.
  void SetCompositingModeEnabled(bool);

  // Notifies about changes to PreferCompositingToLCDText or
  // AcceleratedCompositing.
  void UpdateAcceleratedCompositingSettings();

  // Used to indicate that a compositing update will be needed for the next
  // frame that gets drawn. If called from before the compositing inputs
  // step has run, and the type is > kCompositingUpdateNone, compositing
  // inputs will be re-computed. If called during pre-paint (which is after
  // compositing inputs and before the rest of compositing), it will cause
  // the rest of compositing to run, but not compositing inputs.
  void SetNeedsCompositingUpdate(CompositingUpdateType);

  // Whether the given layer needs an extra 'contents' layer.
  bool NeedsContentsCompositingLayer(const PaintLayer*) const;

  // Issue paint invalidations of the appropriate layers when the given Layer
  // starts or stops being composited.
  static void PaintInvalidationOnCompositingChange(PaintLayer*);

  PaintLayer* RootLayer() const;

  // The LayoutView's main GraphicsLayer.
  GraphicsLayer* RootGraphicsLayer() const;

  // Returns the GraphicsLayer we should start painting from. This can differ
  // from above in some cases, e.g.  when the RootGraphicsLayer is detached and
  // swapped out for an overlay video or immersive-ar DOM overlay layer.
  GraphicsLayer* PaintRootGraphicsLayer() const;

  static PaintLayerCompositor* FrameContentsCompositor(
      const LayoutEmbeddedContent&);

  void UpdateTrackingRasterInvalidations();

  DocumentLifecycle& Lifecycle() const;

  void UpdatePotentialCompositingReasonsFromStyle(PaintLayer&);

  // Whether the layer could ever be composited.
  bool CanBeComposited(const PaintLayer*) const;

  void ClearRootLayerAttachmentDirty() { root_layer_attachment_dirty_ = false; }

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

 private:
#if DCHECK_IS_ON()
  void AssertNoUnresolvedDirtyBits();
#endif

  void UpdateAssignmentsIfNeededRecursiveInternal(
      DocumentLifecycle::LifecycleState target_state,
      CompositingReasonsStats&);
  void UpdateInputsIfNeededRecursiveInternal(
      DocumentLifecycle::LifecycleState target_state);

  void UpdateAssignmentsIfNeeded(DocumentLifecycle::LifecycleState target_state,
                                 CompositingReasonsStats&);

  void SetOwnerNeedsCompositingInputsUpdate();

  Page* GetPage() const;

  // Checks the given graphics layer against the compositor's horizontal and
  // vertical scrollbar graphics layers, returning the associated Scrollbar
  // instance if any, else nullptr.
  Scrollbar* GraphicsLayerToScrollbar(const GraphicsLayer*) const;

  bool IsMainFrame() const;

  LayoutView* const layout_view_;

  bool compositing_ = false;
  bool root_layer_attachment_dirty_ = false;

  // After initialization, compositing updates must be done, so start dirty.
  CompositingUpdateType pending_update_type_ =
      kCompositingUpdateAfterCompositingInputChange;

  CompositingInputsRoot compositing_inputs_root_;

  FRIEND_TEST_ALL_PREFIXES(FrameThrottlingTest,
                           IntersectionObservationOverridesThrottling);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_PAINT_LAYER_COMPOSITOR_H_

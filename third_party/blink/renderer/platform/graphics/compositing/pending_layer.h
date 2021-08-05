// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PENDING_LAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PENDING_LAYER_H_

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"

namespace blink {

class GraphicsLayer;

// Information of a composited layer that is created during compositing update
// in pre-CompositeAfterPaint. In CompositeAfterPaint, this is expected to
// contain all paint chunks, as if we created one root layer that needs to be
// future layerized.
struct PreCompositedLayerInfo {
  // For now this is used only when graphics_layer == nullptr. This will also
  // contain the paint chunks for the graphics layer when we unify
  // PaintController for pre-CAP and CAP.
  PaintChunkSubset chunks;
  // If this is not nullptr, we should use the composited layer created by the
  // GraphicsLayer. Otherwise we should layerize |chunks|. A GraphicsLayer with
  // ShouldCreateLayersAfterPaint() == true should set this field to nullptr.
  const GraphicsLayer* graphics_layer = nullptr;
};

// A pending layer is a collection of paint chunks that will end up in the same
// cc::Layer.
class PLATFORM_EXPORT PendingLayer {
 public:
  enum CompositingType {
    kScrollHitTestLayer,
    kPreCompositedLayer,
    kForeignLayer,
    kScrollbarLayer,
    kOverlap,
    kOther,
  };

  PendingLayer(const PaintChunkSubset&, const PaintChunkIterator&);
  explicit PendingLayer(const PreCompositedLayerInfo&);

  // Returns the offset/bounds for the final cc::Layer, rounded if needed.
  FloatPoint LayerOffset() const;
  IntSize LayerBounds() const;

  const FloatRect& BoundsForTesting() const { return bounds_; }

  const FloatRect& RectKnownToBeOpaque() const {
    return rect_known_to_be_opaque_;
  }
  bool TextKnownToBeOnOpaqueBackground() const {
    return text_known_to_be_on_opaque_background_;
  }
  const PaintChunkSubset& Chunks() const { return chunks_; }
  const PropertyTreeState& GetPropertyTreeState() const {
    return property_tree_state_;
  }
  const FloatPoint& OffsetOfDecompositedTransforms() const {
    return offset_of_decomposited_transforms_;
  }
  PaintPropertyChangeType ChangeOfDecompositedTransforms() const {
    return change_of_decomposited_transforms_;
  }
  const GraphicsLayer* GetGraphicsLayer() const { return graphics_layer_; }
  CompositingType GetCompositingType() const { return compositing_type_; }

  void SetCompositingType(CompositingType new_type) {
    compositing_type_ = new_type;
  }

  void SetPaintArtifact(scoped_refptr<const PaintArtifact> paint_artifact) {
    chunks_.SetPaintArtifact(paint_artifact);
  }

  // Merges |guest| into |this| if it can, by appending chunks of |guest|
  // after chunks of |this|, with appropriate space conversion applied to
  // both layers from their original property tree states to |merged_state|.
  // Returns whether the merge is successful.
  bool Merge(const PendingLayer& guest, bool prefers_lcd_text = false) {
    return MergeInternal(guest, guest.property_tree_state_, prefers_lcd_text,
                         /*dry_run*/ false);
  }

  // Returns true if |guest| can be merged into |this|.
  // |guest_state| is for cases where we want to check if we can merge |guest|
  // if it has |guest_state| in the future (which may be different from its
  // current state).
  bool CanMerge(const PendingLayer& guest,
                const PropertyTreeState& guest_state,
                bool prefers_lcd_text = false) const {
    return const_cast<PendingLayer*>(this)->MergeInternal(
        guest, guest_state, prefers_lcd_text, /*dry_run*/ true);
  }

  // Mutate this layer's property tree state to a more general (shallower)
  // state, thus the name "upcast". The concrete effect of this is to
  // "decomposite" some of the properties, so that fewer properties will be
  // applied by the compositor, and more properties will be applied internally
  // to the chunks as Skia commands.
  void Upcast(const PropertyTreeState&);

  const PaintChunk& FirstPaintChunk() const;
  const DisplayItem& FirstDisplayItem() const;

  const TransformPaintPropertyNode& ScrollTranslationForScrollHitTestLayer()
      const;

  std::unique_ptr<JSONObject> ToJSON() const;

  bool MayDrawContent() const;

  bool RequiresOwnLayer() const {
    return compositing_type_ != kOverlap && compositing_type_ != kOther;
  }

  bool PropertyTreeStateChanged() const;

  bool MightOverlap(const PendingLayer& other) const;

  static void DecompositeTransforms(Vector<PendingLayer>& pending_layers);

 private:
  PendingLayer(const PaintChunkSubset&,
               const PaintChunk& first_chunk,
               wtf_size_t first_chunk_index_in_paint_artifact);
  FloatRect VisualRectForOverlapTesting(
      const PropertyTreeState& ancestor_state) const;
  FloatRect MapRectKnownToBeOpaque(const PropertyTreeState&) const;
  bool MergeInternal(const PendingLayer& guest,
                     const PropertyTreeState& guest_state,
                     bool prefers_lcd_text,
                     bool dry_run);

  // True if this contains only a single solid color DrawingDisplayItem.
  bool IsSolidColor() const;

  // The rects are in the space of property_tree_state.
  FloatRect bounds_;
  FloatRect rect_known_to_be_opaque_;
  bool has_text_ = false;
  bool text_known_to_be_on_opaque_background_ = false;
  PaintChunkSubset chunks_;
  PropertyTreeState property_tree_state_;
  FloatPoint offset_of_decomposited_transforms_;
  PaintPropertyChangeType change_of_decomposited_transforms_ =
      PaintPropertyChangeType::kUnchanged;
  const GraphicsLayer* graphics_layer_ = nullptr;
  CompositingType compositing_type_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PENDING_LAYER_H_

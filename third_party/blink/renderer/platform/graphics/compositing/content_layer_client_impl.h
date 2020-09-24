// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_CONTENT_LAYER_CLIENT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_CONTENT_LAYER_CLIENT_IMPL_H_

#include "base/macros.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/renderer/platform/graphics/compositing/layers_as_json.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer_client.h"
#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidator.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class JSONArray;
class JSONObject;
class PaintArtifact;
class PaintChunkSubset;

class PLATFORM_EXPORT ContentLayerClientImpl : public cc::ContentLayerClient,
                                               public LayerAsJSONClient {
  USING_FAST_MALLOC(ContentLayerClientImpl);

 public:
  ContentLayerClientImpl();
  ~ContentLayerClientImpl() override;

  // cc::ContentLayerClient
  gfx::Rect PaintableRegion() override {
    return gfx::Rect(raster_invalidator_.LayerBounds().size());
  }
  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting) override {
    return cc_display_item_list_;
  }
  bool FillsBoundsCompletely() const override { return false; }
  size_t GetApproximateUnsharedMemoryUsage() const override {
    // TODO(jbroman): Actually calculate memory usage.
    return 0;
  }

  // LayerAsJSONClient implementation
  void AppendAdditionalInfoAsJSON(LayerTreeFlags,
                                  const cc::Layer&,
                                  JSONObject&) const override;

  const cc::Layer& Layer() const { return *cc_picture_layer_.get(); }
  const PropertyTreeState& State() const { return layer_state_; }

  bool Matches(const PaintChunk& paint_chunk) const {
    return id_ && paint_chunk.Matches(*id_);
  }

  scoped_refptr<cc::PictureLayer> UpdateCcPictureLayer(
      scoped_refptr<const PaintArtifact>,
      const PaintChunkSubset&,
      const gfx::Rect& layer_bounds,
      const PropertyTreeState&);

  RasterInvalidator& GetRasterInvalidator() { return raster_invalidator_; }

 private:
  // Callback from raster_invalidator_.
  void InvalidateRect(const IntRect& rect) {
    raster_invalidated_ = true;
    cc_picture_layer_->SetNeedsDisplayRect(rect);
  }

  base::Optional<PaintChunk::Id> id_;
  scoped_refptr<cc::PictureLayer> cc_picture_layer_;
  scoped_refptr<cc::DisplayItemList> cc_display_item_list_;
  RasterInvalidator raster_invalidator_;
  RasterInvalidator::RasterInvalidationFunction raster_invalidation_function_;
  bool raster_invalidated_ = false;

  PropertyTreeState layer_state_;

  String debug_name_;
#if DCHECK_IS_ON()
  std::unique_ptr<JSONArray> paint_chunk_debug_data_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ContentLayerClientImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_CONTENT_LAYER_CLIENT_IMPL_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/content_layer_client_impl.h"

#include <memory>
#include "base/bind.h"
#include "base/optional.h"
#include "base/trace_event/traced_value.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op_buffer.h"
#include "third_party/blink/renderer/platform/geometry/geometry_as_json.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"
#include "third_party/blink/renderer/platform/graphics/logging_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidation_tracking.h"
#include "third_party/blink/renderer/platform/json/json_values.h"

namespace blink {

ContentLayerClientImpl::ContentLayerClientImpl()
    : cc_picture_layer_(cc::PictureLayer::Create(this)),
      raster_invalidator_(
          base::BindRepeating(&ContentLayerClientImpl::InvalidateRect,
                              base::Unretained(this))),
      layer_state_(PropertyTreeState::Uninitialized()) {}

ContentLayerClientImpl::~ContentLayerClientImpl() {
  cc_picture_layer_->ClearClient();
}

void ContentLayerClientImpl::AppendAdditionalInfoAsJSON(
    LayerTreeFlags flags,
    const cc::Layer& layer,
    JSONObject& json) const {
#if DCHECK_IS_ON()
  if (flags & kLayerTreeIncludesDebugInfo)
    json.SetValue("paintChunkContents", paint_chunk_debug_data_->Clone());
#endif

  if ((flags & kLayerTreeIncludesPaintInvalidations) &&
      raster_invalidator_.GetTracking())
    raster_invalidator_.GetTracking()->AsJSON(&json);

#if DCHECK_IS_ON()
  if (flags & kLayerTreeIncludesPaintRecords) {
    LoggingCanvas canvas;
    cc_display_item_list_->Raster(&canvas);
    json.SetValue("paintRecord", canvas.Log());
  }
#endif
}

scoped_refptr<cc::PictureLayer> ContentLayerClientImpl::UpdateCcPictureLayer(
    scoped_refptr<const PaintArtifact> paint_artifact,
    const PaintChunkSubset& paint_chunks,
    const gfx::Rect& layer_bounds,
    const PropertyTreeState& layer_state) {
  if (paint_chunks[0].is_cacheable)
    id_.emplace(paint_chunks[0].id);
  else
    id_ = base::nullopt;

  const auto& display_item_list = paint_artifact->GetDisplayItemList();

#if DCHECK_IS_ON()
  paint_chunk_debug_data_ = std::make_unique<JSONArray>();
  for (const auto& chunk : paint_chunks) {
    auto json = std::make_unique<JSONObject>();
    json->SetString("data", chunk.ToString());
    json->SetArray("displayItems",
                   paint_artifact->GetDisplayItemList().SubsequenceAsJSON(
                       chunk.begin_index, chunk.end_index,
                       DisplayItemList::kShowOnlyDisplayItemTypes));
    paint_chunk_debug_data_->PushObject(std::move(json));
  }
#endif

  // The raster invalidator will only handle invalidations within a cc::Layer so
  // we need this invalidation if the layer's properties have changed.
  if (layer_state != layer_state_)
    cc_picture_layer_->SetSubtreePropertyChanged();

  raster_invalidator_.Generate(paint_artifact, paint_chunks, layer_bounds,
                               layer_state);
  layer_state_ = layer_state;

  // Note: cc::Layer API assumes the layer bounds start at (0, 0), but the
  // bounding box of a paint chunk does not necessarily start at (0, 0) (and
  // could even be negative). Internally the generated layer translates the
  // paint chunk to align the bounding box to (0, 0) and we set the layer's
  // offset_to_transform_parent with the origin of the paint chunk here.
  cc_picture_layer_->SetOffsetToTransformParent(
      layer_bounds.OffsetFromOrigin());
  cc_picture_layer_->SetBounds(layer_bounds.size());
  cc_picture_layer_->SetIsDrawable(true);
  cc_picture_layer_->SetHitTestable(true);

  base::Optional<RasterUnderInvalidationCheckingParams> params;
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    params.emplace(*raster_invalidator_.GetTracking(),
                   IntRect(0, 0, layer_bounds.width(), layer_bounds.height()),
                   paint_chunks[0].id.client.DebugName());
  }
  cc_display_item_list_ = PaintChunksToCcLayer::Convert(
      paint_chunks, layer_state, layer_bounds.OffsetFromOrigin(),
      display_item_list, cc::DisplayItemList::kTopLevelDisplayItemList,
      base::OptionalOrNullptr(params));

  cc_picture_layer_->SetSafeOpaqueBackgroundColor(
      paint_chunks[0].safe_opaque_background_color);
  // TODO(masonfreed): We don't need to set the background color here; only the
  // safe opaque background color matters. But making that change would require
  // rebaselining 787 tests to remove the "background_color" property from the
  // layer dumps.
  cc_picture_layer_->SetBackgroundColor(
      paint_chunks[0].safe_opaque_background_color);
  return cc_picture_layer_;
}

}  // namespace blink

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/content_layer_client_impl.h"

#include <memory>
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
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

ContentLayerClientImpl::ContentLayerClientImpl()
    : cc_picture_layer_(cc::PictureLayer::Create(this)),
      raster_invalidator_([this](const IntRect& rect) {
        cc_picture_layer_->SetNeedsDisplayRect(rect);
      }),
      layer_state_(nullptr, nullptr, nullptr),
      weak_ptr_factory_(this) {
  cc_picture_layer_->SetLayerClient(weak_ptr_factory_.GetWeakPtr());
}

ContentLayerClientImpl::~ContentLayerClientImpl() = default;

static int GetTransformId(const TransformPaintPropertyNode* transform,
                          ContentLayerClientImpl::LayerAsJSONContext& context) {
  if (!transform)
    return 0;

  auto it = context.transform_id_map.find(transform);
  if (it != context.transform_id_map.end())
    return it->value;

  int parent_id = GetTransformId(transform->Parent(), context);
  if (transform->Matrix().IsIdentity() && !transform->RenderingContextId()) {
    context.transform_id_map.Set(transform, parent_id);
    return parent_id;
  }

  int transform_id = context.next_transform_id++;
  context.transform_id_map.Set(transform, transform_id);

  auto json = JSONObject::Create();
  json->SetInteger("id", transform_id);
  if (parent_id)
    json->SetInteger("parent", parent_id);

  if (!transform->Matrix().IsIdentity())
    json->SetArray("transform", TransformAsJSONArray(transform->Matrix()));

  if (!transform->Matrix().IsIdentityOrTranslation())
    json->SetArray("origin", PointAsJSONArray(transform->Origin()));

  if (!transform->FlattensInheritedTransform())
    json->SetBoolean("flattenInheritedTransform", false);

  if (auto rendering_context = transform->RenderingContextId()) {
    auto it = context.rendering_context_map.find(rendering_context);
    int rendering_id = context.rendering_context_map.size() + 1;
    if (it == context.rendering_context_map.end())
      context.rendering_context_map.Set(rendering_context, rendering_id);
    else
      rendering_id = it->value;

    json->SetInteger("renderingContext", rendering_id);
  }

  if (!context.transforms_json)
    context.transforms_json = JSONArray::Create();
  context.transforms_json->PushObject(std::move(json));

  return transform_id;
}

// This is the SPv2 version of GraphicsLayer::LayerAsJSONInternal().
std::unique_ptr<JSONObject> ContentLayerClientImpl::LayerAsJSON(
    LayerAsJSONContext& context) const {
  std::unique_ptr<JSONObject> json = JSONObject::Create();
  json->SetString("name", debug_name_);

  if (context.flags & kLayerTreeIncludesDebugInfo)
    json->SetString("this", String::Format("%p", cc_picture_layer_.get()));

  FloatPoint position(cc_picture_layer_->offset_to_transform_parent().x(),
                      cc_picture_layer_->offset_to_transform_parent().y());
  if (position != FloatPoint())
    json->SetArray("position", PointAsJSONArray(position));

  IntSize bounds(cc_picture_layer_->bounds().width(),
                 cc_picture_layer_->bounds().height());
  if (!bounds.IsEmpty())
    json->SetArray("bounds", SizeAsJSONArray(bounds));

  if (cc_picture_layer_->contents_opaque())
    json->SetBoolean("contentsOpaque", true);

  if (!cc_picture_layer_->DrawsContent())
    json->SetBoolean("drawsContent", false);

  if (!cc_picture_layer_->double_sided())
    json->SetString("backfaceVisibility", "hidden");

  Color background_color(cc_picture_layer_->background_color());
  if (background_color.Alpha()) {
    json->SetString("backgroundColor",
                    background_color.NameForLayoutTreeAsText());
  }

#if DCHECK_IS_ON()
  if (context.flags & kLayerTreeIncludesDebugInfo)
    json->SetValue("paintChunkContents", paint_chunk_debug_data_->Clone());
#endif

  if ((context.flags & kLayerTreeIncludesPaintInvalidations) &&
      raster_invalidator_.GetTracking())
    raster_invalidator_.GetTracking()->AsJSON(json.get());

  if (int transform_id = GetTransformId(layer_state_.Transform(), context))
    json->SetInteger("transform", transform_id);

#if DCHECK_IS_ON()
  if (context.flags & kLayerTreeIncludesPaintRecords) {
    LoggingCanvas canvas;
    cc_display_item_list_->Raster(&canvas);
    json->SetValue("paintRecord", canvas.Log());
  }
#endif

  return json;
}

std::unique_ptr<base::trace_event::TracedValue>
ContentLayerClientImpl::TakeDebugInfo(cc::Layer* layer) {
  DCHECK_EQ(layer, cc_picture_layer_.get());
  auto traced_value = std::make_unique<base::trace_event::TracedValue>();
  traced_value->SetString("layer_name",
                          WTF::StringUTF8Adaptor(debug_name_).AsStringPiece());
  if (auto* tracking = raster_invalidator_.GetTracking()) {
    tracking->AddToTracedValue(*traced_value);
    tracking->ClearInvalidations();
  }
  // TODO(wangxianzhu): Do we need compositing_reasons,
  // squashing_disallowed_reasons and owner_node_id?
  return traced_value;
}

static SkColor DisplayItemBackgroundColor(const DisplayItem& item) {
  if (item.GetType() != DisplayItem::kBoxDecorationBackground &&
      item.GetType() != DisplayItem::kDocumentBackground)
    return SK_ColorTRANSPARENT;

  const auto& drawing_item = static_cast<const DrawingDisplayItem&>(item);
  const auto record = drawing_item.GetPaintRecord();
  if (!record)
    return SK_ColorTRANSPARENT;

  for (cc::PaintOpBuffer::Iterator it(record.get()); it; ++it) {
    const auto* op = *it;
    if (op->GetType() == cc::PaintOpType::DrawRect ||
        op->GetType() == cc::PaintOpType::DrawRRect) {
      const auto& flags = static_cast<const cc::PaintOpWithFlags*>(op)->flags;
      // Skip op with looper which may modify the color.
      if (!flags.getLooper() && flags.getStyle() == cc::PaintFlags::kFill_Style)
        return flags.getColor();
    }
  }
  return SK_ColorTRANSPARENT;
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

  // TODO(wangxianzhu): Avoid calling DebugName() in official release build.
  debug_name_ = paint_chunks[0].id.client.DebugName();
  const auto& display_item_list = paint_artifact->GetDisplayItemList();

#if DCHECK_IS_ON()
  paint_chunk_debug_data_ = JSONArray::Create();
  for (const auto& chunk : paint_chunks) {
    auto json = JSONObject::Create();
    json->SetString("data", chunk.ToString());
    json->SetArray("displayItems",
                   paint_artifact->GetDisplayItemList().SubsequenceAsJSON(
                       chunk.begin_index, chunk.end_index,
                       DisplayItemList::kSkipNonDrawings |
                           DisplayItemList::kShownOnlyDisplayItemTypes));
    json->SetString("propertyTreeState", chunk.properties.ToTreeString());
    paint_chunk_debug_data_->PushObject(std::move(json));
  }
#endif

  raster_invalidator_.Generate(paint_artifact, paint_chunks, layer_bounds,
                               layer_state);
  layer_state_ = layer_state;

  cc_picture_layer_->SetBounds(layer_bounds.size());
  cc_picture_layer_->SetIsDrawable(true);

  base::Optional<RasterUnderInvalidationCheckingParams> params;
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    params.emplace(*raster_invalidator_.GetTracking(),
                   IntRect(0, 0, layer_bounds.width(), layer_bounds.height()),
                   debug_name_);
  }
  cc_display_item_list_ = PaintChunksToCcLayer::Convert(
      paint_chunks, layer_state, layer_bounds.OffsetFromOrigin(),
      display_item_list, cc::DisplayItemList::kTopLevelDisplayItemList,
      base::OptionalOrNullptr(params));

  if (paint_chunks[0].size()) {
    cc_picture_layer_->SetBackgroundColor(DisplayItemBackgroundColor(
        display_item_list[paint_chunks[0].begin_index]));
  }

  return cc_picture_layer_;
}

}  // namespace blink

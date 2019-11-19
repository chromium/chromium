// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/layers_as_json.h"

#include "cc/layers/layer.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/geometry_as_json.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"

namespace blink {

namespace {

String PointerAsString(const void* ptr) {
  WTF::TextStream ts;
  ts << ptr;
  return ts.Release();
}

}  // namespace

// Create a JSON version of the specified |layer|.
std::unique_ptr<JSONObject> CCLayerAsJSON(
    const cc::Layer* layer,
    LayerTreeFlags flags,
    const FloatPoint& offset_from_transform_node) {
  auto json = std::make_unique<JSONObject>();

  if (flags & kLayerTreeIncludesDebugInfo) {
    json->SetString("this", PointerAsString(layer));
    json->SetInteger("ccLayerId", layer->id());
  }

  json->SetString("name", String(layer->DebugName().c_str()));

  if (offset_from_transform_node != FloatPoint())
    json->SetArray("position", PointAsJSONArray(offset_from_transform_node));

  // This is testing against gfx::Size(), *not* whether the size is empty.
  if (layer->bounds() != gfx::Size())
    json->SetArray("bounds", SizeAsJSONArray(IntSize(layer->bounds())));

  if (layer->contents_opaque())
    json->SetBoolean("contentsOpaque", true);

  if (!layer->DrawsContent())
    json->SetBoolean("drawsContent", false);

  if (!layer->double_sided())
    json->SetString("backfaceVisibility", "hidden");

  if (Color(layer->background_color()).Alpha()) {
    json->SetString("backgroundColor",
                    Color(layer->background_color()).NameForLayoutTreeAsText());
  }

  if (flags &
      (kLayerTreeIncludesDebugInfo | kLayerTreeIncludesCompositingReasons)) {
    if (layer->debug_info()) {
      auto compositing_reasons_json = std::make_unique<JSONArray>();
      for (const char* name : layer->debug_info()->compositing_reasons)
        compositing_reasons_json->PushString(name);
      json->SetArray("compositingReasons", std::move(compositing_reasons_json));
    }
  }

  return json;
}

LayersAsJSON::LayersAsJSON(LayerTreeFlags flags)
    : flags_(flags),
      next_transform_id_(1),
      layers_json_(std::make_unique<JSONArray>()),
      transforms_json_(std::make_unique<JSONArray>()) {}

int LayersAsJSON::AddTransformJSON(
    const TransformPaintPropertyNode& transform) {
  auto it = transform_id_map_.find(&transform);
  if (it != transform_id_map_.end())
    return it->value;

  int parent_id = 0;
  if (transform.Parent())
    parent_id = AddTransformJSON(*transform.Parent());
  if (transform.IsIdentity() && !transform.RenderingContextId()) {
    transform_id_map_.Set(&transform, parent_id);
    return parent_id;
  }

  auto transform_json = std::make_unique<JSONObject>();
  int transform_id = next_transform_id_++;
  transform_id_map_.Set(&transform, transform_id);
  transform_json->SetInteger("id", transform_id);
  if (parent_id)
    transform_json->SetInteger("parent", parent_id);

  if (!transform.IsIdentity()) {
    transform_json->SetArray("transform",
                             TransformAsJSONArray(transform.SlowMatrix()));
  }

  if (!transform.IsIdentityOr2DTranslation() &&
      !transform.Matrix().IsIdentityOrTranslation())
    transform_json->SetArray("origin", PointAsJSONArray(transform.Origin()));

  if (!transform.FlattensInheritedTransform())
    transform_json->SetBoolean("flattenInheritedTransform", false);

  if (auto rendering_context = transform.RenderingContextId()) {
    auto context_lookup_result = rendering_context_map_.find(rendering_context);
    int rendering_id = rendering_context_map_.size() + 1;
    if (context_lookup_result == rendering_context_map_.end())
      rendering_context_map_.Set(rendering_context, rendering_id);
    else
      rendering_id = context_lookup_result->value;

    transform_json->SetInteger("renderingContext", rendering_id);
  }

  transforms_json_->PushObject(std::move(transform_json));
  return transform_id;
}

void LayersAsJSON::AddLayer(const cc::Layer& layer,
                            const FloatPoint& offset,
                            const TransformPaintPropertyNode& transform,
                            const LayerAsJSONClient* json_client) {
  auto layer_json = CCLayerAsJSON(&layer, Flags(), offset);
  if (json_client) {
    json_client->AppendAdditionalInfoAsJSON(Flags(), layer,
                                            *(layer_json.get()));
  }
  int transform_id = AddTransformJSON(transform);
  if (transform_id)
    layer_json->SetInteger("transform", transform_id);
  layers_json_->PushObject(std::move(layer_json));
}

std::unique_ptr<JSONObject> LayersAsJSON::Finalize() {
  auto json = std::make_unique<JSONObject>();
  json->SetArray("layers", std::move(layers_json_));
  if (transforms_json_->size())
    json->SetArray("transforms", std::move(transforms_json_));
  return json;
}

}  // namespace blink

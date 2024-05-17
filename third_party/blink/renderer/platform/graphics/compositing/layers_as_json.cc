// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/layers_as_json.h"

#include "cc/layers/layer.h"
#include "third_party/blink/renderer/platform/geometry/geometry_as_json.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/compositing/content_layer_client_impl.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

namespace {

String PointerAsString(const void* ptr) {
  WTF::TextStream ts;
  ts << ptr;
  return ts.Release();
}

double RoundCloseToZero(double number) {
  return std::abs(number) < 1e-7 ? 0 : number;
}

std::unique_ptr<JSONArray> TransformAsJSONArray(const gfx::Transform& t) {
  auto array = std::make_unique<JSONArray>();
  for (int c = 0; c < 4; c++) {
    auto col = std::make_unique<JSONArray>();
    for (int r = 0; r < 4; r++)
      col->PushDouble(RoundCloseToZero(t.rc(r, c)));
    array->PushArray(std::move(col));
  }
  return array;
}

}  // namespace

// Create a JSON version of the specified |layer|.
std::unique_ptr<JSONObject> CCLayerAsJSON(const cc::Layer& layer,
                                          LayerTreeFlags flags) {
  auto json = std::make_unique<JSONObject>();

  if (flags & kLayerTreeIncludesDebugInfo) {
    json->SetString("this", PointerAsString(&layer));
    json->SetInteger("ccLayerId", layer.id());
  }

  String debug_name(layer.DebugName());
  json->SetString("name", debug_name);

  if (layer.offset_to_transform_parent() != gfx::Vector2dF()) {
    json->SetArray("position",
                   VectorAsJSONArray(layer.offset_to_transform_parent()));
  }

  // This is testing against gfx::Size(), *not* whether the size is empty.
  if (layer.bounds() != gfx::Size())
    json->SetArray("bounds", SizeAsJSONArray(layer.bounds()));

  if (layer.contents_opaque())
    json->SetBoolean("contentsOpaque", true);
  else if (layer.contents_opaque_for_text())
    json->SetBoolean("contentsOpaqueForText", true);

  if (!layer.draws_content())
    json->SetBoolean("drawsContent", false);

  if (layer.should_check_backface_visibility())
    json->SetString("backfaceVisibility", "hidden");

  if (!Color::FromSkColor4f(layer.background_color()).IsFullyTransparent() &&
      ((flags & kLayerTreeIncludesDebugInfo) ||
       // Omit backgroundColor for these layers because it's not interesting
       // and we want to avoid platform differences and changes with CLs
       // affecting backgroundColor in web tests that dump layer trees.
       (debug_name != "Caret" && !debug_name.Contains("Scroll corner of")))) {
    json->SetString("backgroundColor",
                    Color::FromSkColor4f(layer.background_color())
                        .NameForLayoutTreeAsText());
  }

  if (flags &
      (kLayerTreeIncludesDebugInfo | kLayerTreeIncludesCompositingReasons)) {
    if (layer.debug_info()) {
      auto compositing_reasons_json = std::make_unique<JSONArray>();
      for (const char* name : layer.debug_info()->compositing_reasons)
        compositing_reasons_json->PushString(name);
      json->SetArray("compositingReasons", std::move(compositing_reasons_json));
    }
  }

  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled() &&
      (flags & kLayerTreeIncludesDebugInfo) &&
      layer.hit_test_opaqueness() != cc::HitTestOpaqueness::kOpaque) {
    json->SetString("hitTestOpaqueness",
                    cc::HitTestOpaquenessToString(layer.hit_test_opaqueness()));
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
    parent_id = AddTransformJSON(*transform.UnaliasedParent());
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
                             TransformAsJSONArray(transform.Matrix()));
  }

  if (!transform.Matrix().IsIdentityOrTranslation()) {
    transform_json->SetArray("origin", Point3AsJSONArray(transform.Origin()));
  }

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
                            const TransformPaintPropertyNode& transform,
                            const ContentLayerClientImpl* layer_client) {
  if (!(flags_ & kLayerTreeIncludesAllLayers) && !layer.draws_content()) {
    std::string debug_name = layer.DebugName();
    if (debug_name == "LayoutView #document" ||
        debug_name == "Inner Viewport Scroll Layer" ||
        debug_name == "Scrolling Contents Layer") {
      return;
    }
  }

  auto layer_json = CCLayerAsJSON(layer, flags_);
  if (layer_client) {
    layer_client->AppendAdditionalInfoAsJSON(flags_, layer, *layer_json);
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

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/graphics_layer_tree_as_text.h"

#include "cc/layers/picture_layer.h"
#include "third_party/blink/renderer/platform/geometry/geometry_as_json.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/json/json_values.h"

namespace blink {

namespace {

std::unique_ptr<JSONObject> GraphicsLayerAsJSON(const GraphicsLayer* layer,
                                                LayerTreeFlags flags) {
  auto json = CCLayerAsJSON(layer->CcLayer(), flags);
  // CCLayerAsJSON() doesn't know the name before paint or if the layer is a
  // legacy GraphicsLayer which doesn't contribute to the cc layer list.
  json->SetString("name", layer->DebugName());

  // Content dumped after this point, down to AppendAdditionalInfoAsJSON, is
  // specific to GraphicsLayer tree dumping when called from one of the methods
  // in this file.

  if (flags & kLayerTreeIncludesDebugInfo) {
    if (layer->HasContentsLayer())
      json->SetInteger("ccContentsLayerId", layer->ContentsLayer()->id());
  }

  if (flags & kLayerTreeIncludesDebugInfo &&
      layer->OffsetFromLayoutObject() != IntSize()) {
    json->SetArray("offsetFromLayoutObject",
                   SizeAsJSONArray(layer->OffsetFromLayoutObject()));
  }

  if (!layer->ContentsAreVisible())
    json->SetBoolean("contentsVisible", false);

  if (layer->HasLayerState() && (flags & (kLayerTreeIncludesDebugInfo |
                                          kLayerTreeIncludesPaintRecords))) {
    json->SetString("layerState", layer->GetPropertyTreeState().ToString());
    json->SetValue("layerOffset",
                   PointAsJSONArray(layer->GetOffsetFromTransformNode()));
  }

  layer->AppendAdditionalInfoAsJSON(flags, layer->CcLayer(), *json.get());

  return json;
}

}  // namespace

std::unique_ptr<JSONObject> GraphicsLayerTreeAsJSON(const GraphicsLayer* layer,
                                                    LayerTreeFlags flags) {
  DCHECK(flags & kOutputAsLayerTree);
  std::unique_ptr<JSONObject> json = GraphicsLayerAsJSON(layer, flags);
  if (layer->Children().size()) {
    auto children_json = std::make_unique<JSONArray>();
    for (wtf_size_t i = 0; i < layer->Children().size(); i++) {
      children_json->PushObject(
          GraphicsLayerTreeAsJSON(layer->Children()[i], flags));
    }
    json->SetArray("children", std::move(children_json));
  }
  return json;
}

String GraphicsLayerTreeAsTextForTesting(const GraphicsLayer* layer,
                                         LayerTreeFlags flags) {
  return GraphicsLayerTreeAsJSON(layer, flags)->ToPrettyJSONString();
}

}  // namespace blink

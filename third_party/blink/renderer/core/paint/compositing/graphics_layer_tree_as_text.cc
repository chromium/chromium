// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/graphics_layer_tree_as_text.h"

#include "cc/layers/picture_layer.h"
#include "third_party/blink/renderer/platform/geometry/geometry_as_json.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/json/json_values.h"

namespace blink {

namespace {

std::unique_ptr<JSONObject> GraphicsLayerAsJSON(const GraphicsLayer* layer,
                                                LayerTreeFlags flags) {
  // Intentionally passing through 0, 0 for the offset from the transform node
  // as this dump implementation doesn't support transform/position information.
  auto json = CCLayerAsJSON(layer->CcLayer(), flags, FloatPoint());

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

  // MaskLayers are output via ForeignLayerDisplayItem iteration in the other
  // dumping code paths.
  if (layer->MaskLayer()) {
    auto mask_layer_json = std::make_unique<JSONArray>();
    mask_layer_json->PushObject(GraphicsLayerAsJSON(layer->MaskLayer(), flags));
    json->SetArray("maskLayer", std::move(mask_layer_json));
  }

  if (layer->HasLayerState() && (flags & (kLayerTreeIncludesDebugInfo |
                                          kLayerTreeIncludesPaintRecords))) {
    json->SetString("layerState", layer->GetPropertyTreeState().ToString());
    json->SetValue("layerOffset",
                   PointAsJSONArray(layer->GetOffsetFromTransformNode()));
  }

  layer->AppendAdditionalInfoAsJSON(flags, *layer->CcLayer(), *json.get());

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

#if DCHECK_IS_ON()
void VerboseLogGraphicsLayerTree(const GraphicsLayer* root) {
  if (!VLOG_IS_ON(2))
    return;

  using GraphicsLayerTreeMap = HashMap<const GraphicsLayer*, String>;
  DEFINE_STATIC_LOCAL(GraphicsLayerTreeMap, s_previous_trees, ());
  LayerTreeFlags flags = VLOG_IS_ON(3) ? 0xffffffff : kOutputAsLayerTree;
  String new_tree = GraphicsLayerTreeAsTextForTesting(root, flags);
  auto it = s_previous_trees.find(root);
  if (it == s_previous_trees.end() || it->value != new_tree) {
    VLOG(2) << "GraphicsLayer tree:\n" << new_tree.Utf8();
    s_previous_trees.Set(root, new_tree);
    // For simplification, we don't remove deleted GraphicsLayers from the
    // map.
  }
}
#endif

}  // namespace blink
#if DCHECK_IS_ON()
void showGraphicsLayerTree(const blink::GraphicsLayer* layer) {
  if (!layer) {
    LOG(ERROR) << "Cannot showGraphicsLayerTree for (nil).";
    return;
  }

  String output = blink::GraphicsLayerTreeAsTextForTesting(layer, 0xffffffff);
  LOG(ERROR) << output.Utf8();
}
#endif

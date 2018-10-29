// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/graphics_layer_tree_as_text.h"

#include "cc/layers/picture_layer.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/geometry/geometry_as_json.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/logging_canvas.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"

namespace blink {

namespace {

typedef HashMap<int, int> RenderingContextMap;

String PointerAsString(const void* ptr) {
  WTF::TextStream ts;
  ts << ptr;
  return ts.Release();
}

FloatPoint ScrollPosition(const GraphicsLayer& layer) {
  if (const auto* scrollable_area =
          layer.Client().GetScrollableAreaForTesting(&layer)) {
    return scrollable_area->ScrollPosition();
  }
  return FloatPoint();
}

void AddFlattenInheritedTransformJSON(const GraphicsLayer* layer,
                                      JSONObject& json) {
  if (layer->Parent() && !layer->Parent()->ShouldFlattenTransform())
    json.SetBoolean("flattenInheritedTransform", false);
}

void AddTransformJSONProperties(const GraphicsLayer* layer,
                                JSONObject& json,
                                RenderingContextMap& rendering_context_map) {
  const TransformationMatrix& transform = layer->Transform();
  if (!transform.IsIdentity())
    json.SetArray("transform", TransformAsJSONArray(transform));

  if (!transform.IsIdentityOrTranslation()) {
    json.SetArray("origin",
                  PointAsJSONArray(FloatPoint3D(layer->TransformOrigin())));
  }

  AddFlattenInheritedTransformJSON(layer, json);

  if (int rendering_context3d = layer->GetRenderingContext3D()) {
    auto it = rendering_context_map.find(rendering_context3d);
    int context_id = rendering_context_map.size() + 1;
    if (it == rendering_context_map.end())
      rendering_context_map.Set(rendering_context3d, context_id);
    else
      context_id = it->value;

    json.SetInteger("renderingContext", context_id);
  }
}

std::unique_ptr<JSONObject> GraphicsLayerAsJSON(
    const GraphicsLayer* layer,
    LayerTreeFlags flags,
    RenderingContextMap& rendering_context_map,
    const FloatPoint& position) {
  std::unique_ptr<JSONObject> json = JSONObject::Create();

  if (flags & kLayerTreeIncludesDebugInfo) {
    json->SetString("this", PointerAsString(layer));
    json->SetInteger("ccLayerId", layer->CcLayer()->id());
    if (layer->HasContentsLayer())
      json->SetInteger("ccContentsLayerId", layer->ContentsLayer()->id());
  }

  json->SetString("name", layer->DebugName());

  if (position != FloatPoint())
    json->SetArray("position", PointAsJSONArray(position));

  if (flags & kLayerTreeIncludesDebugInfo &&
      layer->OffsetFromLayoutObject() != IntSize()) {
    json->SetArray("offsetFromLayoutObject",
                   SizeAsJSONArray(layer->OffsetFromLayoutObject()));
  }

  // This is testing against gfx::Size(), *not* whether the size is empty.
  if (layer->Size() != gfx::Size())
    json->SetArray("bounds", SizeAsJSONArray(IntSize(layer->Size())));

  if (layer->Opacity() != 1)
    json->SetDouble("opacity", layer->Opacity());

  if (layer->GetBlendMode() != BlendMode::kNormal) {
    json->SetString("blendMode", CompositeOperatorName(kCompositeSourceOver,
                                                       layer->GetBlendMode()));
  }

  if (layer->IsRootForIsolatedGroup())
    json->SetBoolean("isolate", true);

  if (layer->ContentsOpaque())
    json->SetBoolean("contentsOpaque", true);

  if (!layer->DrawsContent())
    json->SetBoolean("drawsContent", false);

  if (!layer->ContentsAreVisible())
    json->SetBoolean("contentsVisible", false);

  if (!layer->BackfaceVisibility())
    json->SetString("backfaceVisibility", "hidden");

  if (flags & kLayerTreeIncludesDebugInfo)
    json->SetString("client", PointerAsString(&layer->Client()));

  if (Color(layer->BackgroundColor()).Alpha()) {
    json->SetString("backgroundColor",
                    Color(layer->BackgroundColor()).NameForLayoutTreeAsText());
  }

  if (flags & kOutputAsLayerTree) {
    AddTransformJSONProperties(layer, *json, rendering_context_map);
    if (!layer->ShouldFlattenTransform())
      json->SetBoolean("shouldFlattenTransform", false);
    FloatPoint scroll_position(ScrollPosition(*layer));
    if (scroll_position != FloatPoint())
      json->SetArray("scrollPosition", PointAsJSONArray(scroll_position));
  }

  if ((flags & kLayerTreeIncludesPaintInvalidations) &&
      layer->Client().IsTrackingRasterInvalidations() &&
      layer->GetRasterInvalidationTracking()) {
    layer->GetRasterInvalidationTracking()->AsJSON(json.get());
  }

  GraphicsLayerPaintingPhase painting_phase = layer->PaintingPhase();
  if ((flags & kLayerTreeIncludesPaintingPhases) && painting_phase) {
    std::unique_ptr<JSONArray> painting_phases_json = JSONArray::Create();
    if (painting_phase & kGraphicsLayerPaintBackground)
      painting_phases_json->PushString("GraphicsLayerPaintBackground");
    if (painting_phase & kGraphicsLayerPaintForeground)
      painting_phases_json->PushString("GraphicsLayerPaintForeground");
    if (painting_phase & kGraphicsLayerPaintMask)
      painting_phases_json->PushString("GraphicsLayerPaintMask");
    if (painting_phase & kGraphicsLayerPaintChildClippingMask)
      painting_phases_json->PushString("GraphicsLayerPaintChildClippingMask");
    if (painting_phase & kGraphicsLayerPaintAncestorClippingMask) {
      painting_phases_json->PushString(
          "GraphicsLayerPaintAncestorClippingMask");
    }
    if (painting_phase & kGraphicsLayerPaintOverflowContents)
      painting_phases_json->PushString("GraphicsLayerPaintOverflowContents");
    if (painting_phase & kGraphicsLayerPaintCompositedScroll)
      painting_phases_json->PushString("GraphicsLayerPaintCompositedScroll");
    if (painting_phase & kGraphicsLayerPaintDecoration)
      painting_phases_json->PushString("GraphicsLayerPaintDecoration");
    json->SetArray("paintingPhases", std::move(painting_phases_json));
  }

  if (flags & kLayerTreeIncludesClipAndScrollParents) {
    if (layer->HasScrollParent())
      json->SetBoolean("hasScrollParent", true);
    if (layer->HasClipParent())
      json->SetBoolean("hasClipParent", true);
  }

  if (flags &
      (kLayerTreeIncludesDebugInfo | kLayerTreeIncludesCompositingReasons)) {
    bool debug = flags & kLayerTreeIncludesDebugInfo;
    {
      std::unique_ptr<JSONArray> compositing_reasons_json = JSONArray::Create();
      CompositingReasons compositing_reasons = layer->GetCompositingReasons();
      auto names = debug ? CompositingReason::Descriptions(compositing_reasons)
                         : CompositingReason::ShortNames(compositing_reasons);
      for (const char* name : names)
        compositing_reasons_json->PushString(name);
      json->SetArray("compositingReasons", std::move(compositing_reasons_json));
    }
    {
      std::unique_ptr<JSONArray> squashing_disallowed_reasons_json =
          JSONArray::Create();
      SquashingDisallowedReasons squashing_disallowed_reasons =
          layer->GetSquashingDisallowedReasons();
      auto names = debug ? SquashingDisallowedReason::Descriptions(
                               squashing_disallowed_reasons)
                         : SquashingDisallowedReason::ShortNames(
                               squashing_disallowed_reasons);
      for (const char* name : names)
        squashing_disallowed_reasons_json->PushString(name);
      json->SetArray("squashingDisallowedReasons",
                     std::move(squashing_disallowed_reasons_json));
    }
  }

  if (layer->MaskLayer()) {
    std::unique_ptr<JSONArray> mask_layer_json = JSONArray::Create();
    mask_layer_json->PushObject(
        GraphicsLayerAsJSON(layer->MaskLayer(), flags, rendering_context_map,
                            FloatPoint(layer->MaskLayer()->GetPosition())));
    json->SetArray("maskLayer", std::move(mask_layer_json));
  }

  if (layer->ContentsClippingMaskLayer()) {
    std::unique_ptr<JSONArray> contents_clipping_mask_layer_json =
        JSONArray::Create();
    contents_clipping_mask_layer_json->PushObject(GraphicsLayerAsJSON(
        layer->ContentsClippingMaskLayer(), flags, rendering_context_map,
        FloatPoint(layer->ContentsClippingMaskLayer()->GetPosition())));
    json->SetArray("contentsClippingMaskLayer",
                   std::move(contents_clipping_mask_layer_json));
  }

  if (layer->HasLayerState() && (flags & (kLayerTreeIncludesDebugInfo |
                                          kLayerTreeIncludesPaintRecords))) {
    json->SetString("layerState", layer->GetPropertyTreeState().ToString());
    json->SetValue("layerOffset",
                   PointAsJSONArray(layer->GetOffsetFromTransformNode()));
  }

#if DCHECK_IS_ON()
  if (layer->DrawsContent() && (flags & kLayerTreeIncludesPaintRecords))
    json->SetValue("paintRecord", RecordAsJSON(*layer->CapturePaintRecord()));
#endif

  return json;
}

class LayersAsJSONArray {
 public:
  LayersAsJSONArray(LayerTreeFlags flags)
      : flags_(flags),
        next_transform_id_(1),
        layers_json_(JSONArray::Create()),
        transforms_json_(JSONArray::Create()) {}

  // Outputs the layer tree rooted at |layer| as a JSON array, in paint order,
  // and the transform tree also as a JSON array.
  std::unique_ptr<JSONObject> operator()(const GraphicsLayer& layer) {
    auto json = JSONObject::Create();
    Walk(layer, 0, FloatPoint());
    json->SetArray("layers", std::move(layers_json_));
    if (transforms_json_->size())
      json->SetArray("transforms", std::move(transforms_json_));
    return json;
  }

  JSONObject* AddTransformJSON(int& transform_id) {
    auto transform_json = JSONObject::Create();
    int parent_transform_id = transform_id;
    transform_id = next_transform_id_++;
    transform_json->SetInteger("id", transform_id);
    if (parent_transform_id)
      transform_json->SetInteger("parent", parent_transform_id);
    auto* result = transform_json.get();
    transforms_json_->PushObject(std::move(transform_json));
    return result;
  }

  void AddLayer(const GraphicsLayer& layer,
                int& transform_id,
                FloatPoint& position) {
    FloatPoint scroll_position = ScrollPosition(layer);
    if (scroll_position != FloatPoint()) {
      // Output scroll position as a transform.
      auto* scroll_translate_json = AddTransformJSON(transform_id);
      scroll_translate_json->SetArray(
          "transform", TransformAsJSONArray(TransformationMatrix().Translate(
                           -scroll_position.X(), -scroll_position.Y())));
      AddFlattenInheritedTransformJSON(&layer, *scroll_translate_json);
    }

    if (!layer.Transform().IsIdentity() || layer.GetRenderingContext3D() ||
        layer.GetCompositingReasons() & CompositingReason::k3DTransform) {
      if (position != FloatPoint()) {
        // Output position offset as a transform.
        auto* position_translate_json = AddTransformJSON(transform_id);
        position_translate_json->SetArray(
            "transform", TransformAsJSONArray(TransformationMatrix().Translate(
                             position.X(), position.Y())));
        AddFlattenInheritedTransformJSON(&layer, *position_translate_json);
        if (layer.Parent() && !layer.Parent()->ShouldFlattenTransform()) {
          position_translate_json->SetBoolean("flattenInheritedTransform",
                                              false);
        }
        position = FloatPoint();
      }

      if (!layer.Transform().IsIdentity() || layer.GetRenderingContext3D()) {
        auto* transform_json = AddTransformJSON(transform_id);
        AddTransformJSONProperties(&layer, *transform_json,
                                   rendering_context_map_);
      }
    }

    auto json =
        GraphicsLayerAsJSON(&layer, flags_, rendering_context_map_, position);
    if (transform_id)
      json->SetInteger("transform", transform_id);
    layers_json_->PushObject(std::move(json));
  }

  void Walk(const GraphicsLayer& layer,
            int parent_transform_id,
            const FloatPoint& parent_position) {
    FloatPoint position = parent_position + FloatPoint(layer.GetPosition());
    int transform_id = parent_transform_id;
    AddLayer(layer, transform_id, position);
    for (auto* const child : layer.Children())
      Walk(*child, transform_id, position);
  }

 private:
  LayerTreeFlags flags_;
  int next_transform_id_;
  RenderingContextMap rendering_context_map_;
  std::unique_ptr<JSONArray> layers_json_;
  std::unique_ptr<JSONArray> transforms_json_;
};

// This is the SPv1 version of ContentLayerClientImpl::LayerAsJSON().
std::unique_ptr<JSONObject> GraphicsLayerTreeAsJSON(
    const GraphicsLayer* layer,
    LayerTreeFlags flags,
    RenderingContextMap& rendering_context_map) {
  std::unique_ptr<JSONObject> json = GraphicsLayerAsJSON(
      layer, flags, rendering_context_map, FloatPoint(layer->GetPosition()));

  if (layer->Children().size()) {
    std::unique_ptr<JSONArray> children_json = JSONArray::Create();
    for (wtf_size_t i = 0; i < layer->Children().size(); i++) {
      children_json->PushObject(GraphicsLayerTreeAsJSON(
          layer->Children()[i], flags, rendering_context_map));
    }
    json->SetArray("children", std::move(children_json));
  }

  return json;
}

}  // namespace

std::unique_ptr<JSONObject> GraphicsLayerTreeAsJSON(const GraphicsLayer* layer,
                                                    LayerTreeFlags flags) {
  if (flags & kOutputAsLayerTree) {
    RenderingContextMap rendering_context_map;
    return GraphicsLayerTreeAsJSON(layer, flags, rendering_context_map);
  }

  return LayersAsJSONArray(flags)(*layer);
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
    VLOG(2) << "GraphicsLayer tree:\n" << new_tree.Utf8().data();
    s_previous_trees.Set(root, new_tree);
    // For simplification, we don't remove deleted GraphicsLayers from the map.
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
  LOG(ERROR) << output.Utf8().data();
}

void showGraphicsLayers(const blink::GraphicsLayer* layer) {
  if (!layer) {
    LOG(ERROR) << "Cannot showGraphicsLayers for (nil).";
    return;
  }

  String output = blink::GraphicsLayerTreeAsTextForTesting(
      layer, 0xffffffff & ~blink::kOutputAsLayerTree);
  LOG(ERROR) << output.Utf8().data();
}
#endif

/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/inspector/inspector_layer_tree_agent.h"

#include <memory>

#include "cc/base/region.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/transform_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/graphics/compositing_reasons.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/picture_snapshot.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/inspector_protocol/crdtp/json.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

using protocol::Array;
using protocol::Maybe;
unsigned InspectorLayerTreeAgent::last_snapshot_id_;

inline String IdForLayer(const cc::Layer* layer) {
  return String::Number(layer->id());
}

static std::unique_ptr<protocol::DOM::Rect> BuildObjectForRect(
    const gfx::Rect& rect) {
  return protocol::DOM::Rect::create()
      .setX(rect.x())
      .setY(rect.y())
      .setHeight(rect.height())
      .setWidth(rect.width())
      .build();
}

static std::unique_ptr<protocol::DOM::Rect> BuildObjectForRect(
    const gfx::RectF& rect) {
  return protocol::DOM::Rect::create()
      .setX(rect.x())
      .setY(rect.y())
      .setHeight(rect.height())
      .setWidth(rect.width())
      .build();
}

static std::unique_ptr<protocol::LayerTree::ScrollRect> BuildScrollRect(
    const gfx::Rect& rect,
    const String& type) {
  std::unique_ptr<protocol::DOM::Rect> rect_object = BuildObjectForRect(rect);
  std::unique_ptr<protocol::LayerTree::ScrollRect> scroll_rect_object =
      protocol::LayerTree::ScrollRect::create()
          .setRect(std::move(rect_object))
          .setType(type)
          .build();
  return scroll_rect_object;
}

static std::unique_ptr<Array<protocol::LayerTree::ScrollRect>>
BuildScrollRectsForLayer(const cc::Layer* layer) {
  auto scroll_rects =
      std::make_unique<protocol::Array<protocol::LayerTree::ScrollRect>>();
  for (gfx::Rect rect : layer->main_thread_scroll_hit_test_region()) {
    // TODO(crbug.com/41495630): Now main thread scroll hit test and
    // RepaintsOnScroll are different things.
    scroll_rects->emplace_back(BuildScrollRect(
        rect, protocol::LayerTree::ScrollRect::TypeEnum::RepaintsOnScroll));
  }
  const cc::Region& touch_event_handler_regions =
      layer->touch_action_region().GetAllRegions();
  for (gfx::Rect rect : touch_event_handler_regions) {
    scroll_rects->emplace_back(BuildScrollRect(
        rect, protocol::LayerTree::ScrollRect::TypeEnum::TouchEventHandler));
  }
  const cc::Region& wheel_event_handler_region = layer->wheel_event_region();
  for (gfx::Rect rect : wheel_event_handler_region) {
    scroll_rects->emplace_back(BuildScrollRect(
        rect, protocol::LayerTree::ScrollRect::TypeEnum::WheelEventHandler));
  }
  return scroll_rects->empty() ? nullptr : std::move(scroll_rects);
}

// TODO(flackr): We should be getting the sticky position constraints from the
// property tree once blink is able to access them. https://crbug.com/754339
static const cc::Layer* FindLayerByElementId(const cc::Layer* root,
                                             CompositorElementId element_id) {
  if (root->element_id() == element_id)
    return root;
  for (auto child : root->children()) {
    if (const auto* layer = FindLayerByElementId(child.get(), element_id))
      return layer;
  }
  return nullptr;
}

static std::unique_ptr<protocol::LayerTree::StickyPositionConstraint>
BuildStickyInfoForLayer(const cc::Layer* root, const cc::Layer* layer) {
  if (!layer->has_transform_node())
    return nullptr;
  // Note that we'll miss the sticky transform node if multiple transform nodes
  // apply to the layer.
  const cc::StickyPositionNodeData* sticky_data =
      layer->layer_tree_host()
          ->property_trees()
          ->transform_tree()
          .GetStickyPositionData(layer->transform_tree_index());
  if (!sticky_data)
    return nullptr;
  const cc::StickyPositionConstraint& constraints = sticky_data->constraints;

  std::unique_ptr<protocol::DOM::Rect> sticky_box_rect =
      BuildObjectForRect(constraints.scroll_container_relative_sticky_box_rect);

  std::unique_ptr<protocol::DOM::Rect> containing_block_rect =
      BuildObjectForRect(
          constraints.scroll_container_relative_containing_block_rect);

  std::unique_ptr<protocol::LayerTree::StickyPositionConstraint>
      constraints_obj =
          protocol::LayerTree::StickyPositionConstraint::create()
              .setStickyBoxRect(std::move(sticky_box_rect))
              .setContainingBlockRect(std::move(containing_block_rect))
              .build();
  if (constraints.nearest_element_shifting_sticky_box) {
    const cc::Layer* constraint_layer = FindLayerByElementId(
        root, constraints.nearest_element_shifting_sticky_box);
    if (!constraint_layer)
      return nullptr;
    constraints_obj->setNearestLayerShiftingStickyBox(
        String::Number(constraint_layer->id()));
  }
  if (constraints.nearest_element_shifting_containing_block) {
    const cc::Layer* constraint_layer = FindLayerByElementId(
        root, constraints.nearest_element_shifting_containing_block);
    if (!constraint_layer)
      return nullptr;
    constraints_obj->setNearestLayerShiftingContainingBlock(
        String::Number(constraint_layer->id()));
  }

  return constraints_obj;
}

static std::unique_ptr<protocol::LayerTree::Layer> BuildObjectForLayer(
    const cc::Layer* root,
    const cc::Layer* layer) {
  // When the front-end doesn't show internal layers, it will use the the first
  // DrawsContent layer as the root of the shown layer tree. This doesn't work
  // because the non-DrawsContent root layer is the parent of all DrawsContent
  // layers. We have to cheat the front-end by setting drawsContent to true for
  // the root layer.
  bool draws_content = root == layer || layer->draws_content();

  // TODO(pdr): Now that BlinkGenPropertyTrees has launched, we can remove
  // setOffsetX and setOffsetY.
  std::unique_ptr<protocol::LayerTree::Layer> layer_object =
      protocol::LayerTree::Layer::create()
          .setLayerId(IdForLayer(layer))
          .setOffsetX(0)
          .setOffsetY(0)
          .setWidth(layer->bounds().width())
          .setHeight(layer->bounds().height())
          .setPaintCount(layer->debug_info() ? layer->debug_info()->paint_count
                                             : 0)
          .setDrawsContent(draws_content)
          .build();

  if (layer->debug_info()) {
    if (auto node_id = layer->debug_info()->owner_node_id)
      layer_object->setBackendNodeId(node_id);
  }

  if (const auto* parent = layer->parent())
    layer_object->setParentLayerId(IdForLayer(parent));

  gfx::Transform transform = layer->ScreenSpaceTransform();

  if (!transform.IsIdentity()) {
    auto transform_array = std::make_unique<protocol::Array<double>>(16);
    transform.GetColMajor(transform_array->data());
    layer_object->setTransform(std::move(transform_array));
    // FIXME: rename these to setTransformOrigin*
    // TODO(pdr): Now that BlinkGenPropertyTrees has launched, we can remove
    // setAnchorX, setAnchorY, and setAnchorZ.
    layer_object->setAnchorX(0.f);
    layer_object->setAnchorY(0.f);
    layer_object->setAnchorZ(0.f);
  }
  std::unique_ptr<Array<protocol::LayerTree::ScrollRect>> scroll_rects =
      BuildScrollRectsForLayer(layer);
  if (scroll_rects)
    layer_object->setScrollRects(std::move(scroll_rects));
  std::unique_ptr<protocol::LayerTree::StickyPositionConstraint> sticky_info =
      BuildStickyInfoForLayer(root, layer);
  if (sticky_info)
    layer_object->setStickyPositionConstraint(std::move(sticky_info));
  return layer_object;
}

InspectorLayerTreeAgent::InspectorLayerTreeAgent(
    InspectedFrames* inspected_frames,
    Client* client)
    : inspected_frames_(inspected_frames),
      client_(client),
      suppress_layer_paint_events_(false) {}

InspectorLayerTreeAgent::~InspectorLayerTreeAgent() = default;

void InspectorLayerTreeAgent::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent::Trace(visitor);
}

void InspectorLayerTreeAgent::Restore() {
  // We do not re-enable layer agent automatically after navigation. This is
  // because it depends on DOMAgent and node ids in particular, so we let
  // front-end request document and re-enable the agent manually after this.
}

protocol::Response InspectorLayerTreeAgent::enable() {
  instrumenting_agents_->AddInspectorLayerTreeAgent(this);
  if (auto* view = inspected_frames_->Root()->View()) {
    view->ScheduleAnimation();
    return protocol::Response::Success();
  }
  return protocol::Response::ServerError("The root frame doesn't have a view");
}

protocol::Response InspectorLayerTreeAgent::disable() {
  instrumenting_agents_->RemoveInspectorLayerTreeAgent(this);
  snapshot_by_id_.clear();
  return protocol::Response::Success();
}

void InspectorLayerTreeAgent::LayerTreeDidChange() {
  GetFrontend()->layerTreeDidChange(BuildLayerTree());
}

void InspectorLayerTreeAgent::LayerTreePainted() {
  for (const auto& layer : RootLayer()->children()) {
    if (!layer->update_rect().IsEmpty()) {
      GetFrontend()->layerPainted(IdForLayer(layer.get()),
                                  BuildObjectForRect(layer->update_rect()));
    }
  }
}

std::unique_ptr<Array<protocol::LayerTree::Layer>>
InspectorLayerTreeAgent::BuildLayerTree() {
  const auto* root_layer = RootLayer();
  if (!root_layer)
    return nullptr;

  auto layers = std::make_unique<protocol::Array<protocol::LayerTree::Layer>>();
  GatherLayers(root_layer, layers);
  return layers;
}

void InspectorLayerTreeAgent::GatherLayers(
    const cc::Layer* layer,
    std::unique_ptr<Array<protocol::LayerTree::Layer>>& layers) {
  if (client_->IsInspectorLayer(layer))
    return;
  if (layer->layer_tree_host()->is_hud_layer(layer))
    return;
  layers->emplace_back(BuildObjectForLayer(RootLayer(), layer));
  for (auto child : layer->children())
    GatherLayers(child.get(), layers);
}

const cc::Layer* InspectorLayerTreeAgent::RootLayer() {
  return inspected_frames_->Root()->View()->RootCcLayer();
}

static const cc::Layer* FindLayerById(const cc::Layer* root, int layer_id) {
  if (!root)
    return nullptr;
  if (root->id() == layer_id)
    return root;
  for (auto child : root->children()) {
    if (const auto* layer = FindLayerById(child.get(), layer_id))
      return layer;
  }
  return nullptr;
}

protocol::Response InspectorLayerTreeAgent::LayerById(
    const String& layer_id,
    const cc::Layer*& result) {
  bool ok;
  int id = layer_id.ToInt(&ok);
  if (!ok)
    return protocol::Response::ServerError("Invalid layer id");

  result = FindLayerById(RootLayer(), id);
  if (!result)
    return protocol::Response::ServerError("No layer matching given id found");
  return protocol::Response::Success();
}

protocol::Response InspectorLayerTreeAgent::compositingReasons(
    const String& layer_id,
    std::unique_ptr<Array<String>>* compositing_reasons,
    std::unique_ptr<Array<String>>* compositing_reason_ids) {
  const cc::Layer* layer = nullptr;
  protocol::Response response = LayerById(layer_id, layer);
  if (!response.IsSuccess())
    return response;
  *compositing_reasons = std::make_unique<protocol::Array<String>>();
  *compositing_reason_ids = std::make_unique<protocol::Array<String>>();
  if (layer->debug_info()) {
    for (const char* compositing_reason :
         layer->debug_info()->compositing_reasons) {
      (*compositing_reasons)->emplace_back(compositing_reason);
    }
    for (const char* compositing_reason_id :
         layer->debug_info()->compositing_reason_ids) {
      (*compositing_reason_ids)->emplace_back(compositing_reason_id);
    }
  }

  return protocol::Response::Success();
}

protocol::Response InspectorLayerTreeAgent::makeSnapshot(const String& layer_id,
                                                         String* snapshot_id) {
  suppress_layer_paint_events_ = true;

  // If we hit a devtool break point in the middle of document lifecycle, for
  // example, https://crbug.com/788219, this will prevent crash when clicking
  // the "layer" panel.
  if (inspected_frames_->Root()->GetDocument() && inspected_frames_->Root()
                                                      ->GetDocument()
                                                      ->Lifecycle()
                                                      .LifecyclePostponed())
    return protocol::Response::ServerError("Layer does not draw content");

  inspected_frames_->Root()->View()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kInspector);

  suppress_layer_paint_events_ = false;

  const cc::Layer* layer = nullptr;
  protocol::Response response = LayerById(layer_id, layer);
  if (!response.IsSuccess())
    return response;
  if (!layer->draws_content())
    return protocol::Response::ServerError("Layer does not draw content");

  auto picture = layer->GetPicture();
  if (!picture)
    return protocol::Response::ServerError("Layer does not produce picture");

  auto snapshot = base::MakeRefCounted<PictureSnapshot>(std::move(picture));
  *snapshot_id = String::Number(++last_snapshot_id_);
  bool new_entry = snapshot_by_id_.insert(*snapshot_id, snapshot).is_new_entry;
  DCHECK(new_entry);
  return protocol::Response::Success();
}

protocol::Response InspectorLayerTreeAgent::loadSnapshot(
    std::unique_ptr<Array<protocol::LayerTree::PictureTile>> tiles,
    String* snapshot_id) {
  if (tiles->empty()) {
    return protocol::Response::ServerError(
        "Invalid argument, no tiles provided");
  }
  if (tiles->size() > UINT_MAX) {
    return protocol::Response::ServerError(
        "Invalid argument, too many tiles provided");
  }
  wtf_size_t tiles_length = static_cast<wtf_size_t>(tiles->size());
  Vector<scoped_refptr<PictureSnapshot::TilePictureStream>> decoded_tiles;
  decoded_tiles.Grow(tiles_length);
  for (wtf_size_t i = 0; i < tiles_length; ++i) {
    protocol::LayerTree::PictureTile* tile = (*tiles)[i].get();
    decoded_tiles[i] = base::AdoptRef(new PictureSnapshot::TilePictureStream());
    decoded_tiles[i]->layer_offset.SetPoint(tile->getX(), tile->getY());
    const protocol::Binary& data = tile->getPicture();
    decoded_tiles[i]->picture =
        SkPicture::MakeFromData(data.data(), data.size());
  }
  scoped_refptr<PictureSnapshot> snapshot =
      PictureSnapshot::Load(decoded_tiles);
  if (!snapshot)
    return protocol::Response::ServerError("Invalid snapshot format");
  if (snapshot->IsEmpty())
    return protocol::Response::ServerError("Empty snapshot");

  *snapshot_id = String::Number(++last_snapshot_id_);
  bool new_entry = snapshot_by_id_.insert(*snapshot_id, snapshot).is_new_entry;
  DCHECK(new_entry);
  return protocol::Response::Success();
}

protocol::Response InspectorLayerTreeAgent::releaseSnapshot(
    const String& snapshot_id) {
  SnapshotById::iterator it = snapshot_by_id_.find(snapshot_id);
  if (it == snapshot_by_id_.end())
    return protocol::Response::ServerError("Snapshot not found");
  snapshot_by_id_.erase(it);
  return protocol::Response::Success();
}

protocol::Response InspectorLayerTreeAgent::GetSnapshotById(
    const String& snapshot_id,
    const PictureSnapshot*& result) {
  SnapshotById::iterator it = snapshot_by_id_.find(snapshot_id);
  if (it == snapshot_by_id_.end())
    return protocol::Response::ServerError("Snapshot not found");
  result = it->value.get();
  return protocol::Response::Success();
}

protocol::Response InspectorLayerTreeAgent::replaySnapshot(
    const String& snapshot_id,
    Maybe<int> from_step,
    Maybe<int> to_step,
    Maybe<double> scale,
    String* data_url) {
  const PictureSnapshot* snapshot = nullptr;
  protocol::Response response = GetSnapshotById(snapshot_id, snapshot);
  if (!response.IsSuccess())
    return response;
  auto png_data = snapshot->Replay(from_step.value_or(0), to_step.value_or(0),
                                   scale.value_or(1.0));
  if (png_data.empty())
    return protocol::Response::ServerError("Image encoding failed");
  *data_url = "data:image/png;base64," + Base64Encode(png_data);
  return protocol::Response::Success();
}

static void ParseRect(protocol::DOM::Rect& object, gfx::RectF* rect) {
  *rect = gfx::RectF(object.getX(), object.getY(), object.getWidth(),
                     object.getHeight());
}

protocol::Response InspectorLayerTreeAgent::profileSnapshot(
    const String& snapshot_id,
    Maybe<int> min_repeat_count,
    Maybe<double> min_duration,
    Maybe<protocol::DOM::Rect> clip_rect,
    std::unique_ptr<protocol::Array<protocol::Array<double>>>* out_timings) {
  const PictureSnapshot* snapshot = nullptr;
  protocol::Response response = GetSnapshotById(snapshot_id, snapshot);
  if (!response.IsSuccess())
    return response;
  gfx::RectF rect;
  if (clip_rect.has_value()) {
    ParseRect(clip_rect.value(), &rect);
  }
  auto timings = snapshot->Profile(min_repeat_count.value_or(1),
                                   base::Seconds(min_duration.value_or(0)),
                                   clip_rect.has_value() ? &rect : nullptr);
  *out_timings = std::make_unique<Array<Array<double>>>();
  for (const auto& row : timings) {
    auto out_row = std::make_unique<protocol::Array<double>>();
    for (base::TimeDelta delta : row)
      out_row->emplace_back(delta.InSecondsF());
    (*out_timings)->emplace_back(std::move(out_row));
  }
  return protocol::Response::Success();
}

protocol::Response InspectorLayerTreeAgent::snapshotCommandLog(
    const String& snapshot_id,
    std::unique_ptr<Array<protocol::DictionaryValue>>* command_log) {
  const PictureSnapshot* snapshot = nullptr;
  protocol::Response response = GetSnapshotById(snapshot_id, snapshot);
  if (!response.IsSuccess())
    return response;
  protocol::ErrorSupport errors;
  const String& json = snapshot->SnapshotCommandLog()->ToJSONString();
  std::vector<uint8_t> cbor;
  if (json.Is8Bit()) {
    crdtp::json::ConvertJSONToCBOR(
        crdtp::span<uint8_t>(json.Characters8(), json.length()), &cbor);
  } else {
    crdtp::json::ConvertJSONToCBOR(
        crdtp::span<uint16_t>(
            reinterpret_cast<const uint16_t*>(json.Characters16()),
            json.length()),
        &cbor);
  }
  auto log_value = protocol::Value::parseBinary(cbor.data(), cbor.size());
  *command_log = protocol::ValueConversions<
      protocol::Array<protocol::DictionaryValue>>::fromValue(log_value.get(),
                                                             &errors);
  auto err = errors.Errors();
  if (err.empty())
    return protocol::Response::Success();
  return protocol::Response::ServerError(std::string(err.begin(), err.end()));
}

}  // namespace blink

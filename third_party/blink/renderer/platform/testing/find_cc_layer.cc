// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/testing/find_cc_layer.h"

#include "cc/layers/layer.h"
#include "cc/layers/scrollbar_layer_base.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blink {

Vector<cc::Layer*> CcLayersByName(cc::Layer* root, const String& name_regex) {
  Vector<cc::Layer*> layers;
  ::testing::Matcher<std::string> matcher(
      ::testing::ContainsRegex(name_regex.Utf8()));
  for (auto& layer : root->children()) {
    if (matcher.Matches(layer->DebugName()))
      layers.push_back(layer.get());
  }
  return layers;
}

Vector<const cc::Layer*> CcLayersByName(const cc::Layer* root,
                                        const String& name_regex) {
  Vector<cc::Layer*> non_const_result =
      CcLayersByName(const_cast<cc::Layer*>(root), name_regex);
  Vector<const cc::Layer*> result(non_const_result.size());
  auto it = non_const_result.begin();
  auto end = non_const_result.end();
  for (unsigned i = 0; it != end; ++it, ++i)
    result[i] = *it;
  return result;
}

Vector<cc::Layer*> CcLayersByDOMElementId(cc::Layer* root,
                                          const String& dom_id) {
  return CcLayersByName(root, String("id='") + dom_id + "'");
}

Vector<const cc::Layer*> CcLayersByDOMElementId(const cc::Layer* root,
                                                const String& dom_id) {
  Vector<cc::Layer*> non_const_result =
      CcLayersByDOMElementId(const_cast<cc::Layer*>(root), dom_id);
  Vector<const cc::Layer*> result(non_const_result.size());
  auto it = non_const_result.begin();
  auto end = non_const_result.end();
  for (unsigned i = 0; it != end; ++it, ++i)
    result[i] = *it;
  return result;
}

cc::Layer* CcLayerByCcElementId(cc::Layer* root,
                                const CompositorElementId& element_id) {
  return root->layer_tree_host()->LayerByElementId(element_id);
}

const cc::Layer* CcLayerByCcElementId(const cc::Layer* root,
                                      const CompositorElementId& element_id) {
  return CcLayerByCcElementId(const_cast<cc::Layer*>(root), element_id);
}

cc::Layer* CcLayerByOwnerNodeId(cc::Layer* root, DOMNodeId id) {
  for (auto& layer : root->children()) {
    if (layer->debug_info() && layer->debug_info()->owner_node_id == id) {
      return layer.get();
    }
  }
  return nullptr;
}

const cc::Layer* CcLayerByOwnerNodeId(const cc::Layer* root, DOMNodeId id) {
  return CcLayerByOwnerNodeId(const_cast<cc::Layer*>(root), id);
}

cc::Layer* ScrollingContentsCcLayerByScrollElementId(
    cc::Layer* root,
    const CompositorElementId& scroll_element_id) {
  const auto& scroll_tree =
      root->layer_tree_host()->property_trees()->scroll_tree();
  for (auto& layer : root->children()) {
    const auto* scroll_node = scroll_tree.Node(layer->scroll_tree_index());
    if (scroll_node && scroll_node->element_id == scroll_element_id &&
        scroll_node->transform_id == layer->transform_tree_index())
      return layer.get();
  }
  return nullptr;
}

const cc::Layer* ScrollingContentsCcLayerByScrollElementId(
    const cc::Layer* root,
    const CompositorElementId& scroll_element_id) {
  return ScrollingContentsCcLayerByScrollElementId(const_cast<cc::Layer*>(root),
                                                   scroll_element_id);
}

cc::ScrollbarLayerBase* ScrollbarLayerForScrollNode(
    cc::Layer* root,
    cc::ScrollNode* scroll_node,
    cc::ScrollbarOrientation orientation) {
  if (!scroll_node)
    return nullptr;
  for (auto& layer : root->children()) {
    if (!layer->IsScrollbarLayerForTesting())
      continue;
    auto* scrollbar_layer = static_cast<cc::ScrollbarLayerBase*>(layer.get());
    if (scrollbar_layer->scroll_element_id() == scroll_node->element_id &&
        scrollbar_layer->orientation() == orientation)
      return scrollbar_layer;
  }
  return nullptr;
}

const cc::ScrollbarLayerBase* ScrollbarLayerForScrollNode(
    const cc::Layer* root,
    const cc::ScrollNode* scroll_node,
    cc::ScrollbarOrientation orientation) {
  return ScrollbarLayerForScrollNode(const_cast<cc::Layer*>(root),
                                     const_cast<cc::ScrollNode*>(scroll_node),
                                     orientation);
}

}  // namespace blink

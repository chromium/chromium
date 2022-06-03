// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_AX_TREE_CONVERTER_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_AX_TREE_CONVERTER_H_

#include <fuchsia/accessibility/semantics/cpp/fidl.h>

#include <unordered_map>

#include "base/containers/flat_map.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "fuchsia_web/webengine/web_engine_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_node.h"

// Maps AXNode IDs to Fuchsia Node IDs.
// This class saves the remapped values.
class WEB_ENGINE_EXPORT NodeIDMapper {
 public:
  NodeIDMapper();
  virtual ~NodeIDMapper();

  // Maps |ax_tree_id| and the signed |ax_node_id| to a unique unsigned
  // |fuchsia_node_id|, with special handling of root IDs. A Fuchsia Node ID of
  // 0 indicates the root, and is always returned if |is_tree_root| is true.
  // No value is returned if this mapper can't produce more unique IDs.
  virtual uint32_t ToFuchsiaNodeID(const ui::AXTreeID& ax_tree_id,
                                   int32_t ax_node_id,
                                   bool is_tree_root);

  // From a Fuchsia Node ID, returns the pair of the AXTreeID and the AXNode ID
  // that maps to it. If the Fuchsia Node ID is not in the map, returns no
  // value.
  virtual absl::optional<std::pair<ui::AXTreeID, int32_t>> ToAXNodeID(
      uint32_t fuchsia_node_id);

  // Updates the  AXNode IDs to point to the new |ax_tree_id|. This method
  // must be called whenever an AXTree changes its AXTreeID, so that stored
  // values here will point to the correct tree.
  // Returns true if the update was applied successfully.
  virtual bool UpdateAXTreeIDForCachedNodeIDs(
      const ui::AXTreeID& old_ax_tree_id,
      const ui::AXTreeID& new_ax_tree_id);

 private:
  // Keeps track of the next unused, unique node ID.
  uint32_t next_fuchsia_id_ = 1;

  // Pair that represents the current root.
  std::pair<ui::AXTreeID, int32_t> root_;

  // Stores the node ID mappings. Note that because converting from AXNode IDs
  // to Fuchsia Node IDs is more common, the map makes access in this
  // direction O(1). The storage representation is chosen here to use a map to
  // hold the AXTreeIDs (which are just a few), but an unordered_map to hold the
  // IDs (which can be thousands).
  base::flat_map<ui::AXTreeID, std::unordered_map<int32_t, uint32_t>> id_map_;
};

// Converts an AXNode to a Fuchsia Semantic Node.
// Both data types represent a single node, and no additional state is needed.
// AXNode is used to convey partial updates, so not all fields may be
// present. Those that are will be converted. The Fuchsia SemanticsManager
// accepts partial updates, so |node| does not require all fields to be set.
WEB_ENGINE_EXPORT fuchsia::accessibility::semantics::Node
AXNodeDataToSemanticNode(const ui::AXNode& ax_node,
                         const ui::AXNode& container_node,
                         const ui::AXTreeID& tree_id,
                         bool is_root,
                         float scale_factor,
                         NodeIDMapper* id_mapper);

// Converts Fuchsia action of type |fuchsia_action| to an ax::mojom::Action of
// type |mojom_action|. Function will return true if |fuchsia_action| is
// supported in Chromium.
bool ConvertAction(fuchsia::accessibility::semantics::Action fuchsia_action,
                   ax::mojom::Action* mojom_action);

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_AX_TREE_CONVERTER_H_

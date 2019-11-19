// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_PLUGIN_AX_TREE_SOURCE_H_
#define CONTENT_PUBLIC_RENDERER_PLUGIN_AX_TREE_SOURCE_H_

#include "ui/accessibility/ax_action_target.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_source.h"

namespace content {

class PluginAXTreeSource : public ui::AXTreeSource<const ui::AXNode*,
                                                   ui::AXNodeData,
                                                   ui::AXTreeData> {
 public:
  virtual std::unique_ptr<ui::AXActionTarget> CreateActionTarget(
      const ui::AXNode& target_node) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_PLUGIN_AX_TREE_SOURCE_H_
// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_PLUGIN_AX_TREE_ACTION_TARGET_ADAPTER_H_
#define CONTENT_PUBLIC_RENDERER_PLUGIN_AX_TREE_ACTION_TARGET_ADAPTER_H_

#include "third_party/blink/public/platform/web_common.h"
#include "ui/accessibility/ax_action_target.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_source.h"

namespace ui {
class AXNode;
}

namespace content {

// Adapts accessibility actions via AXActionTarget for plugin accessibility
// trees (e.g. PDF accessibility).
class PluginAXTreeActionTargetAdapter {
 public:
  virtual std::unique_ptr<ui::AXActionTarget> CreateActionTarget(
      const ui::AXNode& target_node) = 0;
  virtual const ui::AXNode* GetFromId(ui::AXNodeID id) const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_PLUGIN_AX_TREE_ACTION_TARGET_ADAPTER_H_

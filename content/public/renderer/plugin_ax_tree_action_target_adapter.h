// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_PLUGIN_AX_TREE_ACTION_TARGET_ADAPTER_H_
#define CONTENT_PUBLIC_RENDERER_PLUGIN_AX_TREE_ACTION_TARGET_ADAPTER_H_

#include "ui/accessibility/ax_action_target.h"
#include "ui/accessibility/ax_node_id_forward.h"

namespace content {

// Adapts accessibility actions via AXActionTarget for plugin accessibility
// trees (e.g. PDF accessibility).
class PluginAXTreeActionTargetAdapter {
 public:
  virtual std::unique_ptr<ui::AXActionTarget> CreateActionTarget(
      ui::AXNodeID id) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_PLUGIN_AX_TREE_ACTION_TARGET_ADAPTER_H_

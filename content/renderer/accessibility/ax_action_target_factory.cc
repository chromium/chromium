// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/ax_action_target_factory.h"

#include "content/public/renderer/plugin_ax_tree_action_target_adapter.h"
#include "content/renderer/accessibility/blink_ax_action_target.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "ui/accessibility/null_ax_action_target.h"

namespace content {

// static
std::unique_ptr<ui::AXActionTarget> AXActionTargetFactory::CreateFromNodeId(
    const blink::WebDocument& document,
    content::PluginAXTreeActionTargetAdapter* plugin_tree_adapter,
    ui::AXNodeID node_id) {
  blink::WebAXObject blink_target =
      blink::WebAXObject::FromWebDocumentByID(document, node_id);
  if (!blink_target.IsNull())
    return std::make_unique<BlinkAXActionTarget>(blink_target);

  // Plugin tree is not present in only HTML scenario. In case of plugins,
  // it will be nullptr till the time plugin sets the tree source.
  if (plugin_tree_adapter) {
    return plugin_tree_adapter->CreateActionTarget(node_id);
  }
  return std::make_unique<ui::NullAXActionTarget>();
}

}  // namespace content

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_AX_ACTION_TARGET_FACTORY_H_
#define CONTENT_RENDERER_ACCESSIBILITY_AX_ACTION_TARGET_FACTORY_H_

#include <memory>

#include "content/common/content_export.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"

namespace blink {
class WebDocument;
}

namespace ui {
class AXActionTarget;
}

namespace content {
class PluginAXTreeActionTargetAdapter;

class CONTENT_EXPORT AXActionTargetFactory {
 public:
  // Given a node id or an accessibility role, obtain a node from the
  // appropriate tree source and wrap it in an abstraction for dispatching
  // accessibility actions. In the case of a role, find the first node with the
  // given role.
  static std::unique_ptr<ui::AXActionTarget> CreateFromNodeIdOrRole(
      const blink::WebDocument& document,
      content::PluginAXTreeActionTargetAdapter* plugin_tree_adapter,
      ui::AXNodeID node_id,
      ax::mojom::Role role = ax::mojom::Role::kUnknown);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_AX_ACTION_TARGET_FACTORY_H_

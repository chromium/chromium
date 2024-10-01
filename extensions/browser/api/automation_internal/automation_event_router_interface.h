// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_INTERFACE_H_
#define EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_INTERFACE_H_

#include <optional>
#include <set>
#include <vector>

#include "extensions/common/api/automation_internal.h"
#include "extensions/common/extension_id.h"
#include "ui/accessibility/ax_location_and_scroll_updates.h"
#include "ui/accessibility/ax_updates_and_events.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ui {
struct AXActionData;
}  // namespace ui

namespace extensions {

class AutomationEventRouterInterface {
 public:
  virtual void DispatchAccessibilityEvents(
      const ui::AXTreeID& tree_id,
      const std::vector<ui::AXTreeUpdate>& updates,
      const gfx::Point& mouse_location,
      const std::vector<ui::AXEvent>& events) = 0;
  virtual void DispatchAccessibilityLocationChange(
      const ui::AXTreeID& tree_id,
      const ui::AXLocationChange& details) = 0;

  // Notify all automation extensions that an accessibility tree was
  // destroyed. If |browser_context| is null, use the currently active context.
  virtual void DispatchTreeDestroyedEvent(ui::AXTreeID tree_id) = 0;

  // Notify the source extension of the action of an action result.
  virtual void DispatchActionResult(
      const ui::AXActionData& data,
      bool result,
      content::BrowserContext* browser_context = nullptr) = 0;

  // Notify the source extension of the result to getTextLocation.
  // Currently only supported by ARC++ in response to
  // ax::mojom::Action::kGetTextLocation.
  virtual void DispatchGetTextLocationDataResult(
      const ui::AXActionData& data,
      const std::optional<gfx::Rect>& rect) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_INTERFACE_H_

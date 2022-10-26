// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_INTERFACE_H_
#define EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_INTERFACE_H_

#include <set>
#include <vector>

#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/common/api/automation_internal.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_messages.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ui {
struct AXActionData;
}  // namespace ui

struct ExtensionMsg_AccessibilityLocationChangeParams;

namespace extensions {

class AutomationEventRouterInterface {
 public:
  virtual void DispatchAccessibilityEvents(
      const ui::AXTreeID& tree_id,
      std::vector<ui::AXTreeUpdate> updates,
      const gfx::Point& mouse_location,
      std::vector<ui::AXEvent> events) = 0;
  virtual void DispatchAccessibilityLocationChange(
      const ExtensionMsg_AccessibilityLocationChangeParams& params) = 0;

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
      const absl::optional<gfx::Rect>& rect) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_INTERFACE_H_

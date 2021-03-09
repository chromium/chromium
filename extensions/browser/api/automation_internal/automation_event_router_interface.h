// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_INTERFACE_H_
#define EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_INTERFACE_H_

#include <set>
#include <vector>

#include "base/macros.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/common/api/automation_internal.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_messages.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ui {
struct AXActionData;
}  // namespace ui

struct ExtensionMsg_AccessibilityEventBundleParams;
struct ExtensionMsg_AccessibilityLocationChangeParams;

namespace extensions {

// NOTE: This interface is implemented in chromecast/internal by
// ax_tree_source_flutter_unittest.cc
class AutomationEventRouterInterface {
 public:
  virtual void DispatchAccessibilityEvents(
      const ExtensionMsg_AccessibilityEventBundleParams& events) = 0;

  virtual void DispatchAccessibilityLocationChange(
      const ExtensionMsg_AccessibilityLocationChangeParams& params) = 0;

  // Notify all automation extensions that an accessibility tree was
  // destroyed. If |browser_context| is null, use the currently active context.
  virtual void DispatchTreeDestroyedEvent(
      ui::AXTreeID tree_id,
      content::BrowserContext* browser_context) = 0;

  // Notify the source extension of the action of an action result.
  virtual void DispatchActionResult(
      const ui::AXActionData& data,
      bool result,
      content::BrowserContext* browser_context = nullptr) = 0;

  // Notify the source extension of the result to getTextLocation.
  virtual void DispatchGetTextLocationDataResult(
      const ui::AXActionData& data,
      const base::Optional<gfx::Rect>& rect) {}

  AutomationEventRouterInterface() {}
  virtual ~AutomationEventRouterInterface() {}

  DISALLOW_COPY_AND_ASSIGN(AutomationEventRouterInterface);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_INTERFACE_H_

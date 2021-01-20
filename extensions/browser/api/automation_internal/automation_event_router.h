// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_H_
#define EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_H_

#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/scoped_observer.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "extensions/browser/api/automation_internal/automation_event_router_interface.h"
#include "extensions/common/api/automation_internal.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_messages.h"
#include "ui/accessibility/ax_event_bundle_sink.h"
#include "ui/accessibility/ax_tree_id.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ui {
struct AXActionData;
}  // namespace ui

struct ExtensionMsg_AccessibilityEventBundleParams;
struct ExtensionMsg_AccessibilityLocationChangeParams;

namespace extensions {
struct AutomationListener;

class AutomationEventRouter : public ui::AXEventBundleSink,
                              public content::RenderProcessHostObserver,
                              public AutomationEventRouterInterface {
 public:
  static AutomationEventRouter* GetInstance();

  // Indicates that the listener at |listener_process_id| wants to receive
  // automation events from the accessibility tree indicated by
  // |source_ax_tree_id|. Automation events are forwarded from now on until the
  // listener process dies.
  void RegisterListenerForOneTree(const ExtensionId& extension_id,
                                  int listener_process_id,
                                  ui::AXTreeID source_ax_tree_id);

  // Indicates that the listener at |listener_process_id| wants to receive
  // automation events from all accessibility trees because it has Desktop
  // permission.
  void RegisterListenerWithDesktopPermission(const ExtensionId& extension_id,
                                             int listener_process_id);

  void DispatchAccessibilityEvents(
      const ExtensionMsg_AccessibilityEventBundleParams& events) override;

  void DispatchAccessibilityLocationChange(
      const ExtensionMsg_AccessibilityLocationChangeParams& params) override;

  // Notify all automation extensions that an accessibility tree was
  // destroyed. If |browser_context| is null, use the currently active context.
  void DispatchTreeDestroyedEvent(
      ui::AXTreeID tree_id,
      content::BrowserContext* browser_context) override;

  // Notify the source extension of the action of an action result.
  void DispatchActionResult(
      const ui::AXActionData& data,
      bool result,
      content::BrowserContext* browser_context = nullptr) override;

  // Notify the source extension of the result to getTextLocation.
  void DispatchGetTextLocationDataResult(
      const ui::AXActionData& data,
      const base::Optional<gfx::Rect>& rect) override;

 private:
  struct AutomationListener {
    AutomationListener();
    AutomationListener(const AutomationListener& other);
    ~AutomationListener();

    ExtensionId extension_id;
    int process_id;
    bool desktop;
    std::set<ui::AXTreeID> tree_ids;
    bool is_active_context;
  };

  AutomationEventRouter();
  ~AutomationEventRouter() override;

  void Register(const ExtensionId& extension_id,
                int listener_process_id,
                ui::AXTreeID source_ax_tree_id,
                bool desktop);

  // ui::AXEventBundleSink:
  void DispatchAccessibilityEvents(const ui::AXTreeID& tree_id,
                                   std::vector<ui::AXTreeUpdate> updates,
                                   const gfx::Point& mouse_location,
                                   std::vector<ui::AXEvent> events) override;

  // RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // Called when the user switches profiles or when a listener is added
  // or removed. The purpose is to ensure that multiple instances of the
  // same extension running in different profiles don't interfere with one
  // another, so in that case only the one associated with the active profile
  // is marked as active.
  //
  // This is needed on Chrome OS because ChromeVox loads into the login profile
  // in addition to the active profile.  If a similar fix is needed on other
  // platforms, we'd need an equivalent of SessionStateObserver that works
  // everywhere.
  void UpdateActiveProfile();

  content::NotificationRegistrar registrar_;
  std::vector<AutomationListener> listeners_;

  content::BrowserContext* active_context_;

  ScopedObserver<content::RenderProcessHost, content::RenderProcessHostObserver>
      rph_observers_{this};

  friend struct base::DefaultSingletonTraits<AutomationEventRouter>;

  DISALLOW_COPY_AND_ASSIGN(AutomationEventRouter);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_H_

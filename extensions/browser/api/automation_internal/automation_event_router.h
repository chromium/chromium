// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_H_
#define EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_H_

#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/api/automation_internal/automation_event_router_interface.h"
#include "extensions/common/api/automation_internal.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_messages.h"
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
struct WorkerId;

using RenderProcessHostId = int;

class AutomationEventRouterObserver {
 public:
  virtual void AllAutomationExtensionsGone() = 0;
  virtual void ExtensionListenerAdded() = 0;
};

class AutomationEventRouter : public content::RenderProcessHostObserver,
                              public AutomationEventRouterInterface,
                              public ui::AXActionHandlerObserver {
 public:
  static AutomationEventRouter* GetInstance();

  // Indicates that the listener at |listener_rph_id| wants to receive
  // automation events from the accessibility tree indicated by
  // |source_ax_tree_id|. Automation events are forwarded from now on until the
  // listener process dies.
  void RegisterListenerForOneTree(const ExtensionId& extension_id,
                                  const RenderProcessHostId& listener_rph_id,
                                  content::WebContents* web_contents,
                                  ui::AXTreeID source_ax_tree_id);

  // Indicates that the listener at |listener_rph_id| wants to receive
  // automation events from all accessibility trees because it has Desktop
  // permission.
  void RegisterListenerWithDesktopPermission(
      const ExtensionId& extension_id,
      const RenderProcessHostId& listener_rph_id,
      content::WebContents* web_contents);

  // Undoes the Register call above. May result in disabling of automation.
  void UnregisterListenerWithDesktopPermission(
      const RenderProcessHostId& listener_rph_id);

  // Like the above function, but for all listeners. Definitely results in
  // disabling of automation.
  void UnregisterAllListenersWithDesktopPermission();

  // The following two methods should only be called by Lacros.
  void NotifyAllAutomationExtensionsGone();
  void NotifyExtensionListenerAdded();

  void AddObserver(AutomationEventRouterObserver* observer);
  void RemoveObserver(AutomationEventRouterObserver* observer);
  bool HasObserver(AutomationEventRouterObserver* observer);

  // AutomationEventRouterInterface:
  void DispatchAccessibilityEvents(const ui::AXTreeID& tree_id,
                                   std::vector<ui::AXTreeUpdate> updates,
                                   const gfx::Point& mouse_location,
                                   std::vector<ui::AXEvent> events) override;
  void DispatchAccessibilityLocationChange(
      const ExtensionMsg_AccessibilityLocationChangeParams& params) override;
  void DispatchTreeDestroyedEvent(ui::AXTreeID tree_id) override;
  void DispatchActionResult(
      const ui::AXActionData& data,
      bool result,
      content::BrowserContext* browser_context = nullptr) override;
  void DispatchGetTextLocationDataResult(
      const ui::AXActionData& data,
      const absl::optional<gfx::Rect>& rect) override;

  // If a remote router is registered, then all events are directly forwarded to
  // it. The caller of this method is responsible for calling it again with
  // |nullptr| before the remote router is destroyed to prevent UaF.
  void RegisterRemoteRouter(AutomationEventRouterInterface* router);

 private:
  class AutomationListener : public content::WebContentsObserver {
   public:
    explicit AutomationListener(content::WebContents* web_contents);
    AutomationListener(const AutomationListener& other) = delete;
    AutomationListener& operator=(const AutomationListener&) = delete;
    ~AutomationListener() override;

    // content:WebContentsObserver:
    void PrimaryPageChanged(content::Page& page) override;

    raw_ptr<AutomationEventRouter> router;
    ExtensionId extension_id;
    RenderProcessHostId render_process_host_id;
    bool desktop;
    std::set<ui::AXTreeID> tree_ids;
    bool is_active_context;
  };

  AutomationEventRouter();

  AutomationEventRouter(const AutomationEventRouter&) = delete;
  AutomationEventRouter& operator=(const AutomationEventRouter&) = delete;

  ~AutomationEventRouter() override;

  void Register(const ExtensionId& extension_id,
                const RenderProcessHostId& listener_rph_id,
                content::WebContents* web_contents,
                ui::AXTreeID source_ax_tree_id,
                bool desktop);

  void DispatchAccessibilityEventsInternal(
      const ExtensionMsg_AccessibilityEventBundleParams& events);

  // RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // ui::AXActionHandlerObserver:
  void TreeRemoved(ui::AXTreeID ax_tree_id) override;

  void RemoveAutomationListener(content::RenderProcessHost* host);

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
  std::vector<std::unique_ptr<AutomationListener>> listeners_;

  std::map<WorkerId, std::string> keepalive_request_uuid_for_worker_;

  raw_ptr<content::BrowserContext> active_context_;

  // The caller of RegisterRemoteRouter is responsible for ensuring that this
  // pointer is valid. The remote router must be unregistered with
  // RegisterRemoteRouter(nullptr) before it is destroyed.
  raw_ptr<AutomationEventRouterInterface> remote_router_ = nullptr;

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      rph_observers_{this};

  base::ObserverList<AutomationEventRouterObserver>::Unchecked observers_;

  friend struct base::DefaultSingletonTraits<AutomationEventRouter>;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_H_

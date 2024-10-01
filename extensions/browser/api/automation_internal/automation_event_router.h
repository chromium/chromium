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
#include "base/uuid.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/api/automation_internal/automation_event_router_interface.h"
#include "extensions/common/api/automation_internal.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/automation_registry.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/accessibility/public/mojom/automation.mojom.h"
#include "ui/accessibility/ax_location_and_scroll_updates.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_updates_and_events.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ui {
struct AXActionData;
}  // namespace ui

namespace extensions {
struct AutomationListener;
struct WorkerId;

class AutomationEventRouterObserver {
 public:
  virtual void AllAutomationExtensionsGone() = 0;
  virtual void ExtensionListenerAdded() = 0;
};

// Routes accessibility events from the browser process to the extension's
// renderer process.
class AutomationEventRouter
    : public content::RenderProcessHostObserver,
      public AutomationEventRouterInterface,
      public ui::AXActionHandlerObserver,
      public extensions::mojom::RendererAutomationRegistry {
 public:
  using RenderProcessHostId = int;

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
  void DispatchAccessibilityEvents(
      const ui::AXTreeID& tree_id,
      const std::vector<ui::AXTreeUpdate>& updates,
      const gfx::Point& mouse_location,
      const std::vector<ui::AXEvent>& events) override;
  void DispatchAccessibilityLocationChange(
      const ui::AXTreeID& tree_id,
      const ui::AXLocationChange& details) override;
  void DispatchTreeDestroyedEvent(ui::AXTreeID tree_id) override;
  void DispatchActionResult(
      const ui::AXActionData& data,
      bool result,
      content::BrowserContext* browser_context = nullptr) override;
  void DispatchGetTextLocationDataResult(
      const ui::AXActionData& data,
      const std::optional<gfx::Rect>& rect) override;

  // If a remote router is registered, then all events are directly forwarded to
  // it. The caller of this method is responsible for calling it again with
  // |nullptr| before the remote router is destroyed to prevent UaF.
  void RegisterRemoteRouter(AutomationEventRouterInterface* router);

  static void BindForRenderer(
      RenderProcessHostId render_process_id,
      mojo::PendingAssociatedReceiver<
          extensions::mojom::RendererAutomationRegistry> receiver);

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

  // RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // ui::AXActionHandlerObserver:
  void TreeRemoved(ui::AXTreeID ax_tree_id) override;

  void RemoveAutomationListener(content::RenderProcessHost* host);

  // Returns the listener for the provided ID, or `nullptr` if none is found.
  AutomationListener* GetListenerByRenderProcessID(
      const RenderProcessHostId& listener_rph_id) const;

  // ax::mojom::AutomationClient:
  void BindAutomation(
      mojo::PendingAssociatedRemote<ax::mojom::Automation> automation) override;

  std::vector<std::unique_ptr<AutomationListener>> listeners_;

  std::map<WorkerId, base::Uuid> keepalive_request_uuid_for_worker_;

  // The caller of RegisterRemoteRouter is responsible for ensuring that this
  // pointer is valid. The remote router must be unregistered with
  // RegisterRemoteRouter(nullptr) before it is destroyed.
  raw_ptr<AutomationEventRouterInterface> remote_router_ = nullptr;

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      rph_observers_{this};

  base::ObserverList<AutomationEventRouterObserver>::Unchecked observers_;

  mojo::AssociatedReceiverSet<extensions::mojom::RendererAutomationRegistry,
                              RenderProcessHostId>
      receivers_;

  mojo::AssociatedRemoteSet<ax::mojom::Automation> automation_remote_set_;

  base::WeakPtrFactory<AutomationEventRouter> weak_ptr_factory_{this};
  friend struct base::DefaultSingletonTraits<AutomationEventRouter>;
  friend class AutomationEventRouterExtensionBrowserTest;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_H_

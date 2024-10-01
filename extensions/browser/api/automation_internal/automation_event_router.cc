// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/automation_internal/automation_event_router.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/browser/api/automation_internal/automation_internal_api_delegate.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/api/automation_internal.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"

namespace extensions {

// static
AutomationEventRouter* AutomationEventRouter::GetInstance() {
  return base::Singleton<
      AutomationEventRouter,
      base::LeakySingletonTraits<AutomationEventRouter>>::get();
}

AutomationEventRouter::AutomationEventRouter() {
#if defined(USE_AURA)
  // Not reset because |this| is leaked.
  ExtensionsAPIClient::Get()
      ->GetAutomationInternalApiDelegate()
      ->SetAutomationEventRouterInterface(this);
#endif

  ui::AXActionHandlerRegistry::GetInstance()->AddObserver(this);
}

AutomationEventRouter::~AutomationEventRouter() {
  CHECK(!remote_router_);
}

void AutomationEventRouter::RegisterListenerForOneTree(
    const ExtensionId& extension_id,
    const RenderProcessHostId& listener_rph_id,
    content::WebContents* web_contents,
    ui::AXTreeID source_ax_tree_id) {
  Register(extension_id, listener_rph_id, web_contents, source_ax_tree_id,
           /*desktop=*/false);
}

void AutomationEventRouter::RegisterListenerWithDesktopPermission(
    const ExtensionId& extension_id,
    const RenderProcessHostId& listener_rph_id,
    content::WebContents* web_contents) {
  Register(extension_id, listener_rph_id, web_contents, ui::AXTreeIDUnknown(),
           /*desktop=*/true);
}

void AutomationEventRouter::UnregisterListenerWithDesktopPermission(
    const RenderProcessHostId& listener_rph_id) {
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(listener_rph_id);
  if (host)
    RemoveAutomationListener(host);
}

void AutomationEventRouter::UnregisterAllListenersWithDesktopPermission() {
  for (const auto& request_pair : keepalive_request_uuid_for_worker_) {
    const WorkerId& worker_id = request_pair.first;
    const base::Uuid& request_uuid = request_pair.second;
    content::RenderProcessHost* host =
        content::RenderProcessHost::FromID(worker_id.render_process_id);

    if (rph_observers_.IsObservingSource(host))
      rph_observers_.RemoveObservation(host);

    ProcessManager* process_manager =
        ProcessManager::Get(host->GetBrowserContext());
    DCHECK(process_manager);

    process_manager->DecrementServiceWorkerKeepaliveCount(
        worker_id, request_uuid, Activity::ACCESSIBILITY, std::string());
  }
  keepalive_request_uuid_for_worker_.clear();
}

void AutomationEventRouter::DispatchAccessibilityLocationChange(
    const ui::AXTreeID& tree_id,
    const ui::AXLocationChange& details) {
  if (remote_router_) {
    remote_router_->DispatchAccessibilityLocationChange(tree_id, details);
    return;
  }

  for (const auto& remote : automation_remote_set_) {
    remote->DispatchAccessibilityLocationChange(tree_id, details.id,
                                                details.new_location);
  }
}

void AutomationEventRouter::DispatchTreeDestroyedEvent(ui::AXTreeID tree_id) {
  if (remote_router_) {
    remote_router_->DispatchTreeDestroyedEvent(tree_id);
    return;
  }

  for (const auto& remote : automation_remote_set_) {
    remote->DispatchTreeDestroyedEvent(tree_id);
  }
}

void AutomationEventRouter::DispatchActionResult(
    const ui::AXActionData& data,
    bool result,
    content::BrowserContext* browser_context) {
  CHECK(!data.source_extension_id.empty());

  for (const auto& remote : automation_remote_set_) {
    remote->DispatchActionResult(data, result);
  }
}

void AutomationEventRouter::DispatchGetTextLocationDataResult(
    const ui::AXActionData& data,
    const std::optional<gfx::Rect>& rect) {
#if BUILDFLAG(IS_CHROMEOS)
  CHECK(!data.source_extension_id.empty());

  for (const auto& remote : automation_remote_set_) {
    remote->DispatchGetTextLocationResult(data, rect);
  }
#else
  NOTREACHED();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void AutomationEventRouter::NotifyAllAutomationExtensionsGone() {
  for (AutomationEventRouterObserver& observer : observers_)
    observer.AllAutomationExtensionsGone();
}

void AutomationEventRouter::NotifyExtensionListenerAdded() {
  for (AutomationEventRouterObserver& observer : observers_)
    observer.ExtensionListenerAdded();
}

void AutomationEventRouter::AddObserver(
    AutomationEventRouterObserver* observer) {
  observers_.AddObserver(observer);
}

void AutomationEventRouter::RemoveObserver(
    AutomationEventRouterObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool AutomationEventRouter::HasObserver(
    AutomationEventRouterObserver* observer) {
  return observers_.HasObserver(observer);
}

void AutomationEventRouter::RegisterRemoteRouter(
    AutomationEventRouterInterface* router) {
  // There can be at most 1 remote router. So either this method is setting the
  // remote router, or it's unsetting the remote router.
  CHECK(!router || !remote_router_);
  remote_router_ = router;
}

AutomationEventRouter::AutomationListener::AutomationListener(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

AutomationEventRouter::AutomationListener::~AutomationListener() = default;

void AutomationEventRouter::AutomationListener::PrimaryPageChanged(
    content::Page& page) {
  router->RemoveAutomationListener(
      content::RenderProcessHost::FromID(render_process_host_id));
}

void AutomationEventRouter::Register(const ExtensionId& extension_id,
                                     const RenderProcessHostId& listener_rph_id,
                                     content::WebContents* web_contents,
                                     ui::AXTreeID ax_tree_id,
                                     bool desktop) {
  DCHECK(desktop || ax_tree_id != ui::AXTreeIDUnknown());

  AutomationListener* listener = GetListenerByRenderProcessID(listener_rph_id);

  // We have an entry with that process so update the set of tree ids it wants
  // to listen to, and update its desktop permission.
  if (listener) {
    if (desktop) {
      listener->desktop = true;
    } else {
      listener->tree_ids.insert(ax_tree_id);
    }

    return;
  }

  // Add a new entry if we don't have one with that process.
  auto new_listener = std::make_unique<AutomationListener>(web_contents);
  new_listener->router = this;
  new_listener->extension_id = extension_id;
  new_listener->render_process_host_id = listener_rph_id;
  new_listener->desktop = desktop;
  if (!desktop) {
    new_listener->tree_ids.insert(ax_tree_id);
  }

  listeners_.emplace_back(std::move(new_listener));

  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(listener_rph_id);
  rph_observers_.AddObservation(host);
  for (AutomationEventRouterObserver& observer : observers_)
    observer.ExtensionListenerAdded();

  if (!desktop)
    return;

  ProcessManager* process_manager =
      ProcessManager::Get(host->GetBrowserContext());
  DCHECK(process_manager);

  std::vector<WorkerId> all_worker_ids =
      process_manager->GetServiceWorkersForExtension(extension_id);
  for (const WorkerId& worker_id : all_worker_ids) {
    if (worker_id.render_process_id != listener_rph_id) {
      continue;
    }

    keepalive_request_uuid_for_worker_[worker_id] =
        process_manager->IncrementServiceWorkerKeepaliveCount(
            worker_id,
            content::ServiceWorkerExternalRequestTimeoutType::kDoesNotTimeout,
            Activity::ACCESSIBILITY, std::string());
  }
}

void AutomationEventRouter::DispatchAccessibilityEvents(
    const ui::AXTreeID& tree_id,
    const std::vector<ui::AXTreeUpdate>& updates,
    const gfx::Point& mouse_location,
    const std::vector<ui::AXEvent>& events) {
  if (remote_router_) {
    remote_router_->DispatchAccessibilityEvents(
        tree_id, std::move(updates), mouse_location, std::move(events));
    return;
  }

  for (const auto& remote : automation_remote_set_) {
    remote->DispatchAccessibilityEvents(tree_id, updates, mouse_location,
                                        events);
  }
}

void AutomationEventRouter::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  RemoveAutomationListener(host);
}

void AutomationEventRouter::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  RemoveAutomationListener(host);
}

void AutomationEventRouter::RemoveAutomationListener(
    content::RenderProcessHost* host) {
  RenderProcessHostId rph_id = host->GetID();
  ExtensionId extension_id;
  for (auto listener = listeners_.begin(); listener != listeners_.end();) {
    if ((*listener)->render_process_host_id == rph_id) {
      // Copy the extension ID, as we're about to erase the source.
      extension_id = (*listener)->extension_id;
      listener = listeners_.erase(listener);
    } else {
      listener++;
    }
  }

  if (rph_observers_.IsObservingSource(host))
    rph_observers_.RemoveObservation(host);

  if (!rph_observers_.IsObservingAnySource()) {
    for (AutomationEventRouterObserver& observer : observers_)
      observer.AllAutomationExtensionsGone();
  }

  auto* process_manager = ProcessManager::Get(host->GetBrowserContext());
  DCHECK(process_manager);

  std::vector<WorkerId> all_worker_ids =
      process_manager->GetServiceWorkersForExtension(extension_id);

  for (const WorkerId& worker_id : all_worker_ids) {
    if (worker_id.render_process_id != rph_id) {
      continue;
    }
    const auto& request_uuid_iter =
        keepalive_request_uuid_for_worker_.find(worker_id);
    if (request_uuid_iter == keepalive_request_uuid_for_worker_.end())
      continue;

    base::Uuid request_uuid = std::move(request_uuid_iter->second);
    keepalive_request_uuid_for_worker_.erase(worker_id);

    process_manager->DecrementServiceWorkerKeepaliveCount(
        worker_id, request_uuid, Activity::ACCESSIBILITY, std::string());
  }
}

void AutomationEventRouter::TreeRemoved(ui::AXTreeID ax_tree_id) {
  DispatchTreeDestroyedEvent(ax_tree_id);
}

AutomationEventRouter::AutomationListener*
AutomationEventRouter::GetListenerByRenderProcessID(
    const RenderProcessHostId& listener_rph_id) const {
  const auto iter = base::ranges::find(
      listeners_, listener_rph_id, &AutomationListener::render_process_host_id);

  if (iter != listeners_.end()) {
    return iter->get();
  }
  return nullptr;
}

void AutomationEventRouter::BindAutomation(
    mojo::PendingAssociatedRemote<ax::mojom::Automation> automation) {
  automation_remote_set_.Add(std::move(automation));
}

// static
void AutomationEventRouter::BindForRenderer(
    RenderProcessHostId render_process_id,
    mojo::PendingAssociatedReceiver<
        extensions::mojom::RendererAutomationRegistry> receiver) {
  AutomationEventRouter* router = AutomationEventRouter::GetInstance();
  CHECK(router);

  router->receivers_.Add(router, std::move(receiver), render_process_id);
}

}  // namespace extensions

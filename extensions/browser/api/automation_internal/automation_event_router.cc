// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/automation_internal/automation_event_router.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/cxx20_erase.h"
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
#include "extensions/common/extension_messages.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"

namespace extensions {

// static
AutomationEventRouter* AutomationEventRouter::GetInstance() {
  return base::Singleton<
      AutomationEventRouter,
      base::LeakySingletonTraits<AutomationEventRouter>>::get();
}

AutomationEventRouter::AutomationEventRouter()
    : active_context_(ExtensionsAPIClient::Get()
                          ->GetAutomationInternalApiDelegate()
                          ->GetActiveUserContext()) {
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
           false);
}

void AutomationEventRouter::RegisterListenerWithDesktopPermission(
    const ExtensionId& extension_id,
    const RenderProcessHostId& listener_rph_id,
    content::WebContents* web_contents) {
  Register(extension_id, listener_rph_id, web_contents, ui::AXTreeIDUnknown(),
           true);
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
        worker_id, request_uuid, extensions::Activity::ACCESSIBILITY,
        std::string());
  }
  keepalive_request_uuid_for_worker_.clear();

  UpdateActiveProfile();
}

void AutomationEventRouter::DispatchAccessibilityEventsInternal(
    const ExtensionMsg_AccessibilityEventBundleParams& event_bundle) {
  content::BrowserContext* active_context =
      ExtensionsAPIClient::Get()
          ->GetAutomationInternalApiDelegate()
          ->GetActiveUserContext();
  if (active_context_ != active_context) {
    active_context_ = active_context;
    UpdateActiveProfile();
  }

  for (const auto& listener : listeners_) {
    // Skip listeners that don't want to listen to this tree.
    if (!listener->desktop && listener->tree_ids.find(event_bundle.tree_id) ==
                                  listener->tree_ids.end()) {
      continue;
    }

    content::RenderProcessHost* rph =
        content::RenderProcessHost::FromID(listener->render_process_host_id);
    rph->Send(new ExtensionMsg_AccessibilityEventBundle(
        event_bundle, listener->is_active_context));
  }
}

void AutomationEventRouter::DispatchAccessibilityLocationChange(
    const ExtensionMsg_AccessibilityLocationChangeParams& params) {
  if (remote_router_) {
    remote_router_->DispatchAccessibilityLocationChange(params);
    return;
  }

  for (const auto& listener : listeners_) {
    // Skip listeners that don't want to listen to this tree.
    if (!listener->desktop &&
        listener->tree_ids.find(params.tree_id) == listener->tree_ids.end()) {
      continue;
    }

    content::RenderProcessHost* rph =
        content::RenderProcessHost::FromID(listener->render_process_host_id);
    rph->Send(new ExtensionMsg_AccessibilityLocationChange(params));
  }
}

void AutomationEventRouter::DispatchTreeDestroyedEvent(ui::AXTreeID tree_id) {
  if (remote_router_) {
    remote_router_->DispatchTreeDestroyedEvent(tree_id);
    return;
  }

  if (listeners_.empty())
    return;

  auto args(api::automation_internal::OnAccessibilityTreeDestroyed::Create(
      tree_id.ToString()));
  auto event = std::make_unique<Event>(
      events::AUTOMATION_INTERNAL_ON_ACCESSIBILITY_TREE_DESTROYED,
      api::automation_internal::OnAccessibilityTreeDestroyed::kEventName,
      std::move(args), active_context_.get());
  EventRouter::Get(active_context_.get())->BroadcastEvent(std::move(event));
}

void AutomationEventRouter::DispatchActionResult(
    const ui::AXActionData& data,
    bool result,
    content::BrowserContext* browser_context) {
  CHECK(!data.source_extension_id.empty());

  browser_context = browser_context ? browser_context : active_context_.get();
  if (listeners_.empty())
    return;

  auto args(api::automation_internal::OnActionResult::Create(
      data.target_tree_id.ToString(), data.request_id, result));
  auto event = std::make_unique<Event>(
      events::AUTOMATION_INTERNAL_ON_ACTION_RESULT,
      api::automation_internal::OnActionResult::kEventName, std::move(args),
      browser_context);
  EventRouter::Get(browser_context)
      ->DispatchEventToExtension(data.source_extension_id, std::move(event));
}

void AutomationEventRouter::DispatchGetTextLocationDataResult(
    const ui::AXActionData& data,
    const absl::optional<gfx::Rect>& rect) {
  CHECK(!data.source_extension_id.empty());

  if (listeners_.empty())
    return;
  extensions::api::automation_internal::AXTextLocationParams params;
  params.tree_id = data.target_tree_id.ToString();
  params.node_id = data.target_node_id;
  params.result = false;
  if (rect) {
    params.left = rect.value().x();
    params.top = rect.value().y();
    params.width = rect.value().width();
    params.height = rect.value().height();
    params.result = true;
  }
  params.request_id = data.request_id;

  auto args(api::automation_internal::OnGetTextLocationResult::Create(params));
  auto event = std::make_unique<Event>(
      events::AUTOMATION_INTERNAL_ON_GET_TEXT_LOCATION_RESULT,
      api::automation_internal::OnGetTextLocationResult::kEventName,
      std::move(args), active_context_);
  EventRouter::Get(active_context_)
      ->DispatchEventToExtension(data.source_extension_id, std::move(event));
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

  const auto& iter = base::ranges::find(
      listeners_, listener_rph_id, &AutomationListener::render_process_host_id);

  // We have an entry with that process so update the set of tree ids it wants
  // to listen to, and update its desktop permission.
  if (iter != listeners_.end()) {
    if (desktop)
      iter->get()->desktop = true;
    else
      iter->get()->tree_ids.insert(ax_tree_id);
    return;
  }

  // Add a new entry if we don't have one with that process.
  auto listener = std::make_unique<AutomationListener>(web_contents);
  listener->router = this;
  listener->extension_id = extension_id;
  listener->render_process_host_id = listener_rph_id;
  listener->desktop = desktop;
  if (!desktop)
    listener->tree_ids.insert(ax_tree_id);
  listeners_.emplace_back(std::move(listener));
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(listener_rph_id);
  rph_observers_.AddObservation(host);
  UpdateActiveProfile();
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
            extensions::Activity::ACCESSIBILITY, std::string());
  }
}

void AutomationEventRouter::DispatchAccessibilityEvents(
    const ui::AXTreeID& tree_id,
    std::vector<ui::AXTreeUpdate> updates,
    const gfx::Point& mouse_location,
    std::vector<ui::AXEvent> events) {
  if (remote_router_) {
    remote_router_->DispatchAccessibilityEvents(
        tree_id, std::move(updates), mouse_location, std::move(events));
    return;
  }

  ExtensionMsg_AccessibilityEventBundleParams event_bundle;
  event_bundle.tree_id = tree_id;
  event_bundle.updates = std::move(updates);
  event_bundle.mouse_location = mouse_location;
  event_bundle.events = std::move(events);

  DispatchAccessibilityEventsInternal(event_bundle);
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
  UpdateActiveProfile();

  if (rph_observers_.GetSourcesCount() == 0) {
    for (AutomationEventRouterObserver& observer : observers_)
      observer.AllAutomationExtensionsGone();
  }

  extensions::ProcessManager* process_manager =
      ProcessManager::Get(host->GetBrowserContext());
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
        worker_id, request_uuid, extensions::Activity::ACCESSIBILITY,
        std::string());
  }
}

void AutomationEventRouter::TreeRemoved(ui::AXTreeID ax_tree_id) {
  DispatchTreeDestroyedEvent(ax_tree_id);
}

void AutomationEventRouter::UpdateActiveProfile() {
  for (auto& listener : listeners_) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    int extension_id_count = 0;
    for (const auto& listener2 : listeners_) {
      if (listener2->extension_id == listener->extension_id)
        extension_id_count++;
    }
    content::RenderProcessHost* rph =
        content::RenderProcessHost::FromID(listener->render_process_host_id);

    // The purpose of is_active_context is to ensure different instances of
    // the same extension running in different profiles don't interfere with
    // one another. If an automation extension is only running in one profile,
    // always mark it as active. If it's running in two or more profiles,
    // only mark one as active.
    listener->is_active_context = (extension_id_count == 1 ||
                                   rph->GetBrowserContext() == active_context_);
#else
    listener->is_active_context = true;
#endif
  }
}

}  // namespace extensions

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/automation_internal/automation_event_router.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/stl_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "extensions/browser/api/automation_internal/automation_internal_api_delegate.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/automation_internal.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
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
      ->SetEventBundleSink(this);
#endif
}

AutomationEventRouter::~AutomationEventRouter() {}

void AutomationEventRouter::RegisterListenerForOneTree(
    const ExtensionId& extension_id,
    int listener_process_id,
    ui::AXTreeID source_ax_tree_id) {
  Register(extension_id, listener_process_id, source_ax_tree_id, false);
}

void AutomationEventRouter::RegisterListenerWithDesktopPermission(
    const ExtensionId& extension_id,
    int listener_process_id) {
  Register(extension_id, listener_process_id, ui::AXTreeIDUnknown(), true);
}

void AutomationEventRouter::DispatchAccessibilityEvents(
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
    if (!listener.desktop && listener.tree_ids.find(event_bundle.tree_id) ==
                                 listener.tree_ids.end()) {
      continue;
    }

    content::RenderProcessHost* rph =
        content::RenderProcessHost::FromID(listener.process_id);
    rph->Send(new ExtensionMsg_AccessibilityEventBundle(
        event_bundle, listener.is_active_context));
  }
}

void AutomationEventRouter::DispatchAccessibilityLocationChange(
    const ExtensionMsg_AccessibilityLocationChangeParams& params) {
  for (const auto& listener : listeners_) {
    // Skip listeners that don't want to listen to this tree.
    if (!listener.desktop &&
        listener.tree_ids.find(params.tree_id) == listener.tree_ids.end()) {
      continue;
    }

    content::RenderProcessHost* rph =
        content::RenderProcessHost::FromID(listener.process_id);
    rph->Send(new ExtensionMsg_AccessibilityLocationChange(params));
  }
}

void AutomationEventRouter::DispatchTreeDestroyedEvent(
    ui::AXTreeID tree_id,
    content::BrowserContext* browser_context) {
  if (listeners_.empty())
    return;

  browser_context = browser_context ? browser_context : active_context_;
  std::unique_ptr<base::ListValue> args(
      api::automation_internal::OnAccessibilityTreeDestroyed::Create(
          tree_id.ToString()));
  auto event = std::make_unique<Event>(
      events::AUTOMATION_INTERNAL_ON_ACCESSIBILITY_TREE_DESTROYED,
      api::automation_internal::OnAccessibilityTreeDestroyed::kEventName,
      std::move(args), browser_context);
  EventRouter::Get(browser_context)->BroadcastEvent(std::move(event));
}

void AutomationEventRouter::DispatchActionResult(
    const ui::AXActionData& data,
    bool result,
    content::BrowserContext* browser_context) {
  CHECK(!data.source_extension_id.empty());

  browser_context = browser_context ? browser_context : active_context_;
  if (listeners_.empty())
    return;

  std::unique_ptr<base::ListValue> args(
      api::automation_internal::OnActionResult::Create(
          data.target_tree_id.ToString(), data.request_id, result));
  auto event = std::make_unique<Event>(
      events::AUTOMATION_INTERNAL_ON_ACTION_RESULT,
      api::automation_internal::OnActionResult::kEventName, std::move(args),
      active_context_);
  EventRouter::Get(active_context_)
      ->DispatchEventToExtension(data.source_extension_id, std::move(event));
}

void AutomationEventRouter::DispatchGetTextLocationDataResult(
    const ui::AXActionData& data,
    const base::Optional<gfx::Rect>& rect) {
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

  std::unique_ptr<base::ListValue> args(
      api::automation_internal::OnGetTextLocationResult::Create(params));
  auto event = std::make_unique<Event>(
      events::AUTOMATION_INTERNAL_ON_GET_TEXT_LOCATION_RESULT,
      api::automation_internal::OnGetTextLocationResult::kEventName,
      std::move(args), active_context_);
  EventRouter::Get(active_context_)
      ->DispatchEventToExtension(data.source_extension_id, std::move(event));
}

AutomationEventRouter::AutomationListener::AutomationListener() {}

AutomationEventRouter::AutomationListener::AutomationListener(
    const AutomationListener& other) = default;

AutomationEventRouter::AutomationListener::~AutomationListener() {}

void AutomationEventRouter::Register(const ExtensionId& extension_id,
                                     int listener_process_id,
                                     ui::AXTreeID ax_tree_id,
                                     bool desktop) {
  DCHECK(desktop || ax_tree_id != ui::AXTreeIDUnknown());
  auto iter =
      std::find_if(listeners_.begin(), listeners_.end(),
                   [listener_process_id](const AutomationListener& item) {
                     return item.process_id == listener_process_id;
                   });

  // Add a new entry if we don't have one with that process.
  if (iter == listeners_.end()) {
    AutomationListener listener;
    listener.extension_id = extension_id;
    listener.process_id = listener_process_id;
    listener.desktop = desktop;
    if (!desktop)
      listener.tree_ids.insert(ax_tree_id);
    listeners_.push_back(listener);
    rph_observers_.Add(content::RenderProcessHost::FromID(listener_process_id));
    UpdateActiveProfile();
    return;
  }

  // We have an entry with that process so update the set of tree ids it wants
  // to listen to, and update its desktop permission.
  if (desktop)
    iter->desktop = true;
  else
    iter->tree_ids.insert(ax_tree_id);
}

void AutomationEventRouter::DispatchAccessibilityEvents(
    const ui::AXTreeID& tree_id,
    std::vector<ui::AXTreeUpdate> updates,
    const gfx::Point& mouse_location,
    std::vector<ui::AXEvent> events) {
  ExtensionMsg_AccessibilityEventBundleParams event_bundle;
  event_bundle.tree_id = tree_id;
  event_bundle.updates = std::move(updates);
  event_bundle.mouse_location = mouse_location;
  event_bundle.events = std::move(events);

  DispatchAccessibilityEvents(event_bundle);
}

void AutomationEventRouter::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  RenderProcessHostDestroyed(host);
}

void AutomationEventRouter::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  int process_id = host->GetID();
  base::EraseIf(listeners_, [process_id](const AutomationListener& item) {
    return item.process_id == process_id;
  });
  rph_observers_.Remove(host);
  UpdateActiveProfile();
}

void AutomationEventRouter::UpdateActiveProfile() {
  for (auto& listener : listeners_) {
#if defined(OS_CHROMEOS)
    int extension_id_count = 0;
    for (const auto& listener2 : listeners_) {
      if (listener2.extension_id == listener.extension_id)
        extension_id_count++;
    }
    content::RenderProcessHost* rph =
        content::RenderProcessHost::FromID(listener.process_id);

    // The purpose of is_active_context is to ensure different instances of
    // the same extension running in different profiles don't interfere with
    // one another. If an automation extension is only running in one profile,
    // always mark it as active. If it's running in two or more profiles,
    // only mark one as active.
    listener.is_active_context = (extension_id_count == 1 ||
                                  rph->GetBrowserContext() == active_context_);
#else
    listener.is_active_context = true;
#endif
  }
}

}  // namespace extensions

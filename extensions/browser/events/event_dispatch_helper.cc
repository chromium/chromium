// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/events/event_dispatch_helper.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/browser_process_context_data.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/common/permissions/permissions_data.h"

using content::BrowserContext;

namespace extensions {

namespace {

// Returns whether an event would cross the incognito boundary. e.g.
// incognito->regular or regular->incognito. This is allowed for some extensions
// that enable spanning-mode but is always disallowed for webUI.
// `context` refers to the BrowserContext of the receiver of the event.
bool CrossesIncognito(const BrowserContext& context, const Event& event) {
  return event.restrict_to_browser_context &&
         &context != event.restrict_to_browser_context;
}

// Returns false when the event is scoped to a context and the listening
// extension does not have access to events from that context.
bool CanDispatchEventToBrowserContext(BrowserContext& context,
                                      const Extension* extension,
                                      const Event& event) {
  // Is this event from a different browser context than the renderer (ie, an
  // incognito tab event sent to a normal process, or vice versa).
  bool crosses_incognito = CrossesIncognito(context, event);
  if (!crosses_incognito) {
    return true;
  }
  return ExtensionsBrowserClient::Get()->CanExtensionCrossIncognito(extension,
                                                                    &context);
}

// Returns true if the listener has permission to receive the given `event`.
//
// For extensions, this checks for host permissions to the event's URL and
// whether the extension can receive events from the event's browser context
// (e.g., an incognito event sent to a regular process).
//
// For non-extension listeners (e.g., WebUI), this primarily checks that the
// event does not cross the incognito boundary.
bool CheckPermissions(const Extension* extension,
                      const Event& event,
                      BrowserContext& listener_context,
                      mojom::ContextType target_context_type) {
  if (extension) {
    // Extension-specific checks.
    // Firstly, if the event is for a URL, the Extension must have permission
    // to access that URL.
    if (!event.event_url.is_empty() &&
        event.event_url.GetHost() != extension->id() &&  // event for self is ok
        !extension->permissions_data()
             ->active_permissions()
             .HasEffectiveAccessToURL(event.event_url)) {
      return false;
    }
    // Secondly, if the event is for incognito mode, the Extension must be
    // enabled in incognito mode.
    if (!CanDispatchEventToBrowserContext(listener_context, extension, event)) {
      return false;
    }
  } else {
    // Non-extension (e.g. WebUI and web pages) checks. In general we don't
    // allow context-bound events to cross the incognito barrier.
    if (CrossesIncognito(listener_context, event)) {
      return false;
    }
  }

  // Don't dispatch an event when target context doesn't match the restricted
  // context type.
  if (event.restrict_to_context_type.has_value() &&
      event.restrict_to_context_type.value() != target_context_type) {
    return false;
  }

  return true;
}

}  // namespace

EventDispatchHelper::EventDispatchHelper(
    const ExtensionRegistry& extension_registry,
    BrowserContext& browser_context,
    EventListenerMap& listeners,
    DispatchFunction dispatch_function,
    DispatchToProcessFunction dispatch_to_process_function)
    : extension_registry_(extension_registry),
      browser_context_(browser_context),
      listeners_(listeners),
      dispatch_function_(std::move(dispatch_function)),
      dispatch_to_process_function_(std::move(dispatch_to_process_function)) {}

EventDispatchHelper::~EventDispatchHelper() = default;

// static
void EventDispatchHelper::DispatchEvent(
    content::BrowserContext& browser_context,
    EventListenerMap& listeners,
    DispatchFunction dispatch_function,
    DispatchToProcessFunction dispatch_to_process_function,
    const ExtensionId& restrict_to_extension_id,
    const GURL& restrict_to_url,
    std::unique_ptr<Event> event) {
  const ExtensionRegistry* extension_registry =
      ExtensionRegistry::Get(&browser_context);
  DCHECK(extension_registry);

  EventDispatchHelper(*extension_registry, browser_context, listeners,
                      dispatch_function, dispatch_to_process_function)
      .DispatchEventImpl(restrict_to_extension_id, restrict_to_url,
                         std::move(event));
}

// static
bool EventDispatchHelper::CheckFeatureAvailability(
    const Event& event,
    const Extension* extension,
    const GURL& listener_url,
    content::RenderProcessHost& process,
    BrowserContext& listener_context,
    mojom::ContextType target_context_type) {
  // We shouldn't be dispatching an event to a webpage, since all such events
  // (e.g. messaging) don't go through EventRouter. The exceptions to this are
  // the new chrome webstore domain, which has permission to receive extension
  // events and features with delegated availability checks, such as Controlled
  // Frame which runs within Isolated Web Apps and appear as web pages.
  Feature::Availability availability =
      ExtensionAPI::GetSharedInstance()->IsAvailable(
          event.event_name, extension, target_context_type, listener_url,
          CheckAliasStatus::ALLOWED,
          util::GetBrowserContextId(&listener_context),
          BrowserProcessContextData(&process));
  if (!availability.is_available()) {
    // TODO(crbug.com/40255138): Ideally it shouldn't be possible to reach here,
    // because access is checked on registration. However, we don't always
    // refresh the list of events an extension has registered when other factors
    // which affect availability change (e.g. API allowlists changing). Those
    // situations should be identified and addressed.
    return false;
  }

  return true;
}

void EventDispatchHelper::DispatchEventImpl(
    const ExtensionId& restrict_to_extension_id,
    const GURL& restrict_to_url,
    std::unique_ptr<Event> event) {
  std::set<const EventListener*> listeners(
      listeners_->GetEventListeners(*event));

  // We dispatch events for lazy background pages first because attempting to do
  // so will cause those that are being suspended to cancel that suspension.
  // As canceling a suspension entails sending an event to the affected
  // background page, and as that event needs to be delivered before we dispatch
  // the event we are dispatching here, we dispatch to the lazy listeners here
  // first.
  for (const EventListener* listener : listeners) {
    if (listener->IsLazy()) {
      DispatchEventToLazyListener(restrict_to_extension_id, restrict_to_url,
                                  *event, listener);
    }
  }

  for (const EventListener* listener : listeners) {
    if (!listener->IsLazy()) {
      DispatchEventToActiveListener(restrict_to_extension_id, restrict_to_url,
                                    *event, listener);
    }
  }
}

void EventDispatchHelper::DispatchEventToLazyListener(
    const ExtensionId& restrict_to_extension_id,
    const GURL& restrict_to_url,
    Event& event,
    const EventListener* listener) {
  DCHECK(listener->IsLazy());
  if (!ListenerMeetsRestrictions(listener, restrict_to_extension_id,
                                 restrict_to_url)) {
    return;
  }

  // Lazy listeners don't have a process, take the stored browser context
  // for lazy context.
  TryQueueEventForLazyListener(
      event, LazyContextIdForListener(listener, *browser_context_),
      listener->filter());

  // Dispatch to lazy listener in the incognito context.
  // We need to use the incognito context in the case of split-mode
  // extensions.
  BrowserContext* incognito_context =
      GetIncognitoContextIfAccessible(listener->extension_id());
  if (incognito_context) {
    TryQueueEventForLazyListener(
        event, LazyContextIdForListener(listener, *incognito_context),
        listener->filter());
  }
}

void EventDispatchHelper::DispatchEventToActiveListener(
    const ExtensionId& restrict_to_extension_id,
    const GURL& restrict_to_url,
    const Event& event,
    const EventListener* listener) {
  DCHECK(!listener->IsLazy());
  if (!ListenerMeetsRestrictions(listener, restrict_to_extension_id,
                                 restrict_to_url)) {
    return;
  }

  // Non-lazy listeners take the process browser context for context.
  content::RenderProcessHost* process = listener->process();
  BrowserContext* listener_context = process->GetBrowserContext();

  if (IsAlreadyQueued(LazyContextIdForListener(listener, *listener_context))) {
    return;
  }

  // Determine the target context type.
  ProcessMap* listener_process_map = ProcessMap::Get(listener_context);
  const Extension* extension = GetExtension(listener->extension_id());
  const GURL* url = listener->service_worker_version_id() ==
                            blink::mojom::kInvalidServiceWorkerVersionId
                        ? &listener->listener_url()
                        : nullptr;
  auto context_type = listener_process_map->GetMostLikelyContextType(
      extension, process->GetDeprecatedID(), url);

  if (!CheckPermissions(extension, event, *listener_context, context_type) ||
      !CheckFeatureAvailability(event, extension, listener->listener_url(),
                                *process, *listener_context, context_type)) {
    return;
  }

  // Prepare event for dispatch, running the `will_dispatch_callback` if any.
  bool dispatch_separate_event = true;
  std::unique_ptr<Event> dispatched_event = CreateEventForDispatch(
      event, listener->filter(), extension, *listener_context, context_type,
      &dispatch_separate_event);
  if (!dispatched_event) {
    // The event has been canceled.
    return;
  }

  // Check if we've already dispatched this event to this active context.
  // If multiple listeners match the same event within the same active context,
  // we only dispatch the event once, provided de-duplication is enabled
  // (i.e., `dispatch_separate_event` is true, indicating arguments are expected
  // to be consistent across listeners).
  auto [_, inserted] = dispatched_active_ids_.emplace(
      ActiveContextId{.render_process = process,
                      .worker_thread_id = listener->worker_thread_id(),
                      .extension_id = listener->extension_id(),
                      .browser_context = listener_context,
                      .listener_url = listener->listener_url()});
  if (dispatch_separate_event && !inserted) {
    return;
  }

  dispatch_to_process_function_.Run(
      listener->extension_id(), listener->listener_url(), process,
      listener->service_worker_version_id(), listener->worker_thread_id(),
      std::move(dispatched_event), /*did_enqueue=*/false);
}

void EventDispatchHelper::TryQueueEventForLazyListener(
    Event& event,
    const LazyContextId& dispatch_context,
    const base::Value::Dict* listener_filter) {
  const Extension* extension = GetExtension(dispatch_context.extension_id());
  if (!extension) {
    return;
  }

  // Check both the browser context to see if we should load a
  // non-persistent context (a lazy background page or an extension
  // service worker) to handle the event.
  if (TryQueueEventDispatch(event, dispatch_context, extension,
                            listener_filter)) {
    RecordAlreadyQueued(dispatch_context);
  }
}

bool EventDispatchHelper::TryQueueEventDispatch(
    Event& event,
    const LazyContextId& dispatch_context,
    const Extension* extension,
    const base::Value::Dict* listener_filter) {
  if (IsAlreadyQueued(dispatch_context)) {
    return false;
  }

  // The only lazy listeners belong to an extension's background context (either
  // an event page or a service worker), which are always kPrivilegedExtension
  // contexts.
  auto context_type = mojom::ContextType::kPrivilegedExtension;
  BrowserContext* browser_context = dispatch_context.browser_context();

  if (!CheckPermissions(extension, event, *browser_context, context_type)) {
    return false;
  }

  LazyContextTaskQueue* queue = dispatch_context.GetTaskQueue();
  event.lazy_background_active_on_dispatch =
      queue->IsReadyToRunTasks(browser_context, extension);
  if (!queue->ShouldEnqueueTask(browser_context, extension)) {
    return false;
  }

  // Prepare the event for dispatch, running the `will_dispatch_callback` if
  // any. We do this now (rather than dispatch time) to avoid lifetime issues.
  std::unique_ptr<Event> dispatched_event = CreateEventForDispatch(
      event, listener_filter, extension, *browser_context, context_type,
      /*dispatch_separate_event_out=*/nullptr);

  if (!dispatched_event) {
    // The event has been canceled.
    return true;
  }

  queue->AddPendingTask(
      dispatch_context,
      base::BindOnce(dispatch_function_, std::move(dispatched_event)));

  return true;
}

std::unique_ptr<Event> EventDispatchHelper::CreateEventForDispatch(
    const Event& event,
    const base::Value::Dict* listener_filter,
    const Extension* extension,
    BrowserContext& listener_context,
    mojom::ContextType target_context_type,
    bool* dispatch_separate_event_out) {
  if (event.will_dispatch_callback.is_null()) {
    return event.DeepCopy();
  }

  // Run the callback before copying the event to determine if events need
  // de-duplicating based on the `dispatch_separate_event_out` argument.
  std::optional<base::Value::List> modified_event_args;
  mojom::EventFilteringInfoPtr modified_event_filter_info;
  if (!event.will_dispatch_callback.Run(
          &listener_context, target_context_type, extension, listener_filter,
          modified_event_args, modified_event_filter_info,
          dispatch_separate_event_out)) {
    // The event has been canceled.
    return nullptr;
  }

  // If `event_args` or `filter_info` are modified, we avoid cloning the
  // original ones (which can be costly) by using a selective copy mechanism.
  const bool is_event_args_modified = modified_event_args.has_value();
  const bool is_filter_info_modified = !!modified_event_filter_info;
  std::unique_ptr<Event> dispatched_event = event.CopySelectively(
      /*copy_event_args=*/!is_event_args_modified,
      /*copy_filter_info=*/!is_filter_info_modified);

  if (is_event_args_modified) {
    dispatched_event->event_args = std::move(*modified_event_args);
  }
  if (is_filter_info_modified) {
    dispatched_event->filter_info = std::move(modified_event_filter_info);
  }
  dispatched_event->will_dispatch_callback.Reset();

  return dispatched_event;
}

void EventDispatchHelper::RecordAlreadyQueued(
    const LazyContextId& dispatch_context) {
  dispatched_ids_.insert(dispatch_context);
}

bool EventDispatchHelper::IsAlreadyQueued(
    const LazyContextId& dispatch_context) const {
  return base::Contains(dispatched_ids_, dispatch_context);
}

bool EventDispatchHelper::ListenerMeetsRestrictions(
    const EventListener* listener,
    const ExtensionId& restrict_to_extension_id,
    const GURL& restrict_to_url) const {
  if (!restrict_to_extension_id.empty() &&
      restrict_to_extension_id != listener->extension_id()) {
    return false;
  }

  if (!restrict_to_url.is_empty() &&
      !url::IsSameOriginWith(restrict_to_url, listener->listener_url())) {
    return false;
  }

  return true;
}

BrowserContext* EventDispatchHelper::GetIncognitoContextIfAccessible(
    const ExtensionId& extension_id) const {
  DCHECK(!extension_id.empty());
  const Extension* extension = GetExtension(extension_id);
  if (!extension) {
    return nullptr;
  }
  if (!IncognitoInfo::IsSplitMode(extension)) {
    return nullptr;
  }
  if (!util::IsIncognitoEnabled(extension_id, &browser_context_.get())) {
    return nullptr;
  }

  return GetIncognitoContext();
}

BrowserContext* EventDispatchHelper::GetIncognitoContext() const {
  ExtensionsBrowserClient* browser_client = ExtensionsBrowserClient::Get();
  if (!browser_client->HasOffTheRecordContext(&browser_context_.get())) {
    return nullptr;
  }

  return browser_client->GetOffTheRecordContext(&browser_context_.get());
}

// Browser context is required for lazy context id. Before adding browser
// context member to EventListener, callers must pass in the browser context as
// a parameter.
// TODO(richardzh): Once browser context is added as a member to EventListener,
//                  update this method to get browser_context from listener
//                  instead of parameter.
LazyContextId EventDispatchHelper::LazyContextIdForListener(
    const EventListener* listener,
    BrowserContext& browser_context) const {
  const Extension* extension = GetExtension(listener->extension_id());
  const bool is_service_worker_based_extension =
      extension && BackgroundInfo::IsServiceWorkerBased(extension);
  // Note: It is possible that the prefs' listener->is_for_service_worker() and
  // its extension background type do not agree. This happens when one changes
  // extension's manifest, typically during unpacked extension development.
  // Fallback to non-Service worker based LazyContextId to avoid surprising
  // ServiceWorkerTaskQueue (and crashing), see https://crbug.com/1239752 for
  // details.
  // TODO(lazyboy): Clean these inconsistencies across different types of event
  // listener and their corresponding background types.
  if (is_service_worker_based_extension && listener->is_for_service_worker()) {
    return LazyContextId::ForServiceWorker(&browser_context,
                                           listener->extension_id());
  }

  return LazyContextId::ForBackgroundPage(&browser_context,
                                          listener->extension_id());
}

const Extension* EventDispatchHelper::GetExtension(
    const ExtensionId& extension_id) const {
  return extension_registry_->enabled_extensions().GetByID(extension_id);
}

}  // namespace extensions

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/events/event_dispatch_helper.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"

using content::BrowserContext;

namespace extensions {

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
    const std::string& restrict_to_extension_id,
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

void EventDispatchHelper::DispatchEventImpl(
    const std::string& restrict_to_extension_id,
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
    if (!restrict_to_extension_id.empty() &&
        restrict_to_extension_id != listener->extension_id()) {
      continue;
    }
    if (!restrict_to_url.is_empty() &&
        !url::IsSameOriginWith(restrict_to_url, listener->listener_url())) {
      continue;
    }
    if (!listener->IsLazy()) {
      continue;
    }

    // TODO(richardzh): Move cross browser context check (by calling
    // EventRouter::CanDispatchEventToBrowserContext) to here. So the check
    // happens before instead of during the dispatch.

    // Lazy listeners don't have a process, take the stored browser context
    // for lazy context.
    TryQueueEventForLazyListener(
        *event, LazyContextIdForListener(listener, *browser_context_),
        listener->filter());

    // Dispatch to lazy listener in the incognito context.
    // We need to use the incognito context in the case of split-mode
    // extensions.
    BrowserContext* incognito_context =
        GetIncognitoContextIfAccessible(listener->extension_id());
    if (incognito_context) {
      TryQueueEventForLazyListener(
          *event, LazyContextIdForListener(listener, *incognito_context),
          listener->filter());
    }
  }

  for (const EventListener* listener : listeners) {
    if (!restrict_to_extension_id.empty() &&
        restrict_to_extension_id != listener->extension_id()) {
      continue;
    }
    if (!restrict_to_url.is_empty() &&
        !url::IsSameOriginWith(restrict_to_url, listener->listener_url())) {
      continue;
    }
    if (listener->IsLazy()) {
      continue;
    }
    // Non-lazy listeners take the process browser context for
    // lazy context
    if (IsAlreadyQueued(LazyContextIdForListener(
            listener, *listener->process()->GetBrowserContext()))) {
      continue;
    }

    dispatch_to_process_function_.Run(
        listener->extension_id(), listener->listener_url(), listener->process(),
        listener->service_worker_version_id(), listener->worker_thread_id(),
        *event, listener->filter(), false /* did_enqueue */);
  }
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
  if (!EventRouter::CanDispatchEventToBrowserContext(
          dispatch_context.browser_context(), extension, event)) {
    return false;
  }

  if (IsAlreadyQueued(dispatch_context)) {
    return false;
  }

  LazyContextTaskQueue* queue = dispatch_context.GetTaskQueue();
  event.lazy_background_active_on_dispatch =
      queue->IsReadyToRunTasks(dispatch_context.browser_context(), extension);
  if (!queue->ShouldEnqueueTask(dispatch_context.browser_context(),
                                extension)) {
    return false;
  }

  // TODO(devlin): This results in a copy each time we dispatch events to
  // ServiceWorkers and inactive event pages. It'd be nice to avoid that.
  std::unique_ptr<Event> dispatched_event = event.DeepCopy();

  // If there's a dispatch callback, call it now (rather than dispatch time)
  // to avoid lifetime issues. Use a separate copy of the event args, so they
  // last until the event is dispatched.
  if (!dispatched_event->will_dispatch_callback.is_null()) {
    std::optional<base::Value::List> modified_event_args;
    mojom::EventFilteringInfoPtr modified_event_filter_info;
    if (!dispatched_event->will_dispatch_callback.Run(
            dispatch_context.browser_context(),
            // The only lazy listeners belong to an extension's background
            // context (either an event page or a service worker), which are
            // always kPrivilegedExtension contexts
            extensions::mojom::ContextType::kPrivilegedExtension, extension,
            listener_filter, modified_event_args, modified_event_filter_info)) {
      // The event has been canceled.
      return true;
    }
    if (modified_event_args) {
      dispatched_event->event_args = std::move(*modified_event_args);
    }
    if (modified_event_filter_info) {
      dispatched_event->filter_info = std::move(modified_event_filter_info);
    }
    // Ensure we don't call it again at dispatch time.
    dispatched_event->will_dispatch_callback.Reset();
  }

  queue->AddPendingTask(
      dispatch_context,
      base::BindOnce(dispatch_function_, std::move(dispatched_event)));

  return true;
}

void EventDispatchHelper::RecordAlreadyQueued(
    const LazyContextId& dispatch_context) {
  dispatched_ids_.insert(dispatch_context);
}

bool EventDispatchHelper::IsAlreadyQueued(
    const LazyContextId& dispatch_context) const {
  return base::Contains(dispatched_ids_, dispatch_context);
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

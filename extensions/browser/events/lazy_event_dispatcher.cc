// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/events/lazy_event_dispatcher.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"

using content::BrowserContext;

namespace extensions {

LazyEventDispatcher::LazyEventDispatcher(BrowserContext* browser_context,
                                         DispatchFunction dispatch_function)
    : browser_context_(browser_context),
      dispatch_function_(std::move(dispatch_function)) {}

LazyEventDispatcher::~LazyEventDispatcher() = default;

void LazyEventDispatcher::Dispatch(Event& event,
                                   const LazyContextId& dispatch_context,
                                   const base::Value::Dict* listener_filter) {
  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(dispatch_context.extension_id());
  if (!extension)
    return;

  // Check both the browser context to see if we should load a
  // non-peristent context (a lazy background page or an extension
  // service worker) to handle the event.
  if (QueueEventDispatch(event, dispatch_context, extension, listener_filter))
    RecordAlreadyDispatched(dispatch_context);
}

bool LazyEventDispatcher::HasAlreadyDispatched(
    const LazyContextId& dispatch_context) const {
  return base::Contains(dispatched_ids_, dispatch_context);
}

bool LazyEventDispatcher::QueueEventDispatch(
    Event& event,
    const LazyContextId& dispatch_context,
    const Extension* extension,
    const base::Value::Dict* listener_filter) {
  if (!EventRouter::CanDispatchEventToBrowserContext(
          dispatch_context.browser_context(), extension, event)) {
    return false;
  }

  if (HasAlreadyDispatched(dispatch_context))
    return false;

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
    if (modified_event_filter_info)
      dispatched_event->filter_info = std::move(modified_event_filter_info);
    // Ensure we don't call it again at dispatch time.
    dispatched_event->will_dispatch_callback.Reset();
  }

  queue->AddPendingTask(
      dispatch_context,
      base::BindOnce(dispatch_function_, std::move(dispatched_event)));

  return true;
}

void LazyEventDispatcher::RecordAlreadyDispatched(
    const LazyContextId& dispatch_context) {
  dispatched_ids_.insert(dispatch_context);
}

}  // namespace extensions

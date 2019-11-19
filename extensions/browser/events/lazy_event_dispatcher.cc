// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/events/lazy_event_dispatcher.h"

#include "base/bind.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/manifest_handlers/incognito_info.h"

using content::BrowserContext;

namespace extensions {

LazyEventDispatcher::LazyEventDispatcher(BrowserContext* browser_context,
                                         DispatchFunction dispatch_function)
    : browser_context_(browser_context),
      dispatch_function_(std::move(dispatch_function)) {}

LazyEventDispatcher::~LazyEventDispatcher() {}

void LazyEventDispatcher::Dispatch(
    const Event& event,
    const LazyContextId& dispatch_context,
    const base::DictionaryValue* listener_filter) {
  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(dispatch_context.extension_id());
  if (!extension)
    return;

  // Check both the original and the incognito browser context to see if we
  // should load a non-peristent context (a lazy background page or an
  // extension service worker) to handle the event. We need to use the incognito
  // context in the case of split-mode extensions.
  if (QueueEventDispatch(event, dispatch_context, extension, listener_filter))
    RecordAlreadyDispatched(dispatch_context);

  BrowserContext* additional_context = GetIncognitoContext(extension);
  if (!additional_context)
    return;

  LazyContextId additional_context_id(dispatch_context);
  additional_context_id.set_browser_context(additional_context);
  if (QueueEventDispatch(event, additional_context_id, extension,
                         listener_filter)) {
    RecordAlreadyDispatched(additional_context_id);
  }
}

bool LazyEventDispatcher::HasAlreadyDispatched(
    const LazyContextId& dispatch_context) const {
  return base::Contains(dispatched_ids_, dispatch_context);
}

bool LazyEventDispatcher::QueueEventDispatch(
    const Event& event,
    const LazyContextId& dispatch_context,
    const Extension* extension,
    const base::DictionaryValue* listener_filter) {
  if (!EventRouter::CanDispatchEventToBrowserContext(
          dispatch_context.browser_context(), extension, event)) {
    return false;
  }

  if (HasAlreadyDispatched(dispatch_context))
    return false;

  LazyContextTaskQueue* queue = dispatch_context.GetTaskQueue();
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
    if (!dispatched_event->will_dispatch_callback.Run(
            dispatch_context.browser_context(),
            // The only lazy listeners belong to an extension's background
            // context (either an event page or a service worker), which are
            // always BLESSED_EXTENSION_CONTEXTs
            extensions::Feature::BLESSED_EXTENSION_CONTEXT, extension,
            dispatched_event.get(), listener_filter)) {
      // The event has been canceled.
      return true;
    }
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

BrowserContext* LazyEventDispatcher::GetIncognitoContext(
    const Extension* extension) {
  if (!IncognitoInfo::IsSplitMode(extension))
    return nullptr;
  ExtensionsBrowserClient* browser_client = ExtensionsBrowserClient::Get();
  if (!browser_client->HasOffTheRecordContext(browser_context_))
    return nullptr;
  return browser_client->GetOffTheRecordContext(browser_context_);
}

}  // namespace extensions

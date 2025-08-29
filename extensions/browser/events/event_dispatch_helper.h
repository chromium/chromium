// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EVENTS_EVENT_DISPATCH_HELPER_H_
#define EXTENSIONS_BROWSER_EVENTS_EVENT_DISPATCH_HELPER_H_

#include <set>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/values.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
class RenderProcessHost;
}  // namespace content

namespace extensions {
class EventListener;
class EventListenerMap;
class Extension;
class ExtensionRegistry;
struct Event;

// Helper class for EventRouter to dispatch events.
//
// Handles both lazy and non-lazy contexts.
// Manages waking up lazy contexts if they are stopped.
class EventDispatchHelper {
 public:
  using DispatchFunction = base::RepeatingCallback<void(
      std::unique_ptr<Event>,
      std::unique_ptr<LazyContextTaskQueue::ContextInfo>)>;

  using DispatchToProcessFunction =
      base::RepeatingCallback<void(const ExtensionId& extension_id,
                                   const GURL& listener_url,
                                   content::RenderProcessHost* process,
                                   int64_t service_worker_version_id,
                                   int worker_thread_id,
                                   const Event& event,
                                   const base::Value::Dict* listener_filter,
                                   bool did_enqueue)>;

  EventDispatchHelper(const EventDispatchHelper&) = delete;
  EventDispatchHelper& operator=(const EventDispatchHelper&) = delete;

  // Dispatches the given `event` to any matching `listeners` (lazy and
  // non-lazy).
  static void DispatchEvent(
      content::BrowserContext& browser_context,
      EventListenerMap& listeners,
      DispatchFunction dispatch_function,
      DispatchToProcessFunction dispatch_to_process_function,
      const ExtensionId& restrict_to_extension_id,
      const GURL& restrict_to_url,
      std::unique_ptr<Event> event);

 private:
  EventDispatchHelper(const ExtensionRegistry& extension_registry,
                      content::BrowserContext& browser_context,
                      EventListenerMap& listeners,
                      DispatchFunction dispatch_function,
                      DispatchToProcessFunction dispatch_to_process_function);

  ~EventDispatchHelper();

  void DispatchEventImpl(const std::string& restrict_to_extension_id,
                         const GURL& restrict_to_url,
                         std::unique_ptr<Event> event);

  // Possibly queues the lazy `event` for dispatch to `dispatch_context`.
  //
  // If [dispatch_context| is for an event page, it ensures all of the pages
  // interested in the event are loaded and queues the event if any pages are
  // not ready yet.
  //
  // If [dispatch_context| is for a service worker, it ensures the worker is
  // started before dispatching the event.
  //
  // NOTE: this method will not dispatch to a lazy listener if the context
  // is active.
  void TryQueueEventForLazyListener(Event& event,
                                    const LazyContextId& dispatch_context,
                                    const base::Value::Dict* listener_filter);

  // Possibly loads given extension's background page or extension Service
  // Worker in preparation to dispatch an event. Returns true if the event was
  // queued for subsequent dispatch, false otherwise.
  bool TryQueueEventDispatch(Event& event,
                             const LazyContextId& dispatch_context,
                             const Extension* extension,
                             const base::Value::Dict* listener_filter);

  // Records that an event has been queued for dispatch to a lazy listener to
  // avoid dispatching it again to a non-lazy listener.
  void RecordAlreadyQueued(const LazyContextId& dispatch_context);

  // Returns whether or not an event listener identical for `dispatch_context`
  // is already queued for dispatch to a lazy listener.
  bool IsAlreadyQueued(const LazyContextId& dispatch_context) const;

  // Gets off-the-record browser context if
  //     - The extension has incognito mode set to "split"
  //     - The on-the-record browser context has an off-the-record context
  //       attached
  content::BrowserContext* GetIncognitoContextIfAccessible(
      const ExtensionId& extension_id) const;

  // Returns the off-the-record context for the BrowserContext associated
  // with this EventRouter, if any.
  content::BrowserContext* GetIncognitoContext() const;

  LazyContextId LazyContextIdForListener(
      const EventListener* listener,
      content::BrowserContext& browser_context) const;

  const Extension* GetExtension(const ExtensionId& extension_id) const;

  const raw_ref<const ExtensionRegistry> extension_registry_;
  const raw_ref<content::BrowserContext> browser_context_;
  const raw_ref<EventListenerMap> listeners_;
  DispatchFunction dispatch_function_;
  DispatchToProcessFunction dispatch_to_process_function_;

  std::set<LazyContextId> dispatched_ids_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EVENTS_EVENT_DISPATCH_HELPER_H_

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
#include "extensions/common/mojom/context_type.mojom.h"

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

// A unique identifier for an active listener context. This is used to
// de-duplicate event dispatches to the same active listener context.
struct ActiveContextId {
  raw_ptr<content::RenderProcessHost> render_process;
  int worker_thread_id;
  ExtensionId extension_id;
  raw_ptr<content::BrowserContext> browser_context;
  GURL listener_url;

  auto operator<=>(const ActiveContextId&) const = default;
};

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
                                   std::unique_ptr<Event> event,
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

  // Returns true if the feature for the given `event` is available to the
  // listener's context.
  static bool CheckFeatureAvailability(
      const Event& event,
      const Extension* extension,
      const GURL& listener_url,
      content::RenderProcessHost& process,
      content::BrowserContext& listener_context,
      mojom::ContextType target_context_type);

 private:
  EventDispatchHelper(const ExtensionRegistry& extension_registry,
                      content::BrowserContext& browser_context,
                      EventListenerMap& listeners,
                      DispatchFunction dispatch_function,
                      DispatchToProcessFunction dispatch_to_process_function);

  ~EventDispatchHelper();

  void DispatchEventImpl(const ExtensionId& restrict_to_extension_id,
                         const GURL& restrict_to_url,
                         std::unique_ptr<Event> event);

  // Attempts to dispatch the given `event` to the specified lazy `listener`.
  // This will queue the event to be dispatched later if the lazy context
  // is not currently running.
  //
  // NOTE: this method will not dispatch to a lazy listener if the context
  // is active, so that it can be dispatched to the corresponding active
  // (non-lazy) listener instead.
  void DispatchEventToLazyListener(const ExtensionId& restrict_to_extension_id,
                                   const GURL& restrict_to_url,
                                   Event& event,
                                   const EventListener* listener);

  // Dispatches the given `event` to the specified active `listener`. Avoids
  // dispatching if an event has already been queued for a lazy listener with
  // the same context.
  void DispatchEventToActiveListener(
      const ExtensionId& restrict_to_extension_id,
      const GURL& restrict_to_url,
      const Event& event,
      const EventListener* listener);

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
  // handled (either queued for subsequent dispatch or canceled), false
  // otherwise.
  bool TryQueueEventDispatch(Event& event,
                             const LazyContextId& dispatch_context,
                             const Extension* extension,
                             const base::Value::Dict* listener_filter);

  // Creates a copy of `event` for dispatching to a specific listener context.
  // If `event` has a `will_dispatch_callback`, it is run before copying. If the
  // callback cancels the event, returns nullptr. Otherwise, returns a copy of
  // the event with any modifications applied by the callback.
  std::unique_ptr<Event> CreateEventForDispatch(
      const Event& event,
      const base::Value::Dict* listener_filter,
      const Extension* extension,
      content::BrowserContext& listener_context,
      mojom::ContextType target_context_type,
      bool* dispatch_separate_event_out);

  // Records that an event has been queued for dispatch to a lazy listener to
  // avoid dispatching it again to a non-lazy listener.
  void RecordAlreadyQueued(const LazyContextId& dispatch_context);

  // Returns whether or not an event listener identical for `dispatch_context`
  // is already queued for dispatch to a lazy listener.
  bool IsAlreadyQueued(const LazyContextId& dispatch_context) const;

  // Returns true if the given `listener` meets dispatch restrictions. Events
  // may be restricted to a particular extension ID or URL context.
  //
  // If `restrict_to_extension_id` is non-empty, the listener's extension ID
  // must match it. If `restrict_to_url` is non-empty, the listener's URL must
  // be same-origin with it.
  bool ListenerMeetsRestrictions(const EventListener* listener,
                                 const ExtensionId& restrict_to_extension_id,
                                 const GURL& restrict_to_url) const;

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
  std::set<ActiveContextId> dispatched_active_ids_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EVENTS_EVENT_DISPATCH_HELPER_H_

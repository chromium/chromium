// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EVENTS_LAZY_EVENT_DISPATCHER_H_
#define EXTENSIONS_BROWSER_EVENTS_LAZY_EVENT_DISPATCHER_H_

#include <set>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
struct Event;

// Helper class for EventRouter to dispatch lazy events to lazy contexts.
//
// Manages waking up lazy contexts if they are stopped.
class LazyEventDispatcher {
 public:
  using DispatchFunction = base::RepeatingCallback<void(
      std::unique_ptr<Event>,
      std::unique_ptr<LazyContextTaskQueue::ContextInfo>)>;

  LazyEventDispatcher(content::BrowserContext* browser_context,
                      DispatchFunction dispatch_function);

  LazyEventDispatcher(const LazyEventDispatcher&) = delete;
  LazyEventDispatcher& operator=(const LazyEventDispatcher&) = delete;

  ~LazyEventDispatcher();

  // Dispatches the lazy |event| to |dispatch_context|.
  //
  // If [dispatch_context| is for an event page, it ensures all of the pages
  // interested in the event are loaded and queues the event if any pages are
  // not ready yet.
  //
  // If [dispatch_context| is for a service worker, it ensures the worker is
  // started before dispatching the  event.
  void Dispatch(Event& event,
                const LazyContextId& dispatch_context,
                const base::Value::Dict* listener_filter);

  // Returns whether or not an event listener identical for |dispatch_context|
  // is already queued for dispatch.
  bool HasAlreadyDispatched(const LazyContextId& dispatch_context) const;

 private:
  // Possibly loads given extension's background page or extension Service
  // Worker in preparation to dispatch an event.  Returns true if the event was
  // queued for subsequent dispatch, false otherwise.
  bool QueueEventDispatch(Event& event,
                          const LazyContextId& dispatch_context,
                          const Extension* extension,
                          const base::Value::Dict* listener_filter);

  void RecordAlreadyDispatched(const LazyContextId& dispatch_context);

  const raw_ptr<content::BrowserContext> browser_context_;
  DispatchFunction dispatch_function_;

  std::set<LazyContextId> dispatched_ids_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EVENTS_LAZY_EVENT_DISPATCHER_H_

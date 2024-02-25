// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/event_listener_map.h"

#include <stddef.h>

#include <utility>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "ipc/ipc_message.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

using MatcherID = EventFilter::MatcherID;

// static
std::unique_ptr<EventListener> EventListener::ForExtension(
    const std::string& event_name,
    const ExtensionId& extension_id,
    content::RenderProcessHost* process,
    std::optional<base::Value::Dict> filter) {
  DCHECK(process);

  return base::WrapUnique(new EventListener(
      event_name, extension_id, GURL(), process, process->GetBrowserContext(),
      false, blink::mojom::kInvalidServiceWorkerVersionId, kMainThreadId,
      std::move(filter)));
}

// static
std::unique_ptr<EventListener> EventListener::ForURL(
    const std::string& event_name,
    const GURL& listener_url,
    content::RenderProcessHost* process,
    std::optional<base::Value::Dict> filter) {
  // Use only the origin to identify the event listener, e.g. chrome://settings
  // for chrome://settings/accounts, to avoid multiple events being triggered
  // for the same process. See crbug.com/536858 for details. // TODO(devlin): If
  // we dispatched events to processes more intelligently this could be avoided.
  return base::WrapUnique(new EventListener(
      event_name, ExtensionId(), url::Origin::Create(listener_url).GetURL(),
      process, process->GetBrowserContext(), false,
      blink::mojom::kInvalidServiceWorkerVersionId, kMainThreadId,
      std::move(filter)));
}

std::unique_ptr<EventListener> EventListener::ForExtensionServiceWorker(
    const std::string& event_name,
    const ExtensionId& extension_id,
    content::RenderProcessHost* process,
    content::BrowserContext* browser_context,
    const GURL& service_worker_scope,
    int64_t service_worker_version_id,
    int worker_thread_id,
    std::optional<base::Value::Dict> filter) {
  return base::WrapUnique(new EventListener(
      event_name, extension_id, service_worker_scope, process, browser_context,
      true, service_worker_version_id, worker_thread_id, std::move(filter)));
}

std::unique_ptr<EventListener> EventListener::CreateLazyListener(
    const std::string& event_name,
    const ExtensionId& extension_id,
    content::BrowserContext* browser_context,
    bool is_for_service_worker,
    const GURL& service_worker_scope,
    std::optional<base::Value::Dict> filter) {
  return base::WrapUnique(new EventListener(
      event_name, extension_id, service_worker_scope, /*process=*/nullptr,
      browser_context, is_for_service_worker,
      blink::mojom::kInvalidServiceWorkerVersionId, kMainThreadId,
      std::move(filter)));
}

EventListener::~EventListener() = default;

bool EventListener::Equals(const EventListener* other) const {
  // TODO(richardzh): compare browser_context_. We are making a change with two
  // steps here. The first step is simply add the browser_context_ member. The
  // next step is to compare this member and create separate lazy listeners for
  // regular and incognito(split) context.

  // We don't check matcher_id equality because we want a listener with a
  // filter that hasn't been added to EventFilter to match one that is
  // equivalent but has.
  return event_name_ == other->event_name_ &&
         extension_id_ == other->extension_id_ &&
         listener_url_ == other->listener_url_ && process_ == other->process_ &&
         is_for_service_worker_ == other->is_for_service_worker_ &&
         service_worker_version_id_ == other->service_worker_version_id_ &&
         worker_thread_id_ == other->worker_thread_id_ &&
         filter_ == other->filter_;
}

std::unique_ptr<EventListener> EventListener::Copy() const {
  std::optional<base::Value::Dict> filter_copy;
  if (filter_)
    filter_copy = filter_->Clone();
  return base::WrapUnique(new EventListener(
      event_name_, extension_id_, listener_url_, process_, browser_context_,
      is_for_service_worker_, service_worker_version_id_, worker_thread_id_,
      std::move(filter_copy)));
}

bool EventListener::IsLazy() const {
  return !process_;
}

void EventListener::MakeLazy() {
  // A lazy listener neither has a process attached to it nor it has a worker
  // thread (if the listener was for a service worker), so reset these values
  // below to reflect that.
  worker_thread_id_ = kMainThreadId;
  service_worker_version_id_ = blink::mojom::kInvalidServiceWorkerVersionId;
  process_ = nullptr;
}

EventListener::EventListener(const std::string& event_name,
                             const ExtensionId& extension_id,
                             const GURL& listener_url,
                             content::RenderProcessHost* process,
                             content::BrowserContext* browser_context,
                             bool is_for_service_worker,
                             int64_t service_worker_version_id,
                             int worker_thread_id,
                             std::optional<base::Value::Dict> filter)
    : event_name_(event_name),
      extension_id_(extension_id),
      listener_url_(listener_url),
      process_(process),
      browser_context_(browser_context),
      is_for_service_worker_(is_for_service_worker),
      service_worker_version_id_(service_worker_version_id),
      worker_thread_id_(worker_thread_id),
      filter_(std::move(filter)) {
  if (!IsLazy()) {
    DCHECK_EQ(is_for_service_worker, worker_thread_id != kMainThreadId);
    DCHECK_EQ(is_for_service_worker,
              service_worker_version_id !=
                  blink::mojom::kInvalidServiceWorkerVersionId);
  }
}

EventListenerMap::EventListenerMap(Delegate* delegate)
    : delegate_(delegate) {
}

EventListenerMap::~EventListenerMap() = default;

bool EventListenerMap::AddListener(std::unique_ptr<EventListener> listener) {
  if (HasListener(listener.get()))
    return false;
  if (listener->filter()) {
    std::unique_ptr<EventMatcher> matcher(
        ParseEventMatcher(*listener->filter()));
    MatcherID id = event_filter_.AddEventMatcher(listener->event_name(),
                                                 std::move(matcher));
    listener->set_matcher_id(id);
    listeners_by_matcher_id_[id] = listener.get();
    filtered_events_.insert(listener->event_name());
  }
  EventListener* listener_ptr = listener.get();
  listeners_[listener->event_name()].push_back(std::move(listener));

  delegate_->OnListenerAdded(listener_ptr);

  return true;
}

std::unique_ptr<EventMatcher> EventListenerMap::ParseEventMatcher(
    const base::Value::Dict& filter_dict) {
  return std::make_unique<EventMatcher>(
      std::make_unique<base::Value::Dict>(filter_dict.Clone()),
      MSG_ROUTING_NONE);
}

bool EventListenerMap::RemoveListener(const EventListener* listener) {
  auto listener_itr = listeners_.find(listener->event_name());
  if (listener_itr == listeners_.end())
    return false;
  ListenerList& listeners = listener_itr->second;
  for (auto& it : listeners) {
    if (it->Equals(listener)) {
      CleanupListener(it.get());
      // Popping from the back should be cheaper than erase(it).
      std::swap(it, listeners.back());
      listeners.pop_back();
      if (listeners.empty())
        listeners_.erase(listener_itr);
      delegate_->OnListenerRemoved(listener);
      return true;
    }
  }
  return false;
}

bool EventListenerMap::HasListenerForEvent(
    const std::string& event_name) const {
  auto it = listeners_.find(event_name);
  return it != listeners_.end() && !it->second.empty();
}

bool EventListenerMap::HasListenerForExtension(
    const ExtensionId& extension_id,
    const std::string& event_name) const {
  auto it = listeners_.find(event_name);
  if (it == listeners_.end())
    return false;

  for (const auto& listener_to_search : it->second) {
    if (listener_to_search->extension_id() == extension_id)
      return true;
  }
  return false;
}

bool EventListenerMap::HasListenerForURL(const GURL& url,
                                         const std::string& event_name) const {
  auto it = listeners_.find(event_name);
  if (it == listeners_.end())
    return false;

  for (const auto& listener_to_search : it->second) {
    if (url::IsSameOriginWith(listener_to_search->listener_url(), url))
      return true;
  }
  return false;
}

bool EventListenerMap::HasListener(const EventListener* listener) const {
  auto it = listeners_.find(listener->event_name());
  if (it == listeners_.end())
    return false;

  for (const auto& listener_to_search : it->second) {
    if (listener_to_search->Equals(listener))
      return true;
  }
  return false;
}

bool EventListenerMap::HasProcessListener(
    content::RenderProcessHost* process,
    int worker_thread_id,
    const ExtensionId& extension_id) const {
  for (const auto& it : listeners_) {
    for (const auto& listener : it.second) {
      if (listener->process() == process &&
          listener->extension_id() == extension_id &&
          listener->worker_thread_id() == worker_thread_id) {
        return true;
      }
    }
  }
  return false;
}

bool EventListenerMap::HasProcessListenerForEvent(
    content::RenderProcessHost* process,
    int worker_thread_id,
    const ExtensionId& extension_id,
    const std::string& event_name) const {
  for (const auto& it : listeners_) {
    for (const auto& listener : it.second) {
      if (listener->process() == process &&
          listener->extension_id() == extension_id &&
          listener->worker_thread_id() == worker_thread_id &&
          listener->event_name() == event_name) {
        return true;
      }
    }
  }
  return false;
}

void EventListenerMap::RemoveListenersForExtension(
    const ExtensionId& extension_id) {
  for (auto it = listeners_.begin(); it != listeners_.end();) {
    auto& listener_list = it->second;
    for (auto it2 = listener_list.begin(); it2 != listener_list.end();) {
      if ((*it2)->extension_id() == extension_id) {
        std::unique_ptr<EventListener> listener_removed = std::move(*it2);
        CleanupListener(listener_removed.get());
        it2 = listener_list.erase(it2);
        delegate_->OnListenerRemoved(listener_removed.get());
      } else {
        ++it2;
      }
    }
    // Check if we removed all the listeners from the list. If so,
    // remove the list entry entirely.
    if (listener_list.empty())
      it = listeners_.erase(it);
    else
      ++it;
  }
}

void EventListenerMap::LoadUnfilteredLazyListeners(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    bool is_for_service_worker,
    const std::set<std::string>& event_names) {
  for (const auto& name : event_names) {
    AddListener(EventListener::CreateLazyListener(
        name, extension_id, browser_context, is_for_service_worker,
        is_for_service_worker
            ? Extension::GetBaseURLFromExtensionId(extension_id)
            : GURL(),
        std::nullopt));
  }
}

void EventListenerMap::LoadFilteredLazyListeners(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    bool is_for_service_worker,
    const base::Value::Dict& filtered) {
  for (const auto item : filtered) {
    // We skip entries if they are malformed.
    if (!item.second.is_list())
      continue;
    for (const base::Value& filter_value : item.second.GetList()) {
      if (!filter_value.is_dict())
        continue;
      const base::Value::Dict& filter = filter_value.GetDict();
      AddListener(EventListener::CreateLazyListener(
          item.first, extension_id, browser_context, is_for_service_worker,
          is_for_service_worker
              ? Extension::GetBaseURLFromExtensionId(extension_id)
              : GURL(),
          filter.Clone()));
    }
  }
}

std::set<const EventListener*> EventListenerMap::GetEventListeners(
    const Event& event) {
  std::set<const EventListener*> interested_listeners;
  if (IsFilteredEvent(event)) {
    // Look up the interested listeners via the EventFilter.
    std::set<MatcherID> ids = event_filter_.MatchEvent(
        event.event_name, *event.filter_info, MSG_ROUTING_NONE);
    for (const MatcherID& id : ids) {
      EventListener* listener = listeners_by_matcher_id_[id];
      CHECK(listener);
      interested_listeners.insert(listener);
    }
  } else {
    for (const auto& listener : listeners_[event.event_name])
      interested_listeners.insert(listener.get());
  }

  return interested_listeners;
}

void EventListenerMap::RemoveListenersForProcess(
    const content::RenderProcessHost* process) {
  CHECK(process);
  for (auto it = listeners_.begin(); it != listeners_.end();) {
    auto& listener_list = it->second;
    for (auto it2 = listener_list.begin(); it2 != listener_list.end();) {
      if ((*it2)->process() == process) {
        std::unique_ptr<EventListener> listener_removed = std::move(*it2);
        CleanupListener(listener_removed.get());
        it2 = listener_list.erase(it2);
        delegate_->OnListenerRemoved(listener_removed.get());
      } else {
        ++it2;
      }
    }
    // Check if we removed all the listeners from the list. If so,
    // remove the list entry entirely.
    if (listener_list.empty())
      it = listeners_.erase(it);
    else
      ++it;
  }
}

void EventListenerMap::CleanupListener(EventListener* listener) {
  // If the listener doesn't have a filter then we have nothing to clean up.
  if (listener->matcher_id() == -1)
    return;
  // If we're removing the final listener for an event, we can remove the
  // entry from |filtered_events_|, as well.
  auto iter = listeners_.find(listener->event_name());
  if (iter->second.size() == 1)
    filtered_events_.erase(iter->first);
  event_filter_.RemoveEventMatcher(listener->matcher_id());
  CHECK_EQ(1u, listeners_by_matcher_id_.erase(listener->matcher_id()));
}

bool EventListenerMap::IsFilteredEvent(const Event& event) const {
  return base::Contains(filtered_events_, event.event_name);
}

}  // namespace extensions

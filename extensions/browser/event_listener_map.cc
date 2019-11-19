// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/event_listener_map.h"

#include <stddef.h>

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/constants.h"
#include "ipc/ipc_message.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::DictionaryValue;

namespace extensions {

typedef EventFilter::MatcherID MatcherID;

// static
std::unique_ptr<EventListener> EventListener::ForExtension(
    const std::string& event_name,
    const std::string& extension_id,
    content::RenderProcessHost* process,
    std::unique_ptr<base::DictionaryValue> filter) {
  return base::WrapUnique(
      new EventListener(event_name, extension_id, GURL(), process, false,
                        blink::mojom::kInvalidServiceWorkerVersionId,
                        kMainThreadId, std::move(filter)));
}

// static
std::unique_ptr<EventListener> EventListener::ForURL(
    const std::string& event_name,
    const GURL& listener_url,
    content::RenderProcessHost* process,
    std::unique_ptr<base::DictionaryValue> filter) {
  // Use only the origin to identify the event listener, e.g. chrome://settings
  // for chrome://settings/accounts, to avoid multiple events being triggered
  // for the same process. See crbug.com/536858 for details. // TODO(devlin): If
  // we dispatched events to processes more intelligently this could be avoided.
  return base::WrapUnique(new EventListener(
      event_name, ExtensionId(), url::Origin::Create(listener_url).GetURL(),
      process, false, blink::mojom::kInvalidServiceWorkerVersionId,
      kMainThreadId, std::move(filter)));
}

std::unique_ptr<EventListener> EventListener::ForExtensionServiceWorker(
    const std::string& event_name,
    const std::string& extension_id,
    content::RenderProcessHost* process,
    const GURL& service_worker_scope,
    int64_t service_worker_version_id,
    int worker_thread_id,
    std::unique_ptr<base::DictionaryValue> filter) {
  return base::WrapUnique(new EventListener(
      event_name, extension_id, service_worker_scope, process, true,
      service_worker_version_id, worker_thread_id, std::move(filter)));
}

EventListener::~EventListener() {}

bool EventListener::Equals(const EventListener* other) const {
  // We don't check matcher_id equality because we want a listener with a
  // filter that hasn't been added to EventFilter to match one that is
  // equivalent but has.
  return event_name_ == other->event_name_ &&
         extension_id_ == other->extension_id_ &&
         listener_url_ == other->listener_url_ && process_ == other->process_ &&
         is_for_service_worker_ == other->is_for_service_worker_ &&
         service_worker_version_id_ == other->service_worker_version_id_ &&
         worker_thread_id_ == other->worker_thread_id_ &&
         ((!!filter_.get()) == (!!other->filter_.get())) &&
         (!filter_.get() || filter_->Equals(other->filter_.get()));
}

std::unique_ptr<EventListener> EventListener::Copy() const {
  std::unique_ptr<DictionaryValue> filter_copy;
  if (filter_)
    filter_copy = filter_->CreateDeepCopy();
  return base::WrapUnique(
      new EventListener(event_name_, extension_id_, listener_url_, process_,
                        is_for_service_worker_, service_worker_version_id_,
                        worker_thread_id_, std::move(filter_copy)));
}

bool EventListener::IsLazy() const {
  return !process_;
}

void EventListener::MakeLazy() {
  // A lazy listener neither has a process attached to it nor it has a worker
  // thread id (if the listener was for a service worker), so reset these values
  // below to reflect that.
  if (is_for_service_worker_)
    worker_thread_id_ = kMainThreadId;
  process_ = nullptr;
}

content::BrowserContext* EventListener::GetBrowserContext() const {
  return process_ ? process_->GetBrowserContext() : nullptr;
}

EventListener::EventListener(const std::string& event_name,
                             const std::string& extension_id,
                             const GURL& listener_url,
                             content::RenderProcessHost* process,
                             bool is_for_service_worker,
                             int64_t service_worker_version_id,
                             int worker_thread_id,
                             std::unique_ptr<DictionaryValue> filter)
    : event_name_(event_name),
      extension_id_(extension_id),
      listener_url_(listener_url),
      process_(process),
      is_for_service_worker_(is_for_service_worker),
      service_worker_version_id_(service_worker_version_id),
      worker_thread_id_(worker_thread_id),
      filter_(std::move(filter)),
      matcher_id_(-1) {
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

EventListenerMap::~EventListenerMap() {}

bool EventListenerMap::AddListener(std::unique_ptr<EventListener> listener) {
  if (HasListener(listener.get()))
    return false;
  if (listener->filter()) {
    std::unique_ptr<EventMatcher> matcher(
        ParseEventMatcher(listener->filter()));
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
    DictionaryValue* filter_dict) {
  return std::make_unique<EventMatcher>(filter_dict->CreateDeepCopy(),
                                        MSG_ROUTING_NONE);
}

bool EventListenerMap::RemoveListener(const EventListener* listener) {
  ListenerList& listeners = listeners_[listener->event_name()];
  for (auto& it : listeners) {
    if (it->Equals(listener)) {
      CleanupListener(it.get());
      // Popping from the back should be cheaper than erase(it).
      std::swap(it, listeners.back());
      listeners.pop_back();
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
    const std::string& extension_id,
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
    const std::string& extension_id) const {
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

void EventListenerMap::RemoveListenersForExtension(
    const std::string& extension_id) {
  for (auto& it : listeners_) {
    auto& listener_map = it.second;
    for (auto it2 = listener_map.begin(); it2 != listener_map.end();) {
      if ((*it2)->extension_id() == extension_id) {
        std::unique_ptr<EventListener> listener_removed = std::move(*it2);
        CleanupListener(listener_removed.get());
        it2 = listener_map.erase(it2);
        delegate_->OnListenerRemoved(listener_removed.get());
      } else {
        ++it2;
      }
    }
  }
}

void EventListenerMap::LoadUnfilteredLazyListeners(
    const std::string& extension_id,
    const std::set<std::string>& event_names) {
  for (const auto& name : event_names) {
    AddListener(EventListener::ForExtension(
        name, extension_id, nullptr, std::unique_ptr<DictionaryValue>()));
  }
}

void EventListenerMap::LoadUnfilteredWorkerListeners(
    const ExtensionId& extension_id,
    const std::set<std::string>& event_names) {
  for (const auto& name : event_names) {
    AddListener(EventListener::ForExtensionServiceWorker(
        name, extension_id, nullptr,
        // TODO(lazyboy): We need to store correct scopes of each worker into
        // ExtensionPrefs for events. This currently assumes all workers are
        // registered in the '/' scope. https://crbug.com/773103.
        Extension::GetBaseURLFromExtensionId(extension_id),
        blink::mojom::kInvalidServiceWorkerVersionId, kMainThreadId, nullptr));
  }
}

void EventListenerMap::LoadFilteredLazyListeners(
    const std::string& extension_id,
    bool is_for_service_worker,
    const DictionaryValue& filtered) {
  for (DictionaryValue::Iterator it(filtered); !it.IsAtEnd(); it.Advance()) {
    // We skip entries if they are malformed.
    const base::ListValue* filter_list = nullptr;
    if (!it.value().GetAsList(&filter_list))
      continue;
    for (size_t i = 0; i < filter_list->GetSize(); i++) {
      const DictionaryValue* filter = nullptr;
      if (!filter_list->GetDictionary(i, &filter))
        continue;
      if (is_for_service_worker) {
        AddListener(EventListener::ForExtensionServiceWorker(
            it.key(), extension_id, nullptr,
            // TODO(lazyboy): We need to store correct scopes of each worker
            // into ExtensionPrefs for events. This currently assumes all
            // workers are registered in the '/' scope.
            // https://crbug.com/773103.
            Extension::GetBaseURLFromExtensionId(extension_id),
            blink::mojom::kInvalidServiceWorkerVersionId, kMainThreadId,
            nullptr));
      } else {
        AddListener(EventListener::ForExtension(it.key(), extension_id, nullptr,
                                                filter->CreateDeepCopy()));
      }
    }
  }
}

std::set<const EventListener*> EventListenerMap::GetEventListeners(
    const Event& event) {
  std::set<const EventListener*> interested_listeners;
  if (IsFilteredEvent(event)) {
    // Look up the interested listeners via the EventFilter.
    std::set<MatcherID> ids =
        event_filter_.MatchEvent(event.event_name, event.filter_info,
            MSG_ROUTING_NONE);
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
  for (auto& it : listeners_) {
    auto& listener_map = it.second;
    for (auto it2 = listener_map.begin(); it2 != listener_map.end();) {
      if ((*it2)->process() == process) {
        std::unique_ptr<EventListener> listener_removed = std::move(*it2);
        CleanupListener(listener_removed.get());
        it2 = listener_map.erase(it2);
        delegate_->OnListenerRemoved(listener_removed.get());
      } else {
        ++it2;
      }
    }
  }
}

void EventListenerMap::CleanupListener(EventListener* listener) {
  // If the listener doesn't have a filter then we have nothing to clean up.
  if (listener->matcher_id() == -1)
    return;
  event_filter_.RemoveEventMatcher(listener->matcher_id());
  CHECK_EQ(1u, listeners_by_matcher_id_.erase(listener->matcher_id()));
}

bool EventListenerMap::IsFilteredEvent(const Event& event) const {
  return base::Contains(filtered_events_, event.event_name);
}

}  // namespace extensions

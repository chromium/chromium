// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/event_listener_map.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "ipc/constants.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

using MatcherID = EventFilter::MatcherID;

// static
std::unique_ptr<EventListener> EventListener::ForExtension(
    const std::string& event_name,
    const ExtensionId& extension_id,
    content::RenderProcessHost* process,
    std::optional<base::DictValue> filter) {
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
    std::optional<base::DictValue> filter) {
  // Use only the origin to identify the event listener, e.g. chrome://settings
  // for chrome://settings/accounts, to avoid multiple events being triggered
  // for the same process. See crbug.com/40437704 for details.
  // TODO(devlin): If we dispatched events to processes more intelligently this
  // could be avoided.
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
    std::optional<base::DictValue> filter) {
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
    std::optional<base::DictValue> filter) {
  return base::WrapUnique(new EventListener(
      event_name, extension_id, service_worker_scope, /*process=*/nullptr,
      browser_context, is_for_service_worker,
      blink::mojom::kInvalidServiceWorkerVersionId, kMainThreadId,
      std::move(filter)));
}

EventListener::~EventListener() = default;

bool EventListener::EqualsIgnoringFilter(const EventListener* other) const {
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
         worker_thread_id_ == other->worker_thread_id_;
}

bool EventListener::Equals(const EventListener* other) const {
  return EqualsIgnoringFilter(other) && filter_ == other->filter_;
}

std::unique_ptr<EventListener> EventListener::Copy() const {
  std::optional<base::DictValue> filter_copy;
  if (filter_) {
    filter_copy = filter_->Clone();
  }
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
                             std::optional<base::DictValue> filter)
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

EventListenerMap::EventListenerMap(Delegate* delegate) : delegate_(delegate) {}

EventListenerMap::~EventListenerMap() = default;

const EventListenerMap::ListenerList& EventListenerMap::GetEventListenersByName(
    const std::string& event_name) const {
  auto it = listeners_.find(event_name);
  if (it != listeners_.end()) {
    return it->second;
  }
  static const base::NoDestructor<ListenerList> empty_list;
  return *empty_list;
}

bool EventListenerMap::AddListener(std::unique_ptr<EventListener> listener) {
  if (HasListener(listener.get())) {
    return false;
  }
  if (listener->filter()) {
    std::unique_ptr<EventMatcher> matcher(
        ParseEventMatcher(*listener->filter()));
    MatcherID id = event_filter_.AddEventMatcher(listener->event_name(),
                                                 std::move(matcher));
    if (id == -1) {
      return false;
    }
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
    const base::DictValue& filter_dict) {
  return std::make_unique<EventMatcher>(
      std::make_unique<base::DictValue>(filter_dict.Clone()),
      IPC::mojom::kRoutingIdNone);
}

bool EventListenerMap::RemoveListener(const EventListener* listener) {
  auto listener_itr = listeners_.find(listener->event_name());
  if (listener_itr == listeners_.end()) {
    return false;
  }
  ListenerList& listeners = listener_itr->second;
  for (auto& it : listeners) {
    if (it->Equals(listener)) {
      std::vector<MatcherID> matcher_ids_to_remove;
      if (it->matcher_id() != -1) {
        matcher_ids_to_remove.push_back(it->matcher_id());
      }
      event_filter_.RemoveEventMatchers(matcher_ids_to_remove);
      CleanupListener(it.get());
      // Popping from the back should be cheaper than erase(it).
      std::swap(it, listeners.back());
      listeners.pop_back();
      if (listeners.empty()) {
        listeners_.erase(listener_itr);
      }
      delegate_->OnListenerRemoved(listener);
      return true;
    }
  }
  return false;
}

bool EventListenerMap::UpdateFilter(const EventListener& listener) {
  // Only lazy listeners are updated in place.
  CHECK(listener.IsLazy());
  // With the current logic, a filterless replacement would leave
  // `filtered_events_` stale. Handling that correctly needs bookkeeping similar
  // to `CleanupListener()`. But in practice, this method is only reached via
  // `AddFilteredEventListener()` which always provides a filter object, even if
  // it's empty, so we simply CHECK here.
  CHECK(listener.filter());

  auto listener_it = listeners_.find(listener.event_name());
  if (listener_it == listeners_.end()) {
    return false;
  }

  // The prefs-side overwrite for sub-events (crrev.com/c/7859497) keeps each
  // sub-event key to a single filter, so at most one listener can match
  // (ignoring filter).
  CHECK_LE(std::ranges::count_if(listener_it->second,
                                 [&](const auto& e) {
                                   return e->EqualsIgnoringFilter(&listener);
                                 }),
           1);

  for (std::unique_ptr<EventListener>& existing : listener_it->second) {
    if (!existing->EqualsIgnoringFilter(&listener)) {
      continue;
    }
    CHECK(existing->IsLazy());
    if (existing->Equals(&listener)) {
      // Same registration, same filter: nothing to update.
      return true;
    }

    // Tear down the old event matcher, if any. Unlike `RemoveListener()`, the
    // entry stays in the list (with the same event name and a filter), so
    // `filtered_events_` membership is unchanged and `CleanupListener()` is not
    // used here.
    if (existing->matcher_id() != -1) {
      event_filter_.RemoveEventMatchers({existing->matcher_id()});
      CHECK_EQ(1u, listeners_by_matcher_id_.erase(existing->matcher_id()));
    }

    // Replace the stored listener with a copy carrying the new filter,
    // preserving its position in the list.
    existing = listener.Copy();
    EventListener* listener_ptr = existing.get();
    // Set up the new event matcher, if any.
    if (listener_ptr->filter()) {
      MatcherID id = event_filter_.AddEventMatcher(
          listener_ptr->event_name(),
          ParseEventMatcher(*listener_ptr->filter()));
      if (id == -1) {
        return false;
      }
      listener_ptr->set_matcher_id(id);
      listeners_by_matcher_id_[id] = listener_ptr;
      filtered_events_.insert(listener_ptr->event_name());
    }

    delegate_->OnListenerUpdated(listener_ptr);
    return true;
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
  if (it == listeners_.end()) {
    return false;
  }

  for (const auto& listener_to_search : it->second) {
    if (listener_to_search->extension_id() == extension_id) {
      return true;
    }
  }
  return false;
}

bool EventListenerMap::HasListenerForURL(const GURL& url,
                                         const std::string& event_name) const {
  auto it = listeners_.find(event_name);
  if (it == listeners_.end()) {
    return false;
  }

  for (const auto& listener_to_search : it->second) {
    if (url::IsSameOriginWith(listener_to_search->listener_url(), url)) {
      return true;
    }
  }
  return false;
}

bool EventListenerMap::HasListener(const EventListener* listener) const {
  auto it = listeners_.find(listener->event_name());
  if (it == listeners_.end()) {
    return false;
  }

  for (const auto& listener_to_search : it->second) {
    if (listener_to_search->Equals(listener)) {
      return true;
    }
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
  RemoveListenersForExtensionImpl(
      extension_id, /*removal_predicate=*/base::BindRepeating(
          [](const ExtensionId& extension_id, EventListener* listener) {
            return listener->extension_id() == extension_id;
          }));
}

void EventListenerMap::RemoveActiveServiceWorkerListenersForExtension(
    const WorkerId& worker_id) {
  RemoveListenersForExtensionImpl(
      worker_id.extension_id, /*removal_predicate=*/base::BindRepeating(
          [](const WorkerId& worker_id, const ExtensionId& extension_id,
             EventListener* listener) {
            return listener->extension_id() == worker_id.extension_id &&
                   listener->is_for_service_worker() && !listener->IsLazy() &&
                   listener->service_worker_version_id() ==
                       worker_id.version_id &&
                   listener->process()->GetID() ==
                       worker_id.render_process_id &&
                   listener->worker_thread_id() == worker_id.thread_id;
          },
          worker_id));
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
    const base::DictValue& filtered) {
  for (const auto item : filtered) {
    // We skip entries if they are malformed.
    if (!item.second.is_list()) {
      continue;
    }
    for (const base::Value& filter_value : item.second.GetList()) {
      if (!filter_value.is_dict()) {
        continue;
      }
      const base::DictValue& filter = filter_value.GetDict();
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
        event.event_name, *event.filter_info, IPC::mojom::kRoutingIdNone);
    for (const MatcherID& id : ids) {
      auto it = listeners_by_matcher_id_.find(id);
      if (it != listeners_by_matcher_id_.end()) {
        interested_listeners.insert(it->second);
      }
    }
  } else {
    for (const auto& listener : GetEventListenersByName(event.event_name)) {
      interested_listeners.insert(listener.get());
    }
  }

  return interested_listeners;
}

void EventListenerMap::RemoveListenersForProcess(
    const content::RenderProcessHost* process) {
  CHECK(process);
  std::vector<MatcherID> matcher_ids_to_remove;
  for (const auto& [event_name, listener_list] : listeners_) {
    for (const auto& listener : listener_list) {
      if (listener->process() == process && listener->matcher_id() != -1) {
        matcher_ids_to_remove.push_back(listener->matcher_id());
      }
    }
  }
  event_filter_.RemoveEventMatchers(matcher_ids_to_remove);

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
    if (listener_list.empty()) {
      it = listeners_.erase(it);
    } else {
      ++it;
    }
  }
}

void EventListenerMap::RemoveListenersForExtensionImpl(
    const ExtensionId& extension_id,
    base::RepeatingCallback<bool(const ExtensionId&, EventListener*)>
        removal_predicate) {
  std::vector<MatcherID> matcher_ids_to_remove;
  for (const auto& [event_name, listener_list] : listeners_) {
    for (const auto& listener : listener_list) {
      if (removal_predicate.Run(extension_id, listener.get()) &&
          listener->matcher_id() != -1) {
        matcher_ids_to_remove.push_back(listener->matcher_id());
      }
    }
  }
  event_filter_.RemoveEventMatchers(matcher_ids_to_remove);

  for (auto it = listeners_.begin(); it != listeners_.end();) {
    auto& listener_list = it->second;
    for (auto it2 = listener_list.begin(); it2 != listener_list.end();) {
      if (removal_predicate.Run(extension_id, it2->get())) {
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
    if (listener_list.empty()) {
      it = listeners_.erase(it);
    } else {
      ++it;
    }
  }
}

void EventListenerMap::CleanupListener(EventListener* listener) {
  // If the listener doesn't have a filter then we have nothing to clean up.
  if (listener->matcher_id() == -1) {
    return;
  }
  // If we're removing the final listener for an event, we can remove the
  // entry from |filtered_events_|, as well.
  auto iter = listeners_.find(listener->event_name());
  if (iter->second.size() == 1) {
    filtered_events_.erase(iter->first);
  }
  CHECK_EQ(1u, listeners_by_matcher_id_.erase(listener->matcher_id()));
}

bool EventListenerMap::IsFilteredEvent(const Event& event) const {
  return filtered_events_.contains(event.event_name);
}

}  // namespace extensions

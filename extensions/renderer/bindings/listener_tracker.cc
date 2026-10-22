// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/listener_tracker.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/common/value_counter.h"

namespace extensions {

ListenerTracker::ListenerTracker() = default;
ListenerTracker::~ListenerTracker() = default;

bool ListenerTracker::AddUnfilteredListener(const std::string& context_owner_id,
                                            const std::string& event_name) {
  ListenerCountMap& listeners = unfiltered_listeners_[context_owner_id];
  return ++listeners[event_name] == 1;
}

bool ListenerTracker::RemoveUnfilteredListener(
    const std::string& context_owner_id,
    const std::string& event_name) {
  ListenerCountMap& listeners = unfiltered_listeners_[context_owner_id];
  auto iter = listeners.find(event_name);
  CHECK(iter != listeners.end());
  if (--(iter->second) == 0) {
    listeners.erase(iter);
    return true;
  }
  return false;
}

std::pair<bool, int> ListenerTracker::AddFilteredListener(
    const std::string& context_owner_id,
    const std::string& event_name,
    base::DictValue filter,
    int routing_id) {
  int filter_id = event_filter_.AddEventMatcher(
      event_name,
      std::make_unique<EventMatcher>(std::move(filter), routing_id));
  if (filter_id == -1) {
    return std::make_pair(false, -1);
  }

  FilteredEventListenerKey key(context_owner_id, event_name);
  std::unique_ptr<ValueCounter>& counts = filtered_listeners_[key];
  if (!counts) {
    counts = std::make_unique<ValueCounter>();
  }

  const EventMatcher* matcher = event_filter_.GetEventMatcher(filter_id);
  bool was_first_of_kind = counts->Add(base::Value(matcher->value().Clone()));
  return std::make_pair(was_first_of_kind, filter_id);
}

std::pair<bool, base::DictValue> ListenerTracker::RemoveFilteredListener(
    const std::string& context_owner_id,
    const std::string& event_name,
    int filter_id) {
  EventMatcher* matcher = event_filter_.GetEventMatcher(filter_id);
  DCHECK(matcher);

  FilteredEventListenerKey key(context_owner_id, event_name);
  FilteredListeners::const_iterator counts = filtered_listeners_.find(key);

  bool was_last_of_kind = false;
  CHECK(counts != filtered_listeners_.end());
  base::Value filter_copy = base::Value(matcher->value().Clone());
  if (counts->second->Remove(filter_copy)) {
    if (counts->second->is_empty()) {
      // Clean up if there are no more filters.
      filtered_listeners_.erase(counts);
    }
    was_last_of_kind = true;
  }

  event_filter_.RemoveEventMatcher(filter_id, nullptr);
  return std::make_pair(was_last_of_kind, std::move(filter_copy).TakeDict());
}

const base::DictValue& ListenerTracker::GetFilter(int filter_id) {
  const EventMatcher* matcher = event_filter_.GetEventMatcher(filter_id);
  CHECK(matcher);
  return matcher->value();
}

std::set<int> ListenerTracker::GetMatchingFilteredListeners(
    const std::string& event_name,
    mojom::EventFilteringInfoPtr filter,
    int routing_id) {
  DCHECK(!filter.is_null());
  return event_filter_.MatchEvent(event_name, *filter, routing_id);
}

void ListenerTracker::RecordFilteredListenerCounts() const {
  for (const auto& [key, counts] : filtered_listeners_) {
    const size_t total = counts->total_count();
    const size_t deduplicated = total - counts->distinct_count();
    base::UmaHistogramCounts100(
        "Extensions.Events.ServiceWorkerStarted.FilteredListenerCount", total);
    base::UmaHistogramCounts100(
        "Extensions.Events.ServiceWorkerStarted.DeduplicatedListenerCount",
        deduplicated);
  }
}

}  // namespace extensions

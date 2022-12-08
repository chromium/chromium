// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_LISTENER_TRACKER_H_
#define EXTENSIONS_RENDERER_BINDINGS_LISTENER_TRACKER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/values.h"
#include "extensions/common/event_filter.h"
#include "extensions/common/mojom/event_dispatcher.mojom-forward.h"

namespace extensions {
class EventFilter;
class ValueCounter;

// A class to track all event listeners across multiple v8::Contexts. Each
// context has a "context owner", which may be the same across multiple
// contexts. For instance, an extension may listen to the same event in
// multiple pages. Since these contexts may have the same context owner,
// tracking when a new listener is added requires looking at more than a
// single context.
//
// TODO(devlin): We should incorporate the notifications for newly added/
// removed listeners into this class, rather than having callers worry about
// it based on return values.
class ListenerTracker {
 public:
  ListenerTracker();

  ListenerTracker(const ListenerTracker&) = delete;
  ListenerTracker& operator=(const ListenerTracker&) = delete;

  ~ListenerTracker();

  // Adds a record of an unfiltered listener for the given |event_name|,
  // associated with the given |context_owner_id|.
  // Returns true if this was the first listener for this event by this
  // |context_owner_id| across all contexts.
  // Note that unfiltered listeners should only be added once per unique
  // context; callers are responsible for ensuring this isn't called for
  // multiple listeners in the same context (though it may be for the same
  // context owner).
  bool AddUnfilteredListener(const std::string& context_owner_id,
                             const std::string& event_name);

  // Removes a record of an unfiltered listener for the given |event_name|,
  // associated with the given |context_owner_id|.
  // Returns true if this was the last listener for this event by this
  // |context_owner_id| across all contexts.
  bool RemoveUnfilteredListener(const std::string& context_owner_id,
                                const std::string& event_name);

  // Adds a record of a filtered listener for the given |event_name|,
  // associated with the given |context_owner_id| and with the given |filter|
  // and |routing_id|. Returns a pair, with the bool indicating if this was the
  // first listener added for this event and |context_owner_id| with this
  // specific filter, and an integer for the filter ID. If the filter could not
  // be added (i.e., it was invalid), the filter ID will be -1, and no listener
  // will have been added.
  std::pair<bool, int> AddFilteredListener(
      const std::string& context_owner_id,
      const std::string& event_name,
      std::unique_ptr<base::Value::Dict> filter,
      int routing_id);

  // Removes a record of a filtered listener for the given |event_name|,
  // associated with the given |context_owner_id| and |filter_id|. DCHECKs that
  // such a listener exists.
  // Returns a pair, with the bool indicating if this was the last listener
  // added for this event and |context_owner_id| with this specific filter, and
  // a copy of the filter value.
  std::pair<bool, std::unique_ptr<base::Value::Dict>> RemoveFilteredListener(
      const std::string& context_owner_id,
      const std::string& event_name,
      int filter_id);

  // Returns a set of filter IDs to that correspond to the given |event_name|,
  // |filter|, and |routing_id|.
  std::set<int> GetMatchingFilteredListeners(
      const std::string& event_name,
      mojom::EventFilteringInfoPtr filter,
      int routing_id);

  EventFilter* event_filter_for_testing() { return &event_filter_; }

 private:
  // A map of event name to the number of different contexts listening to that
  // event.
  using ListenerCountMap = std::map<std::string, int>;
  // A map of context owner to the listener counts for all events.
  using UnfilteredListeners = std::map<std::string, ListenerCountMap>;

  // A key for a filtered listener; a pair of <context owner, event name>.
  using FilteredEventListenerKey = std::pair<std::string, std::string>;
  // A map of filtered event listeners, mapping the key to a counter to track
  // the number of listeners with given filters.
  using FilteredListeners =
      std::map<FilteredEventListenerKey, std::unique_ptr<ValueCounter>>;

  UnfilteredListeners unfiltered_listeners_;
  FilteredListeners filtered_listeners_;

  // The event filter.
  EventFilter event_filter_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_LISTENER_TRACKER_H_

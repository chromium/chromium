// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EVENT_FILTER_H_
#define EXTENSIONS_COMMON_EVENT_FILTER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/url_matcher/url_matcher.h"
#include "extensions/common/event_filtering_info.h"
#include "extensions/common/event_matcher.h"

namespace extensions {

// Matches incoming events against a collection of EventMatchers. Each added
// EventMatcher is given an id which is returned by MatchEvent() when it is
// passed a matching event.
class EventFilter {
 public:
  typedef int MatcherID;
  EventFilter();
  ~EventFilter();

  // Adds an event matcher that will be used in calls to MatchEvent(). Returns
  // the id of the matcher, or -1 if there was an error.
  MatcherID AddEventMatcher(const std::string& event_name,
                            std::unique_ptr<EventMatcher> matcher);

  // Retrieve the EventMatcher with the given id.
  EventMatcher* GetEventMatcher(MatcherID id);

  // Retrieve the name of the event that the EventMatcher specified by |id| is
  // referring to.
  const std::string& GetEventName(MatcherID id) const;

  // Removes an event matcher, returning the name of the event that it was for.
  std::string RemoveEventMatcher(MatcherID id);

  // Match an event named |event_name| with filtering info |event_info| against
  // our set of event matchers. Returns a set of ids that correspond to the
  // event matchers that matched the event.
  // TODO(koz): Add a std::string* parameter for retrieving error messages.
  std::set<MatcherID> MatchEvent(const std::string& event_name,
                                 const EventFilteringInfo& event_info,
                                 int routing_id) const;

  int GetMatcherCountForEventForTesting(const std::string& event_name) const;

  bool IsURLMatcherEmptyForTesting() const { return url_matcher_.IsEmpty(); }

 private:
  class EventMatcherEntry {
   public:
    // Adds |condition_sets| to |url_matcher| on construction and removes them
    // again on destruction. |condition_sets| should be the
    // URLMatcherConditionSets that match the URL constraints specified by
    // |event_matcher|.
    EventMatcherEntry(
        std::unique_ptr<EventMatcher> event_matcher,
        url_matcher::URLMatcher* url_matcher,
        const url_matcher::URLMatcherConditionSet::Vector& condition_sets);
    ~EventMatcherEntry();

    // Prevents the removal of condition sets when this class is destroyed. We
    // call this in EventFilter's destructor so that we don't do the costly
    // removal of condition sets when the URLMatcher is going to be destroyed
    // and clean them up anyway.
    void DontRemoveConditionSetsInDestructor();

    EventMatcher* event_matcher() {
      return event_matcher_.get();
    }

   private:
    std::unique_ptr<EventMatcher> event_matcher_;
    // The id sets in |url_matcher_| that this EventMatcher owns.
    std::vector<url_matcher::URLMatcherConditionSet::ID> condition_set_ids_;
    url_matcher::URLMatcher* url_matcher_;

    DISALLOW_COPY_AND_ASSIGN(EventMatcherEntry);
  };

  // Maps from a matcher id to an event matcher entry.
  using EventMatcherMap =
      std::map<MatcherID, std::unique_ptr<EventMatcherEntry>>;

  // Maps from event name to the map of matchers that are registered for it.
  using EventMatcherMultiMap = std::map<std::string, EventMatcherMap>;

  // Adds the list of URL filters in |matcher| to the URL matcher.
  bool CreateConditionSets(
      EventMatcher* matcher,
      url_matcher::URLMatcherConditionSet::Vector* condition_sets);

  bool AddDictionaryAsConditionSet(
      base::DictionaryValue* url_filter,
      url_matcher::URLMatcherConditionSet::Vector* condition_sets);

  url_matcher::URLMatcher url_matcher_;
  EventMatcherMultiMap event_matchers_;

  // The next id to assign to an EventMatcher.
  MatcherID next_id_;

  // The next id to assign to a condition set passed to URLMatcher.
  url_matcher::URLMatcherConditionSet::ID next_condition_set_id_;

  // Maps condition set ids, which URLMatcher operates in, to event matcher
  // ids, which the interface to this class operates in. As each EventFilter
  // can specify many condition sets this is a many to one relationship.
  std::map<url_matcher::URLMatcherConditionSet::ID, MatcherID>
      condition_set_id_to_event_matcher_id_;

  // Maps from event matcher ids to the name of the event they match on.
  std::map<MatcherID, std::string> id_to_event_name_;

  DISALLOW_COPY_AND_ASSIGN(EventFilter);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EVENT_FILTER_H_

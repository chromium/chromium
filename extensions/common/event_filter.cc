// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/event_filter.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "components/url_matcher/url_matcher_factory.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "ipc/ipc_message.h"

using url_matcher::URLMatcher;
using url_matcher::URLMatcherConditionSet;
using url_matcher::URLMatcherFactory;

namespace extensions {

EventFilter::EventMatcherEntry::EventMatcherEntry(
    std::unique_ptr<EventMatcher> event_matcher,
    URLMatcher* url_matcher,
    const URLMatcherConditionSet::Vector& condition_sets)
    : event_matcher_(std::move(event_matcher)), url_matcher_(url_matcher) {
  condition_set_ids_.reserve(condition_sets.size());
  for (const scoped_refptr<URLMatcherConditionSet>& condition_set :
       condition_sets) {
    condition_set_ids_.push_back(condition_set->id());
  }
  url_matcher_->AddConditionSets(condition_sets);
}

EventFilter::EventMatcherEntry::~EventMatcherEntry() {
  url_matcher_->RemoveConditionSets(condition_set_ids_);
}

void EventFilter::EventMatcherEntry::DontRemoveConditionSetsInDestructor() {
  condition_set_ids_.clear();
}

EventFilter::EventFilter()
    : next_id_(0),
      next_condition_set_id_(0) {
}

EventFilter::~EventFilter() {
  // Normally when an event matcher entry is removed from event_matchers_ it
  // will remove its condition sets from url_matcher_, but as url_matcher_ is
  // being destroyed anyway there is no need to do that step here.
  for (auto& matcher_map : event_matchers_) {
    for (auto& matcher : matcher_map.second)
      matcher.second->DontRemoveConditionSetsInDestructor();
  }
}

EventFilter::MatcherID EventFilter::AddEventMatcher(
    const std::string& event_name,
    std::unique_ptr<EventMatcher> matcher) {
  URLMatcherConditionSet::Vector condition_sets;
  if (!CreateConditionSets(matcher.get(), &condition_sets))
    return -1;

  MatcherID id = next_id_++;
  for (const scoped_refptr<URLMatcherConditionSet>& condition_set :
       condition_sets) {
    condition_set_id_to_event_matcher_id_.insert(
        std::make_pair(condition_set->id(), id));
  }
  id_to_event_name_[id] = event_name;
  event_matchers_[event_name][id] = std::make_unique<EventMatcherEntry>(
      std::move(matcher), &url_matcher_, condition_sets);
  return id;
}

EventMatcher* EventFilter::GetEventMatcher(MatcherID id) {
  const std::string& event_name = GetEventName(id);
  return event_matchers_[event_name][id]->event_matcher();
}

const std::string& EventFilter::GetEventName(MatcherID id) const {
  auto it = id_to_event_name_.find(id);
  CHECK(it != id_to_event_name_.end(), base::NotFatalUntil::M130);
  return it->second;
}

bool EventFilter::CreateConditionSets(
    EventMatcher* matcher,
    URLMatcherConditionSet::Vector* condition_sets) {
  int url_filter_count = matcher->GetURLFilterCount();
  if (url_filter_count == 0) {
    // If there are no URL filters then we want to match all events, so create a
    // URLFilter from an empty dictionary.
    base::Value::Dict empty_dict;
    return AddDictionaryAsConditionSet(empty_dict, condition_sets);
  }
  for (int i = 0; i < url_filter_count; i++) {
    const base::Value::Dict* url_filter = matcher->GetURLFilter(i);
    if (!url_filter)
      return false;
    if (!AddDictionaryAsConditionSet(*url_filter, condition_sets))
      return false;
  }
  return true;
}

bool EventFilter::AddDictionaryAsConditionSet(
    const base::Value::Dict& url_filter,
    URLMatcherConditionSet::Vector* condition_sets) {
  std::string error;
  base::MatcherStringPattern::ID condition_set_id = next_condition_set_id_++;
  condition_sets->push_back(URLMatcherFactory::CreateFromURLFilterDictionary(
      url_matcher_.condition_factory(),
      url_filter,
      condition_set_id,
      &error));
  if (!error.empty()) {
    LOG(ERROR) << "CreateFromURLFilterDictionary failed: " << error;
    url_matcher_.ClearUnusedConditionSets();
    condition_sets->clear();
    return false;
  }
  return true;
}

std::string EventFilter::RemoveEventMatcher(MatcherID id) {
  auto it = id_to_event_name_.find(id);
  if (it == id_to_event_name_.end()) {
    return "";
  }

  std::string event_name = it->second;
  // EventMatcherEntry's destructor causes the condition set ids to be removed
  // from url_matcher_.
  event_matchers_[event_name].erase(id);
  id_to_event_name_.erase(it);
  return event_name;
}

std::set<EventFilter::MatcherID> EventFilter::MatchEvent(
    const std::string& event_name,
    const mojom::EventFilteringInfo& event_info,
    int routing_id) const {
  std::set<MatcherID> matchers;

  auto it = event_matchers_.find(event_name);
  if (it == event_matchers_.end())
    return matchers;

  const EventMatcherMap& matcher_map = it->second;
  const GURL& url_to_match_against = event_info.url ? *event_info.url : GURL();
  std::set<base::MatcherStringPattern::ID> matching_condition_set_ids =
      url_matcher_.MatchURL(url_to_match_against);
  for (const auto& id_key : matching_condition_set_ids) {
    auto matcher_id = condition_set_id_to_event_matcher_id_.find(id_key);
    if (matcher_id == condition_set_id_to_event_matcher_id_.end()) {
      NOTREACHED_IN_MIGRATION()
          << "id not found in condition set map (" << id_key << ")";
      continue;
    }
    MatcherID id = matcher_id->second;
    auto matcher_entry = matcher_map.find(id);
    if (matcher_entry == matcher_map.end()) {
      // Matcher must be for a different event.
      continue;
    }
    const EventMatcher* event_matcher = matcher_entry->second->event_matcher();
    // The context that installed the event listener should be the same context
    // as the one where the event listener is called.
    if (routing_id != MSG_ROUTING_NONE &&
        event_matcher->routing_id() != routing_id) {
      continue;
    }
    if (event_matcher->MatchNonURLCriteria(event_info)) {
      CHECK(!event_matcher->HasURLFilters() || event_info.url);
      matchers.insert(id);
    }
  }

  return matchers;
}

int EventFilter::GetMatcherCountForEventForTesting(
    const std::string& name) const {
  auto it = event_matchers_.find(name);
  return it != event_matchers_.end() ? it->second.size() : 0;
}

}  // namespace extensions

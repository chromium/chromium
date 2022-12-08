// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/event_filter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/values.h"
#include "extensions/common/event_matcher.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class EventFilterUnittest : public testing::Test {
 public:
  EventFilterUnittest() {
    google_event_.url = GURL("http://google.com");
    yahoo_event_.url = GURL("http://yahoo.com");
    random_url_event_.url = GURL("http://www.something-else.com");
    empty_url_event_.url = GURL();
  }

 protected:
  base::Value::Dict HostSuffixDict(const std::string& host_suffix) {
    base::Value::Dict dict;
    dict.Set("hostSuffix", host_suffix);
    return dict;
  }

  base::Value::List ValueAsList(base::Value value) {
    base::Value::List result;
    result.Append(std::move(value));
    return result;
  }

  std::unique_ptr<EventMatcher> AllURLs() {
    return std::make_unique<EventMatcher>(std::make_unique<base::Value::Dict>(),
                                          MSG_ROUTING_NONE);
  }

  std::unique_ptr<EventMatcher> HostSuffixMatcher(
      const std::string& host_suffix) {
    return MatcherFromURLFilterList(
        ValueAsList(base::Value(HostSuffixDict(host_suffix))));
  }

  std::unique_ptr<EventMatcher> MatcherFromURLFilterList(
      base::Value::List url_filter_list) {
    auto filter_dict = std::make_unique<base::Value::Dict>();
    filter_dict->Set("url", base::Value(std::move(url_filter_list)));
    return std::make_unique<EventMatcher>(std::move(filter_dict),
                                          MSG_ROUTING_NONE);
  }

  EventFilter event_filter_;
  mojom::EventFilteringInfo empty_event_;
  mojom::EventFilteringInfo google_event_;
  mojom::EventFilteringInfo yahoo_event_;
  mojom::EventFilteringInfo random_url_event_;
  mojom::EventFilteringInfo empty_url_event_;
};

TEST_F(EventFilterUnittest, NoMatchersMatchIfEmpty) {
  std::set<int> matches =
      event_filter_.MatchEvent("some-event", empty_event_, MSG_ROUTING_NONE);
  ASSERT_EQ(0u, matches.size());
}

TEST_F(EventFilterUnittest, AddingEventMatcherDoesntCrash) {
  event_filter_.AddEventMatcher("event1", AllURLs());
}

TEST_F(EventFilterUnittest,
    DontMatchAgainstMatchersForDifferentEvents) {
  event_filter_.AddEventMatcher("event1", AllURLs());
  std::set<int> matches =
      event_filter_.MatchEvent("event2", empty_event_, MSG_ROUTING_NONE);
  ASSERT_EQ(0u, matches.size());
}

TEST_F(EventFilterUnittest, DoMatchAgainstMatchersForSameEvent) {
  int id = event_filter_.AddEventMatcher("event1", AllURLs());
  std::set<int> matches =
      event_filter_.MatchEvent("event1", google_event_, MSG_ROUTING_NONE);
  ASSERT_EQ(1u, matches.size());
  ASSERT_EQ(1u, matches.count(id));
}

TEST_F(EventFilterUnittest, DontMatchUnlessMatcherMatches) {
  mojom::EventFilteringInfo info;
  info.url = GURL("http://www.yahoo.com");
  event_filter_.AddEventMatcher("event1", HostSuffixMatcher("google.com"));
  std::set<int> matches =
      event_filter_.MatchEvent("event1", info, MSG_ROUTING_NONE);
  ASSERT_TRUE(matches.empty());
}

TEST_F(EventFilterUnittest, RemovingAnEventMatcherStopsItMatching) {
  int id = event_filter_.AddEventMatcher("event1", AllURLs());
  event_filter_.RemoveEventMatcher(id);
  std::set<int> matches =
      event_filter_.MatchEvent("event1", empty_event_, MSG_ROUTING_NONE);
  ASSERT_TRUE(matches.empty());
}

TEST_F(EventFilterUnittest, MultipleEventMatches) {
  int id1 = event_filter_.AddEventMatcher("event1", AllURLs());
  int id2 = event_filter_.AddEventMatcher("event1", AllURLs());
  std::set<int> matches =
      event_filter_.MatchEvent("event1", google_event_, MSG_ROUTING_NONE);
  ASSERT_EQ(2u, matches.size());
  ASSERT_EQ(1u, matches.count(id1));
  ASSERT_EQ(1u, matches.count(id2));
}

TEST_F(EventFilterUnittest, TestURLMatching) {
  mojom::EventFilteringInfo info;
  info.url = GURL("http://www.google.com");
  int id = event_filter_.AddEventMatcher("event1",
                                         HostSuffixMatcher("google.com"));
  std::set<int> matches =
      event_filter_.MatchEvent("event1", info, MSG_ROUTING_NONE);
  ASSERT_EQ(1u, matches.size());
  ASSERT_EQ(1u, matches.count(id));
}

TEST_F(EventFilterUnittest, TestMultipleURLFiltersMatchOnAny) {
  base::Value::List filters;
  filters.Append(HostSuffixDict("google.com"));
  filters.Append(HostSuffixDict("yahoo.com"));

  std::unique_ptr<EventMatcher> matcher(
      MatcherFromURLFilterList(std::move(filters)));
  int id = event_filter_.AddEventMatcher("event1", std::move(matcher));

  {
    std::set<int> matches =
        event_filter_.MatchEvent("event1", google_event_, MSG_ROUTING_NONE);
    ASSERT_EQ(1u, matches.size());
    ASSERT_EQ(1u, matches.count(id));
  }
  {
    std::set<int> matches =
        event_filter_.MatchEvent("event1", yahoo_event_, MSG_ROUTING_NONE);
    ASSERT_EQ(1u, matches.size());
    ASSERT_EQ(1u, matches.count(id));
  }
  {
    std::set<int> matches =
        event_filter_.MatchEvent("event1", random_url_event_, MSG_ROUTING_NONE);
    ASSERT_EQ(0u, matches.size());
  }
}

TEST_F(EventFilterUnittest, TestStillMatchesAfterRemoval) {
  int id1 = event_filter_.AddEventMatcher("event1", AllURLs());
  int id2 = event_filter_.AddEventMatcher("event1", AllURLs());

  event_filter_.RemoveEventMatcher(id1);
  {
    std::set<int> matches =
        event_filter_.MatchEvent("event1", google_event_, MSG_ROUTING_NONE);
    ASSERT_EQ(1u, matches.size());
    ASSERT_EQ(1u, matches.count(id2));
  }
}

TEST_F(EventFilterUnittest, TestMatchesOnlyAgainstPatternsForCorrectEvent) {
  int id1 = event_filter_.AddEventMatcher("event1", AllURLs());
  event_filter_.AddEventMatcher("event2", AllURLs());

  {
    std::set<int> matches =
        event_filter_.MatchEvent("event1", google_event_, MSG_ROUTING_NONE);
    ASSERT_EQ(1u, matches.size());
    ASSERT_EQ(1u, matches.count(id1));
  }
}

TEST_F(EventFilterUnittest, TestGetMatcherCountForEvent) {
  ASSERT_EQ(0, event_filter_.GetMatcherCountForEventForTesting("event1"));
  int id1 = event_filter_.AddEventMatcher("event1", AllURLs());
  ASSERT_EQ(1, event_filter_.GetMatcherCountForEventForTesting("event1"));
  int id2 = event_filter_.AddEventMatcher("event1", AllURLs());
  ASSERT_EQ(2, event_filter_.GetMatcherCountForEventForTesting("event1"));
  event_filter_.RemoveEventMatcher(id1);
  ASSERT_EQ(1, event_filter_.GetMatcherCountForEventForTesting("event1"));
  event_filter_.RemoveEventMatcher(id2);
  ASSERT_EQ(0, event_filter_.GetMatcherCountForEventForTesting("event1"));
}

TEST_F(EventFilterUnittest, RemoveEventMatcherReturnsEventName) {
  int id1 = event_filter_.AddEventMatcher("event1", AllURLs());
  int id2 = event_filter_.AddEventMatcher("event1", AllURLs());
  int id3 = event_filter_.AddEventMatcher("event2", AllURLs());

  ASSERT_EQ("event1", event_filter_.RemoveEventMatcher(id1));
  ASSERT_EQ("event1", event_filter_.RemoveEventMatcher(id2));
  ASSERT_EQ("event2", event_filter_.RemoveEventMatcher(id3));
}

TEST_F(EventFilterUnittest, InvalidURLFilterCantBeAdded) {
  base::Value::List filter_list;
  filter_list.Append(base::Value::List());  // Should be a dict.
  std::unique_ptr<EventMatcher> matcher(
      MatcherFromURLFilterList(std::move(filter_list)));
  int id1 = event_filter_.AddEventMatcher("event1", std::move(matcher));
  EXPECT_TRUE(event_filter_.IsURLMatcherEmptyForTesting());
  ASSERT_EQ(-1, id1);
}

TEST_F(EventFilterUnittest, EmptyListOfURLFiltersMatchesAllURLs) {
  std::unique_ptr<EventMatcher> matcher(
      MatcherFromURLFilterList(base::Value::List()));
  int id = event_filter_.AddEventMatcher("event1", std::move(matcher));
  std::set<int> matches =
      event_filter_.MatchEvent("event1", google_event_, MSG_ROUTING_NONE);
  ASSERT_EQ(1u, matches.size());
  ASSERT_EQ(1u, matches.count(id));
}

TEST_F(EventFilterUnittest,
    InternalURLMatcherShouldBeEmptyWhenThereAreNoEventMatchers) {
  ASSERT_TRUE(event_filter_.IsURLMatcherEmptyForTesting());
  int id = event_filter_.AddEventMatcher("event1",
                                         HostSuffixMatcher("google.com"));
  ASSERT_FALSE(event_filter_.IsURLMatcherEmptyForTesting());
  event_filter_.RemoveEventMatcher(id);
  ASSERT_TRUE(event_filter_.IsURLMatcherEmptyForTesting());
}

TEST_F(EventFilterUnittest, EmptyURLsShouldBeMatchedByEmptyURLFilters) {
  int id = event_filter_.AddEventMatcher("event1", AllURLs());
  std::set<int> matches =
      event_filter_.MatchEvent("event1", empty_url_event_, MSG_ROUTING_NONE);
  ASSERT_EQ(1u, matches.size());
  ASSERT_EQ(1u, matches.count(id));
}

TEST_F(EventFilterUnittest,
    EmptyURLsShouldBeMatchedByEmptyURLFiltersWithAnEmptyItem) {
  std::unique_ptr<EventMatcher> matcher(
      MatcherFromURLFilterList(ValueAsList(base::Value(base::Value::Dict()))));
  int id = event_filter_.AddEventMatcher("event1", std::move(matcher));
  std::set<int> matches =
      event_filter_.MatchEvent("event1", empty_url_event_, MSG_ROUTING_NONE);
  ASSERT_EQ(1u, matches.size());
  ASSERT_EQ(1u, matches.count(id));
}

}  // namespace extensions

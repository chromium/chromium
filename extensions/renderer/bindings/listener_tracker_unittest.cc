// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/listener_tracker.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

constexpr char kEvent1[] = "Event1";
constexpr char kEvent2[] = "Event2";
constexpr char kOwner1[] = "Owner1";
constexpr char kOwner2[] = "Owner2";
constexpr int kRoutingId = 0;

}  // namespace

TEST(ListenerTrackerTest, UnfilteredListeners) {
  ListenerTracker tracker;

  // Add two listeners for the same event with the same owner. Only the
  // first added should return true.
  EXPECT_TRUE(tracker.AddUnfilteredListener(kOwner1, kEvent1));
  EXPECT_FALSE(tracker.AddUnfilteredListener(kOwner1, kEvent1));

  EXPECT_TRUE(tracker.AddUnfilteredListener(kOwner1, kEvent2));
  EXPECT_TRUE(tracker.AddUnfilteredListener(kOwner2, kEvent1));

  EXPECT_FALSE(tracker.RemoveUnfilteredListener(kOwner1, kEvent1));
  EXPECT_TRUE(tracker.RemoveUnfilteredListener(kOwner1, kEvent1));

  EXPECT_TRUE(tracker.RemoveUnfilteredListener(kOwner1, kEvent2));
  EXPECT_TRUE(tracker.RemoveUnfilteredListener(kOwner2, kEvent1));
}

TEST(ListenerTrackerTest, FilteredListenersWithMultipleFilters) {
  base::Value::Dict filter1 =
      DictValueFromString(R"({"url": [{"hostSuffix": "example.com"}]})");
  base::Value::Dict filter2 =
      DictValueFromString(R"({"url": [{"hostSuffix": "google.com"}]})");

  ListenerTracker tracker;
  auto [was_first_of_kind, filter_id1] = tracker.AddFilteredListener(
      kOwner1, kEvent1, std::make_unique<base::Value::Dict>(filter1.Clone()),
      kRoutingId);
  EXPECT_TRUE(was_first_of_kind);
  EXPECT_NE(-1, filter_id1);

  int filter_id2 = -1;
  std::tie(was_first_of_kind, filter_id2) = tracker.AddFilteredListener(
      kOwner1, kEvent1, std::make_unique<base::Value::Dict>(filter1.Clone()),
      kRoutingId);
  EXPECT_FALSE(was_first_of_kind);
  EXPECT_NE(-1, filter_id2);
  EXPECT_NE(filter_id1, filter_id2);

  int filter_id3 = -1;
  std::tie(was_first_of_kind, filter_id3) = tracker.AddFilteredListener(
      kOwner1, kEvent1, std::make_unique<base::Value::Dict>(filter2.Clone()),
      kRoutingId);
  EXPECT_TRUE(was_first_of_kind);
  EXPECT_NE(-1, filter_id3);

  auto [was_last_of_kind, removed_filter] =
      tracker.RemoveFilteredListener(kOwner1, kEvent1, filter_id1);
  EXPECT_FALSE(was_last_of_kind);
  ASSERT_TRUE(removed_filter);
  EXPECT_EQ(ValueToString(*removed_filter), ValueToString(filter1));

  std::tie(was_last_of_kind, removed_filter) =
      tracker.RemoveFilteredListener(kOwner1, kEvent1, filter_id2);
  EXPECT_TRUE(was_last_of_kind);
  ASSERT_TRUE(removed_filter);
  EXPECT_EQ(ValueToString(*removed_filter), ValueToString(filter1));

  std::tie(was_last_of_kind, removed_filter) =
      tracker.RemoveFilteredListener(kOwner1, kEvent1, filter_id3);
  EXPECT_TRUE(was_last_of_kind);
  ASSERT_TRUE(removed_filter);
  EXPECT_EQ(ValueToString(*removed_filter), ValueToString(filter2));
}

TEST(ListenerTrackerTest, FilteredListenersWithMultipleOwners) {
  base::Value::Dict filter =
      DictValueFromString(R"({"url": [{"hostSuffix": "example.com"}]})");

  ListenerTracker tracker;
  auto [was_first_of_kind, filter_id1] = tracker.AddFilteredListener(
      kOwner1, kEvent1, std::make_unique<base::Value::Dict>(filter.Clone()),
      kRoutingId);
  EXPECT_TRUE(was_first_of_kind);
  EXPECT_NE(-1, filter_id1);

  int filter_id2 = -1;
  std::tie(was_first_of_kind, filter_id2) = tracker.AddFilteredListener(
      kOwner2, kEvent1, std::make_unique<base::Value::Dict>(filter.Clone()),
      kRoutingId);
  EXPECT_TRUE(was_first_of_kind);
  EXPECT_NE(-1, filter_id2);
  EXPECT_NE(filter_id1, filter_id2);

  auto [was_last_of_kind, removed_filter] =
      tracker.RemoveFilteredListener(kOwner1, kEvent1, filter_id1);
  EXPECT_TRUE(was_last_of_kind);
  ASSERT_TRUE(removed_filter);
  EXPECT_EQ(ValueToString(*removed_filter), ValueToString(filter));

  std::tie(was_last_of_kind, removed_filter) =
      tracker.RemoveFilteredListener(kOwner2, kEvent1, filter_id2);
  EXPECT_TRUE(was_last_of_kind);
  ASSERT_TRUE(removed_filter);
  EXPECT_EQ(ValueToString(*removed_filter), ValueToString(filter));
}

TEST(ListenerTrackerTest, FilteredListenersWithMultipleEvents) {
  base::Value::Dict filter =
      DictValueFromString(R"({"url": [{"hostSuffix": "example.com"}]})");

  ListenerTracker tracker;
  auto [was_first_of_kind, filter_id1] = tracker.AddFilteredListener(
      kOwner1, kEvent1, std::make_unique<base::Value::Dict>(filter.Clone()),
      kRoutingId);
  EXPECT_TRUE(was_first_of_kind);
  EXPECT_NE(-1, filter_id1);

  int filter_id2 = -1;
  std::tie(was_first_of_kind, filter_id2) = tracker.AddFilteredListener(
      kOwner1, kEvent2, std::make_unique<base::Value::Dict>(filter.Clone()),
      kRoutingId);
  EXPECT_TRUE(was_first_of_kind);
  EXPECT_NE(-1, filter_id2);
  EXPECT_NE(filter_id1, filter_id2);

  auto [was_last_of_kind, removed_filter] =
      tracker.RemoveFilteredListener(kOwner1, kEvent1, filter_id1);
  EXPECT_TRUE(was_last_of_kind);
  ASSERT_TRUE(removed_filter);
  EXPECT_EQ(ValueToString(*removed_filter), ValueToString(filter));

  std::tie(was_last_of_kind, removed_filter) =
      tracker.RemoveFilteredListener(kOwner1, kEvent2, filter_id2);
  EXPECT_TRUE(was_last_of_kind);
  ASSERT_TRUE(removed_filter);
  EXPECT_EQ(ValueToString(*removed_filter), ValueToString(filter));
}

TEST(ListenerTrackerTest, InvalidFilteredListener) {
  ListenerTracker tracker;

  auto [was_first_of_kind, filter_id] = tracker.AddFilteredListener(
      kOwner1, kEvent1,
      std::make_unique<base::Value::Dict>(
          DictValueFromString(R"({"url": ["Not a dictionary"]})")),
      kRoutingId);
  EXPECT_EQ(-1, filter_id);
  EXPECT_FALSE(was_first_of_kind);
}

TEST(ListenerTrackerTest, GetMatchingFilters) {
  base::Value::Dict filter1 =
      DictValueFromString(R"({"url": [{"hostSuffix": "example.com"}]})");
  base::Value::Dict filter2 =
      DictValueFromString(R"({"url": [{"hostContains": "google"}]})");
  base::Value::Dict filter3 =
      DictValueFromString(R"({"url": [{"hostContains": "example"}]})");

  ListenerTracker tracker;
  auto [was_first_of_kind, filter_id1] = tracker.AddFilteredListener(
      kOwner1, kEvent1, std::make_unique<base::Value::Dict>(filter1.Clone()),
      kRoutingId);
  EXPECT_NE(-1, filter_id1);

  int filter_id2 = -1;
  std::tie(was_first_of_kind, filter_id2) = tracker.AddFilteredListener(
      kOwner1, kEvent1, std::make_unique<base::Value::Dict>(filter2.Clone()),
      kRoutingId);
  EXPECT_NE(-1, filter_id2);

  int filter_id3 = -1;
  std::tie(was_first_of_kind, filter_id3) = tracker.AddFilteredListener(
      kOwner2, kEvent1, std::make_unique<base::Value::Dict>(filter3.Clone()),
      kRoutingId);
  EXPECT_NE(-1, filter_id3);

  mojom::EventFilteringInfoPtr filtering_info =
      mojom::EventFilteringInfo::New();
  filtering_info->url = GURL("https://example.com/foo");
  std::set<int> matching_filters = tracker.GetMatchingFilteredListeners(
      kEvent1, std::move(filtering_info), kRoutingId);
  EXPECT_THAT(matching_filters,
              testing::UnorderedElementsAre(filter_id1, filter_id3));
}

}  // namespace extensions

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_priority_util.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class InterestGroupPriorityUtilTest : public testing::Test {
 public:
  InterestGroupPriorityUtilTest() {
    storage_interest_group_.interest_group.priority = 2.5;
    storage_interest_group_.interest_group.owner = kOrigin;
  }

  ~InterestGroupPriorityUtilTest() override = default;

  blink::AuctionConfig auction_config_;
  StorageInterestGroup storage_interest_group_;

  const url::Origin kOrigin = url::Origin::Create(GURL("https://origin.test"));
  const url::Origin kOtherOrigin =
      url::Origin::Create(GURL("https://other-origin.test"));
};

// All priority signals maps are null.
TEST_F(InterestGroupPriorityUtilTest, NullSignals) {
  EXPECT_EQ(0, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_, base::Time(),
                   /*priority_vector=*/{{"foo", 2}}));
  EXPECT_EQ(2, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_, base::Time(),
                   /*priority_vector=*/{{"browserSignals.one", 2}}));
  EXPECT_EQ(5, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_, base::Time(),
                   /*priority_vector=*/{{"browserSignals.basePriority", 2}}));
  EXPECT_EQ(
      0,
      CalculateInterestGroupPriority(
          auction_config_, storage_interest_group_, base::Time(),
          /*priority_vector=*/{{"browserSignals.firstDotProductPriority", 2}}));
  EXPECT_EQ(
      6,
      CalculateInterestGroupPriority(
          auction_config_, storage_interest_group_, base::Time(),
          /*priority_vector=*/{{"browserSignals.firstDotProductPriority", 2}},
          /*first_dot_product_priority=*/3));
}

// All priority signals maps exist but are empty.
TEST_F(InterestGroupPriorityUtilTest, EmptySignals) {
  auction_config_.non_shared_params.per_buyer_priority_signals.emplace();
  auction_config_.non_shared_params.per_buyer_priority_signals->emplace(
      kOrigin, base::flat_map<std::string, double>{});
  auction_config_.non_shared_params.all_buyers_priority_signals.emplace();
  storage_interest_group_.interest_group.priority_signals_overrides.emplace();

  EXPECT_EQ(0, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_, base::Time(),
                   /*priority_vector=*/{{"foo", 2}}));
  EXPECT_EQ(2, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_, base::Time(),
                   /*priority_vector=*/{{"browserSignals.one", 2}}));
  EXPECT_EQ(5, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_, base::Time(),
                   /*priority_vector=*/{{"browserSignals.basePriority", 2}}));
}

TEST_F(InterestGroupPriorityUtilTest, PerBuyerSignals) {
  auction_config_.non_shared_params.per_buyer_priority_signals.emplace();
  auction_config_.non_shared_params.per_buyer_priority_signals->emplace(
      kOrigin, base::flat_map<std::string, double>{{"foo", 2}, {"bar", -1.5}});

  // <missing>*2
  EXPECT_EQ(0, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_, base::Time(),
                   /*priority_vector=*/{{"one", 2}}));
  // 2*-1.5
  EXPECT_EQ(-3, CalculateInterestGroupPriority(
                    auction_config_, storage_interest_group_, base::Time(),
                    /*priority_vector=*/{{"foo", -1.5}}));
  // 2*4 + -1.5*3
  EXPECT_EQ(3.5, CalculateInterestGroupPriority(
                     auction_config_, storage_interest_group_, base::Time(),
                     /*priority_vector=*/{{"foo", 4}, {"bar", 3}}));

  // 1*3
  EXPECT_EQ(3, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_, base::Time(),
                   /*priority_vector=*/{{"browserSignals.one", 3}}));
  // 2*4 + 1*3
  EXPECT_EQ(11, CalculateInterestGroupPriority(
                    auction_config_, storage_interest_group_, base::Time(),
                    /*priority_vector=*/
                    {{"foo", 4}, {"browserSignals.one", 3}}));
}

TEST_F(InterestGroupPriorityUtilTest, PerBuyerSignalsOtherOrigin) {
  auction_config_.non_shared_params.per_buyer_priority_signals.emplace();
  // Values for another origin should have no effect.
  auction_config_.non_shared_params.per_buyer_priority_signals->emplace(
      kOtherOrigin,
      base::flat_map<std::string, double>{{"foo", 2}, {"bar", -1.5}});

  EXPECT_EQ(0, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_, base::Time(),
                   /*priority_vector=*/{{"foo", 2}}));
  EXPECT_EQ(0, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_, base::Time(),
                   /*priority_vector=*/{{"foo", 3}, {"bar", 4}}));

  // Add an entry for kOrigin with the same key as one of the entries for
  // kOtherOrigin, but a different value.
  auction_config_.non_shared_params.per_buyer_priority_signals->emplace(
      kOrigin, base::flat_map<std::string, double>{{"foo", 5}});
  EXPECT_EQ(15, CalculateInterestGroupPriority(
                    auction_config_, storage_interest_group_, base::Time(),
                    /*priority_vector=*/{{"foo", 3}, {"bar", -30}}));
}

TEST_F(InterestGroupPriorityUtilTest, AllBuyerSignals) {
  auction_config_.non_shared_params.all_buyers_priority_signals = {
      {"foo", 2}, {"bar", -1.5}};

  // <missing>*2
  EXPECT_EQ(0, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_, base::Time(),
                   /*priority_vector=*/{{"one", 2}}));
  // 2*-1.5
  EXPECT_EQ(-3, CalculateInterestGroupPriority(
                    auction_config_, storage_interest_group_, base::Time(),
                    /*priority_vector=*/{{"foo", -1.5}}));
  // 2*4 + -1.5*3
  EXPECT_EQ(3.5, CalculateInterestGroupPriority(
                     auction_config_, storage_interest_group_, base::Time(),
                     /*priority_vector=*/{{"foo", 4}, {"bar", 3}}));

  // 1*3
  EXPECT_EQ(3, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_, base::Time(),
                   /*priority_vector=*/{{"browserSignals.one", 3}}));
  // 2*4 + 1*3
  EXPECT_EQ(11, CalculateInterestGroupPriority(
                    auction_config_, storage_interest_group_, base::Time(),
                    /*priority_vector=*/
                    {{"foo", 4}, {"browserSignals.one", 3}}));
}

TEST_F(InterestGroupPriorityUtilTest, PrioritySignalsOverrides) {
  storage_interest_group_.interest_group.priority_signals_overrides = {
      {"foo", 2},
      {"bar", -1.5},
      // This should override the `browserSignals` values added by default.
      {"browserSignals.one", -4}};

  // <missing>*2
  EXPECT_EQ(0, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_, base::Time(),
                   /*priority_vector=*/{{"one", 2}}));
  // 2*-1.5
  EXPECT_EQ(-3, CalculateInterestGroupPriority(
                    auction_config_, storage_interest_group_, base::Time(),
                    /*priority_vector=*/{{"foo", -1.5}}));
  // 2*4 + -1.5*3
  EXPECT_EQ(3.5, CalculateInterestGroupPriority(
                     auction_config_, storage_interest_group_, base::Time(),
                     /*priority_vector=*/{{"foo", 4}, {"bar", 3}}));

  // -4 * 3
  EXPECT_EQ(-12, CalculateInterestGroupPriority(
                     auction_config_, storage_interest_group_, base::Time(),
                     /*priority_vector=*/{{"browserSignals.one", 3}}));
  // 2*4 + -4*3
  EXPECT_EQ(-4, CalculateInterestGroupPriority(
                    auction_config_, storage_interest_group_, base::Time(),
                    /*priority_vector=*/
                    {{"foo", 4}, {"browserSignals.one", 3}}));
  // browserSignals.priority should not be masked.
  // 2.5 * 5
  EXPECT_EQ(5, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_, base::Time(),
                   /*priority_vector=*/{{"browserSignals.basePriority", 2}}));
}

// Test relative priority of perBuyerSignals, allBuyersSignals, and
// prioritySignalsOverrides (browserSignals are tested in the overrides test
// case).
TEST_F(InterestGroupPriorityUtilTest, PrioritySignalsMasking) {
  storage_interest_group_.interest_group.priority_signals_overrides = {
      {"foo", 1}};
  auction_config_.non_shared_params.per_buyer_priority_signals.emplace();
  auction_config_.non_shared_params.per_buyer_priority_signals->emplace(
      kOrigin, base::flat_map<std::string, double>{{"foo", 10}, {"bar", 2}});
  auction_config_.non_shared_params.all_buyers_priority_signals = {
      {"foo", 10}, {"bar", 10}, {"baz", 3}};

  // "foo" should come from `priority_signals_overrides`, masking the values in
  // the other two maps.
  EXPECT_EQ(1, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_, base::Time(),
                   /*priority_vector=*/{{"foo", 1}}));

  // "bar" should come from `per_buyer_priority_signals`, masking the value in
  // `all_buyers_priority_signals`.
  EXPECT_EQ(2, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_, base::Time(),
                   /*priority_vector=*/{{"bar", 1}}));

  // "baz" should come from `all_buyers_priority_signals`, since no other maps
  // have an entry for it.
  EXPECT_EQ(3, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_, base::Time(),
                   /*priority_vector=*/{{"baz", 1}}));

  // Combine one value from each map.
  //
  // 1*1 + 2*2 + 3*3 + 4*<null>
  EXPECT_EQ(14, CalculateInterestGroupPriority(
                    auction_config_, storage_interest_group_, base::Time(),
                    /*priority_vector=*/
                    {{"foo", 1}, {"bar", 2}, {"baz", 3}, {"qux", 4}}));
}

TEST_F(InterestGroupPriorityUtilTest, BrowserSignalsAge) {
  base::Time base_time = base::Time::Now();
  storage_interest_group_.join_time = base_time;
  EXPECT_EQ(0, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_, base_time,
                   /*priority_vector=*/{{"browserSignals.ageInMinutes", 2}}));
  EXPECT_EQ(0,
            CalculateInterestGroupPriority(
                auction_config_, storage_interest_group_, base_time,
                /*priority_vector=*/{{"browserSignals.ageInMinutesMax60", 2}}));
  EXPECT_EQ(0,
            CalculateInterestGroupPriority(
                auction_config_, storage_interest_group_, base_time,
                /*priority_vector=*/{{"browserSignals.ageInHoursMax24", 2}}));
  EXPECT_EQ(0, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_, base_time,
                   /*priority_vector=*/{{"browserSignals.ageInDaysMax30", 2}}));

  // Add 59 seconds to make sure minutes are not rounded up. Don't need to do
  // this for hours or years because the 59 minutes case test hours aren't
  // rounded up, and the 23 hours test makes sure days aren't rounded up.
  base::Time fifty_nine_minutes_from_base =
      base_time + base::Minutes(59) + base::Seconds(59);
  EXPECT_EQ(118, CalculateInterestGroupPriority(
                     auction_config_, storage_interest_group_,
                     fifty_nine_minutes_from_base,
                     /*priority_vector=*/{{"browserSignals.ageInMinutes", 2}}));
  EXPECT_EQ(118,
            CalculateInterestGroupPriority(
                auction_config_, storage_interest_group_,
                fifty_nine_minutes_from_base,
                /*priority_vector=*/{{"browserSignals.ageInMinutesMax60", 2}}));
  EXPECT_EQ(0,
            CalculateInterestGroupPriority(
                auction_config_, storage_interest_group_,
                fifty_nine_minutes_from_base,
                /*priority_vector=*/{{"browserSignals.ageInHoursMax24", 2}}));
  EXPECT_EQ(0, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_,
                   fifty_nine_minutes_from_base,
                   /*priority_vector=*/{{"browserSignals.ageInDaysMax30", 2}}));

  base::Time twenty_three_hours_frome_base = base_time + base::Hours(23);
  EXPECT_EQ(2 * 23 * 60,
            CalculateInterestGroupPriority(
                auction_config_, storage_interest_group_,
                twenty_three_hours_frome_base,
                /*priority_vector=*/{{"browserSignals.ageInMinutes", 2}}));
  EXPECT_EQ(2 * 60,
            CalculateInterestGroupPriority(
                auction_config_, storage_interest_group_,
                twenty_three_hours_frome_base,
                /*priority_vector=*/{{"browserSignals.ageInMinutesMax60", 2}}));
  EXPECT_EQ(2 * 23,
            CalculateInterestGroupPriority(
                auction_config_, storage_interest_group_,
                twenty_three_hours_frome_base,
                /*priority_vector=*/{{"browserSignals.ageInHoursMax24", 2}}));
  EXPECT_EQ(0, CalculateInterestGroupPriority(
                   auction_config_, storage_interest_group_,
                   twenty_three_hours_frome_base,
                   /*priority_vector=*/{{"browserSignals.ageInDaysMax30", 2}}));

  base::Time twenty_nine_days_frome_base = base_time + base::Days(29);
  EXPECT_EQ(
      2 * 29 * 24 * 60,
      CalculateInterestGroupPriority(
          auction_config_, storage_interest_group_, twenty_nine_days_frome_base,
          /*priority_vector=*/{{"browserSignals.ageInMinutes", 2}}));
  EXPECT_EQ(
      2 * 60,
      CalculateInterestGroupPriority(
          auction_config_, storage_interest_group_, twenty_nine_days_frome_base,
          /*priority_vector=*/{{"browserSignals.ageInMinutesMax60", 2}}));
  EXPECT_EQ(
      2 * 24,
      CalculateInterestGroupPriority(
          auction_config_, storage_interest_group_, twenty_nine_days_frome_base,
          /*priority_vector=*/{{"browserSignals.ageInHoursMax24", 2}}));
  EXPECT_EQ(
      2 * 29,
      CalculateInterestGroupPriority(
          auction_config_, storage_interest_group_, twenty_nine_days_frome_base,
          /*priority_vector=*/{{"browserSignals.ageInDaysMax30", 2}}));

  base::Time one_year_from_base = base_time + base::Days(365);
  EXPECT_EQ(2 * 30 * 24 * 60,
            CalculateInterestGroupPriority(
                auction_config_, storage_interest_group_, one_year_from_base,
                /*priority_vector=*/{{"browserSignals.ageInMinutes", 2}}));
  EXPECT_EQ(2 * 60,
            CalculateInterestGroupPriority(
                auction_config_, storage_interest_group_, one_year_from_base,
                /*priority_vector=*/{{"browserSignals.ageInMinutesMax60", 2}}));
  EXPECT_EQ(2 * 24,
            CalculateInterestGroupPriority(
                auction_config_, storage_interest_group_, one_year_from_base,
                /*priority_vector=*/{{"browserSignals.ageInHoursMax24", 2}}));
  EXPECT_EQ(2 * 30,
            CalculateInterestGroupPriority(
                auction_config_, storage_interest_group_, one_year_from_base,
                /*priority_vector=*/{{"browserSignals.ageInDaysMax30", 2}}));

  base::Time one_year_before_base = base_time - base::Days(365);
  EXPECT_EQ(0,
            CalculateInterestGroupPriority(
                auction_config_, storage_interest_group_, one_year_before_base,
                /*priority_vector=*/{{"browserSignals.ageInMinutes", 2}}));
  EXPECT_EQ(0,
            CalculateInterestGroupPriority(
                auction_config_, storage_interest_group_, one_year_before_base,
                /*priority_vector=*/{{"browserSignals.ageInMinutesMax60", 2}}));
  EXPECT_EQ(0,
            CalculateInterestGroupPriority(
                auction_config_, storage_interest_group_, one_year_before_base,
                /*priority_vector=*/{{"browserSignals.ageInHoursMax24", 2}}));
  EXPECT_EQ(0,
            CalculateInterestGroupPriority(
                auction_config_, storage_interest_group_, one_year_before_base,
                /*priority_vector=*/{{"browserSignals.ageInDaysMax30", 2}}));
}

}  // namespace content

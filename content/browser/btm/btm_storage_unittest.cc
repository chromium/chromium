// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/btm/btm_storage.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "content/browser/btm/btm_state.h"
#include "content/browser/btm/btm_utils.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/common/content_features.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

class TestStorage : public BtmStorage {
 public:
  TestStorage() : BtmStorage(std::nullopt) {}

  void WriteForTesting(GURL url, const StateValue& state) {
    Write(BtmState(this, GetSiteForBtm(url), state));
  }
};

// TODO(crbug.com/376754761): Remove this class, since it no longer sets the
// main DIPS feature
class ScopedBtmInteractionTtlFeatureEnabledWithParams {
 public:
  explicit ScopedBtmInteractionTtlFeatureEnabledWithParams(
      const base::FieldTrialParams& params) {
    features_.InitAndEnableFeatureWithParameters(features::kBtmTtl, params);
  }

 private:
  base::test::ScopedFeatureList features_;
};

}  // namespace

TEST(BtmGetSitesToClearTest, FiltersByTriggerParam) {
  TestStorage storage;

  GURL kBounceUrl("https://bounce.com");
  GURL kStorageUrl("https://storage.com");
  GURL kStatefulBounceUrl("https://stateful_bounce.com");

  TimestampRange event({base::Time::FromSecondsSinceUnixEpoch(1),
                        base::Time::FromSecondsSinceUnixEpoch(1)});
  storage.WriteForTesting(kBounceUrl, StateValue{.bounce_times = event});
  storage.WriteForTesting(kStorageUrl, StateValue{.site_storage_times = event});
  storage.WriteForTesting(kStatefulBounceUrl,
                          StateValue{.site_storage_times = event,
                                     .stateful_bounce_times = event,
                                     .bounce_times = event});
  // Call 'GetSitesToClear' when the trigger is unset.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeatureWithParameters(
        features::kBtm, {{"triggering_action", "none"}});
    EXPECT_THAT(storage.GetSitesToClear(std::nullopt), testing::IsEmpty());
  }
  // Call 'GetSitesToClear' when DIPS is triggered by bounces.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeatureWithParameters(
        features::kBtm, {{"triggering_action", "bounce"}});
    EXPECT_THAT(storage.GetSitesToClear(std::nullopt),
                testing::ElementsAre(GetSiteForBtm(kBounceUrl),
                                     GetSiteForBtm(kStatefulBounceUrl)));
  }
  // Call 'GetSitesToClear' when DIPS is triggered by storage.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeatureWithParameters(
        features::kBtm, {{"triggering_action", "storage"}});
    EXPECT_THAT(storage.GetSitesToClear(std::nullopt),
                testing::ElementsAre(GetSiteForBtm(kStatefulBounceUrl),
                                     GetSiteForBtm(kStorageUrl)));
  }
  // Call 'GetSitesToClear' when DIPS is triggered by stateful bounces.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeatureWithParameters(
        features::kBtm, {{"triggering_action", "stateful_bounce"}});
    EXPECT_THAT(storage.GetSitesToClear(std::nullopt),
                testing::ElementsAre(GetSiteForBtm(kStatefulBounceUrl)));
  }
}

TEST(BtmGetSitesToClearTest, CustomGracePeriod) {
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::FromSecondsSinceUnixEpoch(1));
  base::Time start = clock.Now();
  base::Time late_trigger = start + base::Seconds(10);
  base::TimeDelta custom_grace_period = base::Seconds(5);

  TestStorage storage;
  storage.SetClockForTesting(&clock);

  GURL kUrl("https://example.com");
  GURL kLateUrl("https://late_example.com");

  TimestampRange event({start, start});
  TimestampRange late_event({late_trigger, late_trigger});

  storage.WriteForTesting(kUrl, StateValue{.site_storage_times = event,
                                           .stateful_bounce_times = event,
                                           .bounce_times = event});
  storage.WriteForTesting(kLateUrl,
                          StateValue{.site_storage_times = late_event,
                                     .stateful_bounce_times = late_event,
                                     .bounce_times = late_event});

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kBtm,
      {{"grace_period", "30s"}, {"triggering_action", "stateful_bounce"}});

  // Advance time by less than `features::kBtmGracePeriod` but greater than
  // `start + custom_grace_period` and verify that no sites are returned without
  // using the custom grace period.
  clock.Advance(base::Seconds(10));
  EXPECT_THAT(storage.GetSitesToClear(std::nullopt), testing::IsEmpty());
  // Verify that using a custom grace period less than the amount time was
  // advanced returns only `kUrl` since `kLateUrl` is still within its grace
  // period.
  EXPECT_THAT(storage.GetSitesToClear(custom_grace_period),
              testing::ElementsAre(GetSiteForBtm(kUrl)));
}

TEST(BtmGetSitesToClearTest, CustomGracePeriod_AllTriggers) {
  base::SimpleTestClock clock;
  base::Time start = clock.Now();
  base::TimeDelta grace_period = base::Seconds(1);

  TestStorage storage;
  storage.SetClockForTesting(&clock);

  GURL kBounceUrl("https://bounce.com");
  GURL kStorageUrl("https://storage.com");
  GURL kStatefulBounceUrl("https://stateful_bounce.com");

  TimestampRange event({start, start});
  storage.WriteForTesting(kBounceUrl, StateValue{.bounce_times = event});
  storage.WriteForTesting(kStorageUrl, StateValue{.site_storage_times = event});
  storage.WriteForTesting(kStatefulBounceUrl,
                          StateValue{.site_storage_times = event,
                                     .stateful_bounce_times = event,
                                     .bounce_times = event});

  // Call 'GetSitesToClear' with a custom grace period when the trigger is
  // unset.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(features::kBtm);
    // Advance time by less than `features::kBtmGracePeriod` and verify that
    // no sites are returned
    clock.Advance(features::kBtmGracePeriod.Get() / 2);
    EXPECT_THAT(storage.GetSitesToClear(std::nullopt), testing::IsEmpty());
    // Verify that using a custom grace period less than the amount time was
    // advanced still returns nothing when the trigger is unset.
    EXPECT_THAT(storage.GetSitesToClear(std::nullopt), testing::IsEmpty());

    // Reset `clock` to `start`.
    clock.SetNow(start);
  }

  // Call 'GetSitesToClear' with a custom grace period when DIPS is triggered by
  // bounces.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeatureWithParameters(
        features::kBtm, {{"triggering_action", "bounce"}});
    // Advance time by less than `features::kBtmGracePeriod` and verify that
    // no sites are returned without using a custom grace period.
    clock.Advance(features::kBtmGracePeriod.Get() / 2);
    EXPECT_THAT(storage.GetSitesToClear(std::nullopt), testing::IsEmpty());
    // Verify that using a custom grace period less than the amount time was
    // advanced returns the expected sites for triggering on bounces.
    EXPECT_THAT(storage.GetSitesToClear(grace_period),
                testing::ElementsAre(GetSiteForBtm(kBounceUrl),
                                     GetSiteForBtm(kStatefulBounceUrl)));

    // Reset `clock` to `start`.
    clock.SetNow(start);
  }

  // Call 'GetSitesToClear' with a custom grace period when DIPS is triggered by
  // storage.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeatureWithParameters(
        features::kBtm, {{"triggering_action", "storage"}});
    // Advance time by less than `features::kBtmGracePeriod` and verify that
    // no sites are returned without using a custom grace period.
    clock.Advance(features::kBtmGracePeriod.Get() / 2);
    EXPECT_THAT(storage.GetSitesToClear(std::nullopt), testing::IsEmpty());
    // Verify that using a custom grace period less than the amount time was
    // advanced returns the expected sites for triggering on storage.
    EXPECT_THAT(storage.GetSitesToClear(grace_period),
                testing::ElementsAre(GetSiteForBtm(kStatefulBounceUrl),
                                     GetSiteForBtm(kStorageUrl)));

    // Reset `clock` to `start`.
    clock.SetNow(start);
  }

  // Call 'GetSitesToClear' with a custom grace period when DIPS is triggered by
  // stateful bounces.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeatureWithParameters(
        features::kBtm, {{"triggering_action", "stateful_bounce"}});
    // Advance time by less than `features::kBtmGracePeriod` and verify that
    // no sites are returned without using a custom grace period.
    clock.Advance(features::kBtmGracePeriod.Get() / 2);
    EXPECT_THAT(storage.GetSitesToClear(std::nullopt), testing::IsEmpty());
    // Verify that using a custom grace period less than the amount time was
    // advanced returns the expected sites for triggering on stateful bounces.
    EXPECT_THAT(storage.GetSitesToClear(grace_period),
                testing::ElementsAre(GetSiteForBtm(kStatefulBounceUrl)));
  }
}

class BtmStorageTest : public testing::Test {
 public:
  BtmStorageTest() = default;

  void SetUp() override { storage_.SetClockForTesting(&clock_); }

  TimestampRange ToRange(base::Time first, base::Time last) {
    return {{first, last}};
  }

 protected:
  base::test::TaskEnvironment env_;
  ScopedBtmInteractionTtlFeatureEnabledWithParams feature{
      {{"interaction_ttl", "inf"}}};
  TestStorage storage_;
  base::SimpleTestClock clock_;
};

TEST(DirtyBit, Constructor) {
  ASSERT_FALSE(DirtyBit());
  ASSERT_TRUE(DirtyBit(true));
  ASSERT_FALSE(DirtyBit(false));
}

TEST(DirtyBit, Assignment) {
  DirtyBit bit;

  bit = true;
  ASSERT_TRUE(bit);

  bit = false;
  ASSERT_FALSE(bit);
}

TEST(DirtyBit, Move) {
  DirtyBit bit(true);
  DirtyBit moved(std::move(bit));

  ASSERT_TRUE(moved);
  ASSERT_FALSE(bit);  // NOLINT
}

TEST(BtmUtilsTest, GetSiteForBtm) {
  EXPECT_EQ("example.com", GetSiteForBtm(GURL("http://example.com/foo")));
  EXPECT_EQ("example.com", GetSiteForBtm(GURL("https://www.example.com/bar")));
  EXPECT_EQ("example.com", GetSiteForBtm(GURL("http://other.example.com/baz")));
  EXPECT_EQ("bar.baz.r.appspot.com",
            GetSiteForBtm(GURL("http://foo.bar.baz.r.appspot.com/baz")));
  EXPECT_EQ("localhost", GetSiteForBtm(GURL("http://localhost:8000/qux")));
  EXPECT_EQ("127.0.0.1", GetSiteForBtm(GURL("http://127.0.0.1:8888/")));
  EXPECT_EQ("[::1]", GetSiteForBtm(GURL("http://[::1]/")));
}

TEST_F(BtmStorageTest, NewURL) {
  BtmState state = storage_.Read(GURL("http://example.com/"));
  EXPECT_FALSE(state.was_loaded());
  EXPECT_FALSE(state.site_storage_times().has_value());
  EXPECT_FALSE(state.user_activation_times().has_value());
  EXPECT_FALSE(state.web_authn_assertion_times().has_value());
}

TEST_F(BtmStorageTest, SetValues) {
  GURL url("https://example.com");
  auto time1 = base::Time::FromSecondsSinceUnixEpoch(1);
  auto time2 = base::Time::FromSecondsSinceUnixEpoch(2);
  auto time3 = base::Time::FromSecondsSinceUnixEpoch(3);

  {
    BtmState state = storage_.Read(url);
    state.update_site_storage_time(time1);
    state.update_user_activation_time(time2);
    state.update_web_authn_assertion_time(time3);

    // Before flushing `state`, reads for the same URL won't include its
    // changes.
    BtmState state2 = storage_.Read(url);
    EXPECT_FALSE(state2.site_storage_times().has_value());
    EXPECT_FALSE(state2.user_activation_times().has_value());
    EXPECT_FALSE(state2.web_authn_assertion_times().has_value());
  }

  BtmState state = storage_.Read(url);
  EXPECT_TRUE(state.was_loaded());
  EXPECT_EQ(state.site_storage_times()->first, std::make_optional(time1));
  EXPECT_EQ(state.user_activation_times()->first, std::make_optional(time2));
  EXPECT_EQ(state.web_authn_assertion_times()->first,
            std::make_optional(time3));
}

TEST_F(BtmStorageTest, SameSiteSameState) {
  // The two urls use different subdomains of example.com; and one is HTTPS
  // while the other is HTTP.
  GURL url1("https://subdomain1.example.com");
  GURL url2("http://subdomain2.example.com");
  auto time = base::Time::FromSecondsSinceUnixEpoch(1);

  storage_.Read(url1).update_site_storage_time(time);

  BtmState state = storage_.Read(url2);
  // State was recorded for url1, but can be read for url2.
  EXPECT_EQ(time, state.site_storage_times()->first);
  EXPECT_FALSE(state.user_activation_times().has_value());
}

TEST_F(BtmStorageTest, DifferentSiteDifferentState) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  auto time1 = base::Time::FromSecondsSinceUnixEpoch(1);
  auto time2 = base::Time::FromSecondsSinceUnixEpoch(2);

  storage_.Read(url1).update_site_storage_time(time1);
  storage_.Read(url2).update_site_storage_time(time2);

  // Verify that url1 and url2 have independent state:
  EXPECT_EQ(storage_.Read(url1).site_storage_times()->first,
            std::make_optional(time1));
  EXPECT_EQ(storage_.Read(url2).site_storage_times()->first,
            std::make_optional(time2));
}

// This test is not all-inclusive as only fucuses on some (deemed) important
// overlapping scenarios.
TEST_F(BtmStorageTest, RemoveByTime_WebAuthnAssertion) {
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::FromSecondsSinceUnixEpoch(100));
  auto tiny_delta = base::Milliseconds(1);
  auto delete_begin = clock.Now();
  auto delete_end = delete_begin + tiny_delta * 100;
  auto i = 0;

  {
    const GURL url(base::StringPrintf("https://case%d.test", ++i));
    storage_.WriteForTesting(
        url, {{}, {}, {}, {}, ToRange(delete_begin, delete_end)});
    storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                          BtmEventRemovalType::kHistory);
    EXPECT_FALSE(storage_.Read(url).was_loaded());
  }

  {
    const GURL url(base::StringPrintf("https://case%d.test", ++i));
    storage_.WriteForTesting(
        url, {{}, {}, {}, {}, ToRange(delete_begin + tiny_delta, delete_end)});
    storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                          BtmEventRemovalType::kHistory);
    EXPECT_FALSE(storage_.Read(url).was_loaded());
  }

  {
    const GURL url(base::StringPrintf("https://case%d.test", ++i));
    storage_.WriteForTesting(
        url, {{}, {}, {}, {}, ToRange(delete_begin, delete_end - tiny_delta)});
    storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                          BtmEventRemovalType::kHistory);
    EXPECT_FALSE(storage_.Read(url).was_loaded());
  }

  {
    const GURL url(base::StringPrintf("https://case%d.test", ++i));
    StateValue init_state;
    init_state.web_authn_assertion_times =
        ToRange(delete_begin, delete_end + tiny_delta);
    storage_.WriteForTesting(url, init_state);
    storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                          BtmEventRemovalType::kHistory);
    EXPECT_EQ(
        storage_.Read(url).web_authn_assertion_times(),
        ToRange(delete_end, init_state.web_authn_assertion_times->second));
  }

  {
    const GURL url(base::StringPrintf("https://case%d.test", ++i));
    StateValue init_state;
    init_state.web_authn_assertion_times =
        ToRange(delete_begin - tiny_delta, delete_end);
    storage_.WriteForTesting(url, init_state);
    storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                          BtmEventRemovalType::kHistory);
    EXPECT_EQ(
        storage_.Read(url).web_authn_assertion_times(),
        ToRange(init_state.web_authn_assertion_times->first, delete_begin));
  }

  {
    const GURL url(base::StringPrintf("https://case%d.test", ++i));
    StateValue init_state;
    init_state.web_authn_assertion_times =
        ToRange(delete_begin - tiny_delta, delete_end + tiny_delta);
    storage_.WriteForTesting(url, init_state);
    storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                          BtmEventRemovalType::kHistory);
    EXPECT_EQ(storage_.Read(url).web_authn_assertion_times(),
              init_state.web_authn_assertion_times);
  }

  {
    const GURL url(base::StringPrintf("https://case%d.test", ++i));
    StateValue init_state;
    init_state.web_authn_assertion_times =
        ToRange(delete_end + tiny_delta, delete_end + tiny_delta * 2);
    storage_.WriteForTesting(url, init_state);
    storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                          BtmEventRemovalType::kHistory);
    EXPECT_EQ(storage_.Read(url).web_authn_assertion_times(),
              init_state.web_authn_assertion_times);
  }

  {
    const GURL url(base::StringPrintf("https://case%d.test", ++i));
    StateValue init_state;
    init_state.web_authn_assertion_times =
        ToRange(delete_begin - tiny_delta * 2, delete_begin - tiny_delta);
    storage_.WriteForTesting(url, init_state);
    storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                          BtmEventRemovalType::kHistory);
    EXPECT_EQ(storage_.Read(url).web_authn_assertion_times(),
              init_state.web_authn_assertion_times);
  }
}

TEST_F(BtmStorageTest, RemoveByTimeWithNullRangeEndTime) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  base::Time delete_begin = base::Time::FromSecondsSinceUnixEpoch(2);
  base::Time delete_end = base::Time();

  storage_.WriteForTesting(
      url1,
      {/*site_storage_times= */ {{base::Time::FromSecondsSinceUnixEpoch(1),
                                  base::Time::FromSecondsSinceUnixEpoch(3)}},
       /*user_activation_times= */ {
           {base::Time::FromSecondsSinceUnixEpoch(5),
            base::Time::FromSecondsSinceUnixEpoch(8)}}});
  storage_.WriteForTesting(url2,
                           {/*site_storage_times= */ TimestampRange(),
                            /*user_activation_times= */ {
                                {base::Time::FromSecondsSinceUnixEpoch(3),
                                 base::Time::FromSecondsSinceUnixEpoch(5)}}});
  storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                        BtmEventRemovalType::kAll);

  BtmState state1 = storage_.Read(url1);
  EXPECT_EQ(state1.site_storage_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(1)));  // no change
  EXPECT_EQ(state1.site_storage_times()->second,
            std::make_optional(delete_begin));  // adjusted
  EXPECT_EQ(state1.user_activation_times(),
            std::nullopt);  // removed

  BtmState state2 = storage_.Read(url2);
  EXPECT_FALSE(state2.was_loaded());  // removed
}

TEST_F(BtmStorageTest, RemoveByTimeWithNullRangeBeginTime) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  base::Time delete_begin = base::Time::Min();
  base::Time delete_end = base::Time::FromSecondsSinceUnixEpoch(6);

  storage_.WriteForTesting(
      url1,
      {/*site_storage_times= */ {{base::Time::FromSecondsSinceUnixEpoch(1),
                                  base::Time::FromSecondsSinceUnixEpoch(3)}},
       /*user_activation_times= */ {
           {base::Time::FromSecondsSinceUnixEpoch(5),
            base::Time::FromSecondsSinceUnixEpoch(8)}}});
  storage_.WriteForTesting(url2,
                           {/*site_storage_times= */ TimestampRange(),
                            /*user_activation_times= */ {
                                {base::Time::FromSecondsSinceUnixEpoch(3),
                                 base::Time::FromSecondsSinceUnixEpoch(5)}}});
  storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                        BtmEventRemovalType::kAll);

  BtmState state1 = storage_.Read(url1);
  EXPECT_EQ(state1.site_storage_times(), std::nullopt);  // removed
  EXPECT_EQ(state1.user_activation_times()->first,
            std::make_optional(delete_end));  // adjusted
  EXPECT_EQ(state1.user_activation_times()->second,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(8)));  // no change

  BtmState state2 = storage_.Read(url2);
  EXPECT_FALSE(state2.was_loaded());  // removed
}

TEST_F(BtmStorageTest, RemoveByTimeAdjustsOverlappingTimes) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  base::Time delete_begin = base::Time::FromSecondsSinceUnixEpoch(2);
  base::Time delete_end = base::Time::FromSecondsSinceUnixEpoch(6);

  storage_.WriteForTesting(url1,
                           {{{base::Time::FromSecondsSinceUnixEpoch(1),
                              base::Time::FromSecondsSinceUnixEpoch(3)}},
                            {{base::Time::FromSecondsSinceUnixEpoch(5),
                              base::Time::FromSecondsSinceUnixEpoch(8)}}});
  storage_.WriteForTesting(url2,
                           {TimestampRange(),
                            {{base::Time::FromSecondsSinceUnixEpoch(3),
                              base::Time::FromSecondsSinceUnixEpoch(5)}}});
  storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                        BtmEventRemovalType::kAll);

  BtmState state1 = storage_.Read(url1);
  EXPECT_EQ(state1.site_storage_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(1)));  // no change
  EXPECT_EQ(state1.site_storage_times()->second,
            std::make_optional(delete_begin));  // adjusted
  EXPECT_EQ(state1.user_activation_times()->first,
            std::make_optional(delete_end));  // adjusted
  EXPECT_EQ(state1.user_activation_times()->second,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(8)));  // no change

  BtmState state2 = storage_.Read(url2);
  EXPECT_FALSE(state2.was_loaded());  // removed
}

TEST_F(BtmStorageTest, RemoveByTimeDoesNotAffectTouchingWindowEndpoints) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  base::Time delete_begin = base::Time::FromSecondsSinceUnixEpoch(3);
  base::Time delete_end = base::Time::FromSecondsSinceUnixEpoch(5);

  storage_.WriteForTesting(url1,
                           {{{base::Time::FromSecondsSinceUnixEpoch(1),
                              base::Time::FromSecondsSinceUnixEpoch(3)}},
                            {{base::Time::FromSecondsSinceUnixEpoch(5),
                              base::Time::FromSecondsSinceUnixEpoch(8)}}});
  storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                        BtmEventRemovalType::kAll);

  BtmState state = storage_.Read(url1);
  EXPECT_EQ(state.site_storage_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(1)));  // no change
  EXPECT_EQ(state.site_storage_times()->second,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(3)));  // no change
  EXPECT_EQ(state.user_activation_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(5)));  // no change
  EXPECT_EQ(state.user_activation_times()->second,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(8)));  // no change
}

TEST_F(BtmStorageTest, RemoveByTimeStorageOnly) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  base::Time delete_begin = base::Time::FromSecondsSinceUnixEpoch(2);
  base::Time delete_end = base::Time::FromSecondsSinceUnixEpoch(6);

  storage_.WriteForTesting(url1,
                           {{{base::Time::FromSecondsSinceUnixEpoch(1),
                              base::Time::FromSecondsSinceUnixEpoch(3)}},
                            {{base::Time::FromSecondsSinceUnixEpoch(5),
                              base::Time::FromSecondsSinceUnixEpoch(8)}}});
  storage_.WriteForTesting(url2,
                           {TimestampRange(),
                            {{base::Time::FromSecondsSinceUnixEpoch(3),
                              base::Time::FromSecondsSinceUnixEpoch(5)}}});
  storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                        BtmEventRemovalType::kStorage);

  BtmState state1 = storage_.Read(url1);
  EXPECT_EQ(state1.site_storage_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(1)));  // no change
  EXPECT_EQ(state1.site_storage_times()->second,
            std::make_optional(delete_begin));  // adjusted
  EXPECT_EQ(state1.user_activation_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(5)));  // no change
  EXPECT_EQ(state1.user_activation_times()->second,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(8)));  // no change

  BtmState state2 = storage_.Read(url2);
  EXPECT_EQ(state2.user_activation_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(3)));  // no change
  EXPECT_EQ(state2.user_activation_times()->second,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(5)));  // no change
}

TEST_F(BtmStorageTest, RemoveByTimeInteractionOnly) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  base::Time delete_begin = base::Time::FromSecondsSinceUnixEpoch(2);
  base::Time delete_end = base::Time::FromSecondsSinceUnixEpoch(6);

  storage_.WriteForTesting(url1,
                           {{{base::Time::FromSecondsSinceUnixEpoch(1),
                              base::Time::FromSecondsSinceUnixEpoch(3)}},
                            {{base::Time::FromSecondsSinceUnixEpoch(5),
                              base::Time::FromSecondsSinceUnixEpoch(8)}}});
  storage_.WriteForTesting(url2,
                           {TimestampRange(),
                            {{base::Time::FromSecondsSinceUnixEpoch(3),
                              base::Time::FromSecondsSinceUnixEpoch(5)}}});
  storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                        BtmEventRemovalType::kHistory);

  BtmState state1 = storage_.Read(url1);
  EXPECT_EQ(state1.site_storage_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(1)));  // no change
  EXPECT_EQ(state1.site_storage_times()->second,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(3)));  // no change
  EXPECT_EQ(state1.user_activation_times()->first,
            std::make_optional(delete_end));  // adjusted
  EXPECT_EQ(state1.user_activation_times()->second,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(8)));  // no change

  BtmState state2 = storage_.Read(url2);
  EXPECT_FALSE(state2.was_loaded());  // removed
}

TEST_F(BtmStorageTest, RemovePopupEventsByTime) {
  std::string site1 = GetSiteForBtm(GURL("https://example1.com"));
  std::string site2 = GetSiteForBtm(GURL("https://example2.com"));
  std::string site3 = GetSiteForBtm(GURL("https://example3.com"));
  base::Time delete_begin = base::Time::FromSecondsSinceUnixEpoch(3);
  base::Time delete_end = base::Time::FromSecondsSinceUnixEpoch(5);

  ASSERT_TRUE(storage_.WritePopup(
      site1, site2, /*access_id=*/1u, base::Time::FromSecondsSinceUnixEpoch(2),
      /*is_current_interaction=*/true, /*is_authentication_interaction=*/true));
  ASSERT_TRUE(storage_.WritePopup(site1, site3, /*access_id=*/2u,
                                  base::Time::FromSecondsSinceUnixEpoch(4),
                                  /*is_current_interaction=*/true,
                                  /*is_authentication_interaction=*/false));
  ASSERT_TRUE(storage_.WritePopup(site2, site3, /*access_id=*/3u,
                                  base::Time::FromSecondsSinceUnixEpoch(6),
                                  /*is_current_interaction=*/false,
                                  /*is_authentication_interaction=*/false));

  storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                        BtmEventRemovalType::kHistory);

  // Verify that only the second popup event (with timestamp 4) was cleared.

  std::optional<PopupsStateValue> popup1 = storage_.ReadPopup(site1, site2);
  ASSERT_TRUE(popup1.has_value());
  EXPECT_EQ(popup1.value().access_id, 1u);
  EXPECT_EQ(popup1.value().last_popup_time,
            base::Time::FromSecondsSinceUnixEpoch(2));
  EXPECT_TRUE(popup1.value().is_current_interaction);
  EXPECT_TRUE(popup1.value().is_authentication_interaction);

  std::optional<PopupsStateValue> popup2 = storage_.ReadPopup(site1, site3);
  ASSERT_FALSE(popup2.has_value());

  std::optional<PopupsStateValue> popup3 = storage_.ReadPopup(site2, site3);
  ASSERT_TRUE(popup3.has_value());
  EXPECT_EQ(popup3.value().access_id, 3u);
  EXPECT_EQ(popup3.value().last_popup_time,
            base::Time::FromSecondsSinceUnixEpoch(6));
  EXPECT_FALSE(popup3.value().is_current_interaction);
  EXPECT_FALSE(popup3.value().is_authentication_interaction);
}

TEST_F(BtmStorageTest, RemoveByTimeBounces) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  base::Time delete_begin = base::Time::FromSecondsSinceUnixEpoch(2);
  base::Time delete_end = base::Time::FromSecondsSinceUnixEpoch(6);

  storage_.WriteForTesting(url1,
                           {TimestampRange(),
                            TimestampRange(),
                            {{base::Time::FromSecondsSinceUnixEpoch(1),
                              base::Time::FromSecondsSinceUnixEpoch(3)}},
                            {{base::Time::FromSecondsSinceUnixEpoch(1),
                              base::Time::FromSecondsSinceUnixEpoch(8)}}});
  storage_.WriteForTesting(url2,
                           {TimestampRange(),
                            TimestampRange(),
                            TimestampRange(),
                            {{base::Time::FromSecondsSinceUnixEpoch(3),
                              base::Time::FromSecondsSinceUnixEpoch(5)}}});
  storage_.RemoveEvents(delete_begin, delete_end, nullptr,
                        BtmEventRemovalType::kStorage);

  BtmState state1 = storage_.Read(url1);
  EXPECT_EQ(state1.stateful_bounce_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(1)));  // no change
  EXPECT_EQ(state1.stateful_bounce_times()->second,
            std::make_optional(delete_begin));  // adjusted
  EXPECT_EQ(state1.bounce_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(1)));  // no change
  EXPECT_EQ(state1.bounce_times()->second,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(8)));  // no change

  BtmState state2 = storage_.Read(url2);
  EXPECT_FALSE(state2.was_loaded());  // removed
}

TEST_F(BtmStorageTest, RemoveBySite) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  GURL url3("https://example3.com");
  GURL url4("https://example4.com");

  storage_.WriteForTesting(url1,
                           {{{base::Time::FromSecondsSinceUnixEpoch(1),
                              base::Time::FromSecondsSinceUnixEpoch(1)}},
                            {{base::Time::FromSecondsSinceUnixEpoch(2),
                              base::Time::FromSecondsSinceUnixEpoch(2)}},
                            {{base::Time::FromSecondsSinceUnixEpoch(3),
                              base::Time::FromSecondsSinceUnixEpoch(3)}},
                            {{base::Time::FromSecondsSinceUnixEpoch(3),
                              base::Time::FromSecondsSinceUnixEpoch(4)}}});
  storage_.WriteForTesting(url2,
                           {{{base::Time::FromSecondsSinceUnixEpoch(1),
                              base::Time::FromSecondsSinceUnixEpoch(1)}},
                            {{base::Time::FromSecondsSinceUnixEpoch(2),
                              base::Time::FromSecondsSinceUnixEpoch(2)}},
                            {{base::Time::FromSecondsSinceUnixEpoch(3),
                              base::Time::FromSecondsSinceUnixEpoch(3)}},
                            {{base::Time::FromSecondsSinceUnixEpoch(3),
                              base::Time::FromSecondsSinceUnixEpoch(4)}}});
  storage_.WriteForTesting(url3,
                           {{{base::Time::FromSecondsSinceUnixEpoch(1),
                              base::Time::FromSecondsSinceUnixEpoch(2)}},
                            TimestampRange(),
                            {{base::Time::FromSecondsSinceUnixEpoch(3),
                              base::Time::FromSecondsSinceUnixEpoch(4)}},
                            {{base::Time::FromSecondsSinceUnixEpoch(3),
                              base::Time::FromSecondsSinceUnixEpoch(4)}}});
  storage_.WriteForTesting(url4,
                           {TimestampRange(),
                            {{base::Time::FromSecondsSinceUnixEpoch(2),
                              base::Time::FromSecondsSinceUnixEpoch(2)}},
                            {{base::Time::FromSecondsSinceUnixEpoch(3),
                              base::Time::FromSecondsSinceUnixEpoch(3)}},
                            {{base::Time::FromSecondsSinceUnixEpoch(3),
                              base::Time::FromSecondsSinceUnixEpoch(4)}}});

  std::unique_ptr<BrowsingDataFilterBuilder> builder =
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete);
  builder->AddRegisterableDomain(GetSiteForBtm(url1));
  builder->AddRegisterableDomain(GetSiteForBtm(url3));
  storage_.RemoveEvents(base::Time(), base::Time::Max(),
                        builder->BuildNetworkServiceFilter(),
                        BtmEventRemovalType::kStorage);

  BtmState state1 = storage_.Read(url1);
  EXPECT_FALSE(state1.site_storage_times().has_value());  // removed
  EXPECT_EQ(state1.user_activation_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(2)));  // no change
  EXPECT_FALSE(state1.stateful_bounce_times().has_value());  // removed
  EXPECT_FALSE(state1.bounce_times().has_value());           // removed

  BtmState state2 = storage_.Read(url2);
  EXPECT_EQ(state2.site_storage_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(1)));  // no change
  EXPECT_EQ(state2.user_activation_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(2)));  // no change
  EXPECT_EQ(state2.stateful_bounce_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(3)));  // no change
  EXPECT_EQ(state2.bounce_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(3)));  // no change

  BtmState state3 = storage_.Read(url3);
  EXPECT_FALSE(state3.was_loaded());  // removed

  BtmState state4 = storage_.Read(url2);
  EXPECT_FALSE(state1.site_storage_times().has_value());  // no change
  EXPECT_EQ(state4.user_activation_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(2)));  // no change
  EXPECT_EQ(state4.stateful_bounce_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(3)));  // no change
  EXPECT_EQ(state4.bounce_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(3)));  // no change
}

TEST_F(BtmStorageTest, RemoveBySiteIgnoresDeletionWithTimeRange) {
  GURL url1("https://example1.com");
  base::Time delete_begin = base::Time::FromSecondsSinceUnixEpoch(2);
  base::Time delete_end = base::Time::FromSecondsSinceUnixEpoch(6);

  storage_.WriteForTesting(url1,
                           {{{base::Time::FromSecondsSinceUnixEpoch(1),
                              base::Time::FromSecondsSinceUnixEpoch(1)}},
                            {{base::Time::FromSecondsSinceUnixEpoch(2),
                              base::Time::FromSecondsSinceUnixEpoch(2)}},
                            {{base::Time::FromSecondsSinceUnixEpoch(3),
                              base::Time::FromSecondsSinceUnixEpoch(3)}},
                            {{base::Time::FromSecondsSinceUnixEpoch(3),
                              base::Time::FromSecondsSinceUnixEpoch(4)}}});

  std::unique_ptr<BrowsingDataFilterBuilder> builder =
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete);
  builder->AddRegisterableDomain(GetSiteForBtm(url1));
  storage_.RemoveEvents(delete_begin, delete_end,
                        builder->BuildNetworkServiceFilter(),
                        BtmEventRemovalType::kStorage);

  // Removing events by site (i.e. by using a non-null filter) with a time-range
  // (other than base::Time() to base::Time::Max()), is currently unsupported.
  // So url1's DIPS Storage entry should be unaffected.
  BtmState state1 = storage_.Read(url1);
  EXPECT_EQ(state1.site_storage_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(1)));  // no change
  EXPECT_EQ(state1.user_activation_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(2)));  // no change
  EXPECT_EQ(state1.stateful_bounce_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(3)));  // no change
  EXPECT_EQ(state1.bounce_times()->first,
            std::make_optional(
                base::Time::FromSecondsSinceUnixEpoch(3)));  // no change
}

TEST_F(BtmStorageTest, RemoveRows) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  ASSERT_TRUE(url1.is_valid());
  ASSERT_TRUE(url2.is_valid());

  StateValue test_value = {{{base::Time::FromSecondsSinceUnixEpoch(1),
                             base::Time::FromSecondsSinceUnixEpoch(1)}},
                           {{base::Time::FromSecondsSinceUnixEpoch(2),
                             base::Time::FromSecondsSinceUnixEpoch(2)}},
                           {{base::Time::FromSecondsSinceUnixEpoch(3),
                             base::Time::FromSecondsSinceUnixEpoch(3)}},
                           {{base::Time::FromSecondsSinceUnixEpoch(3),
                             base::Time::FromSecondsSinceUnixEpoch(4)}}};

  storage_.WriteForTesting(url1, test_value);
  storage_.WriteForTesting(url2, test_value);

  ASSERT_EQ(storage_.Read(url1).ToStateValue(), test_value);
  ASSERT_EQ(storage_.Read(url2).ToStateValue(), test_value);

  storage_.RemoveRows({GetSiteForBtm(url1), GetSiteForBtm(url2)});

  EXPECT_FALSE(storage_.Read(url1).was_loaded());
  EXPECT_FALSE(storage_.Read(url2).was_loaded());
}

TEST_F(BtmStorageTest, DidSiteHaveUserActivationSince) {
  GURL url1("https://example1.com");

  EXPECT_FALSE(storage_.DidSiteHaveUserActivationSince(
      url1, base::Time::FromSecondsSinceUnixEpoch(0)));

  storage_.WriteForTesting(
      url1,
      StateValue{
          .site_storage_times{{base::Time::FromSecondsSinceUnixEpoch(1),
                               base::Time::FromSecondsSinceUnixEpoch(1)}},
          .user_activation_times{{base::Time::FromSecondsSinceUnixEpoch(2),
                                  base::Time::FromSecondsSinceUnixEpoch(2)}},
          .stateful_bounce_times{{base::Time::FromSecondsSinceUnixEpoch(3),
                                  base::Time::FromSecondsSinceUnixEpoch(3)}},
          .bounce_times{{base::Time::FromSecondsSinceUnixEpoch(3),
                         base::Time::FromSecondsSinceUnixEpoch(4)}}});

  EXPECT_TRUE(storage_.DidSiteHaveUserActivationSince(
      url1, base::Time::FromSecondsSinceUnixEpoch(0)));
  EXPECT_TRUE(storage_.DidSiteHaveUserActivationSince(
      url1, base::Time::FromSecondsSinceUnixEpoch(1)));
  EXPECT_TRUE(storage_.DidSiteHaveUserActivationSince(
      url1, base::Time::FromSecondsSinceUnixEpoch(2)));
  EXPECT_FALSE(storage_.DidSiteHaveUserActivationSince(
      url1, base::Time::FromSecondsSinceUnixEpoch(3)));
  EXPECT_FALSE(storage_.DidSiteHaveUserActivationSince(
      url1, base::Time::FromSecondsSinceUnixEpoch(4)));
}

TEST_F(BtmStorageTest, GetTimerLastFired_InitiallyReturnsEmpty) {
  ASSERT_EQ(storage_.GetTimerLastFired(), std::nullopt);
}

TEST_F(BtmStorageTest, GetTimerLastFired_ReturnsLastSetValue) {
  const base::Time time1 = base::Time::FromTimeT(1);
  const base::Time time2 = base::Time::FromTimeT(2);

  ASSERT_TRUE(storage_.SetTimerLastFired(time1));
  ASSERT_TRUE(storage_.SetTimerLastFired(time2));
  ASSERT_THAT(storage_.GetTimerLastFired(), testing::Optional(time2));
}

}  // namespace content

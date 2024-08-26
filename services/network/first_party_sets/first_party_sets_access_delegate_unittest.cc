// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_sets_access_delegate.h"

#include <optional>
#include <set>
#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "services/network/public/mojom/first_party_sets_access_delegate.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using OverrideSets =
    base::flat_map<net::SchemefulSite, std::optional<net::FirstPartySetEntry>>;

namespace network {

namespace {

const net::SchemefulSite kSet1Primary(GURL("https://set1primary.test"));
const net::SchemefulSite kSet1AssociatedSite1(
    GURL("https://set1associatedSite1.test"));
const net::SchemefulSite kSet1AssociatedSite2(
    GURL("https://set1associatedSite2.test"));
const net::SchemefulSite kSet2Primary(GURL("https://set2primary.test"));
const net::SchemefulSite kSet2AssociatedSite1(
    GURL("https://set2associatedSite1.test"));
const net::SchemefulSite kSet3Primary(GURL("https://set3primary.test"));
const net::SchemefulSite kSet3AssociatedSite1(
    GURL("https://set3associatedSite1.test"));
const int64_t kClearAtRunId(2);
const int64_t kBrowserRunId(3);

const char kDelayedQueriesCountHistogram[] =
    "Cookie.FirstPartySets.ContextDelayedQueriesCount";
const char kMostDelayedQuerDeltaHistogram[] =
    "Cookie.FirstPartySets.ContextMostDelayedQueryDelta";

mojom::FirstPartySetsAccessDelegateParamsPtr
CreateFirstPartySetsAccessDelegateParams(bool enabled) {
  auto params = mojom::FirstPartySetsAccessDelegateParams::New();
  params->enabled = enabled;
  return params;
}

mojom::FirstPartySetsReadyEventPtr CreateFirstPartySetsReadyEvent(
    std::optional<net::FirstPartySetsContextConfig> config,
    std::optional<net::FirstPartySetsCacheFilter> cache_filter) {
  auto ready_event = mojom::FirstPartySetsReadyEvent::New();
  if (config.has_value())
    ready_event->config = std::move(config.value());
  if (cache_filter.has_value())
    ready_event->cache_filter = std::move(cache_filter.value());
  return ready_event;
}

}  // namespace

class WaitingFeatureInitializer {
 public:
  explicit WaitingFeatureInitializer(bool enabled) {
    if (enabled) {
      features_.InitAndEnableFeature(net::features::kWaitForFirstPartySetsInit);
    } else {
      features_.InitAndDisableFeature(
          net::features::kWaitForFirstPartySetsInit);
    }
  }

 private:
  base::test::ScopedFeatureList features_;
};

// No-op FirstPartySetsAccessDelegate should just pass queries to
// FirstPartySetsManager synchronously.
class NoopFirstPartySetsAccessDelegateTest : public ::testing::Test,
                                             public WaitingFeatureInitializer {
 public:
  NoopFirstPartySetsAccessDelegateTest()
      : WaitingFeatureInitializer(/*enabled=*/true),
        first_party_sets_manager_(/*enabled=*/true),
        delegate_(
            /*receiver=*/mojo::NullReceiver(),
            /*params=*/nullptr,
            &first_party_sets_manager_) {
    first_party_sets_manager_.SetCompleteSets(net::GlobalFirstPartySets(
        base::Version("1.2.3"),
        /*entries=*/
        {
            {kSet1AssociatedSite1,
             net::FirstPartySetEntry(kSet1Primary, net::SiteType::kAssociated,
                                     0)},
            {kSet1AssociatedSite2,
             net::FirstPartySetEntry(kSet1Primary, net::SiteType::kAssociated,
                                     1)},
            {kSet1Primary,
             net::FirstPartySetEntry(kSet1Primary, net::SiteType::kPrimary,
                                     std::nullopt)},
            {kSet2AssociatedSite1,
             net::FirstPartySetEntry(kSet2Primary, net::SiteType::kAssociated,
                                     0)},
            {kSet2Primary,
             net::FirstPartySetEntry(kSet2Primary, net::SiteType::kPrimary,
                                     std::nullopt)},
        },
        /*aliases=*/{}));
  }

  FirstPartySetsAccessDelegate& delegate() { return delegate_; }

 private:
  FirstPartySetsManager first_party_sets_manager_;
  FirstPartySetsAccessDelegate delegate_;
};

TEST_F(NoopFirstPartySetsAccessDelegateTest, ComputeMetadata) {
  net::FirstPartySetEntry primary_entry(kSet1Primary, net::SiteType::kPrimary,
                                        std::nullopt);
  net::FirstPartySetEntry associated_entry(kSet1Primary,
                                           net::SiteType::kAssociated, 0);
  EXPECT_EQ(delegate().ComputeMetadata(kSet1AssociatedSite1, &kSet1Primary,
                                       base::NullCallback()),
            std::make_optional(std::make_pair(
                net::FirstPartySetMetadata(&associated_entry, &primary_entry),
                net::FirstPartySetsCacheFilter::MatchInfo())));
}

TEST_F(NoopFirstPartySetsAccessDelegateTest, FindEntries) {
  EXPECT_THAT(
      delegate().FindEntries({kSet1AssociatedSite1, kSet2AssociatedSite1},
                             base::NullCallback()),
      FirstPartySetsAccessDelegate::EntriesResult({
          {kSet1AssociatedSite1,
           net::FirstPartySetEntry(kSet1Primary, net::SiteType::kAssociated,
                                   0)},
          {kSet2AssociatedSite1,
           net::FirstPartySetEntry(kSet2Primary, net::SiteType::kAssociated,
                                   0)},
      }));
}

class FirstPartySetsAccessDelegateTest : public ::testing::Test,
                                         public WaitingFeatureInitializer {
 public:
  explicit FirstPartySetsAccessDelegateTest(bool enabled, bool wait_for_init)
      : WaitingFeatureInitializer(wait_for_init),
        first_party_sets_manager_(/*enabled=*/true),
        delegate_(delegate_remote_.BindNewPipeAndPassReceiver(),
                  CreateFirstPartySetsAccessDelegateParams(enabled),
                  &first_party_sets_manager_) {
    first_party_sets_manager_.SetCompleteSets(net::GlobalFirstPartySets(
        base::Version("1.2.3"),
        /*entries=*/
        {
            {kSet1AssociatedSite1,
             net::FirstPartySetEntry(kSet1Primary, net::SiteType::kAssociated,
                                     0)},
            {kSet1AssociatedSite2,
             net::FirstPartySetEntry(kSet1Primary, net::SiteType::kAssociated,
                                     1)},
            {kSet1Primary,
             net::FirstPartySetEntry(kSet1Primary, net::SiteType::kPrimary,
                                     std::nullopt)},
            {kSet2AssociatedSite1,
             net::FirstPartySetEntry(kSet2Primary, net::SiteType::kAssociated,
                                     0)},
            {kSet2Primary,
             net::FirstPartySetEntry(kSet2Primary, net::SiteType::kPrimary,
                                     std::nullopt)},
        },
        /*aliases=*/{}));
  }

  std::tuple<net::FirstPartySetMetadata,
             net::FirstPartySetsCacheFilter::MatchInfo>
  ComputeMetadataAndWait(const net::SchemefulSite& site,
                         const net::SchemefulSite* top_frame_site) {
    base::test::TestFuture<net::FirstPartySetMetadata,
                           net::FirstPartySetsCacheFilter::MatchInfo>
        future;
    std::optional<std::tuple<net::FirstPartySetMetadata,
                             net::FirstPartySetsCacheFilter::MatchInfo>>
        result = delegate_.ComputeMetadata(site, top_frame_site,
                                           future.GetCallback());
    return result.has_value() ? std::move(result).value() : future.Take();
  }

  FirstPartySetsAccessDelegate::EntriesResult FindEntriesAndWait(
      const base::flat_set<net::SchemefulSite>& site) {
    base::test::TestFuture<FirstPartySetsAccessDelegate::EntriesResult> future;
    std::optional<FirstPartySetsAccessDelegate::EntriesResult> result =
        delegate_.FindEntries(site, future.GetCallback());
    return result.has_value() ? result.value() : future.Get();
  }

  FirstPartySetsAccessDelegate& delegate() { return delegate_; }

  mojom::FirstPartySetsAccessDelegate* delegate_remote() {
    return delegate_remote_.get();
  }

 private:
  base::test::TaskEnvironment env_;
  FirstPartySetsManager first_party_sets_manager_;
  mojo::Remote<mojom::FirstPartySetsAccessDelegate> delegate_remote_;
  FirstPartySetsAccessDelegate delegate_;
};

// Since the FPSs is disabled for the context, none of the callbacks
// should ever be called, and the return values should all be non-nullopt.
class FirstPartySetsAccessDelegateDisabledTest
    : public FirstPartySetsAccessDelegateTest {
 public:
  FirstPartySetsAccessDelegateDisabledTest()
      : FirstPartySetsAccessDelegateTest(/*enabled=*/false,
                                         /*wait_for_init=*/true) {}
};

TEST_F(FirstPartySetsAccessDelegateDisabledTest, ComputeMetadata) {
  EXPECT_EQ(ComputeMetadataAndWait(kSet1AssociatedSite1, &kSet1AssociatedSite1),
            std::make_tuple(net::FirstPartySetMetadata(
                                /*frame_entry=*/nullptr,
                                /*top_frame_entry=*/nullptr),
                            net::FirstPartySetsCacheFilter::MatchInfo()));
}

TEST_F(FirstPartySetsAccessDelegateDisabledTest, FindEntries) {
  EXPECT_THAT(FindEntriesAndWait({kSet1AssociatedSite1, kSet2AssociatedSite1}),
              IsEmpty());
}

// Test fixture that allows precise control over when the instance gets FPS
// data. Useful for testing async flows.
class AsyncFirstPartySetsAccessDelegateTest
    : public FirstPartySetsAccessDelegateTest {
 public:
  AsyncFirstPartySetsAccessDelegateTest()
      : FirstPartySetsAccessDelegateTest(/*enabled=*/true,
                                         /*wait_for_init=*/true) {}
};

TEST_F(AsyncFirstPartySetsAccessDelegateTest,
       QueryBeforeReady_ComputeMetadata) {
  base::test::TestFuture<net::FirstPartySetMetadata,
                         net::FirstPartySetsCacheFilter::MatchInfo>
      future;
  {
    // Force deallocation to provoke a UAF if the impl just copies the pointer.
    net::SchemefulSite local_associatedSite1(kSet1AssociatedSite1);
    EXPECT_FALSE(delegate().ComputeMetadata(
        kSet1AssociatedSite1, &local_associatedSite1, future.GetCallback()));
  }

  delegate_remote()->NotifyReady(mojom::FirstPartySetsReadyEvent::New());

  net::FirstPartySetEntry entry(kSet1Primary, net::SiteType::kAssociated, 0);
  EXPECT_EQ(future.Get(),
            std::make_tuple(net::FirstPartySetMetadata(&entry, &entry),
                            net::FirstPartySetsCacheFilter::MatchInfo()));
}

TEST_F(AsyncFirstPartySetsAccessDelegateTest, QueryBeforeReady_FindEntries) {
  base::test::TestFuture<FirstPartySetsAccessDelegate::EntriesResult> future;
  EXPECT_FALSE(delegate().FindEntries(
      {kSet1AssociatedSite1, kSet2AssociatedSite1}, future.GetCallback()));

  delegate_remote()->NotifyReady(mojom::FirstPartySetsReadyEvent::New());

  EXPECT_THAT(future.Get(),
              FirstPartySetsAccessDelegate::EntriesResult({
                  {kSet1AssociatedSite1,
                   net::FirstPartySetEntry(kSet1Primary,
                                           net::SiteType::kAssociated, 0)},
                  {kSet2AssociatedSite1,
                   net::FirstPartySetEntry(kSet2Primary,
                                           net::SiteType::kAssociated, 0)},
              }));
}

TEST_F(AsyncFirstPartySetsAccessDelegateTest, OverrideSets_ComputeMetadata) {
  delegate_remote()->NotifyReady(CreateFirstPartySetsReadyEvent(
      net::FirstPartySetsContextConfig({
          {kSet1AssociatedSite1,
           net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
               kSet3Primary, net::SiteType::kAssociated, 0))},
          {kSet3Primary,
           net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
               kSet3Primary, net::SiteType::kPrimary, std::nullopt))},
      }),
      /*cache_filter=*/std::nullopt));

  net::FirstPartySetEntry primary_entry(kSet3Primary, net::SiteType::kPrimary,
                                        std::nullopt);
  net::FirstPartySetEntry associated_entry(kSet3Primary,
                                           net::SiteType::kAssociated, 0);
  EXPECT_EQ(ComputeMetadataAndWait(kSet3Primary, &kSet1AssociatedSite1),
            std::make_tuple(
                net::FirstPartySetMetadata(&primary_entry, &associated_entry),
                net::FirstPartySetsCacheFilter::MatchInfo()));
}

TEST_F(AsyncFirstPartySetsAccessDelegateTest, OverrideSets_FindEntries) {
  delegate_remote()->NotifyReady(CreateFirstPartySetsReadyEvent(
      net::FirstPartySetsContextConfig({
          {kSet3Primary,
           net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
               kSet3Primary, net::SiteType::kPrimary, std::nullopt))},
      }),
      /*cache_filter=*/std::nullopt));

  EXPECT_THAT(FindEntriesAndWait({kSet3Primary}),
              UnorderedElementsAre(Pair(kSet3Primary, _)));
}

class SyncFirstPartySetsAccessDelegateTest
    : public AsyncFirstPartySetsAccessDelegateTest {
 public:
  SyncFirstPartySetsAccessDelegateTest() {
    delegate_remote()->NotifyReady(CreateFirstPartySetsReadyEvent(
        net::FirstPartySetsContextConfig({
            {kSet3AssociatedSite1,
             net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                 kSet3Primary, net::SiteType::kAssociated, 0))},
            {kSet3Primary,
             net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                 kSet3Primary, net::SiteType::kPrimary, std::nullopt))},
        }),
        net::FirstPartySetsCacheFilter({{kSet1Primary, kClearAtRunId}},
                                       kBrowserRunId)));
  }
};

TEST_F(SyncFirstPartySetsAccessDelegateTest, ComputeMetadata) {
  net::FirstPartySetEntry primary_entry(kSet1Primary, net::SiteType::kPrimary,
                                        /*site_index=*/std::nullopt);
  net::FirstPartySetEntry associated_entry(kSet1Primary,
                                           net::SiteType::kAssociated, 0);

  net::FirstPartySetsCacheFilter::MatchInfo match_info;
  match_info.clear_at_run_id = kClearAtRunId;
  match_info.browser_run_id = kBrowserRunId;

  EXPECT_EQ(ComputeMetadataAndWait(kSet1Primary, &kSet1AssociatedSite1),
            std::make_tuple(
                net::FirstPartySetMetadata(&primary_entry, &associated_entry),
                match_info));
}

TEST_F(SyncFirstPartySetsAccessDelegateTest, FindEntries) {
  EXPECT_THAT(FindEntriesAndWait({kSet1AssociatedSite1, kSet2AssociatedSite1,
                                  kSet3AssociatedSite1}),
              FirstPartySetsAccessDelegate::EntriesResult({
                  {kSet1AssociatedSite1,
                   net::FirstPartySetEntry(kSet1Primary,
                                           net::SiteType::kAssociated, 0)},
                  {kSet2AssociatedSite1,
                   net::FirstPartySetEntry(kSet2Primary,
                                           net::SiteType::kAssociated, 0)},
                  {kSet3AssociatedSite1,
                   net::FirstPartySetEntry(kSet3Primary,
                                           net::SiteType::kAssociated, 0)},
              }));
}

// Verifies the behaviors of the delegate when First-Party Sets are initially
// enabled but disabled later on.
// Queries should only be deferred if they arrive when the delegate is enabled
// and NotifyReady hasn't been called yet.
class FirstPartySetsAccessDelegateSetToDisabledTest
    : public FirstPartySetsAccessDelegateTest {
 public:
  FirstPartySetsAccessDelegateSetToDisabledTest()
      : FirstPartySetsAccessDelegateTest(/*enabled=*/true,
                                         /*wait_for_init=*/true) {}
};

TEST_F(FirstPartySetsAccessDelegateSetToDisabledTest,
       DisabledThenReady_ComputeMetadata) {
  std::tuple<net::FirstPartySetMetadata,
             net::FirstPartySetsCacheFilter::MatchInfo>
      expected(net::FirstPartySetMetadata(
                   /*frame_entry=*/nullptr,
                   /*top_frame_entry=*/nullptr),
               net::FirstPartySetsCacheFilter::MatchInfo());

  base::test::TestFuture<net::FirstPartySetMetadata,
                         net::FirstPartySetsCacheFilter::MatchInfo>
      future;
  EXPECT_FALSE(delegate().ComputeMetadata(
      kSet1AssociatedSite1, &kSet1AssociatedSite1, future.GetCallback()));

  delegate().SetEnabled(false);

  // All queries received when the delegate is disabled receive empty responses.
  EXPECT_EQ(ComputeMetadataAndWait(kSet1AssociatedSite1, &kSet1AssociatedSite1),
            expected);

  delegate().NotifyReady(mojom::FirstPartySetsReadyEvent::New());

  // Queries received when the delegate is enabled receive non-empty responses
  // once the config is ready.
  EXPECT_NE(future.Take(), expected);

  EXPECT_EQ(ComputeMetadataAndWait(kSet1AssociatedSite1, &kSet1AssociatedSite1),
            expected);
}

TEST_F(FirstPartySetsAccessDelegateSetToDisabledTest,
       DisabledThenReady_FindEntries) {
  base::test::TestFuture<
      base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>>
      future;
  EXPECT_FALSE(
      delegate().FindEntries({kSet1AssociatedSite1}, future.GetCallback()));

  delegate().SetEnabled(false);

  // All queries received when the delegate is disabled receive empty responses.
  EXPECT_THAT(FindEntriesAndWait({kSet1AssociatedSite1}), IsEmpty());

  delegate().NotifyReady(mojom::FirstPartySetsReadyEvent::New());

  // Queries received when the delegate is enabled receive non-empty responses
  // once the config is ready.
  EXPECT_THAT(future.Take(), Not(IsEmpty()));

  EXPECT_THAT(FindEntriesAndWait({kSet1AssociatedSite1}), IsEmpty());
}

TEST_F(FirstPartySetsAccessDelegateSetToDisabledTest,
       ReadyThenDisabled_ComputeMetadata) {
  base::test::TestFuture<net::FirstPartySetMetadata,
                         net::FirstPartySetsCacheFilter::MatchInfo>
      future;
  EXPECT_FALSE(delegate().ComputeMetadata(
      kSet1AssociatedSite1, &kSet1AssociatedSite1, future.GetCallback()));

  delegate_remote()->NotifyReady(mojom::FirstPartySetsReadyEvent::New());

  net::FirstPartySetEntry entry(kSet1Primary, net::SiteType::kAssociated, 0);
  EXPECT_EQ(future.Get(),
            std::make_tuple(net::FirstPartySetMetadata(&entry, &entry),
                            net::FirstPartySetsCacheFilter::MatchInfo()));

  ComputeMetadataAndWait(kSet1AssociatedSite1, &kSet1AssociatedSite1);

  delegate().SetEnabled(false);
  std::tuple<net::FirstPartySetMetadata,
             net::FirstPartySetsCacheFilter::MatchInfo>
      expected(net::FirstPartySetMetadata(
                   /*frame_entry=*/nullptr,
                   /*top_frame_entry=*/nullptr),
               net::FirstPartySetsCacheFilter::MatchInfo());
  EXPECT_EQ(ComputeMetadataAndWait(kSet1AssociatedSite1, &kSet1AssociatedSite1),
            expected);
}

TEST_F(FirstPartySetsAccessDelegateSetToDisabledTest,
       ReadyThenDisabled_FindEntries) {
  base::test::TestFuture<FirstPartySetsAccessDelegate::EntriesResult> future;

  EXPECT_FALSE(
      delegate().FindEntries({kSet1AssociatedSite1}, future.GetCallback()));
  delegate_remote()->NotifyReady(mojom::FirstPartySetsReadyEvent::New());
  EXPECT_THAT(future.Get(),
              FirstPartySetsAccessDelegate::EntriesResult(
                  {{kSet1AssociatedSite1,
                    net::FirstPartySetEntry(kSet1Primary,
                                            net::SiteType::kAssociated, 0)}}));
  FindEntriesAndWait({kSet1AssociatedSite1});

  delegate().SetEnabled(false);
  EXPECT_THAT(FindEntriesAndWait({kSet1AssociatedSite1}), IsEmpty());
}

// Verifies the behaviors of the delegate when First-Party Sets are initially
// disabled but enabled later on.
// Queries should only be deferred if they arrive when the delegate is enabled
// and NotifyReady hasn't been called yet.
class FirstPartySetsAccessDelegateSetToEnabledTest
    : public FirstPartySetsAccessDelegateTest {
 public:
  FirstPartySetsAccessDelegateSetToEnabledTest()
      : FirstPartySetsAccessDelegateTest(/*enabled=*/false,
                                         /*wait_for_init=*/true) {}
};

// This scenario might not be reproducible in production code but it's worth
// testing nonetheless. We may add metrics to observe how often this case
// occurs.
TEST_F(FirstPartySetsAccessDelegateSetToEnabledTest,
       EnabledThenReady_ComputeMetadata) {
  std::tuple<net::FirstPartySetMetadata,
             net::FirstPartySetsCacheFilter::MatchInfo>
      expected(net::FirstPartySetMetadata(
                   /*frame_entry=*/nullptr,
                   /*top_frame_entry=*/nullptr),
               net::FirstPartySetsCacheFilter::MatchInfo());
  EXPECT_EQ(ComputeMetadataAndWait(kSet1AssociatedSite1, &kSet1AssociatedSite1),
            expected);

  delegate().SetEnabled(true);

  base::test::TestFuture<net::FirstPartySetMetadata,
                         net::FirstPartySetsCacheFilter::MatchInfo>
      future;
  net::FirstPartySetEntry primary_entry(kSet2Primary, net::SiteType::kPrimary,
                                        std::nullopt);
  net::FirstPartySetEntry associated_entry(kSet2Primary,
                                           net::SiteType::kAssociated, 0);
  EXPECT_FALSE(delegate().ComputeMetadata(kSet2Primary, &kSet1AssociatedSite1,
                                          future.GetCallback()));
  delegate_remote()->NotifyReady(CreateFirstPartySetsReadyEvent(
      net::FirstPartySetsContextConfig(
          {{kSet1AssociatedSite1,
            net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                kSet2Primary, net::SiteType::kAssociated, 0))}}),
      /*cache_filter=*/std::nullopt));
  EXPECT_EQ(future.Get(),
            std::make_tuple(
                net::FirstPartySetMetadata(&primary_entry, &associated_entry),
                net::FirstPartySetsCacheFilter::MatchInfo()));
  ComputeMetadataAndWait(kSet1AssociatedSite1, &kSet1AssociatedSite1);
}

TEST_F(FirstPartySetsAccessDelegateSetToEnabledTest,
       EnabledThenReady_FindEntries) {
  EXPECT_EQ(FindEntriesAndWait({kSet1AssociatedSite1}),
            FirstPartySetsAccessDelegate::EntriesResult());

  delegate().SetEnabled(true);

  base::test::TestFuture<FirstPartySetsAccessDelegate::EntriesResult> future;
  EXPECT_FALSE(
      delegate().FindEntries({kSet1AssociatedSite1}, future.GetCallback()));
  delegate_remote()->NotifyReady(CreateFirstPartySetsReadyEvent(
      net::FirstPartySetsContextConfig(
          {{kSet1AssociatedSite1,
            net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                kSet2Primary, net::SiteType::kAssociated, 0))}}),
      /*cache_filter=*/std::nullopt));
  EXPECT_EQ(future.Get(),
            FirstPartySetsAccessDelegate::EntriesResult(
                {{kSet1AssociatedSite1,
                  net::FirstPartySetEntry(kSet2Primary,
                                          net::SiteType::kAssociated, 0)}}));
  FindEntriesAndWait({kSet1AssociatedSite1});
}

TEST_F(FirstPartySetsAccessDelegateSetToEnabledTest,
       ReadyThenEnabled_ComputeMetadata) {
  std::tuple<net::FirstPartySetMetadata,
             net::FirstPartySetsCacheFilter::MatchInfo>
      expected(net::FirstPartySetMetadata(
                   /*frame_entry=*/nullptr,
                   /*top_frame_entry=*/nullptr),
               net::FirstPartySetsCacheFilter::MatchInfo());
  EXPECT_EQ(ComputeMetadataAndWait(kSet1AssociatedSite1, &kSet1AssociatedSite1),
            expected);
  delegate().NotifyReady(mojom::FirstPartySetsReadyEvent::New());
  delegate().SetEnabled(true);

  EXPECT_NE(ComputeMetadataAndWait(kSet1AssociatedSite1, &kSet1AssociatedSite1),
            expected);
}

TEST_F(FirstPartySetsAccessDelegateSetToEnabledTest,
       ReadyThenEnabled_FindEntries) {
  EXPECT_EQ(FindEntriesAndWait({kSet1AssociatedSite1}),
            FirstPartySetsAccessDelegate::EntriesResult());
  delegate().NotifyReady(mojom::FirstPartySetsReadyEvent::New());
  delegate().SetEnabled(true);

  EXPECT_THAT(FindEntriesAndWait({kSet1AssociatedSite1}),
              UnorderedElementsAre(Pair(kSet1AssociatedSite1, _)));
}

class AsyncNonwaitingFirstPartySetsAccessDelegateTest
    : public FirstPartySetsAccessDelegateTest {
 public:
  AsyncNonwaitingFirstPartySetsAccessDelegateTest()
      : FirstPartySetsAccessDelegateTest(/*enabled=*/true,
                                         /*wait_for_init=*/false) {}
};

TEST_F(AsyncNonwaitingFirstPartySetsAccessDelegateTest,
       QueryBeforeReady_ComputeMetadata) {
  base::HistogramTester histogram_tester;
  EXPECT_EQ(
      std::make_optional(
          std::make_pair(net::FirstPartySetMetadata(),
                         net::FirstPartySetsCacheFilter::MatchInfo())),
      delegate().ComputeMetadata(kSet1AssociatedSite1, &kSet1AssociatedSite1,
                                 base::NullCallback()));

  delegate_remote()->NotifyReady(mojom::FirstPartySetsReadyEvent::New());
  base::RunLoop().RunUntilIdle();

  net::FirstPartySetEntry entry(kSet1Primary, net::SiteType::kAssociated, 0);
  EXPECT_EQ(
      std::make_optional(
          std::make_pair(net::FirstPartySetMetadata(&entry, &entry),
                         net::FirstPartySetsCacheFilter::MatchInfo())),
      delegate().ComputeMetadata(kSet1AssociatedSite1, &kSet1AssociatedSite1,
                                 base::NullCallback()));

  histogram_tester.ExpectUniqueSample(
      kDelayedQueriesCountHistogram, /*sample=*/0, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kMostDelayedQuerDeltaHistogram,
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/1);
}

TEST_F(AsyncNonwaitingFirstPartySetsAccessDelegateTest,
       QueryBeforeReady_FindEntries) {
  base::HistogramTester histogram_tester;
  EXPECT_THAT(
      delegate().FindEntries({kSet1AssociatedSite1, kSet2AssociatedSite1},
                             base::NullCallback()),
      Optional(IsEmpty()));

  delegate_remote()->NotifyReady(mojom::FirstPartySetsReadyEvent::New());
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(
      delegate().FindEntries({kSet1AssociatedSite1, kSet2AssociatedSite1},
                             base::NullCallback()),
      FirstPartySetsAccessDelegate::EntriesResult({
          {kSet1AssociatedSite1,
           net::FirstPartySetEntry(kSet1Primary, net::SiteType::kAssociated,
                                   0)},
          {kSet2AssociatedSite1,
           net::FirstPartySetEntry(kSet2Primary, net::SiteType::kAssociated,
                                   0)},
      }));
  histogram_tester.ExpectUniqueSample(
      kDelayedQueriesCountHistogram, /*sample=*/0, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kMostDelayedQuerDeltaHistogram,
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/1);
}

TEST_F(AsyncNonwaitingFirstPartySetsAccessDelegateTest,
       OverrideSets_ComputeMetadata) {
  base::HistogramTester histogram_tester;
  delegate_remote()->NotifyReady(CreateFirstPartySetsReadyEvent(
      net::FirstPartySetsContextConfig({
          {kSet1AssociatedSite1,
           net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
               kSet3Primary, net::SiteType::kAssociated, 0))},
          {kSet3Primary,
           net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
               kSet3Primary, net::SiteType::kPrimary, std::nullopt))},
      }),
      /*cache_filter=*/std::nullopt));
  base::RunLoop().RunUntilIdle();

  net::FirstPartySetEntry primary_entry(kSet3Primary, net::SiteType::kPrimary,
                                        std::nullopt);
  net::FirstPartySetEntry associated_entry(kSet3Primary,
                                           net::SiteType::kAssociated, 0);
  EXPECT_EQ(ComputeMetadataAndWait(kSet3Primary, &kSet1AssociatedSite1),
            std::make_tuple(
                net::FirstPartySetMetadata(&primary_entry, &associated_entry),
                net::FirstPartySetsCacheFilter::MatchInfo()));
  histogram_tester.ExpectUniqueSample(
      kDelayedQueriesCountHistogram, /*sample=*/0, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kMostDelayedQuerDeltaHistogram,
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/1);
}

TEST_F(AsyncNonwaitingFirstPartySetsAccessDelegateTest,
       OverrideSets_FindEntries) {
  base::HistogramTester histogram_tester;
  delegate_remote()->NotifyReady(CreateFirstPartySetsReadyEvent(
      net::FirstPartySetsContextConfig({
          {kSet3Primary,
           net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
               kSet3Primary, net::SiteType::kPrimary, std::nullopt))},
      }),
      /*cache_filter=*/std::nullopt));
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(FindEntriesAndWait({kSet3Primary}),
              UnorderedElementsAre(Pair(kSet3Primary, _)));
  histogram_tester.ExpectUniqueSample(
      kDelayedQueriesCountHistogram, /*sample=*/0, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kMostDelayedQuerDeltaHistogram,
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/1);
}

}  // namespace network

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_sets_access_delegate.h"

#include <optional>

#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
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
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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

// No-op FirstPartySetsAccessDelegate should just pass queries to
// FirstPartySetsManager synchronously.
class NoopFirstPartySetsAccessDelegateTest : public ::testing::Test {
 public:
  NoopFirstPartySetsAccessDelegateTest()
      : first_party_sets_manager_(/*enabled=*/true),
        delegate_(
            /*receiver=*/mojo::NullReceiver(),
            /*params=*/nullptr,
            &first_party_sets_manager_) {
    first_party_sets_manager_.SetCompleteSets(
        net::GlobalFirstPartySets::CreateForTesting(
            base::Version("1.2.3"),
            /*entries=*/
            {
                {kSet1AssociatedSite1,
                 net::FirstPartySetEntry(kSet1Primary,
                                         net::SiteType::kAssociated)},
                {kSet1AssociatedSite2,
                 net::FirstPartySetEntry(kSet1Primary,
                                         net::SiteType::kAssociated)},
                {kSet1Primary, net::FirstPartySetEntry(
                                   kSet1Primary, net::SiteType::kPrimary)},
                {kSet2AssociatedSite1,
                 net::FirstPartySetEntry(kSet2Primary,
                                         net::SiteType::kAssociated)},
                {kSet2Primary, net::FirstPartySetEntry(
                                   kSet2Primary, net::SiteType::kPrimary)},
            },
            /*aliases=*/{}));
  }

  FirstPartySetsAccessDelegate& delegate() { return delegate_; }

 private:
  FirstPartySetsManager first_party_sets_manager_;
  FirstPartySetsAccessDelegate delegate_;
};

TEST_F(NoopFirstPartySetsAccessDelegateTest, ComputeMetadata) {
  EXPECT_EQ(
      delegate().ComputeMetadata(kSet1AssociatedSite1, &kSet1Primary),
      std::make_optional(std::make_pair(
          net::FirstPartySetMetadata(
              net::FirstPartySetEntry(kSet1Primary, net::SiteType::kAssociated),
              net::FirstPartySetEntry(kSet1Primary, net::SiteType::kPrimary)),
          net::FirstPartySetsCacheFilter::MatchInfo())));
}

class FirstPartySetsAccessDelegateTest : public ::testing::Test {
 public:
  explicit FirstPartySetsAccessDelegateTest(bool enabled)
      : first_party_sets_manager_(/*enabled=*/true),
        delegate_(delegate_remote_.BindNewPipeAndPassReceiver(),
                  CreateFirstPartySetsAccessDelegateParams(enabled),
                  &first_party_sets_manager_) {
    first_party_sets_manager_.SetCompleteSets(
        net::GlobalFirstPartySets::CreateForTesting(
            base::Version("1.2.3"),
            /*entries=*/
            {
                {kSet1AssociatedSite1,
                 net::FirstPartySetEntry(kSet1Primary,
                                         net::SiteType::kAssociated)},
                {kSet1AssociatedSite2,
                 net::FirstPartySetEntry(kSet1Primary,
                                         net::SiteType::kAssociated)},
                {kSet1Primary, net::FirstPartySetEntry(
                                   kSet1Primary, net::SiteType::kPrimary)},
                {kSet2AssociatedSite1,
                 net::FirstPartySetEntry(kSet2Primary,
                                         net::SiteType::kAssociated)},
                {kSet2Primary, net::FirstPartySetEntry(
                                   kSet2Primary, net::SiteType::kPrimary)},
            },
            /*aliases=*/{}));
  }

  std::tuple<net::FirstPartySetMetadata,
             net::FirstPartySetsCacheFilter::MatchInfo>
  ComputeMetadata(const net::SchemefulSite& site,
                  const net::SchemefulSite* top_frame_site) {
    return delegate_.ComputeMetadata(site, top_frame_site);
  }

  FirstPartySetsAccessDelegate& delegate() { return delegate_; }

  void FlushDelegateForTesting() { delegate_remote_.FlushForTesting(); }

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
      : FirstPartySetsAccessDelegateTest(/*enabled=*/false) {}
};

TEST_F(FirstPartySetsAccessDelegateDisabledTest, ComputeMetadata) {
  EXPECT_EQ(ComputeMetadata(kSet1AssociatedSite1, &kSet1AssociatedSite1),
            std::make_tuple(net::FirstPartySetMetadata(),
                            net::FirstPartySetsCacheFilter::MatchInfo()));
}

// Test fixture that allows precise control over when the instance gets FPS
// data. Useful for testing async flows.
class AsyncFirstPartySetsAccessDelegateTest
    : public FirstPartySetsAccessDelegateTest {
 public:
  AsyncFirstPartySetsAccessDelegateTest()
      : FirstPartySetsAccessDelegateTest(/*enabled=*/true) {}
};


class SyncFirstPartySetsAccessDelegateTest
    : public AsyncFirstPartySetsAccessDelegateTest {
 public:
  SyncFirstPartySetsAccessDelegateTest() {
    delegate_remote()->NotifyReady(CreateFirstPartySetsReadyEvent(
        net::FirstPartySetsContextConfig::Create(
            {
                {kSet3AssociatedSite1,
                 net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                     kSet3Primary, net::SiteType::kAssociated))},
                {kSet3Primary,
                 net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                     kSet3Primary, net::SiteType::kPrimary))},
            })
            .value(),
        net::FirstPartySetsCacheFilter({{kSet1Primary, kClearAtRunId}},
                                       kBrowserRunId)));
    FlushDelegateForTesting();
  }
};

TEST_F(SyncFirstPartySetsAccessDelegateTest, ComputeMetadata) {
  net::FirstPartySetsCacheFilter::MatchInfo match_info;
  match_info.clear_at_run_id = kClearAtRunId;
  match_info.browser_run_id = kBrowserRunId;

  EXPECT_EQ(ComputeMetadata(kSet1Primary, &kSet1AssociatedSite1),
            std::make_tuple(net::FirstPartySetMetadata(
                                net::FirstPartySetEntry(
                                    kSet1Primary, net::SiteType::kPrimary),
                                net::FirstPartySetEntry(
                                    kSet1Primary, net::SiteType::kAssociated)),
                            match_info));
}

class AsyncNonwaitingFirstPartySetsAccessDelegateTest
    : public FirstPartySetsAccessDelegateTest {
 public:
  AsyncNonwaitingFirstPartySetsAccessDelegateTest()
      : FirstPartySetsAccessDelegateTest(/*enabled=*/true) {}
};

TEST_F(AsyncNonwaitingFirstPartySetsAccessDelegateTest,
       QueryBeforeReady_ComputeMetadata) {
  EXPECT_EQ(
      std::make_optional(
          std::make_pair(net::FirstPartySetMetadata(),
                         net::FirstPartySetsCacheFilter::MatchInfo())),
      delegate().ComputeMetadata(kSet1AssociatedSite1, &kSet1AssociatedSite1));

  delegate_remote()->NotifyReady(mojom::FirstPartySetsReadyEvent::New());
  FlushDelegateForTesting();

  net::FirstPartySetEntry entry(kSet1Primary, net::SiteType::kAssociated);
  EXPECT_EQ(
      std::make_optional(
          std::make_pair(net::FirstPartySetMetadata(entry, entry),
                         net::FirstPartySetsCacheFilter::MatchInfo())),
      delegate().ComputeMetadata(kSet1AssociatedSite1, &kSet1AssociatedSite1));
}

TEST_F(AsyncNonwaitingFirstPartySetsAccessDelegateTest,
       OverrideSets_ComputeMetadata) {
  delegate_remote()->NotifyReady(CreateFirstPartySetsReadyEvent(
      net::FirstPartySetsContextConfig::Create(
          {
              {kSet1AssociatedSite1,
               net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                   kSet3Primary, net::SiteType::kAssociated))},
              {kSet3Primary,
               net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                   kSet3Primary, net::SiteType::kPrimary))},
          })
          .value(),
      /*cache_filter=*/std::nullopt));
  FlushDelegateForTesting();

  EXPECT_EQ(ComputeMetadata(kSet3Primary, &kSet1AssociatedSite1),
            std::make_tuple(net::FirstPartySetMetadata(
                                net::FirstPartySetEntry(
                                    kSet3Primary, net::SiteType::kPrimary),
                                net::FirstPartySetEntry(
                                    kSet3Primary, net::SiteType::kAssociated)),
                            net::FirstPartySetsCacheFilter::MatchInfo()));
}

}  // namespace network

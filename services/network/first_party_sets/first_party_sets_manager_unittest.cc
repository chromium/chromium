// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_sets_manager.h"

#include <initializer_list>
#include <optional>
#include <set>
#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace network {

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

class FirstPartySetsManagerTest : public ::testing::Test,
                                  public WaitingFeatureInitializer {
 public:
  explicit FirstPartySetsManagerTest(bool enabled, bool wait_for_init)
      : WaitingFeatureInitializer(wait_for_init), manager_(enabled) {}

  void SetCompleteSets(
      const base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>&
          content,
      const base::flat_map<net::SchemefulSite, net::SchemefulSite>& aliases) {
    manager_.SetCompleteSets(
        net::GlobalFirstPartySets(base::Version("1.2.3"), content, aliases));
  }

  FirstPartySetsManager::EntriesResult FindEntriesAndWait(
      const base::flat_set<net::SchemefulSite>& site) {
    base::test::TestFuture<FirstPartySetsManager::EntriesResult> future;
    std::optional<FirstPartySetsManager::EntriesResult> result =
        manager_.FindEntries(site, net::FirstPartySetsContextConfig(),
                             future.GetCallback());
    return result.has_value() ? result.value() : future.Get();
  }

  FirstPartySetsManager& manager() { return manager_; }

 private:
  base::test::TaskEnvironment env_;
  FirstPartySetsManager manager_;
};

class FirstPartySetsManagerDisabledTest : public FirstPartySetsManagerTest {
 public:
  FirstPartySetsManagerDisabledTest()
      : FirstPartySetsManagerTest(/*enabled=*/false, /*wait_for_init=*/true) {}
};

TEST_F(FirstPartySetsManagerDisabledTest, SetCompleteSets) {
  net::SchemefulSite example_cctld(GURL("https://example.cctld"));
  net::SchemefulSite example_test(GURL("https://example.test"));
  net::SchemefulSite aaaa(GURL("https://aaaa.test"));

  SetCompleteSets({{aaaa, net::FirstPartySetEntry(example_test,
                                                  net::SiteType::kAssociated)},
                   {example_test, net::FirstPartySetEntry(
                                      example_test, net::SiteType::kPrimary)}},
                  {{example_cctld, example_test}});

  EXPECT_THAT(manager().FindEntries(
                  {
                      aaaa,
                      example_test,
                      example_cctld,
                  },
                  net::FirstPartySetsContextConfig(), base::NullCallback()),
              Optional(IsEmpty()));
}

TEST_F(FirstPartySetsManagerDisabledTest, FindEntries) {
  EXPECT_THAT(manager().FindEntries(
                  {net::SchemefulSite(GURL("https://example.test"))},
                  net::FirstPartySetsContextConfig(), base::NullCallback()),
              Optional(IsEmpty()));
}

class FirstPartySetsManagerEnabledTest : public FirstPartySetsManagerTest {
 public:
  FirstPartySetsManagerEnabledTest()
      : FirstPartySetsManagerTest(/*enabled=*/true, /*wait_for_init=*/true) {}
};

TEST_F(FirstPartySetsManagerEnabledTest, SetCompleteSets) {
  net::SchemefulSite example_cctld(GURL("https://example.cctld"));
  net::SchemefulSite example_test(GURL("https://example.test"));
  net::SchemefulSite aaaa(GURL("https://aaaa.test"));

  SetCompleteSets({{aaaa, net::FirstPartySetEntry(example_test,
                                                  net::SiteType::kAssociated)},
                   {example_test, net::FirstPartySetEntry(
                                      example_test, net::SiteType::kPrimary)}},
                  {{example_cctld, example_test}});

  EXPECT_THAT(
      FindEntriesAndWait({
          aaaa,
          example_test,
          example_cctld,
      }),
      UnorderedElementsAre(
          Pair(example_test,
               net::FirstPartySetEntry(example_test, net::SiteType::kPrimary)),
          Pair(example_cctld,
               net::FirstPartySetEntry(example_test, net::SiteType::kPrimary)),
          Pair(aaaa, net::FirstPartySetEntry(example_test,
                                             net::SiteType::kAssociated))));
}

TEST_F(FirstPartySetsManagerEnabledTest, SetCompleteSets_Idempotent) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite aaaa(GURL("https://aaaa.test"));

  SetCompleteSets({}, {});
  EXPECT_THAT(FindEntriesAndWait({}), IsEmpty());

  // The second call to SetCompleteSets should have no effect.
  SetCompleteSets(
      {{aaaa, net::FirstPartySetEntry(example, net::SiteType::kAssociated)},
       {example, net::FirstPartySetEntry(example, net::SiteType::kPrimary)}},
      {});
  EXPECT_THAT(FindEntriesAndWait({
                  aaaa,
                  example,
              }),
              IsEmpty());
}

// Test fixture that allows precise control over when the instance gets FPS
// data. Useful for testing async flows.
class AsyncPopulatedFirstPartySetsManagerTest
    : public FirstPartySetsManagerTest {
 public:
  explicit AsyncPopulatedFirstPartySetsManagerTest(bool wait_for_init)
      : FirstPartySetsManagerTest(/*enabled=*/true,
                                  /*wait_for_init=*/wait_for_init) {}

 protected:
  void Populate() {
    net::SchemefulSite foo(GURL("https://foo.test"));
    net::SchemefulSite example_test(GURL("https://example.test"));
    net::SchemefulSite example_cctld(GURL("https://example.cctld"));
    // /*content=*/ R"(
    //   [
    //     {
    //       "primary": "https://example.test",
    //       "associatedSites": ["https://associatedSite1.test",
    //       "https://associatedSite3.test"]
    //     },
    //     {
    //       "primary": "https://foo.test",
    //       "associatedSites": ["https://associatedSite2.test"]
    //     }
    //   ]
    //   )";

    SetCompleteSets(
        {
            {net::SchemefulSite(GURL("https://associatedSite1.test")),
             net::FirstPartySetEntry(example_test, net::SiteType::kAssociated)},
            {net::SchemefulSite(GURL("https://associatedSite3.test")),
             net::FirstPartySetEntry(example_test, net::SiteType::kAssociated)},
            {example_test,
             net::FirstPartySetEntry(example_test, net::SiteType::kPrimary)},
            {net::SchemefulSite(GURL("https://associatedSite2.test")),
             net::FirstPartySetEntry(foo, net::SiteType::kAssociated)},
            {foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary)},
        },
        {{example_cctld, example_test}});

    // We don't wait for the sets to be loaded before returning, in order to let
    // the tests provoke raciness if any exists.
  }
};

class AsyncWaitingFirstPartySetsManagerTest
    : public AsyncPopulatedFirstPartySetsManagerTest {
 public:
  AsyncWaitingFirstPartySetsManagerTest()
      : AsyncPopulatedFirstPartySetsManagerTest(/*wait_for_init=*/true) {}
};

TEST_F(AsyncWaitingFirstPartySetsManagerTest,
       QueryBeforeReady_ComputeMetadata) {
  base::test::TestFuture<net::FirstPartySetMetadata> future;
  {
    // Force deallocation to provoke a UAF if the impl just copies the pointer.
    net::SchemefulSite associatedSite(GURL("https://associatedSite1.test"));

    EXPECT_FALSE(manager().ComputeMetadata(associatedSite, &associatedSite,
                                           net::FirstPartySetsContextConfig(),
                                           future.GetCallback()));
  }

  Populate();

  {
    net::FirstPartySetEntry entry(
        net::SchemefulSite(GURL("https://example.test")),
        net::SiteType::kAssociated);

    EXPECT_EQ(future.Get(), net::FirstPartySetMetadata(entry, entry));
  }
}

TEST_F(AsyncWaitingFirstPartySetsManagerTest, QueryBeforeReady_FindEntries) {
  net::SchemefulSite associatedSite1(GURL("https://associatedSite1.test"));
  net::SchemefulSite associatedSite2(GURL("https://associatedSite2.test"));
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite example_cctld(GURL("https://example.cctld"));

  base::test::TestFuture<FirstPartySetsManager::EntriesResult> future;
  EXPECT_FALSE(manager().FindEntries(
      {associatedSite1, associatedSite2, example_cctld},
      net::FirstPartySetsContextConfig(), future.GetCallback()));

  Populate();

  EXPECT_THAT(
      future.Get(),
      UnorderedElementsAre(
          Pair(associatedSite1,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated)),
          Pair(example_cctld,
               net::FirstPartySetEntry(example, net::SiteType::kPrimary)),
          Pair(associatedSite2,
               net::FirstPartySetEntry(
                   net::SchemefulSite(GURL("https://foo.test")),
                   net::SiteType::kAssociated))));
}

class AsyncNonwaitingFirstPartySetsManagerTest
    : public AsyncPopulatedFirstPartySetsManagerTest {
 public:
  AsyncNonwaitingFirstPartySetsManagerTest()
      : AsyncPopulatedFirstPartySetsManagerTest(/*wait_for_init=*/false) {}
};

TEST_F(AsyncNonwaitingFirstPartySetsManagerTest,
       QueryBeforeReady_ComputeMetadata) {
  net::SchemefulSite associatedSite(GURL("https://associatedSite1.test"));

  EXPECT_EQ(net::FirstPartySetMetadata(),
            manager().ComputeMetadata(associatedSite, &associatedSite,
                                      net::FirstPartySetsContextConfig(),
                                      base::NullCallback()));

  Populate();

  net::FirstPartySetEntry entry(
      net::SchemefulSite(GURL("https://example.test")),
      net::SiteType::kAssociated);

  EXPECT_EQ(net::FirstPartySetMetadata(entry, entry),
            manager().ComputeMetadata(associatedSite, &associatedSite,
                                      net::FirstPartySetsContextConfig(),
                                      base::NullCallback()));
}

TEST_F(AsyncNonwaitingFirstPartySetsManagerTest, QueryBeforeReady_FindEntries) {
  net::SchemefulSite associatedSite1(GURL("https://associatedSite1.test"));
  net::SchemefulSite associatedSite2(GURL("https://associatedSite2.test"));
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite example_cctld(GURL("https://example.cctld"));

  EXPECT_THAT(manager().FindEntries(
                  {associatedSite1, associatedSite2, example_cctld},
                  net::FirstPartySetsContextConfig(), base::NullCallback()),
              Optional(IsEmpty()));

  Populate();

  EXPECT_THAT(
      manager().FindEntries({associatedSite1, associatedSite2, example_cctld},
                            net::FirstPartySetsContextConfig(),
                            base::NullCallback()),
      Optional(UnorderedElementsAre(
          Pair(associatedSite1,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated)),
          Pair(example_cctld,
               net::FirstPartySetEntry(example, net::SiteType::kPrimary)),
          Pair(associatedSite2,
               net::FirstPartySetEntry(
                   net::SchemefulSite(GURL("https://foo.test")),
                   net::SiteType::kAssociated)))));
}

}  // namespace network

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_sets_manager.h"

#include <optional>

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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace network {

class FirstPartySetsManagerTest : public ::testing::Test {
 public:
  explicit FirstPartySetsManagerTest(bool enabled) : manager_(enabled) {}

  void SetCompleteSets(
      const base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>&
          content,
      const base::flat_map<net::SchemefulSite, net::SchemefulSite>& aliases) {
    manager_.SetCompleteSets(net::GlobalFirstPartySets::CreateForTesting(
        base::Version("1.2.3"), content, aliases));
  }

  base::flat_map<net::SchemefulSite, net::FirstPartySetEntry> FindEntries(
      const base::flat_set<net::SchemefulSite>& sites,
      const net::FirstPartySetsContextConfig& config) {
    std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>> entries;
    for (const auto& site : sites) {
      net::FirstPartySetMetadata metadata = manager_.ComputeMetadata(
          site, /*top_frame_site=*/std::nullopt, config);
      if (metadata.frame_entry()) {
        entries.emplace_back(site, metadata.frame_entry().value());
      }
    }
    return entries;
  }

  FirstPartySetsManager& manager() { return manager_; }

 private:
  base::test::TaskEnvironment env_;
  FirstPartySetsManager manager_;
};

class FirstPartySetsManagerDisabledTest : public FirstPartySetsManagerTest {
 public:
  FirstPartySetsManagerDisabledTest()
      : FirstPartySetsManagerTest(/*enabled=*/false) {}
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

  EXPECT_THAT(FindEntries(
                  {
                      aaaa,
                      example_test,
                      example_cctld,
                  },
                  {}),
              IsEmpty());
}

class FirstPartySetsManagerEnabledTest : public FirstPartySetsManagerTest {
 public:
  FirstPartySetsManagerEnabledTest()
      : FirstPartySetsManagerTest(/*enabled=*/true) {}
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
      FindEntries(
          {
              aaaa,
              example_test,
              example_cctld,
          },
          {}),
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
  EXPECT_THAT(FindEntries({}, {}), IsEmpty());

  // The second call to SetCompleteSets should have no effect.
  SetCompleteSets(
      {{aaaa, net::FirstPartySetEntry(example, net::SiteType::kAssociated)},
       {example, net::FirstPartySetEntry(example, net::SiteType::kPrimary)}},
      {});
  EXPECT_THAT(FindEntries(
                  {
                      aaaa,
                      example,
                  },
                  {}),
              IsEmpty());
}

// Test fixture that allows precise control over when the instance gets FPS
// data. Useful for testing async flows.
class AsyncPopulatedFirstPartySetsManagerTest
    : public FirstPartySetsManagerTest {
 public:
  explicit AsyncPopulatedFirstPartySetsManagerTest()
      : FirstPartySetsManagerTest(/*enabled=*/true) {}

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

TEST_F(AsyncPopulatedFirstPartySetsManagerTest,
       QueryBeforeReady_ComputeMetadata) {
  net::SchemefulSite associatedSite(GURL("https://associatedSite1.test"));

  EXPECT_EQ(net::FirstPartySetMetadata(),
            manager().ComputeMetadata(associatedSite, &associatedSite,
                                      net::FirstPartySetsContextConfig()));

  Populate();

  net::FirstPartySetEntry entry(
      net::SchemefulSite(GURL("https://example.test")),
      net::SiteType::kAssociated);

  EXPECT_EQ(net::FirstPartySetMetadata(entry, entry),
            manager().ComputeMetadata(associatedSite, &associatedSite,
                                      net::FirstPartySetsContextConfig()));
}

}  // namespace network

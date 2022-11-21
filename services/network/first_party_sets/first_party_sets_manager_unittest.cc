// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_sets_manager.h"

#include <initializer_list>
#include <set>
#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_constants.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "net/first_party_sets/same_party_context.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

using Type = net::SamePartyContext::Type;

namespace network {

class FirstPartySetsManagerTest : public ::testing::Test {
 public:
  explicit FirstPartySetsManagerTest(bool enabled) : manager_(enabled) {}

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
    absl::optional<FirstPartySetsManager::EntriesResult> result =
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
      : FirstPartySetsManagerTest(/*enabled=*/false) {}
};

TEST_F(FirstPartySetsManagerDisabledTest, SetCompleteSets) {
  net::SchemefulSite example_cctld(GURL("https://example.cctld"));
  net::SchemefulSite example_test(GURL("https://example.test"));
  net::SchemefulSite aaaa(GURL("https://aaaa.test"));
  SetCompleteSets({{aaaa, net::FirstPartySetEntry(
                              example_test, net::SiteType::kAssociated, 0)},
                   {example_test,
                    net::FirstPartySetEntry(
                        example_test, net::SiteType::kPrimary, absl::nullopt)}},
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

class FirstPartySetsEnabledTest : public FirstPartySetsManagerTest {
 public:
  FirstPartySetsEnabledTest() : FirstPartySetsManagerTest(/*enabled=*/true) {}
};

TEST_F(FirstPartySetsEnabledTest, SetCompleteSets) {
  net::SchemefulSite example_cctld(GURL("https://example.cctld"));
  net::SchemefulSite example_test(GURL("https://example.test"));
  net::SchemefulSite aaaa(GURL("https://aaaa.test"));

  SetCompleteSets({{aaaa, net::FirstPartySetEntry(
                              example_test, net::SiteType::kAssociated, 0)},
                   {example_test,
                    net::FirstPartySetEntry(
                        example_test, net::SiteType::kPrimary, absl::nullopt)}},
                  {{example_cctld, example_test}});

  EXPECT_THAT(
      FindEntriesAndWait({
          aaaa,
          example_test,
          example_cctld,
      }),
      UnorderedElementsAre(
          Pair(example_test,
               net::FirstPartySetEntry(example_test, net::SiteType::kPrimary,
                                       absl::nullopt)),
          Pair(example_cctld,
               net::FirstPartySetEntry(example_test, net::SiteType::kPrimary,
                                       absl::nullopt)),
          Pair(aaaa, net::FirstPartySetEntry(example_test,
                                             net::SiteType::kAssociated, 0))));
}

TEST_F(FirstPartySetsEnabledTest, SetCompleteSets_Idempotent) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite aaaa(GURL("https://aaaa.test"));

  SetCompleteSets({}, {});
  EXPECT_THAT(FindEntriesAndWait({}), IsEmpty());

  // The second call to SetCompleteSets should have no effect.
  SetCompleteSets(
      {{aaaa, net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)},
       {example, net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                         absl::nullopt)}},
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
    : public FirstPartySetsEnabledTest {
 public:
  AsyncPopulatedFirstPartySetsManagerTest() = default;

 protected:
  void Populate() {
    net::SchemefulSite foo(GURL("https://foo.test"));
    net::SchemefulSite example_test(GURL("https://example.test"));
    net::SchemefulSite example_cctld(GURL("https://example.cctld"));
    // /*content=*/ R"(
    //   [
    //     {
    //       "owner": "https://example.test",
    //       "members": ["https://member1.test", "https://member3.test"]
    //     },
    //     {
    //       "owner": "https://foo.test",
    //       "members": ["https://member2.test"]
    //     }
    //   ]
    //   )";

    SetCompleteSets(
        {
            {net::SchemefulSite(GURL("https://member1.test")),
             net::FirstPartySetEntry(example_test, net::SiteType::kAssociated,
                                     0)},
            {net::SchemefulSite(GURL("https://member3.test")),
             net::FirstPartySetEntry(example_test, net::SiteType::kAssociated,
                                     0)},
            {example_test,
             net::FirstPartySetEntry(example_test, net::SiteType::kPrimary,
                                     absl::nullopt)},
            {net::SchemefulSite(GURL("https://member2.test")),
             net::FirstPartySetEntry(foo, net::SiteType::kAssociated, 0)},
            {foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                          absl::nullopt)},
        },
        {{example_cctld, example_test}});

    // We don't wait for the sets to be loaded before returning, in order to let
    // the tests provoke raciness if any exists.
  }
};

TEST_F(AsyncPopulatedFirstPartySetsManagerTest,
       QueryBeforeReady_ComputeMetadata) {
  base::test::TestFuture<net::FirstPartySetMetadata> future;
  {
    // Force deallocation to provoke a UAF if the impl just copies the pointer.
    net::SchemefulSite member(GURL("https://member1.test"));

    EXPECT_FALSE(manager().ComputeMetadata(member, &member, {member},
                                           net::FirstPartySetsContextConfig(),
                                           future.GetCallback()));
  }

  Populate();

  {
    net::SchemefulSite owner(GURL("https://example.test"));
    net::FirstPartySetEntry entry(owner, net::SiteType::kAssociated, 0);

    EXPECT_EQ(future.Get(),
              net::FirstPartySetMetadata(
                  net::SamePartyContext(Type::kSameParty), &entry, &entry));
  }
}

TEST_F(AsyncPopulatedFirstPartySetsManagerTest, QueryBeforeReady_FindEntries) {
  net::SchemefulSite member1(GURL("https://member1.test"));
  net::SchemefulSite member2(GURL("https://member2.test"));
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite example_cctld(GURL("https://example.cctld"));

  base::test::TestFuture<FirstPartySetsManager::EntriesResult> future;
  EXPECT_FALSE(manager().FindEntries({member1, member2, example_cctld},
                                     net::FirstPartySetsContextConfig(),
                                     future.GetCallback()));

  Populate();

  EXPECT_THAT(
      future.Get(),
      UnorderedElementsAre(
          Pair(member1,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)),
          Pair(example_cctld,
               net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                       absl::nullopt)),
          Pair(member2, net::FirstPartySetEntry(
                            net::SchemefulSite(GURL("https://foo.test")),
                            net::SiteType::kAssociated, 0))));
}

}  // namespace network

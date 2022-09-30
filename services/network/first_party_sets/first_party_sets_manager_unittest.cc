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
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_constants.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_metadata.h"
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
using ::testing::Value;

using Type = net::SamePartyContext::Type;
using OverrideSets =
    base::flat_map<net::SchemefulSite, absl::optional<net::FirstPartySetEntry>>;

namespace network {

class FirstPartySetsManagerTest : public ::testing::Test {
 public:
  explicit FirstPartySetsManagerTest(bool enabled) : manager_(enabled) {}

  void SetCompleteSets(
      const base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>&
          content,
      const base::flat_map<net::SchemefulSite, net::SchemefulSite>& aliases) {
    manager_.SetCompleteSets(net::GlobalFirstPartySets(content, aliases));
  }

  net::FirstPartySetMetadata ComputeMetadataAndWait(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      const std::set<net::SchemefulSite>& party_context) {
    base::test::TestFuture<net::FirstPartySetMetadata> future;
    absl::optional<net::FirstPartySetMetadata> result =
        manager_.ComputeMetadata(site, top_frame_site, party_context,
                                 fps_context_config_, future.GetCallback());
    return result.has_value() ? std::move(result).value() : future.Take();
  }

  FirstPartySetsManager::EntriesResult FindEntriesAndWait(
      const base::flat_set<net::SchemefulSite>& site) {
    base::test::TestFuture<FirstPartySetsManager::EntriesResult> future;
    absl::optional<FirstPartySetsManager::EntriesResult> result =
        manager_.FindEntries(site, fps_context_config_, future.GetCallback());
    return result.has_value() ? result.value() : future.Get();
  }

  FirstPartySetsManager& manager() { return manager_; }

  net::FirstPartySetsContextConfig& fps_context_config() {
    return fps_context_config_;
  }

  base::test::TaskEnvironment& env() { return env_; }

 protected:
  void SetFirstPartySetsContextConfig(OverrideSets customizations) {
    fps_context_config_ =
        net::FirstPartySetsContextConfig(std::move(customizations));
  }

 private:
  base::test::TaskEnvironment env_;
  FirstPartySetsManager manager_;
  net::FirstPartySetsContextConfig fps_context_config_;
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
                  fps_context_config(), base::NullCallback()),
              Optional(IsEmpty()));
}

TEST_F(FirstPartySetsManagerDisabledTest, FindEntries) {
  EXPECT_THAT(
      manager().FindEntries({net::SchemefulSite(GURL("https://example.test"))},
                            fps_context_config(), base::NullCallback()),
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

    EXPECT_FALSE(manager().ComputeMetadata(
        member, &member, {member}, fps_context_config(), future.GetCallback()));
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
                                     fps_context_config(),
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

class PopulatedFirstPartySetsManagerTest
    : public AsyncPopulatedFirstPartySetsManagerTest {
 public:
  PopulatedFirstPartySetsManagerTest() { Populate(); }
};

TEST_F(PopulatedFirstPartySetsManagerTest, ComputeMetadata_EmptyContext) {
  net::SchemefulSite example_site(GURL("https://example.test"));
  net::SchemefulSite nonmember(GURL("https://nonmember.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_EQ(ComputeMetadataAndWait(nonmember, top_frame, {})
                  .context()
                  .context_type(),
              Type::kCrossParty);

    EXPECT_EQ(ComputeMetadataAndWait(example_site, top_frame, {})
                  .context()
                  .context_type(),
              Type::kSameParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("http://example.test")),
                               top_frame, {})
            .context()
            .context_type(),
        Type::kCrossParty);
  }

  EXPECT_EQ(ComputeMetadataAndWait(example_site, &nonmember, {})
                .context()
                .context_type(),
            Type::kCrossParty);
  EXPECT_EQ(ComputeMetadataAndWait(nonmember, &example_site, {})
                .context()
                .context_type(),
            Type::kCrossParty);
}

TEST_F(PopulatedFirstPartySetsManagerTest, ComputeMetadata_ContextIsNonmember) {
  net::SchemefulSite nonmember(GURL("https://nonmember.test"));
  std::set<net::SchemefulSite> context({nonmember});

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    for (const net::SchemefulSite& site :
         std::initializer_list<net::SchemefulSite>{
             example_site,
             net::SchemefulSite(GURL("http://example.test")),
             net::SchemefulSite(GURL("http://member1.test")),
             net::SchemefulSite(GURL("http://foo.test")),
             net::SchemefulSite(GURL("http://member2.test")),
             nonmember,
         }) {
      EXPECT_EQ(ComputeMetadataAndWait(site, top_frame, context)
                    .context()
                    .context_type(),
                Type::kCrossParty)
          << site;
    }
  }
}

TEST_F(PopulatedFirstPartySetsManagerTest, ComputeMetadata_ContextIsOwner) {
  net::SchemefulSite example_site(GURL("https://example.test"));
  std::set<net::SchemefulSite> context({example_site});

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    for (const net::SchemefulSite& site :
         std::initializer_list<net::SchemefulSite>{
             net::SchemefulSite(GURL("http://example.test")),
             net::SchemefulSite(GURL("https://foo.test")),
             net::SchemefulSite(GURL("https://member2.test")),
             net::SchemefulSite(GURL("https://nonmember.test")),
         }) {
      EXPECT_EQ(ComputeMetadataAndWait(site, top_frame, context)
                    .context()
                    .context_type(),
                Type::kCrossParty)
          << site;
    }
    EXPECT_EQ(ComputeMetadataAndWait(example_site, top_frame, context)
                  .context()
                  .context_type(),
              Type::kSameParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://member1.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kSameParty);
  }
}

TEST_F(PopulatedFirstPartySetsManagerTest, ComputeMetadata_ContextIsMember) {
  net::SchemefulSite member1(GURL("https://member1.test"));
  std::set<net::SchemefulSite> context({member1});

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    for (const net::SchemefulSite& site :
         std::initializer_list<net::SchemefulSite>{
             net::SchemefulSite(GURL("http://example.test")),
             net::SchemefulSite(GURL("https://foo.test")),
             net::SchemefulSite(GURL("https://member2.test")),
             net::SchemefulSite(GURL("https://nonmember.test")),
         }) {
      EXPECT_EQ(ComputeMetadataAndWait(site, top_frame, context)
                    .context()
                    .context_type(),
                Type::kCrossParty)
          << site;
    }
    EXPECT_EQ(ComputeMetadataAndWait(example_site, top_frame, context)
                  .context()
                  .context_type(),
              Type::kSameParty);

    EXPECT_EQ(ComputeMetadataAndWait(example_site, top_frame, context)
                  .context()
                  .context_type(),
              Type::kSameParty);

    EXPECT_EQ(ComputeMetadataAndWait(member1, top_frame, context)
                  .context()
                  .context_type(),
              Type::kSameParty);
  }
}

TEST_F(PopulatedFirstPartySetsManagerTest,
       ComputeMetadata_ContextIsOwnerAndMember) {
  net::SchemefulSite example_site(GURL("https://example.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));
  std::set<net::SchemefulSite> context({example_site, member1});

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    for (const net::SchemefulSite& site :
         std::initializer_list<net::SchemefulSite>{
             net::SchemefulSite(GURL("http://example.test")),
             net::SchemefulSite(GURL("https://foo.test")),
             net::SchemefulSite(GURL("https://member2.test")),
             net::SchemefulSite(GURL("https://nonmember.test")),
         }) {
      EXPECT_EQ(ComputeMetadataAndWait(site, top_frame, context)
                    .context()
                    .context_type(),
                Type::kCrossParty)
          << site;
    }
    EXPECT_EQ(ComputeMetadataAndWait(example_site, top_frame, context)
                  .context()
                  .context_type(),
              Type::kSameParty);

    EXPECT_EQ(ComputeMetadataAndWait(member1, top_frame, context)
                  .context()
                  .context_type(),
              Type::kSameParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://member3.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kSameParty);
  }
}

TEST_F(PopulatedFirstPartySetsManagerTest,
       ComputeMetadata_ContextMixesParties) {
  net::SchemefulSite example_site(GURL("https://example.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));
  net::SchemefulSite foo(GURL("https://foo.test"));
  std::set<net::SchemefulSite> context({example_site, member1, foo});

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    for (const net::SchemefulSite& site :
         std::initializer_list<net::SchemefulSite>{
             example_site,
             net::SchemefulSite(GURL("http://example.test")),
             member1,
             foo,
             net::SchemefulSite(GURL("https://member2.test")),
             net::SchemefulSite(GURL("https://nonmember.test")),
         }) {
      EXPECT_EQ(ComputeMetadataAndWait(site, top_frame, context)
                    .context()
                    .context_type(),
                Type::kCrossParty)
          << site;
    }
  }
}

TEST_F(PopulatedFirstPartySetsManagerTest,
       ComputeMetadata_ContextMixesMembersAndNonmembers) {
  net::SchemefulSite example_site(GURL("https://example.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));
  std::set<net::SchemefulSite> context({
      example_site,
      member1,
      net::SchemefulSite(GURL("http://nonmember.test")),
  });

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    for (const net::SchemefulSite& site :
         std::initializer_list<net::SchemefulSite>{
             example_site,
             net::SchemefulSite(GURL("http://example.test")),
             member1,
             net::SchemefulSite(GURL("https://foo.test")),
             net::SchemefulSite(GURL("https://member2.test")),
             net::SchemefulSite(GURL("https://nonmember.test")),
         }) {
      EXPECT_EQ(ComputeMetadataAndWait(site, top_frame, context)
                    .context()
                    .context_type(),
                Type::kCrossParty)
          << site;
    }
  }
}

TEST_F(PopulatedFirstPartySetsManagerTest,
       ComputeMetadata_ContextMixesSchemes) {
  net::SchemefulSite example_site(GURL("https://example.test"));
  net::SchemefulSite example_http(GURL("http://example.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));
  std::set<net::SchemefulSite> context({example_site, member1, example_http});

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    for (const net::SchemefulSite& site :
         std::initializer_list<net::SchemefulSite>{
             example_site,
             example_http,
             net::SchemefulSite(GURL("https://member1.test")),
             net::SchemefulSite(GURL("https://foo.test")),
             net::SchemefulSite(GURL("https://member2.test")),
             net::SchemefulSite(GURL("https://nonmember.test")),
         }) {
      EXPECT_EQ(ComputeMetadataAndWait(site, top_frame, context)
                    .context()
                    .context_type(),
                Type::kCrossParty)
          << site;
    }
  }
}

TEST_F(PopulatedFirstPartySetsManagerTest, ComputeMetadata) {
  net::SchemefulSite nonmember(GURL("https://nonmember.test"));
  net::SchemefulSite nonmember1(GURL("https://nonmember1.test"));
  net::SchemefulSite member(GURL("https://member1.test"));
  net::SchemefulSite owner(GURL("https://example.test"));
  net::SchemefulSite wss_member(GURL("wss://member1.test"));
  net::SchemefulSite wss_nonmember(GURL("wss://nonmember.test"));
  net::FirstPartySetEntry primary_entry(owner, net::SiteType::kPrimary,
                                        absl::nullopt);
  net::FirstPartySetEntry associated_entry(owner, net::SiteType::kAssociated,
                                           0);

  // Works as usual for sites that are in First-Party sets.
  EXPECT_EQ(ComputeMetadataAndWait(member, &member, {member}),
            net::FirstPartySetMetadata(net::SamePartyContext(Type::kSameParty),
                                       &associated_entry, &associated_entry));
  EXPECT_EQ(ComputeMetadataAndWait(owner, &member, {member}),
            net::FirstPartySetMetadata(net::SamePartyContext(Type::kSameParty),
                                       &primary_entry, &associated_entry));
  EXPECT_EQ(ComputeMetadataAndWait(member, &owner, {member}),
            net::FirstPartySetMetadata(net::SamePartyContext(Type::kSameParty),
                                       &associated_entry, &primary_entry));
  EXPECT_EQ(ComputeMetadataAndWait(member, &member, {owner}),
            net::FirstPartySetMetadata(net::SamePartyContext(Type::kSameParty),
                                       &associated_entry, &associated_entry));
  EXPECT_EQ(ComputeMetadataAndWait(member, &member, {member, owner}),
            net::FirstPartySetMetadata(net::SamePartyContext(Type::kSameParty),
                                       &associated_entry, &associated_entry));

  // Works if the site is provided with WSS scheme instead of HTTPS.
  EXPECT_EQ(ComputeMetadataAndWait(wss_member, &member, {member, owner}),
            net::FirstPartySetMetadata(net::SamePartyContext(Type::kSameParty),
                                       &associated_entry, &associated_entry));

  EXPECT_EQ(ComputeMetadataAndWait(nonmember, &member, {member}),
            net::FirstPartySetMetadata(net::SamePartyContext(Type::kCrossParty),
                                       nullptr, &associated_entry));
  EXPECT_EQ(ComputeMetadataAndWait(member, &nonmember, {member}),
            net::FirstPartySetMetadata(net::SamePartyContext(Type::kCrossParty),
                                       &associated_entry, nullptr));
  EXPECT_EQ(ComputeMetadataAndWait(wss_nonmember, &wss_member, {member, owner}),
            net::FirstPartySetMetadata(net::SamePartyContext(Type::kCrossParty),
                                       nullptr, &associated_entry));

  EXPECT_EQ(ComputeMetadataAndWait(nonmember, &nonmember, {nonmember}),
            net::FirstPartySetMetadata(net::SamePartyContext(Type::kCrossParty),
                                       nullptr, nullptr));

  EXPECT_EQ(ComputeMetadataAndWait(member, &member, {member, nonmember}),
            net::FirstPartySetMetadata(net::SamePartyContext(Type::kCrossParty),
                                       &associated_entry, &associated_entry));
}

class OverrideSetsFirstPartySetsManagerTest : public FirstPartySetsEnabledTest {
 public:
  OverrideSetsFirstPartySetsManagerTest() {
    net::SchemefulSite foo(GURL("https://foo.test"));
    net::SchemefulSite example_test(GURL("https://example.test"));
    net::SchemefulSite example_cctld(GURL("https://example.cctld"));
    net::SchemefulSite member1(GURL("https://member1.test"));
    net::SchemefulSite member2(GURL("https://member2.test"));

    SetCompleteSets(
        {
            {member1, net::FirstPartySetEntry(example_test,
                                              net::SiteType::kAssociated, 0)},
            {member2, net::FirstPartySetEntry(example_test,
                                              net::SiteType::kAssociated, 0)},
            // Below are the owner self mappings.
            {example_test,
             net::FirstPartySetEntry(example_test, net::SiteType::kPrimary,
                                     absl::nullopt)},
        },
        {{example_cctld, example_test}});

    SetFirstPartySetsContextConfig({
        // New entry:
        {foo,
         {net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                  absl::nullopt)}},
        // Removed entry:
        {member1, absl::nullopt},
        // Remapped entry:
        {member2,
         {net::FirstPartySetEntry(foo, net::SiteType::kAssociated, 0)}},
        // Removed alias:
        {example_cctld, absl::nullopt},
    });
  }
};

TEST_F(OverrideSetsFirstPartySetsManagerTest, ComputeMetadata) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));
  net::SchemefulSite member2(GURL("https://member2.test"));

  net::FirstPartySetEntry example_primary_entry(
      example, net::SiteType::kPrimary, absl::nullopt);
  net::FirstPartySetEntry foo_primary_entry(foo, net::SiteType::kPrimary,
                                            absl::nullopt);
  net::FirstPartySetEntry foo_associated_entry(foo, net::SiteType::kAssociated,
                                               0);

  // member1 has been removed from its set.
  EXPECT_EQ(ComputeMetadataAndWait(member1, &example, {}),
            net::FirstPartySetMetadata(net::SamePartyContext(Type::kCrossParty),
                                       nullptr, &example_primary_entry));

  // member2 and foo are sites in a new set.
  EXPECT_EQ(
      ComputeMetadataAndWait(member2, &foo, {}),
      net::FirstPartySetMetadata(net::SamePartyContext(Type::kSameParty),
                                 &foo_associated_entry, &foo_primary_entry));
}

}  // namespace network

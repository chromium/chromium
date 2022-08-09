// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_sets_manager.h"

#include <set>
#include <string>

#include "base/containers/flat_set.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/first_party_set_entry.h"
#include "net/cookies/first_party_set_metadata.h"
#include "net/cookies/same_party_context.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::Value;

using Type = net::SamePartyContext::Type;
using OverrideSets =
    base::flat_map<net::SchemefulSite, absl::optional<net::FirstPartySetEntry>>;

namespace network {

MATCHER_P(SerializesTo, want, "") {
  const std::string got = arg.Serialize();
  return testing::ExplainMatchResult(testing::Eq(want), got, result_listener);
}

class FirstPartySetsManagerTest : public ::testing::Test {
 public:
  explicit FirstPartySetsManagerTest(bool enabled, bool context_enabled)
      : manager_(enabled), fps_context_config_(context_enabled) {}

  void SetCompleteSets(const base::flat_map<net::SchemefulSite,
                                            net::FirstPartySetEntry>& content) {
    manager_.SetCompleteSets(content);
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

  FirstPartySetsManager::OwnerResult FindOwnerAndWait(
      const net::SchemefulSite& site) {
    base::test::TestFuture<FirstPartySetsManager::OwnerResult> future;
    absl::optional<FirstPartySetsManager::OwnerResult> result =
        manager_.FindOwner(site, fps_context_config_, future.GetCallback());
    return result.has_value() ? result.value() : future.Get();
  }

  FirstPartySetsManager::OwnersResult FindOwnersAndWait(
      const base::flat_set<net::SchemefulSite>& site) {
    base::test::TestFuture<FirstPartySetsManager::OwnersResult> future;
    absl::optional<FirstPartySetsManager::OwnersResult> result =
        manager_.FindOwners(site, fps_context_config_, future.GetCallback());
    return result.has_value() ? result.value() : future.Get();
  }

  FirstPartySetsManager& manager() { return manager_; }

  FirstPartySetsContextConfig& fps_context_config() {
    return fps_context_config_;
  }

  base::test::TaskEnvironment& env() { return env_; }

 protected:
  void SetFirstPartySetsContextConfig(bool enabled,
                                      OverrideSets customizations) {
    fps_context_config_ = FirstPartySetsContextConfig(enabled);
    fps_context_config_.SetCustomizations(std::move(customizations));
  }

 private:
  base::test::TaskEnvironment env_;
  FirstPartySetsManager manager_;
  FirstPartySetsContextConfig fps_context_config_;
};

class FirstPartySetsManagerDisabledTest : public FirstPartySetsManagerTest {
 public:
  FirstPartySetsManagerDisabledTest()
      : FirstPartySetsManagerTest(/*enabled=*/false, /*context_enabled=*/true) {
  }
};

TEST_F(FirstPartySetsManagerDisabledTest, SetCompleteSets) {
  SetCompleteSets({{net::SchemefulSite(GURL("https://aaaa.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated)},
                   {net::SchemefulSite(GURL("https://example.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary)}});

  EXPECT_THAT(FindOwnersAndWait({
                  net::SchemefulSite(GURL("https://aaaa.test")),
                  net::SchemefulSite(GURL("https://example.test")),
              }),
              IsEmpty());
}

TEST_F(FirstPartySetsManagerDisabledTest, FindOwners) {
  net::SchemefulSite kExample =
      net::SchemefulSite(GURL("https://example.test"));

  EXPECT_THAT(FindOwnersAndWait({kExample}), IsEmpty());
}

TEST_F(FirstPartySetsManagerDisabledTest, ComputeMetadata_InfersSingletons) {
  net::SchemefulSite member(GURL("https://member1.test"));
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite wss_member(GURL("wss://member1.test"));

  SetFirstPartySetsContextConfig(
      true, {{net::SchemefulSite(GURL("https://member1.test")),
              {net::FirstPartySetEntry(
                  net::SchemefulSite(GURL("https://example.test")),
                  net::SiteType::kAssociated)}},
             {net::SchemefulSite(GURL("https://example.test")),
              {net::FirstPartySetEntry(
                  net::SchemefulSite(GURL("https://example.test")),
                  net::SiteType::kPrimary)}}});

  // Works if the site is provided with WSS scheme instead of HTTPS.
  EXPECT_THAT(
      ComputeMetadataAndWait(wss_member, &member, {member, example}).context(),
      net::SamePartyContext(Type::kCrossParty));

  EXPECT_THAT(ComputeMetadataAndWait(example, &member, {member}).context(),
              net::SamePartyContext(Type::kCrossParty));
  EXPECT_THAT(ComputeMetadataAndWait(member, &example, {member}).context(),
              net::SamePartyContext(Type::kCrossParty));

  EXPECT_THAT(
      ComputeMetadataAndWait(member, &member, {member, example}).context(),
      net::SamePartyContext(Type::kCrossParty));
}

TEST_F(FirstPartySetsManagerDisabledTest, FindOwner) {
  SetCompleteSets({{net::SchemefulSite(GURL("https://member.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated)},
                   {net::SchemefulSite(GURL("https://example.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary)}});

  SetFirstPartySetsContextConfig(
      true, {{net::SchemefulSite(GURL("https://aaaa.test")),
              {net::FirstPartySetEntry(
                  net::SchemefulSite(GURL("https://example.test")),
                  net::SiteType::kAssociated)}},
             {net::SchemefulSite(GURL("https://example.test")),
              {net::FirstPartySetEntry(
                  net::SchemefulSite(GURL("https://example.test")),
                  net::SiteType::kPrimary)}}});

  EXPECT_FALSE(
      FindOwnerAndWait(net::SchemefulSite(GURL("https://example.test"))));
  EXPECT_FALSE(
      FindOwnerAndWait(net::SchemefulSite(GURL("https://member.test"))));
}

class FirstPartySetsEnabledTest : public FirstPartySetsManagerTest {
 public:
  FirstPartySetsEnabledTest()
      : FirstPartySetsManagerTest(/*enabled=*/true, /*context_enabled=*/true) {}
};

TEST_F(FirstPartySetsEnabledTest, SetCompleteSets) {
  SetCompleteSets({{net::SchemefulSite(GURL("https://aaaa.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated)},
                   {net::SchemefulSite(GURL("https://example.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary)}});

  EXPECT_THAT(FindOwnersAndWait({
                  net::SchemefulSite(GURL("https://aaaa.test")),
                  net::SchemefulSite(GURL("https://example.test")),
              }),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kPrimary)),
                  Pair(SerializesTo("https://aaaa.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kAssociated))));
}

TEST_F(FirstPartySetsEnabledTest, SetCompleteSets_Idempotent) {
  SetCompleteSets({});
  EXPECT_THAT(FindOwnersAndWait({}), IsEmpty());

  // The second call to SetCompleteSets should have no effect.
  SetCompleteSets({{net::SchemefulSite(GURL("https://aaaa.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated)},
                   {net::SchemefulSite(GURL("https://example.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary)}});
  EXPECT_THAT(FindOwnersAndWait({
                  net::SchemefulSite(GURL("https://aaaa.test")),
                  net::SchemefulSite(GURL("https://example.test")),
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

    SetCompleteSets({
        {net::SchemefulSite(GURL("https://member1.test")),
         net::FirstPartySetEntry(
             net::SchemefulSite(GURL("https://example.test")),
             net::SiteType::kAssociated)},
        {net::SchemefulSite(GURL("https://member3.test")),
         net::FirstPartySetEntry(
             net::SchemefulSite(GURL("https://example.test")),
             net::SiteType::kAssociated)},
        {net::SchemefulSite(GURL("https://example.test")),
         net::FirstPartySetEntry(
             net::SchemefulSite(GURL("https://example.test")),
             net::SiteType::kPrimary)},
        {net::SchemefulSite(GURL("https://member2.test")),
         net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                 net::SiteType::kAssociated)},
        {net::SchemefulSite(GURL("https://foo.test")),
         net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                 net::SiteType::kPrimary)},
    });

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
    net::FirstPartySetEntry entry(owner, net::SiteType::kAssociated);

    EXPECT_EQ(future.Get(),
              net::FirstPartySetMetadata(
                  net::SamePartyContext(Type::kSameParty), &entry, &entry));
  }
}

TEST_F(AsyncPopulatedFirstPartySetsManagerTest, QueryBeforeReady_FindOwner) {
  base::test::TestFuture<FirstPartySetsManager::OwnerResult> future;
  EXPECT_FALSE(
      manager().FindOwner(net::SchemefulSite(GURL("https://member1.test")),
                          fps_context_config(), future.GetCallback()));

  Populate();

  EXPECT_THAT(future.Get(),
              absl::make_optional(net::FirstPartySetEntry(
                  net::SchemefulSite(GURL("https://example.test")),
                  net::SiteType::kAssociated)));
}

TEST_F(AsyncPopulatedFirstPartySetsManagerTest, QueryBeforeReady_FindOwners) {
  base::test::TestFuture<FirstPartySetsManager::OwnersResult> future;
  EXPECT_FALSE(manager().FindOwners(
      {
          net::SchemefulSite(GURL("https://member1.test")),
          net::SchemefulSite(GURL("https://member2.test")),
      },
      fps_context_config(), future.GetCallback()));

  Populate();

  EXPECT_THAT(future.Get(),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member1.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kAssociated)),
                  Pair(SerializesTo("https://member2.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://foo.test")),
                           net::SiteType::kAssociated))));
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
    EXPECT_EQ(
        ComputeMetadataAndWait(
            net::SchemefulSite(GURL("https://nonmember.test")), top_frame, {})
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
  std::set<net::SchemefulSite> context({
      net::SchemefulSite(GURL("https://nonmember.test")),
  });

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://example.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("http://example.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://member1.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://foo.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://member2.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(ComputeMetadataAndWait(
                  net::SchemefulSite(GURL("https://nonmember.test")), top_frame,
                  context)
                  .context()
                  .context_type(),
              Type::kCrossParty);
  }
}

TEST_F(PopulatedFirstPartySetsManagerTest, ComputeMetadata_ContextIsOwner) {
  std::set<net::SchemefulSite> context(
      {net::SchemefulSite(GURL("https://example.test"))});

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://example.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kSameParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("http://example.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://member1.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kSameParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://foo.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://member2.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(ComputeMetadataAndWait(
                  net::SchemefulSite(GURL("https://nonmember.test")), top_frame,
                  context)
                  .context()
                  .context_type(),
              Type::kCrossParty);
  }
}

TEST_F(PopulatedFirstPartySetsManagerTest, ComputeMetadata_ContextIsMember) {
  std::set<net::SchemefulSite> context(
      {net::SchemefulSite(GURL("https://member1.test"))});

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://example.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kSameParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("http://example.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://example.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kSameParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://member1.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kSameParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://foo.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://member2.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(ComputeMetadataAndWait(
                  net::SchemefulSite(GURL("https://nonmember.test")), top_frame,
                  context)
                  .context()
                  .context_type(),
              Type::kCrossParty);
  }
}

TEST_F(PopulatedFirstPartySetsManagerTest,
       ComputeMetadata_ContextIsOwnerAndMember) {
  std::set<net::SchemefulSite> context({
      net::SchemefulSite(GURL("https://example.test")),
      net::SchemefulSite(GURL("https://member1.test")),
  });

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://example.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kSameParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("http://example.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://member1.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kSameParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://member3.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kSameParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://foo.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://member2.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(ComputeMetadataAndWait(
                  net::SchemefulSite(GURL("https://nonmember.test")), top_frame,
                  context)
                  .context()
                  .context_type(),
              Type::kCrossParty);
  }
}

TEST_F(PopulatedFirstPartySetsManagerTest,
       ComputeMetadata_ContextMixesParties) {
  std::set<net::SchemefulSite> context({
      net::SchemefulSite(GURL("https://example.test")),
      net::SchemefulSite(GURL("https://member1.test")),
      net::SchemefulSite(GURL("https://foo.test")),
  });

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://example.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("http://example.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://member1.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://foo.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://member2.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(ComputeMetadataAndWait(
                  net::SchemefulSite(GURL("https://nonmember.test")), top_frame,
                  context)
                  .context()
                  .context_type(),
              Type::kCrossParty);
  }
}

TEST_F(PopulatedFirstPartySetsManagerTest,
       ComputeMetadata_ContextMixesMembersAndNonmembers) {
  std::set<net::SchemefulSite> context({
      net::SchemefulSite(GURL("https://example.test")),
      net::SchemefulSite(GURL("https://member1.test")),
      net::SchemefulSite(GURL("http://nonmember.test")),
  });

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://example.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("http://example.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://member1.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://foo.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://member2.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(ComputeMetadataAndWait(
                  net::SchemefulSite(GURL("https://nonmember.test")), top_frame,
                  context)
                  .context()
                  .context_type(),
              Type::kCrossParty);
  }
}

TEST_F(PopulatedFirstPartySetsManagerTest,
       ComputeMetadata_ContextMixesSchemes) {
  std::set<net::SchemefulSite> context({
      net::SchemefulSite(GURL("https://example.test")),
      net::SchemefulSite(GURL("https://member1.test")),
      net::SchemefulSite(GURL("http://example.test")),
  });

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://example.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("http://example.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://member1.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://foo.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        ComputeMetadataAndWait(net::SchemefulSite(GURL("https://member2.test")),
                               top_frame, context)
            .context()
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(ComputeMetadataAndWait(
                  net::SchemefulSite(GURL("https://nonmember.test")), top_frame,
                  context)
                  .context()
                  .context_type(),
              Type::kCrossParty);
  }
}

TEST_F(PopulatedFirstPartySetsManagerTest, ComputeMetadata) {
  net::SchemefulSite nonmember(GURL("https://nonmember.test"));
  net::SchemefulSite nonmember1(GURL("https://nonmember1.test"));
  net::SchemefulSite member(GURL("https://member1.test"));
  net::SchemefulSite owner(GURL("https://example.test"));
  net::SchemefulSite wss_member(GURL("wss://member1.test"));
  net::SchemefulSite wss_nonmember(GURL("wss://nonmember.test"));
  net::FirstPartySetEntry primary_entry(owner, net::SiteType::kPrimary);
  net::FirstPartySetEntry associated_entry(owner, net::SiteType::kAssociated);

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

TEST_F(PopulatedFirstPartySetsManagerTest, FindOwner) {
  const net::SchemefulSite kSetOwner1(GURL("https://example.test"));
  const net::SchemefulSite kSetOwner2(GURL("https://foo.test"));

  struct TestCase {
    const std::string url;
    const absl::optional<net::FirstPartySetEntry> expected;
  } test_cases[] = {
      {"https://example.test",
       net::FirstPartySetEntry(kSetOwner1, net::SiteType::kPrimary)},
      // Insecure URL
      {"http://example.test", absl::nullopt},
      // Test member
      {"https://member1.test",
       net::FirstPartySetEntry(kSetOwner1, net::SiteType::kAssociated)},
      {"http://member1.test", absl::nullopt},
      // Test another disjoint set
      {"https://foo.test",
       net::FirstPartySetEntry(kSetOwner2, net::SiteType::kPrimary)},
      {"https://member2.test",
       net::FirstPartySetEntry(kSetOwner2, net::SiteType::kAssociated)},
      // Test a site not in a set
      {"https://nonmember.test", absl::nullopt},
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(test_case.expected,
              FindOwnerAndWait(net::SchemefulSite(GURL(test_case.url))));
  }
}

TEST_F(PopulatedFirstPartySetsManagerTest, FindOwners) {
  net::SchemefulSite kExample(GURL("https://example.test"));
  net::SchemefulSite kFoo(GURL("https://foo.test"));
  net::SchemefulSite kMember1(GURL("https://member1.test"));
  net::SchemefulSite kMember2(GURL("https://member2.test"));
  net::SchemefulSite kNonmember(GURL("https://nonmember.test"));

  EXPECT_THAT(FindOwnersAndWait({kExample}),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kPrimary))));
  EXPECT_THAT(FindOwnersAndWait({kMember1}),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member1.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kAssociated))));
  EXPECT_THAT(FindOwnersAndWait({kNonmember}), IsEmpty());

  EXPECT_THAT(FindOwnersAndWait({kExample, kNonmember}),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kPrimary))));
  EXPECT_THAT(FindOwnersAndWait({kMember1, kNonmember}),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member1.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kAssociated))));

  EXPECT_THAT(FindOwnersAndWait({kExample, kFoo}),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kPrimary)),
                  Pair(SerializesTo("https://foo.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://foo.test")),
                           net::SiteType::kPrimary))));
  EXPECT_THAT(FindOwnersAndWait({kMember1, kFoo}),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member1.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kAssociated)),
                  Pair(SerializesTo("https://foo.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://foo.test")),
                           net::SiteType::kPrimary))));
  EXPECT_THAT(FindOwnersAndWait({kExample, kMember2}),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kPrimary)),
                  Pair(SerializesTo("https://member2.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://foo.test")),
                           net::SiteType::kAssociated))));
  EXPECT_THAT(FindOwnersAndWait({kMember1, kMember2}),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member1.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kAssociated)),
                  Pair(SerializesTo("https://member2.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://foo.test")),
                           net::SiteType::kAssociated))));
}

class OverrideSetsFirstPartySetsManagerTest : public FirstPartySetsEnabledTest {
 public:
  OverrideSetsFirstPartySetsManagerTest() {
    SetCompleteSets({
        {net::SchemefulSite(GURL("https://member1.test")),
         net::FirstPartySetEntry(
             net::SchemefulSite(GURL("https://example.test")),
             net::SiteType::kAssociated)},
        {net::SchemefulSite(GURL("https://member2.test")),
         net::FirstPartySetEntry(
             net::SchemefulSite(GURL("https://example.test")),
             net::SiteType::kAssociated)},
        // Below are the owner self mappings.
        {net::SchemefulSite(GURL("https://example.test")),
         net::FirstPartySetEntry(
             net::SchemefulSite(GURL("https://example.test")),
             net::SiteType::kPrimary)},
    });

    SetFirstPartySetsContextConfig(
        true,
        {
            // New entry:
            {net::SchemefulSite(GURL("https://foo.test")),
             {net::FirstPartySetEntry(
                 net::SchemefulSite(GURL("https://foo.test")),
                 net::SiteType::kPrimary)}},
            // Removed entry:
            {net::SchemefulSite(GURL("https://member1.test")), absl::nullopt},
            // Remapped entry:
            {net::SchemefulSite(GURL("https://member2.test")),
             {net::FirstPartySetEntry(
                 net::SchemefulSite(GURL("https://foo.test")),
                 net::SiteType::kAssociated)}},
        });
  }
};

TEST_F(OverrideSetsFirstPartySetsManagerTest, FindOwner) {
  EXPECT_EQ(
      FindOwnerAndWait(net::SchemefulSite(GURL("https://foo.test"))),
      net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                              net::SiteType::kPrimary));
  EXPECT_EQ(FindOwnerAndWait(net::SchemefulSite(GURL("https://member1.test"))),
            absl::nullopt);
  EXPECT_EQ(
      FindOwnerAndWait(net::SchemefulSite(GURL("https://member2.test"))),
      net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                              net::SiteType::kAssociated));
}

TEST_F(OverrideSetsFirstPartySetsManagerTest, FindOwners) {
  EXPECT_THAT(FindOwnersAndWait({
                  net::SchemefulSite(GURL("https://member1.test")),
                  net::SchemefulSite(GURL("https://member2.test")),
                  net::SchemefulSite(GURL("https://foo.test")),
              }),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://foo.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://foo.test")),
                           net::SiteType::kPrimary)),
                  Pair(SerializesTo("https://member2.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://foo.test")),
                           net::SiteType::kAssociated))));
}

TEST_F(OverrideSetsFirstPartySetsManagerTest, ComputeMetadata) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));
  net::SchemefulSite member2(GURL("https://member2.test"));

  net::FirstPartySetEntry example_primary_entry(example,
                                                net::SiteType::kPrimary);
  net::FirstPartySetEntry foo_primary_entry(foo, net::SiteType::kPrimary);
  net::FirstPartySetEntry foo_associated_entry(foo, net::SiteType::kAssociated);
  {
    // member1 has been removed from its set.
    net::FirstPartySetMetadata expected(
        net::SamePartyContext(Type::kCrossParty), nullptr,
        &example_primary_entry);

    EXPECT_THAT(ComputeMetadataAndWait(member1, &example, {}),
                testing::Eq(std::ref(expected)));
  }
  {
    // member2 and foo are sites in a new set.
    net::FirstPartySetMetadata expected(net::SamePartyContext(Type::kSameParty),
                                        &foo_associated_entry,
                                        &foo_primary_entry);

    EXPECT_THAT(ComputeMetadataAndWait(member2, &foo, {}),
                testing::Eq(std::ref(expected)));
  }
}

}  // namespace network
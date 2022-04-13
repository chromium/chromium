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
#include "net/base/test_completion_callback.h"
#include "net/cookies/cookie_constants.h"
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

namespace network {

MATCHER_P(SerializesTo, want, "") {
  const std::string got = arg.Serialize();
  return testing::ExplainMatchResult(testing::Eq(want), got, result_listener);
}

class FirstPartySetsManagerTest : public ::testing::Test {
 public:
  explicit FirstPartySetsManagerTest(bool enabled) : manager_(enabled) {}

  void SetCompleteSets(
      const base::flat_map<net::SchemefulSite, net::SchemefulSite>& content) {
    manager_.SetCompleteSets(content);
  }

  FirstPartySetsManager::SetsByOwner SetsAndWait() {
    base::test::TestFuture<FirstPartySetsManager::SetsByOwner> future;
    absl::optional<FirstPartySetsManager::SetsByOwner> result =
        manager_.Sets(future.GetCallback());
    return result.has_value() ? result.value() : future.Get();
  }

  net::FirstPartySetMetadata ComputeMetadataAndWait(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      const std::set<net::SchemefulSite>& party_context) {
    base::test::TestFuture<net::FirstPartySetMetadata> future;
    absl::optional<net::FirstPartySetMetadata> result =
        manager_.ComputeMetadata(site, top_frame_site, party_context,
                                 future.GetCallback());
    return result.has_value() ? std::move(result).value() : future.Take();
  }

  FirstPartySetsManager::OwnerResult FindOwnerAndWait(
      const net::SchemefulSite& site) {
    base::test::TestFuture<FirstPartySetsManager::OwnerResult> future;
    absl::optional<FirstPartySetsManager::OwnerResult> result =
        manager_.FindOwner(site, future.GetCallback());
    return result.has_value() ? result.value() : future.Get();
  }

  FirstPartySetsManager::OwnersResult FindOwnersAndWait(
      const base::flat_set<net::SchemefulSite>& site) {
    base::test::TestFuture<FirstPartySetsManager::OwnersResult> future;
    absl::optional<FirstPartySetsManager::OwnersResult> result =
        manager_.FindOwners(site, future.GetCallback());
    return result.has_value() ? result.value() : future.Get();
  }

  FirstPartySetsManager& manager() { return manager_; }

  base::test::TaskEnvironment& env() { return env_; }

 private:
  base::test::TaskEnvironment env_;
  FirstPartySetsManager manager_;
};

class FirstPartySetsManagerDisabledTest : public FirstPartySetsManagerTest {
 public:
  FirstPartySetsManagerDisabledTest() : FirstPartySetsManagerTest(false) {}
};

TEST_F(FirstPartySetsManagerDisabledTest, SetCompleteSets) {
  SetCompleteSets({{net::SchemefulSite(GURL("https://aaaa.test")),
                    net::SchemefulSite(GURL("https://example.test"))},
                   {net::SchemefulSite(GURL("https://example.test")),
                    net::SchemefulSite(GURL("https://example.test"))}});
  EXPECT_THAT(SetsAndWait(), IsEmpty());
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

  // Works if the site is provided with WSS scheme instead of HTTPS.
  EXPECT_THAT(
      ComputeMetadataAndWait(wss_member, &member, {member, example}).context(),
      net::SamePartyContext(Type::kCrossParty, Type::kCrossParty,
                            Type::kSameParty));

  EXPECT_THAT(ComputeMetadataAndWait(example, &member, {member}).context(),
              net::SamePartyContext(Type::kCrossParty));
  EXPECT_THAT(ComputeMetadataAndWait(member, &example, {member}).context(),
              net::SamePartyContext(Type::kCrossParty));

  // Top&resource differs from Ancestors.
  EXPECT_THAT(ComputeMetadataAndWait(member, &member, {example}).context(),
              net::SamePartyContext(Type::kCrossParty, Type::kCrossParty,
                                    Type::kSameParty));

  // Metrics values infer singleton sets when appropriate.
  EXPECT_THAT(ComputeMetadataAndWait(member, &member, {member}).context(),
              net::SamePartyContext(Type::kCrossParty, Type::kSameParty,
                                    Type::kSameParty));
  EXPECT_THAT(ComputeMetadataAndWait(member, &example, {member}).context(),
              net::SamePartyContext(Type::kCrossParty));
  EXPECT_THAT(ComputeMetadataAndWait(example, &member, {member}).context(),
              net::SamePartyContext(Type::kCrossParty));
  EXPECT_THAT(ComputeMetadataAndWait(member, &member, {example}).context(),
              net::SamePartyContext(Type::kCrossParty, Type::kCrossParty,
                                    Type::kSameParty));

  EXPECT_THAT(
      ComputeMetadataAndWait(member, &member, {member, example}).context(),
      net::SamePartyContext(Type::kCrossParty, Type::kCrossParty,
                            Type::kSameParty));
}

TEST_F(FirstPartySetsManagerDisabledTest, FindOwner) {
  SetCompleteSets({{net::SchemefulSite(GURL("https://member.test")),
                    net::SchemefulSite(GURL("https://example.test"))},
                   {net::SchemefulSite(GURL("https://example.test")),
                    net::SchemefulSite(GURL("https://example.test"))}});
  EXPECT_FALSE(
      FindOwnerAndWait(net::SchemefulSite(GURL("https://example.test"))));
  EXPECT_FALSE(
      FindOwnerAndWait(net::SchemefulSite(GURL("https://member.test"))));
}

TEST_F(FirstPartySetsManagerDisabledTest, Sets_IsEmpty) {
  EXPECT_THAT(SetsAndWait(), IsEmpty());
}

class FirstPartySetsEnabledTest : public FirstPartySetsManagerTest {
 public:
  FirstPartySetsEnabledTest() : FirstPartySetsManagerTest(true) {}
};

TEST_F(FirstPartySetsEnabledTest, Sets_IsEmpty) {
  SetCompleteSets({});
  EXPECT_THAT(SetsAndWait(), IsEmpty());
}

TEST_F(FirstPartySetsEnabledTest, SetCompleteSets) {
  SetCompleteSets(base::flat_map<net::SchemefulSite, net::SchemefulSite>(
      {{net::SchemefulSite(GURL("https://aaaa.test")),
        net::SchemefulSite(GURL("https://example.test"))},
       {net::SchemefulSite(GURL("https://example.test")),
        net::SchemefulSite(GURL("https://example.test"))}}));

  EXPECT_THAT(SetsAndWait(),
              UnorderedElementsAre(Pair(
                  SerializesTo("https://example.test"),
                  UnorderedElementsAre(SerializesTo("https://example.test"),
                                       SerializesTo("https://aaaa.test")))));
}

TEST_F(FirstPartySetsEnabledTest, SetCompleteSets_Idempotent) {
  SetCompleteSets({});
  EXPECT_THAT(SetsAndWait(), IsEmpty());

  // The second call to SetCompleteSets should have no effect.
  SetCompleteSets({{net::SchemefulSite(GURL("https://aaaa.test")),
                    net::SchemefulSite(GURL("https://example.test"))},
                   {net::SchemefulSite(GURL("https://example.test")),
                    net::SchemefulSite(GURL("https://example.test"))}});
  EXPECT_THAT(SetsAndWait(), IsEmpty());
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
         net::SchemefulSite(GURL("https://example.test"))},
        {net::SchemefulSite(GURL("https://member3.test")),
         net::SchemefulSite(GURL("https://example.test"))},
        {net::SchemefulSite(GURL("https://example.test")),
         net::SchemefulSite(GURL("https://example.test"))},
        {net::SchemefulSite(GURL("https://member2.test")),
         net::SchemefulSite(GURL("https://foo.test"))},
        {net::SchemefulSite(GURL("https://foo.test")),
         net::SchemefulSite(GURL("https://foo.test"))},
    });

    // We don't wait for the sets to be loaded before returning, in order to let
    // the tests provoke raciness if any exists.
  }
};

TEST_F(AsyncPopulatedFirstPartySetsManagerTest,
       QueryBeforeReady_ComputeMetadata) {
  net::SchemefulSite member(GURL("https://member1.test"));
  net::SchemefulSite owner(GURL("https://example.test"));

  base::test::TestFuture<net::FirstPartySetMetadata> future;
  EXPECT_FALSE(manager().ComputeMetadata(member, &member, {member},
                                         future.GetCallback()));

  Populate();

  EXPECT_EQ(future.Get(),
            net::FirstPartySetMetadata(
                net::SamePartyContext(Type::kSameParty), &owner, &owner,
                net::FirstPartySetsContextType::kHomogeneous));
}

TEST_F(AsyncPopulatedFirstPartySetsManagerTest, QueryBeforeReady_FindOwner) {
  base::test::TestFuture<FirstPartySetsManager::OwnerResult> future;
  EXPECT_FALSE(manager().FindOwner(
      net::SchemefulSite(GURL("https://member1.test")), future.GetCallback()));

  Populate();

  EXPECT_THAT(
      future.Get(),
      absl::make_optional(net::SchemefulSite(GURL("https://example.test"))));
}

TEST_F(AsyncPopulatedFirstPartySetsManagerTest, QueryBeforeReady_FindOwners) {
  base::test::TestFuture<FirstPartySetsManager::OwnersResult> future;
  EXPECT_FALSE(manager().FindOwners(
      {
          net::SchemefulSite(GURL("https://member1.test")),
          net::SchemefulSite(GURL("https://member2.test")),
      },
      future.GetCallback()));

  Populate();

  EXPECT_THAT(future.Get(),
              UnorderedElementsAre(Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://foo.test"))));
}

TEST_F(AsyncPopulatedFirstPartySetsManagerTest, QueryBeforeReady_Sets) {
  base::test::TestFuture<FirstPartySetsManager::SetsByOwner> future;
  EXPECT_FALSE(manager().Sets(future.GetCallback()));

  Populate();

  EXPECT_THAT(
      future.Get(),
      UnorderedElementsAre(
          Pair(SerializesTo("https://example.test"),
               UnorderedElementsAre(SerializesTo("https://example.test"),
                                    SerializesTo("https://member1.test"),
                                    SerializesTo("https://member3.test"))),
          Pair(SerializesTo("https://foo.test"),
               UnorderedElementsAre(SerializesTo("https://foo.test"),
                                    SerializesTo("https://member2.test")))));
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

  // Works as usual for sites that are in First-Party sets.
  EXPECT_EQ(ComputeMetadataAndWait(member, &member, {member}),
            net::FirstPartySetMetadata(
                net::SamePartyContext(Type::kSameParty), &owner, &owner,
                net::FirstPartySetsContextType::kHomogeneous));
  EXPECT_EQ(ComputeMetadataAndWait(owner, &member, {member}),
            net::FirstPartySetMetadata(
                net::SamePartyContext(Type::kSameParty), &owner, &owner,
                net::FirstPartySetsContextType::kHomogeneous));
  EXPECT_EQ(ComputeMetadataAndWait(member, &owner, {member}),
            net::FirstPartySetMetadata(
                net::SamePartyContext(Type::kSameParty), &owner, &owner,
                net::FirstPartySetsContextType::kHomogeneous));
  EXPECT_EQ(ComputeMetadataAndWait(member, &member, {owner}),
            net::FirstPartySetMetadata(
                net::SamePartyContext(Type::kSameParty), &owner, &owner,
                net::FirstPartySetsContextType::kHomogeneous));
  EXPECT_EQ(ComputeMetadataAndWait(member, &member, {member, owner}),
            net::FirstPartySetMetadata(
                net::SamePartyContext(Type::kSameParty), &owner, &owner,
                net::FirstPartySetsContextType::kHomogeneous));

  // Works if the site is provided with WSS scheme instead of HTTPS.
  EXPECT_EQ(ComputeMetadataAndWait(wss_member, &member, {member, owner}),
            net::FirstPartySetMetadata(
                net::SamePartyContext(Type::kSameParty), &owner, &owner,
                net::FirstPartySetsContextType::kHomogeneous));

  EXPECT_EQ(ComputeMetadataAndWait(nonmember, &member, {member}),
            net::FirstPartySetMetadata(
                net::SamePartyContext(Type::kCrossParty), nullptr, &owner,
                net::FirstPartySetsContextType::kTopResourceMismatch));
  EXPECT_EQ(ComputeMetadataAndWait(member, &nonmember, {member}),
            net::FirstPartySetMetadata(
                net::SamePartyContext(Type::kCrossParty), &owner, nullptr,
                net::FirstPartySetsContextType::kTopResourceMismatch));
  EXPECT_EQ(ComputeMetadataAndWait(wss_nonmember, &wss_member, {member, owner}),
            net::FirstPartySetMetadata(
                net::SamePartyContext(Type::kCrossParty), nullptr, &owner,
                net::FirstPartySetsContextType::kTopResourceMismatch));

  // Top&resource differs from Ancestors.
  EXPECT_EQ(ComputeMetadataAndWait(member, &member, {nonmember}),
            net::FirstPartySetMetadata(
                net::SamePartyContext(Type::kCrossParty, Type::kCrossParty,
                                      Type::kSameParty),
                &owner, &owner,
                net::FirstPartySetsContextType::kTopResourceMatchMixed));

  // Metrics values infer singleton sets when appropriate.
  EXPECT_EQ(
      ComputeMetadataAndWait(nonmember, &nonmember, {nonmember}),
      net::FirstPartySetMetadata(
          net::SamePartyContext(Type::kCrossParty, Type::kSameParty,
                                Type::kSameParty),
          nullptr, nullptr, net::FirstPartySetsContextType::kHomogeneous));
  EXPECT_EQ(ComputeMetadataAndWait(nonmember, &nonmember1, {nonmember}),
            net::FirstPartySetMetadata(
                net::SamePartyContext(Type::kCrossParty), nullptr, nullptr,
                net::FirstPartySetsContextType::kTopResourceMismatch));
  EXPECT_EQ(ComputeMetadataAndWait(nonmember1, &nonmember, {nonmember}),
            net::FirstPartySetMetadata(
                net::SamePartyContext(Type::kCrossParty), nullptr, nullptr,
                net::FirstPartySetsContextType::kTopResourceMismatch));
  EXPECT_EQ(ComputeMetadataAndWait(nonmember, &nonmember, {nonmember1}),
            net::FirstPartySetMetadata(
                net::SamePartyContext(Type::kCrossParty, Type::kCrossParty,
                                      Type::kSameParty),
                nullptr, nullptr,
                net::FirstPartySetsContextType::kTopResourceMatchMixed));

  EXPECT_EQ(ComputeMetadataAndWait(member, &member, {member, nonmember}),
            net::FirstPartySetMetadata(
                net::SamePartyContext(Type::kCrossParty, Type::kCrossParty,
                                      Type::kSameParty),
                &owner, &owner,
                net::FirstPartySetsContextType::kTopResourceMatchMixed));
  EXPECT_EQ(ComputeMetadataAndWait(nonmember, &nonmember, {member, nonmember}),
            net::FirstPartySetMetadata(
                net::SamePartyContext(Type::kCrossParty, Type::kCrossParty,
                                      Type::kSameParty),
                nullptr, nullptr,
                net::FirstPartySetsContextType::kTopResourceMatchMixed));
}

TEST_F(PopulatedFirstPartySetsManagerTest, FindOwner) {
  const absl::optional<net::SchemefulSite> kSetOwner1 =
      absl::make_optional(net::SchemefulSite(GURL("https://example.test")));
  const absl::optional<net::SchemefulSite> kSetOwner2 =
      absl::make_optional(net::SchemefulSite(GURL("https://foo.test")));

  struct TestCase {
    const std::string url;
    const absl::optional<net::SchemefulSite> expected;
  } test_cases[] = {
      {"https://example.test", kSetOwner1},
      // Insecure URL
      {"http://example.test", absl::nullopt},
      // Test member
      {"https://member1.test", kSetOwner1},
      {"http://member1.test", absl::nullopt},
      // Test another disjoint set
      {"https://foo.test", kSetOwner2},
      {"https://member2.test", kSetOwner2},
      // Test a site not in a set
      {"https://nonmember.test", absl::nullopt},
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(test_case.expected,
              FindOwnerAndWait(net::SchemefulSite(GURL(test_case.url))));
  }
}

TEST_F(PopulatedFirstPartySetsManagerTest, FindOwners) {
  net::SchemefulSite kExample =
      net::SchemefulSite(GURL("https://example.test"));
  net::SchemefulSite kFoo = net::SchemefulSite(GURL("https://foo.test"));
  net::SchemefulSite kMember1 =
      net::SchemefulSite(GURL("https://member1.test"));
  net::SchemefulSite kMember2 =
      net::SchemefulSite(GURL("https://member2.test"));
  net::SchemefulSite kNonmember =
      net::SchemefulSite(GURL("https://nonmember.test"));

  EXPECT_THAT(FindOwnersAndWait({kExample}),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test"))));
  EXPECT_THAT(FindOwnersAndWait({kMember1}),
              UnorderedElementsAre(Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test"))));
  EXPECT_THAT(FindOwnersAndWait({kNonmember}), IsEmpty());

  EXPECT_THAT(FindOwnersAndWait({kExample, kNonmember}),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test"))));
  EXPECT_THAT(FindOwnersAndWait({kMember1, kNonmember}),
              UnorderedElementsAre(Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test"))));

  EXPECT_THAT(FindOwnersAndWait({kExample, kFoo}),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://foo.test"),
                                        SerializesTo("https://foo.test"))));
  EXPECT_THAT(FindOwnersAndWait({kMember1, kFoo}),
              UnorderedElementsAre(Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://foo.test"),
                                        SerializesTo("https://foo.test"))));
  EXPECT_THAT(FindOwnersAndWait({kExample, kMember2}),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://foo.test"))));
  EXPECT_THAT(FindOwnersAndWait({kMember1, kMember2}),
              UnorderedElementsAre(Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://foo.test"))));
}

TEST_F(PopulatedFirstPartySetsManagerTest, Sets_NonEmpty) {
  EXPECT_THAT(
      SetsAndWait(),
      UnorderedElementsAre(
          Pair(SerializesTo("https://example.test"),
               UnorderedElementsAre(SerializesTo("https://example.test"),
                                    SerializesTo("https://member1.test"),
                                    SerializesTo("https://member3.test"))),
          Pair(SerializesTo("https://foo.test"),
               UnorderedElementsAre(SerializesTo("https://foo.test"),
                                    SerializesTo("https://member2.test")))));
}

TEST_F(PopulatedFirstPartySetsManagerTest, ComputeContextType) {
  // ComputeContextType assumes that the instance is fully initialized, so we
  // wait for that before proceeding.
  SetsAndWait();

  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));
  net::SchemefulSite foo(GURL("https://foo.test"));

  std::set<net::SchemefulSite> homogeneous_context({
      example,
      member1,
  });
  std::set<net::SchemefulSite> mixed_context({
      example,
      net::SchemefulSite(GURL("https://nonmember.test")),
  });
  net::SchemefulSite singleton(GURL("https://implicit-singleton.test"));

  EXPECT_EQ(net::FirstPartySetsContextType::kTopFrameIgnoredHomogeneous,
            manager().ComputeContextType(example, nullptr, {}));
  EXPECT_EQ(
      net::FirstPartySetsContextType::kTopFrameIgnoredHomogeneous,
      manager().ComputeContextType(example, nullptr, homogeneous_context));

  EXPECT_EQ(net::FirstPartySetsContextType::kTopFrameIgnoredMixed,
            manager().ComputeContextType(example, nullptr, mixed_context));

  EXPECT_EQ(net::FirstPartySetsContextType::kHomogeneous,
            manager().ComputeContextType(example, &member1, {}));
  EXPECT_EQ(
      net::FirstPartySetsContextType::kHomogeneous,
      manager().ComputeContextType(example, &member1, homogeneous_context));
  EXPECT_EQ(net::FirstPartySetsContextType::kHomogeneous,
            manager().ComputeContextType(singleton, &singleton, {singleton}));

  EXPECT_EQ(net::FirstPartySetsContextType::kTopResourceMatchMixed,
            manager().ComputeContextType(example, &member1, {foo}));
  EXPECT_EQ(net::FirstPartySetsContextType::kTopResourceMatchMixed,
            manager().ComputeContextType(example, &member1, mixed_context));
  EXPECT_EQ(net::FirstPartySetsContextType::kTopResourceMatchMixed,
            manager().ComputeContextType(example, &member1, {singleton}));
  EXPECT_EQ(net::FirstPartySetsContextType::kTopResourceMatchMixed,
            manager().ComputeContextType(singleton, &singleton, mixed_context));

  EXPECT_EQ(net::FirstPartySetsContextType::kTopResourceMismatch,
            manager().ComputeContextType(example, &foo, {}));
  EXPECT_EQ(net::FirstPartySetsContextType::kTopResourceMismatch,
            manager().ComputeContextType(example, &foo, homogeneous_context));
  EXPECT_EQ(net::FirstPartySetsContextType::kTopResourceMismatch,
            manager().ComputeContextType(example, &foo, mixed_context));
  EXPECT_EQ(net::FirstPartySetsContextType::kTopResourceMismatch,
            manager().ComputeContextType(example, &singleton, mixed_context));
}

}  // namespace network

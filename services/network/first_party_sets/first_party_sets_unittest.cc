// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_sets.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/same_party_context.h"
#include "services/network/first_party_sets/first_party_set_parser.h"
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

// Some of these tests overlap with FirstPartySetParser unittests, but
// overlapping test coverage isn't the worst thing.

namespace network {

MATCHER_P(SerializesTo, want, "") {
  const std::string got = arg.Serialize();
  return testing::ExplainMatchResult(testing::Eq(want), got, result_listener);
}

class FirstPartySetsTest : public ::testing::Test {
 public:
  explicit FirstPartySetsTest(bool enabled) {
    if (enabled) {
      feature_list_.InitAndEnableFeature(net::features::kFirstPartySets);
    } else {
      feature_list_.InitAndDisableFeature(net::features::kFirstPartySets);
    }
  }

  void SetComponentSetsAndWait(base::StringPiece content) {
    SetComponentSetsAndWait(sets_, content);
  }

  void SetComponentSetsAndWait(FirstPartySets& sets,
                               base::StringPiece content) {
    base::ScopedTempDir temp_dir;
    CHECK(temp_dir.CreateUniqueTempDir());
    base::FilePath path =
        temp_dir.GetPath().Append(FILE_PATH_LITERAL("sets_file.json"));
    CHECK(base::WriteFile(path, content));

    sets.ParseAndSet(
        base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ));
    env_.RunUntilIdle();
  }

  FirstPartySets& sets() { return sets_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment env_;
  FirstPartySets sets_;
};

class FirstPartySetsDisabledTest : public FirstPartySetsTest {
 public:
  FirstPartySetsDisabledTest() : FirstPartySetsTest(false) {}
};

TEST_F(FirstPartySetsDisabledTest, ParseAndSet_IgnoresValid) {
  const std::string input =
      R"([{
        "owner": "https://example.test",
        "members": ["https://aaaa.test"]
        }])";
  ASSERT_TRUE(base::JSONReader::Read(input));

  SetComponentSetsAndWait(input);
  EXPECT_THAT(sets().Sets(), IsEmpty());
}

TEST_F(FirstPartySetsDisabledTest, ParseV2Format_IgnoresValid) {
  const std::string input =
      "{\"owner\": \"https://example.test\",\"members\": "
      "[\"https://aaaa.test\"]}";

  SetComponentSetsAndWait(input);
  EXPECT_THAT(sets().Sets(), IsEmpty());
}

TEST_F(FirstPartySetsDisabledTest, SetsManuallySpecified_IgnoresValid) {
  FirstPartySets sets;
  sets.SetManuallySpecifiedSet("https://example.test,https://member.test");
  EXPECT_THAT(sets.Sets(), IsEmpty());
}

TEST_F(FirstPartySetsDisabledTest, IsInNontrivialFirstPartySet) {
  const std::string input =
      R"([{
        "owner": "https://example.test",
        "members": ["https://aaaa.test"]
        }])";
  ASSERT_TRUE(base::JSONReader::Read(input));

  SetComponentSetsAndWait(input);

  // IsInNontrivialFirstPartySet queries should return false, regardless of what
  // data has been passed to the instance.
  EXPECT_FALSE(sets().IsInNontrivialFirstPartySet(
      net::SchemefulSite(GURL("https://example.test"))));
  EXPECT_FALSE(sets().IsInNontrivialFirstPartySet(
      net::SchemefulSite(GURL("https://aaaa.test"))));
  EXPECT_FALSE(sets().IsInNontrivialFirstPartySet(
      net::SchemefulSite(GURL("https://bbbb.test"))));
}

TEST_F(FirstPartySetsDisabledTest, ComputeContext_InfersSingletons) {
  net::SchemefulSite member(GURL("https://member1.test"));
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite wss_member(GURL("wss://member1.test"));

  FirstPartySets sets;

  // Works if the site is provided with WSS scheme instead of HTTPS.
  EXPECT_THAT(sets.ComputeContext(wss_member, &member, {member, example}),
              net::SamePartyContext(Type::kCrossParty, Type::kCrossParty,
                                    Type::kSameParty));

  EXPECT_THAT(sets.ComputeContext(example, &member, {member}),
              net::SamePartyContext(Type::kCrossParty));
  EXPECT_THAT(sets.ComputeContext(member, &example, {member}),
              net::SamePartyContext(Type::kCrossParty));

  // Top&resource differs from Ancestors.
  EXPECT_THAT(sets.ComputeContext(member, &member, {example}),
              net::SamePartyContext(Type::kCrossParty, Type::kCrossParty,
                                    Type::kSameParty));

  // Metrics values infer singleton sets when appropriate.
  EXPECT_THAT(sets.ComputeContext(member, &member, {member}),
              net::SamePartyContext(Type::kCrossParty, Type::kSameParty,
                                    Type::kSameParty));
  EXPECT_THAT(sets.ComputeContext(member, &example, {member}),
              net::SamePartyContext(Type::kCrossParty));
  EXPECT_THAT(sets.ComputeContext(example, &member, {member}),
              net::SamePartyContext(Type::kCrossParty));
  EXPECT_THAT(sets.ComputeContext(member, &member, {example}),
              net::SamePartyContext(Type::kCrossParty, Type::kCrossParty,
                                    Type::kSameParty));

  EXPECT_THAT(sets.ComputeContext(member, &member, {member, example}),
              net::SamePartyContext(Type::kCrossParty, Type::kCrossParty,
                                    Type::kSameParty));
}

TEST_F(FirstPartySetsDisabledTest, FindOwner) {
  sets().SetManuallySpecifiedSet("https://example.test,https://member.test");
  EXPECT_FALSE(
      sets().FindOwner(net::SchemefulSite(GURL("https://example.test"))));
  EXPECT_FALSE(
      sets().FindOwner(net::SchemefulSite(GURL("https://member.test"))));
}

class FirstPartySetsEnabledTest : public FirstPartySetsTest {
 public:
  FirstPartySetsEnabledTest() : FirstPartySetsTest(true) {}
};

TEST_F(FirstPartySetsEnabledTest, Sets_IsEmpty) {
  EXPECT_THAT(FirstPartySets().Sets(), IsEmpty());
}

TEST_F(FirstPartySetsEnabledTest, ParsesJSON) {
  SetComponentSetsAndWait("[]");
  EXPECT_THAT(sets().Sets(), IsEmpty());
}

TEST_F(FirstPartySetsEnabledTest, AcceptsMinimal) {
  const std::string input =
      R"([{
        "owner": "https://example.test",
        "members": ["https://aaaa.test"]
        }])";
  ASSERT_TRUE(base::JSONReader::Read(input));

  SetComponentSetsAndWait(input);

  EXPECT_THAT(sets().Sets(),
              UnorderedElementsAre(Pair(
                  SerializesTo("https://example.test"),
                  UnorderedElementsAre(SerializesTo("https://example.test"),
                                       SerializesTo("https://aaaa.test")))));
}

TEST_F(FirstPartySetsEnabledTest, V2_AcceptsMinimal) {
  const std::string input =
      "{\"owner\": \"https://example.test\",\"members\": "
      "[\"https://aaaa.test\",],}";

  SetComponentSetsAndWait(input);
  EXPECT_THAT(sets().Sets(),
              UnorderedElementsAre(Pair(
                  SerializesTo("https://example.test"),
                  UnorderedElementsAre(SerializesTo("https://example.test"),
                                       SerializesTo("https://aaaa.test")))));
}

TEST_F(FirstPartySetsEnabledTest, AcceptsMultipleSets) {
  const std::string input = R"(
  [
    {
      "owner": "https://example.test",
      "members": ["https://member1.test"]
    },
    {
      "owner": "https://foo.test",
      "members": ["https://member2.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));
  SetComponentSetsAndWait(input);

  EXPECT_THAT(
      sets().Sets(),
      UnorderedElementsAre(
          Pair(SerializesTo("https://example.test"),
               UnorderedElementsAre(SerializesTo("https://example.test"),
                                    SerializesTo("https://member1.test"))),
          Pair(SerializesTo("https://foo.test"),
               UnorderedElementsAre(SerializesTo("https://foo.test"),
                                    SerializesTo("https://member2.test")))));
}

TEST_F(FirstPartySetsEnabledTest, V2_AcceptsMultipleSets) {
  const std::string input =
      "{\"owner\": \"https://example.test\",\"members\": "
      "[\"https://member1.test\"]}\n"
      "{\"owner\": \"https://foo.test\",\"members\": "
      "[\"https://member2.test\"]}";

  SetComponentSetsAndWait(input);
  EXPECT_THAT(
      sets().Sets(),
      UnorderedElementsAre(
          Pair(SerializesTo("https://example.test"),
               UnorderedElementsAre(SerializesTo("https://example.test"),
                                    SerializesTo("https://member1.test"))),
          Pair(SerializesTo("https://foo.test"),
               UnorderedElementsAre(SerializesTo("https://foo.test"),
                                    SerializesTo("https://member2.test")))));
}

TEST_F(FirstPartySetsEnabledTest, ClearsPreloadedOnError) {
  const std::string input = R"(
  [
    {
      "owner": "https://example.test",
      "members": ["https://member1.test"]
    },
    {
      "owner": "https://foo.test",
      "members": ["https://member2.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));
  SetComponentSetsAndWait(input);

  EXPECT_THAT(
      sets().Sets(),
      UnorderedElementsAre(
          Pair(SerializesTo("https://example.test"),
               UnorderedElementsAre(SerializesTo("https://example.test"),
                                    SerializesTo("https://member1.test"))),
          Pair(SerializesTo("https://foo.test"),
               UnorderedElementsAre(SerializesTo("https://foo.test"),
                                    SerializesTo("https://member2.test")))));

  SetComponentSetsAndWait("{}");

  EXPECT_THAT(sets().Sets(), IsEmpty());
}

TEST_F(FirstPartySetsEnabledTest, OwnerIsOnlyMember) {
  const std::string input = R"(
  [
    {
      "owner": "https://example.test",
      "members": ["https://example.test"]
    },
    {
      "owner": "https://foo.test",
      "members": ["https://member2.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));
  SetComponentSetsAndWait(input);

  EXPECT_THAT(sets().Sets(), IsEmpty());
}

TEST_F(FirstPartySetsEnabledTest, OwnerIsMember) {
  const std::string input = R"(
  [
    {
      "owner": "https://example.test",
      "members": ["https://example.test", "https://member1.test"]
    },
    {
      "owner": "https://foo.test",
      "members": ["https://member2.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));
  SetComponentSetsAndWait(input);

  EXPECT_THAT(sets().Sets(), IsEmpty());
}

TEST_F(FirstPartySetsEnabledTest, RepeatedMember) {
  const std::string input = R"(
  [
    {
      "owner": "https://example.test",
      "members": [
        "https://member1.test",
        "https://member2.test",
        "https://member1.test"
        ]
    },
    {
      "owner": "https://foo.test",
      "members": ["https://member3.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));
  SetComponentSetsAndWait(input);

  EXPECT_THAT(sets().Sets(), IsEmpty());
}

TEST_F(FirstPartySetsEnabledTest, SetsManuallySpecified_Invalid_TooSmall) {
  sets().SetManuallySpecifiedSet("https://example.test");
  EXPECT_THAT(sets().Sets(), IsEmpty());
}

TEST_F(FirstPartySetsEnabledTest, SetsManuallySpecified_Invalid_NotOrigins) {
  sets().SetManuallySpecifiedSet("https://example.test,member1");
  EXPECT_THAT(sets().Sets(), IsEmpty());
}

TEST_F(FirstPartySetsEnabledTest, SetsManuallySpecified_Invalid_NotHTTPS) {
  sets().SetManuallySpecifiedSet("https://example.test,http://member1.test");
  EXPECT_THAT(sets().Sets(), IsEmpty());
}

TEST_F(FirstPartySetsEnabledTest,
       SetsManuallySpecified_Invalid_RegisteredDomain_Owner) {
  sets().SetManuallySpecifiedSet(
      "https://www.example.test..,https://www.member.test");
  EXPECT_THAT(sets().Sets(), IsEmpty());
}

TEST_F(FirstPartySetsEnabledTest,
       SetsManuallySpecified_Invalid_RegisteredDomain_Member) {
  sets().SetManuallySpecifiedSet(
      "https://www.example.test,https://www.member.test..");
  EXPECT_THAT(sets().Sets(), IsEmpty());
}

TEST_F(FirstPartySetsEnabledTest, SetsManuallySpecified_Valid_EmptyValue) {
  sets().SetManuallySpecifiedSet("");

  // Set non-empty existing sets to distinguish the failure case from the no-op
  // case when processing the manually-specified sets.
  const std::string existing_sets = R"(
  [
    {
      "owner": "https://example.test",
      "members": ["https://member.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(existing_sets));
  SetComponentSetsAndWait(existing_sets);

  EXPECT_THAT(sets().Sets(),
              UnorderedElementsAre(Pair(
                  SerializesTo("https://example.test"),
                  UnorderedElementsAre(SerializesTo("https://example.test"),
                                       SerializesTo("https://member.test")))));
}

TEST_F(FirstPartySetsEnabledTest, SetsManuallySpecified_Valid_SingleMember) {
  sets().SetManuallySpecifiedSet("https://example.test,https://member.test");
  EXPECT_THAT(sets().Sets(),
              UnorderedElementsAre(Pair(
                  SerializesTo("https://example.test"),
                  UnorderedElementsAre(SerializesTo("https://example.test"),
                                       SerializesTo("https://member.test")))));
}

TEST_F(FirstPartySetsEnabledTest,
       SetsManuallySpecified_Valid_SingleMember_RegisteredDomain) {
  sets().SetManuallySpecifiedSet(
      "https://www.example.test,https://www.member.test");
  EXPECT_THAT(sets().Sets(),
              UnorderedElementsAre(Pair(
                  SerializesTo("https://example.test"),
                  UnorderedElementsAre(SerializesTo("https://example.test"),
                                       SerializesTo("https://member.test")))));
}

TEST_F(FirstPartySetsEnabledTest, SetsManuallySpecified_Valid_MultipleMembers) {
  sets().SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member2.test");
  EXPECT_THAT(sets().Sets(),
              UnorderedElementsAre(Pair(
                  SerializesTo("https://example.test"),
                  UnorderedElementsAre(SerializesTo("https://example.test"),
                                       SerializesTo("https://member1.test"),
                                       SerializesTo("https://member2.test")))));
}

TEST_F(FirstPartySetsEnabledTest,
       SetsManuallySpecified_Valid_OwnerIsOnlyMember) {
  sets().SetManuallySpecifiedSet("https://example.test,https://example.test");
  EXPECT_THAT(sets().Sets(), IsEmpty());
}

TEST_F(FirstPartySetsEnabledTest, SetsManuallySpecified_Valid_OwnerIsMember) {
  sets().SetManuallySpecifiedSet(
      "https://example.test,https://example.test,https://member1.test");
  EXPECT_THAT(sets().Sets(),
              UnorderedElementsAre(Pair(
                  SerializesTo("https://example.test"),
                  UnorderedElementsAre(SerializesTo("https://example.test"),
                                       SerializesTo("https://member1.test")))));
}

TEST_F(FirstPartySetsEnabledTest, SetsManuallySpecified_Valid_RepeatedMember) {
  sets().SetManuallySpecifiedSet(
      R"(https://example.test,
       https://member1.test,
       https://member2.test,
       https://member1.test)");
  EXPECT_THAT(sets().Sets(),
              UnorderedElementsAre(Pair(
                  SerializesTo("https://example.test"),
                  UnorderedElementsAre(SerializesTo("https://example.test"),
                                       SerializesTo("https://member1.test"),
                                       SerializesTo("https://member2.test")))));
}

TEST_F(FirstPartySetsEnabledTest,
       SetsManuallySpecified_DeduplicatesOwnerOwner) {
  const std::string input = R"(
  [
    {
      "owner": "https://example.test",
      "members": ["https://member2.test", "https://member3.test"]
    },
    {
      "owner": "https://bar.test",
      "members": ["https://member4.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));
  SetComponentSetsAndWait(input);

  sets().SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member2.test");
  EXPECT_THAT(
      sets().Sets(),
      UnorderedElementsAre(
          Pair(SerializesTo("https://example.test"),
               UnorderedElementsAre(SerializesTo("https://example.test"),
                                    SerializesTo("https://member1.test"),
                                    SerializesTo("https://member2.test"))),
          Pair(SerializesTo("https://bar.test"),
               UnorderedElementsAre(SerializesTo("https://bar.test"),
                                    SerializesTo("https://member4.test")))));
}

TEST_F(FirstPartySetsEnabledTest,
       SetsManuallySpecified_DeduplicatesOwnerMember) {
  const std::string input = R"(
  [
    {
      "owner": "https://foo.test",
      "members": ["https://member1.test", "https://example.test"]
    },
    {
      "owner": "https://bar.test",
      "members": ["https://member2.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));
  SetComponentSetsAndWait(input);

  sets().SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member3.test");
  EXPECT_THAT(
      sets().Sets(),
      UnorderedElementsAre(
          Pair(SerializesTo("https://example.test"),
               UnorderedElementsAre(SerializesTo("https://example.test"),
                                    SerializesTo("https://member1.test"),
                                    SerializesTo("https://member3.test"))),
          Pair(SerializesTo("https://bar.test"),
               UnorderedElementsAre(SerializesTo("https://bar.test"),
                                    SerializesTo("https://member2.test")))));
}

TEST_F(FirstPartySetsEnabledTest,
       SetsManuallySpecified_DeduplicatesMemberOwner) {
  const std::string input = R"(
  [
    {
      "owner": "https://foo.test",
      "members": ["https://member1.test", "https://member2.test"]
    },
    {
      "owner": "https://member3.test",
      "members": ["https://member4.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));
  SetComponentSetsAndWait(input);

  sets().SetManuallySpecifiedSet("https://example.test,https://member3.test");
  EXPECT_THAT(
      sets().Sets(),
      UnorderedElementsAre(
          Pair(SerializesTo("https://example.test"),
               UnorderedElementsAre(SerializesTo("https://example.test"),
                                    SerializesTo("https://member3.test"))),
          Pair(SerializesTo("https://foo.test"),
               UnorderedElementsAre(SerializesTo("https://foo.test"),
                                    SerializesTo("https://member1.test"),
                                    SerializesTo("https://member2.test")))));
}

TEST_F(FirstPartySetsEnabledTest,
       SetsManuallySpecified_DeduplicatesMemberMember) {
  const std::string input = R"(
  [
    {
      "owner": "https://foo.test",
      "members": ["https://member2.test", "https://member3.test"]
    },
    {
      "owner": "https://bar.test",
      "members": ["https://member4.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));
  SetComponentSetsAndWait(input);

  sets().SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member2.test");
  EXPECT_THAT(
      sets().Sets(),
      UnorderedElementsAre(
          Pair(SerializesTo("https://example.test"),
               UnorderedElementsAre(SerializesTo("https://example.test"),
                                    SerializesTo("https://member1.test"),
                                    SerializesTo("https://member2.test"))),
          Pair(SerializesTo("https://foo.test"),
               UnorderedElementsAre(SerializesTo("https://foo.test"),
                                    SerializesTo("https://member3.test"))),
          Pair(SerializesTo("https://bar.test"),
               UnorderedElementsAre(SerializesTo("https://bar.test"),
                                    SerializesTo("https://member4.test")))));
}

TEST_F(FirstPartySetsEnabledTest,
       SetsManuallySpecified_ClearsPreloadedOnError) {
  const std::string input = R"(
  [
    {
      "owner": "https://bar.test",
      "members": ["https://member3.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));
  SetComponentSetsAndWait(input);

  sets().SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member2.test");
  EXPECT_THAT(
      sets().Sets(),
      UnorderedElementsAre(
          Pair(SerializesTo("https://example.test"),
               UnorderedElementsAre(SerializesTo("https://example.test"),
                                    SerializesTo("https://member1.test"),
                                    SerializesTo("https://member2.test"))),
          Pair(SerializesTo("https://bar.test"),
               UnorderedElementsAre(SerializesTo("https://bar.test"),
                                    SerializesTo("https://member3.test")))));

  SetComponentSetsAndWait("{}");

  EXPECT_THAT(sets().Sets(),
              UnorderedElementsAre(Pair(
                  SerializesTo("https://example.test"),
                  UnorderedElementsAre(SerializesTo("https://example.test"),
                                       SerializesTo("https://member1.test"),
                                       SerializesTo("https://member2.test")))));
}

TEST_F(FirstPartySetsEnabledTest,
       SetsManuallySpecified_PrunesInducedSingletons) {
  const std::string input = R"(
  [
    {
      "owner": "https://foo.test",
      "members": ["https://member1.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));
  SetComponentSetsAndWait(input);

  sets().SetManuallySpecifiedSet("https://example.test,https://member1.test");
  // If we just erased entries that overlapped with the manually-supplied set,
  // https://foo.test would be left as a singleton set. But since we disallow
  // singleton sets, we ensure that such cases are caught and removed.
  EXPECT_THAT(sets().Sets(),
              UnorderedElementsAre(Pair(
                  SerializesTo("https://example.test"),
                  UnorderedElementsAre(SerializesTo("https://example.test"),
                                       SerializesTo("https://member1.test")))));
}

TEST_F(FirstPartySetsEnabledTest, ComputeSetsDiff_SitesJoined) {
  auto old_sets = base::flat_map<net::SchemefulSite, net::SchemefulSite>{
      {net::SchemefulSite(GURL("https://example.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://member3.test")),
       net::SchemefulSite(GURL("https://example.test"))}};

  // Consistency check the reviewer-friendly JSON format matches the input.
  ASSERT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(R"(
    [
      {
        "owner": "https://example.test",
        "members": ["https://member1.test", "https://member3.test"]
      }
    ]
  )"),
              old_sets);

  SetComponentSetsAndWait(R"(
    [
      {
        "owner": "https://example.test",
        "members": ["https://member1.test", "https://member3.test"]
      },
      {
        "owner": "https://foo.test",
        "members": ["https://member2.test"]
      }
    ]
  )");

  // "https://foo.test" and "https://member2.test" joined FPSs. We don't clear
  // site data upon joining, so the computed diff should be empty set.
  EXPECT_THAT(sets().ComputeSetsDiff(old_sets), IsEmpty());
}

TEST_F(FirstPartySetsEnabledTest, ComputeSetsDiff_SitesLeft) {
  auto old_sets = base::flat_map<net::SchemefulSite, net::SchemefulSite>{
      {net::SchemefulSite(GURL("https://example.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://member3.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::SchemefulSite(GURL("https://foo.test"))},
      {net::SchemefulSite(GURL("https://member2.test")),
       net::SchemefulSite(GURL("https://foo.test"))}};

  // Consistency check the reviewer-friendly JSON format matches the input.
  ASSERT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(R"(
    [
      {
        "owner": "https://example.test",
        "members": ["https://member1.test", "https://member3.test"]
      },
      {
        "owner": "https://foo.test",
        "members": ["https://member2.test"]
      },
    ]
  )"),
              old_sets);

  SetComponentSetsAndWait(R"(
    [
      {
        "owner": "https://example.test",
        "members": ["https://member1.test"]
      },
    ]
  )");
  // Expected diff: "https://foo.test", "https://member2.test" and
  // "https://member3.test" left FPSs.
  EXPECT_THAT(sets().ComputeSetsDiff(old_sets),
              UnorderedElementsAre(SerializesTo("https://foo.test"),
                                   SerializesTo("https://member2.test"),
                                   SerializesTo("https://member3.test")));
}

TEST_F(FirstPartySetsEnabledTest, ComputeSetsDiff_OwnerChanged) {
  auto old_sets = base::flat_map<net::SchemefulSite, net::SchemefulSite>{
      {net::SchemefulSite(GURL("https://example.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::SchemefulSite(GURL("https://foo.test"))},
      {net::SchemefulSite(GURL("https://member2.test")),
       net::SchemefulSite(GURL("https://foo.test"))},
      {net::SchemefulSite(GURL("https://member3.test")),
       net::SchemefulSite(GURL("https://foo.test"))}};

  // Consistency check the reviewer-friendly JSON format matches the input.
  ASSERT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(R"(
    [
      {
        "owner": "https://example.test",
        "members": ["https://member1.test"]
      },
      {
        "owner": "https://foo.test",
        "members": ["https://member2.test", "https://member3.test"]
      },
    ]
  )"),
              old_sets);

  SetComponentSetsAndWait(R"(
    [
      {
        "owner": "https://example.test",
        "members": ["https://member1.test", "https://member3.test"]
      },
      {
        "owner": "https://foo.test",
        "members": ["https://member2.test"]
      }
    ]
  )");
  // Expected diff: "https://member3.test" changed owner.
  EXPECT_THAT(sets().ComputeSetsDiff(old_sets),
              UnorderedElementsAre(SerializesTo("https://member3.test")));
}

TEST_F(FirstPartySetsEnabledTest, ComputeSetsDiff_OwnerLeft) {
  auto old_sets = base::flat_map<net::SchemefulSite, net::SchemefulSite>{
      {net::SchemefulSite(GURL("https://example.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://bar.test")),
       net::SchemefulSite(GURL("https://example.test"))}};

  // Consistency check the reviewer-friendly JSON format matches the input.
  ASSERT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(R"(
    [
      {
        "owner": "https://example.test",
        "members": ["https://foo.test", "https://bar.test"]
      }
    ]
  )"),
              old_sets);

  SetComponentSetsAndWait(R"(
    [
      {
        "owner": "https://foo.test",
        "members": ["https://bar.test"]
      }
    ]
  )");
  // Expected diff: "https://example.test" left FPSs, "https://foo.test" and
  // "https://bar.test" changed owner.
  // It would be valid to only have example.test in the diff, but our logic
  // isn't sophisticated enough yet to know that foo.test and bar.test don't
  // need to be included in the result.
  EXPECT_THAT(sets().ComputeSetsDiff(old_sets),
              UnorderedElementsAre(SerializesTo("https://example.test"),
                                   SerializesTo("https://foo.test"),
                                   SerializesTo("https://bar.test")));
}

TEST_F(FirstPartySetsEnabledTest, ComputeSetsDiff_OwnerMemberRotate) {
  auto old_sets = base::flat_map<net::SchemefulSite, net::SchemefulSite>{
      {net::SchemefulSite(GURL("https://example.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::SchemefulSite(GURL("https://example.test"))}};

  // Consistency check the reviewer-friendly JSON format matches the input.
  ASSERT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(R"(
    [
      {
        "owner": "https://example.test",
        "members": ["https://foo.test"]
      }
    ]
  )"),
              old_sets);

  SetComponentSetsAndWait(R"(
    [
      {
        "owner": "https://foo.test",
        "members": ["https://example.test"]
      }
    ]
  )");
  // Expected diff: "https://example.test" and "https://foo.test" changed owner.
  // It would be valid to not include example.test and foo.test in the result,
  // but our logic isn't sophisticated enough yet to know that.ß
  EXPECT_THAT(sets().ComputeSetsDiff(old_sets),
              UnorderedElementsAre(SerializesTo("https://example.test"),
                                   SerializesTo("https://foo.test")));
}

TEST_F(FirstPartySetsEnabledTest, ComputeSetsDiff_EmptySets) {
  // Empty old_sets.
  SetComponentSetsAndWait(R"(
    [
      {
        "owner": "https://example.test",
        "members": ["https://member1.test"]
      },
    ]
  )");
  EXPECT_THAT(sets().ComputeSetsDiff({}), IsEmpty());

  // Empty current sets.
  auto old_sets = base::flat_map<net::SchemefulSite, net::SchemefulSite>{
      {net::SchemefulSite(GURL("https://example.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::SchemefulSite(GURL("https://example.test"))}};
  // Consistency check the reviewer-friendly JSON format matches the input.
  ASSERT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(R"(
    [
      {
        "owner": "https://example.test",
        "members": ["https://member1.test"]
      }
    ]
  )"),
              old_sets);
  EXPECT_THAT(FirstPartySets().ComputeSetsDiff(old_sets),
              UnorderedElementsAre(SerializesTo("https://example.test"),
                                   SerializesTo("https://member1.test")));
}

TEST_F(FirstPartySetsEnabledTest, ClearSiteDataOnChangedSetsIfReady_NotReady) {
  int callback_calls = 0;
  auto callback = base::BindLambdaForTesting(
      [&](const std::string& got) { callback_calls++; });
  // component sets not ready.
  {
    FirstPartySets sets;
    callback_calls = 0;
    sets.SetPersistedSets("{}");
    sets.SetManuallySpecifiedSet("");
    sets.SetOnSiteDataCleared(callback);
    EXPECT_EQ(callback_calls, 0);
  }
  // manual sets not ready.
  {
    FirstPartySets sets;
    callback_calls = 0;
    SetComponentSetsAndWait(sets, "[]");
    sets.SetPersistedSets("{}");
    sets.SetOnSiteDataCleared(callback);
    EXPECT_EQ(callback_calls, 0);
  }
  // persisted sets not ready.
  {
    FirstPartySets sets;
    callback_calls = 0;
    SetComponentSetsAndWait(sets, "[]");
    sets.SetManuallySpecifiedSet("");
    sets.SetOnSiteDataCleared(callback);
    EXPECT_EQ(callback_calls, 0);
  }
  // callback not set.
  {
    FirstPartySets sets;
    callback_calls = 0;
    SetComponentSetsAndWait(sets, "[]");
    sets.SetManuallySpecifiedSet("");
    sets.SetPersistedSets("{}");
    EXPECT_EQ(callback_calls, 0);
  }
}

// The callback only runs when `old_sets` is generated and `sets` has merged the
// inputs from Component Updater and command line flag.
TEST_F(FirstPartySetsEnabledTest, ClearSiteDataOnChangedSetsIfReady_Ready) {
  int callback_calls = 0;
  SetComponentSetsAndWait(R"([
       {
         "owner": "https://example.test",
         "members": ["https://member1.test"]
       }
     ])");
  sets().SetManuallySpecifiedSet("https://example2.test,https://member2.test");
  sets().SetPersistedSets(
      R"({"https://example.test":"https://example.test",
            "https://member1.test":"https://example.test"})");
  sets().SetOnSiteDataCleared(base::BindLambdaForTesting([&](const std::string&
                                                                 got) {
    EXPECT_EQ(
        got,
        R"({"https://member1.test":"https://example.test","https://member2.test":"https://example2.test"})");
    callback_calls++;
  }));
  EXPECT_EQ(callback_calls, 1);
}

class PopulatedFirstPartySetsTest : public FirstPartySetsEnabledTest {
 public:
  PopulatedFirstPartySetsTest() {
    const std::string input = R"(
      [
        {
          "owner": "https://example.test",
          "members": ["https://member1.test", "https://member3.test"]
        },
        {
          "owner": "https://foo.test",
          "members": ["https://member2.test"]
        }
      ]
      )";
    CHECK(base::JSONReader::Read(input));
    SetComponentSetsAndWait(input);

    CHECK(Value(
        sets().Sets(),
        UnorderedElementsAre(
            Pair(SerializesTo("https://example.test"),
                 UnorderedElementsAre(SerializesTo("https://example.test"),
                                      SerializesTo("https://member1.test"),
                                      SerializesTo("https://member3.test"))),
            Pair(SerializesTo("https://foo.test"),
                 UnorderedElementsAre(SerializesTo("https://foo.test"),
                                      SerializesTo("https://member2.test"))))));
  }
};

TEST_F(PopulatedFirstPartySetsTest, ComputeContext_EmptyContext) {
  net::SchemefulSite example_site(GURL("https://example.test"));
  net::SchemefulSite nonmember(GURL("https://nonmember.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://nonmember.test")),
                            top_frame, {})
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(sets().ComputeContext(example_site, top_frame, {}).context_type(),
              Type::kSameParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("http://example.test")),
                            top_frame, {})
            .context_type(),
        Type::kCrossParty);
  }

  EXPECT_EQ(sets().ComputeContext(example_site, &nonmember, {}).context_type(),
            Type::kCrossParty);
  EXPECT_EQ(sets().ComputeContext(nonmember, &example_site, {}).context_type(),
            Type::kCrossParty);
}

TEST_F(PopulatedFirstPartySetsTest, ComputeContext_ContextIsNonmember) {
  std::set<net::SchemefulSite> context({
      net::SchemefulSite(GURL("https://nonmember.test")),
  });

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://example.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("http://example.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://member1.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(sets()
                  .ComputeContext(net::SchemefulSite(GURL("https://foo.test")),
                                  top_frame, context)
                  .context_type(),
              Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://member2.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://nonmember.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);
  }
}

TEST_F(PopulatedFirstPartySetsTest, ComputeContext_ContextIsOwner) {
  std::set<net::SchemefulSite> context(
      {net::SchemefulSite(GURL("https://example.test"))});

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://example.test")),
                            top_frame, context)
            .context_type(),
        Type::kSameParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("http://example.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://member1.test")),
                            top_frame, context)
            .context_type(),
        Type::kSameParty);

    EXPECT_EQ(sets()
                  .ComputeContext(net::SchemefulSite(GURL("https://foo.test")),
                                  top_frame, context)
                  .context_type(),
              Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://member2.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://nonmember.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);
  }
}

TEST_F(PopulatedFirstPartySetsTest, ComputeContext_ContextIsMember) {
  std::set<net::SchemefulSite> context(
      {net::SchemefulSite(GURL("https://member1.test"))});

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://example.test")),
                            top_frame, context)
            .context_type(),
        Type::kSameParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("http://example.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://example.test")),
                            top_frame, context)
            .context_type(),
        Type::kSameParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://member1.test")),
                            top_frame, context)
            .context_type(),
        Type::kSameParty);

    EXPECT_EQ(sets()
                  .ComputeContext(net::SchemefulSite(GURL("https://foo.test")),
                                  top_frame, context)
                  .context_type(),
              Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://member2.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://nonmember.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);
  }
}

TEST_F(PopulatedFirstPartySetsTest, ComputeContext_ContextIsOwnerAndMember) {
  std::set<net::SchemefulSite> context({
      net::SchemefulSite(GURL("https://example.test")),
      net::SchemefulSite(GURL("https://member1.test")),
  });

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://example.test")),
                            top_frame, context)
            .context_type(),
        Type::kSameParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("http://example.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://member1.test")),
                            top_frame, context)
            .context_type(),
        Type::kSameParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://member3.test")),
                            top_frame, context)
            .context_type(),
        Type::kSameParty);

    EXPECT_EQ(sets()
                  .ComputeContext(net::SchemefulSite(GURL("https://foo.test")),
                                  top_frame, context)
                  .context_type(),
              Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://member2.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://nonmember.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);
  }
}

TEST_F(PopulatedFirstPartySetsTest, ComputeContext_ContextMixesParties) {
  std::set<net::SchemefulSite> context({
      net::SchemefulSite(GURL("https://example.test")),
      net::SchemefulSite(GURL("https://member1.test")),
      net::SchemefulSite(GURL("https://foo.test")),
  });

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://example.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("http://example.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://member1.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(sets()
                  .ComputeContext(net::SchemefulSite(GURL("https://foo.test")),
                                  top_frame, context)
                  .context_type(),
              Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://member2.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://nonmember.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);
  }
}

TEST_F(PopulatedFirstPartySetsTest,
       ComputeContext_ContextMixesMembersAndNonmembers) {
  std::set<net::SchemefulSite> context({
      net::SchemefulSite(GURL("https://example.test")),
      net::SchemefulSite(GURL("https://member1.test")),
      net::SchemefulSite(GURL("http://nonmember.test")),
  });

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://example.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("http://example.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://member1.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(sets()
                  .ComputeContext(net::SchemefulSite(GURL("https://foo.test")),
                                  top_frame, context)
                  .context_type(),
              Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://member2.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://nonmember.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);
  }
}

TEST_F(PopulatedFirstPartySetsTest, ComputeContext_ContextMixesSchemes) {
  std::set<net::SchemefulSite> context({
      net::SchemefulSite(GURL("https://example.test")),
      net::SchemefulSite(GURL("https://member1.test")),
      net::SchemefulSite(GURL("http://example.test")),
  });

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://example.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("http://example.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://member1.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(sets()
                  .ComputeContext(net::SchemefulSite(GURL("https://foo.test")),
                                  top_frame, context)
                  .context_type(),
              Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://member2.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);

    EXPECT_EQ(
        sets()
            .ComputeContext(net::SchemefulSite(GURL("https://nonmember.test")),
                            top_frame, context)
            .context_type(),
        Type::kCrossParty);
  }
}

TEST_F(PopulatedFirstPartySetsTest, ComputeContext) {
  net::SchemefulSite nonmember(GURL("https://nonmember.test"));
  net::SchemefulSite nonmember1(GURL("https://nonmember1.test"));
  net::SchemefulSite member(GURL("https://member1.test"));
  net::SchemefulSite owner(GURL("https://example.test"));
  net::SchemefulSite wss_member(GURL("wss://member1.test"));
  net::SchemefulSite wss_nonmember(GURL("wss://nonmember.test"));

  // Works as usual for sites that are in First-Party sets.
  EXPECT_THAT(sets().ComputeContext(member, &member, {member}),
              net::SamePartyContext(Type::kSameParty));
  EXPECT_THAT(sets().ComputeContext(owner, &member, {member}),
              net::SamePartyContext(Type::kSameParty));
  EXPECT_THAT(sets().ComputeContext(member, &owner, {member}),
              net::SamePartyContext(Type::kSameParty));
  EXPECT_THAT(sets().ComputeContext(member, &member, {owner}),
              net::SamePartyContext(Type::kSameParty));
  EXPECT_THAT(sets().ComputeContext(member, &member, {member, owner}),
              net::SamePartyContext(Type::kSameParty));

  // Works if the site is provided with WSS scheme instead of HTTPS.
  EXPECT_THAT(sets().ComputeContext(wss_member, &member, {member, owner}),
              net::SamePartyContext(Type::kSameParty));

  EXPECT_THAT(sets().ComputeContext(nonmember, &member, {member}),
              net::SamePartyContext(Type::kCrossParty));
  EXPECT_THAT(sets().ComputeContext(member, &nonmember, {member}),
              net::SamePartyContext(Type::kCrossParty));
  EXPECT_THAT(
      sets().ComputeContext(wss_nonmember, &wss_member, {member, owner}),
      net::SamePartyContext(Type::kCrossParty));

  // Top&resource differs from Ancestors.
  EXPECT_THAT(sets().ComputeContext(member, &member, {nonmember}),
              net::SamePartyContext(Type::kCrossParty, Type::kCrossParty,
                                    Type::kSameParty));

  // Metrics values infer singleton sets when appropriate.
  EXPECT_THAT(sets().ComputeContext(nonmember, &nonmember, {nonmember}),
              net::SamePartyContext(Type::kCrossParty, Type::kSameParty,
                                    Type::kSameParty));
  EXPECT_THAT(sets().ComputeContext(nonmember, &nonmember1, {nonmember}),
              net::SamePartyContext(Type::kCrossParty));
  EXPECT_THAT(sets().ComputeContext(nonmember1, &nonmember, {nonmember}),
              net::SamePartyContext(Type::kCrossParty));
  EXPECT_THAT(sets().ComputeContext(nonmember, &nonmember, {nonmember1}),
              net::SamePartyContext(Type::kCrossParty, Type::kCrossParty,
                                    Type::kSameParty));

  EXPECT_THAT(sets().ComputeContext(member, &member, {member, nonmember}),
              net::SamePartyContext(Type::kCrossParty, Type::kCrossParty,
                                    Type::kSameParty));
  EXPECT_THAT(sets().ComputeContext(nonmember, &nonmember, {member, nonmember}),
              net::SamePartyContext(Type::kCrossParty, Type::kCrossParty,
                                    Type::kSameParty));
}

TEST_F(PopulatedFirstPartySetsTest, IsInNontrivialFirstPartySet) {
  EXPECT_TRUE(sets().IsInNontrivialFirstPartySet(
      net::SchemefulSite(GURL("https://example.test"))));

  EXPECT_FALSE(sets().IsInNontrivialFirstPartySet(
      net::SchemefulSite(GURL("http://example.test"))));

  EXPECT_TRUE(sets().IsInNontrivialFirstPartySet(
      net::SchemefulSite(GURL("https://member1.test"))));

  EXPECT_TRUE(sets().IsInNontrivialFirstPartySet(
      net::SchemefulSite(GURL("wss://member1.test"))));

  EXPECT_FALSE(sets().IsInNontrivialFirstPartySet(
      net::SchemefulSite(GURL("ws://member1.test"))));

  EXPECT_FALSE(sets().IsInNontrivialFirstPartySet(
      net::SchemefulSite(GURL("https://nonmember.test"))));
}

TEST_F(PopulatedFirstPartySetsTest, FindOwner) {
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
              sets().FindOwner(net::SchemefulSite(GURL(test_case.url))));
  }
}

TEST_F(PopulatedFirstPartySetsTest, Sets_NonEmpty) {
  EXPECT_THAT(
      sets().Sets(),
      UnorderedElementsAre(
          Pair(SerializesTo("https://example.test"),
               UnorderedElementsAre(SerializesTo("https://example.test"),
                                    SerializesTo("https://member1.test"),
                                    SerializesTo("https://member3.test"))),
          Pair(SerializesTo("https://foo.test"),
               UnorderedElementsAre(SerializesTo("https://foo.test"),
                                    SerializesTo("https://member2.test")))));
}

TEST_F(PopulatedFirstPartySetsTest, ComputeContextType) {
  std::set<net::SchemefulSite> homogeneous_context({
      net::SchemefulSite(GURL("https://example.test")),
      net::SchemefulSite(GURL("https://member1.test")),
  });
  std::set<net::SchemefulSite> mixed_context({
      net::SchemefulSite(GURL("https://example.test")),
      net::SchemefulSite(GURL("https://nonmember.test")),
  });
  net::SchemefulSite singleton(GURL("https://implicit-singleton.test"));

  EXPECT_EQ(
      net::FirstPartySetsContextType::kTopFrameIgnoredHomogeneous,
      sets().ComputeContextType(
          net::SchemefulSite(GURL("https://example.test")), absl::nullopt, {}));
  EXPECT_EQ(net::FirstPartySetsContextType::kTopFrameIgnoredHomogeneous,
            sets().ComputeContextType(
                net::SchemefulSite(GURL("https://example.test")), absl::nullopt,
                homogeneous_context));

  EXPECT_EQ(net::FirstPartySetsContextType::kTopFrameIgnoredMixed,
            sets().ComputeContextType(
                net::SchemefulSite(GURL("https://example.test")), absl::nullopt,
                mixed_context));

  EXPECT_EQ(net::FirstPartySetsContextType::kHomogeneous,
            sets().ComputeContextType(
                net::SchemefulSite(GURL("https://example.test")),
                net::SchemefulSite(GURL("https://member1.test")), {}));
  EXPECT_EQ(net::FirstPartySetsContextType::kHomogeneous,
            sets().ComputeContextType(
                net::SchemefulSite(GURL("https://example.test")),
                net::SchemefulSite(GURL("https://member1.test")),
                homogeneous_context));
  EXPECT_EQ(net::FirstPartySetsContextType::kHomogeneous,
            sets().ComputeContextType(singleton, singleton, {singleton}));

  EXPECT_EQ(net::FirstPartySetsContextType::kTopResourceMatchMixed,
            sets().ComputeContextType(
                net::SchemefulSite(GURL("https://example.test")),
                net::SchemefulSite(GURL("https://member1.test")),
                {net::SchemefulSite(GURL("https://foo.test"))}));
  EXPECT_EQ(
      net::FirstPartySetsContextType::kTopResourceMatchMixed,
      sets().ComputeContextType(
          net::SchemefulSite(GURL("https://example.test")),
          net::SchemefulSite(GURL("https://member1.test")), mixed_context));
  EXPECT_EQ(net::FirstPartySetsContextType::kTopResourceMatchMixed,
            sets().ComputeContextType(
                net::SchemefulSite(GURL("https://example.test")),
                net::SchemefulSite(GURL("https://member1.test")), {singleton}));
  EXPECT_EQ(net::FirstPartySetsContextType::kTopResourceMatchMixed,
            sets().ComputeContextType(singleton, singleton, mixed_context));

  EXPECT_EQ(net::FirstPartySetsContextType::kTopResourceMismatch,
            sets().ComputeContextType(
                net::SchemefulSite(GURL("https://example.test")),
                net::SchemefulSite(GURL("https://foo.test")), {}));
  EXPECT_EQ(
      net::FirstPartySetsContextType::kTopResourceMismatch,
      sets().ComputeContextType(
          net::SchemefulSite(GURL("https://example.test")),
          net::SchemefulSite(GURL("https://foo.test")), homogeneous_context));
  EXPECT_EQ(net::FirstPartySetsContextType::kTopResourceMismatch,
            sets().ComputeContextType(
                net::SchemefulSite(GURL("https://example.test")),
                net::SchemefulSite(GURL("https://foo.test")), mixed_context));
  EXPECT_EQ(net::FirstPartySetsContextType::kTopResourceMismatch,
            sets().ComputeContextType(
                net::SchemefulSite(GURL("https://example.test")), singleton,
                mixed_context));
}

}  // namespace network

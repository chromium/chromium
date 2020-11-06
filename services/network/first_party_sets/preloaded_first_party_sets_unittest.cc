// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/preloaded_first_party_sets.h"

#include "base/json/json_reader.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

// Some of these tests overlap with FirstPartySetParser unittests, but
// overlapping test coverage isn't the worst thing.

namespace network {

MATCHER_P(SerializesTo, want, "") {
  const std::string got = arg.Serialize();
  return testing::ExplainMatchResult(testing::Eq(want), got, result_listener);
}

TEST(PreloadedFirstPartySets, ParsesJSON) {
  EXPECT_THAT(PreloadedFirstPartySets().ParseAndSet("[]"), Pointee(IsEmpty()));
}

TEST(PreloadedFirstPartySets, AcceptsMinimal) {
  const std::string input =
      R"([{
        "owner": "https://example.test",
        "members": ["https://aaaa.test"]
        }])";
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(PreloadedFirstPartySets().ParseAndSet(input),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://aaaa.test"),
                       SerializesTo("https://example.test")))));
}

TEST(PreloadedFirstPartySets, AcceptsMultipleSets) {
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

  EXPECT_THAT(
      PreloadedFirstPartySets().ParseAndSet(input),
      Pointee(UnorderedElementsAre(Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://foo.test")))));
}

TEST(PreloadedFirstPartySets, ClearsPreloadedOnError) {
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

  PreloadedFirstPartySets sets;
  EXPECT_THAT(
      sets.ParseAndSet(input),
      Pointee(UnorderedElementsAre(Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://foo.test")))));

  EXPECT_THAT(sets.ParseAndSet("{}"), Pointee(IsEmpty()));
}

TEST(PreloadedFirstPartySets, OwnerIsOnlyMember) {
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

  EXPECT_THAT(PreloadedFirstPartySets().ParseAndSet(input), Pointee(IsEmpty()));
}

TEST(PreloadedFirstPartySets, OwnerIsMember) {
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

  EXPECT_THAT(PreloadedFirstPartySets().ParseAndSet(input), Pointee(IsEmpty()));
}

TEST(PreloadedFirstPartySets, RepeatedMember) {
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

  EXPECT_THAT(PreloadedFirstPartySets().ParseAndSet(input), Pointee(IsEmpty()));
}

TEST(PreloadedFirstPartySets, SetsManuallySpecified_Invalid_TooSmall) {
  PreloadedFirstPartySets sets;
  sets.SetManuallySpecifiedSet("https://example.test");
  EXPECT_THAT(sets.ParseAndSet("[]"), Pointee(IsEmpty()));
}

TEST(PreloadedFirstPartySets, SetsManuallySpecified_Invalid_NotOrigins) {
  PreloadedFirstPartySets sets;
  sets.SetManuallySpecifiedSet("https://example.test,member1");
  EXPECT_THAT(sets.ParseAndSet("[]"), Pointee(IsEmpty()));
}

TEST(PreloadedFirstPartySets, SetsManuallySpecified_Invalid_NotHTTPS) {
  PreloadedFirstPartySets sets;
  sets.SetManuallySpecifiedSet("https://example.test,http://member1.test");
  EXPECT_THAT(sets.ParseAndSet("[]"), Pointee(IsEmpty()));
}

TEST(PreloadedFirstPartySets,
     SetsManuallySpecified_Invalid_RegisteredDomain_Owner) {
  PreloadedFirstPartySets sets;
  sets.SetManuallySpecifiedSet(
      "https://www.example.test..,https://www.member.test");
  EXPECT_THAT(sets.ParseAndSet("[]"), Pointee(IsEmpty()));
}

TEST(PreloadedFirstPartySets,
     SetsManuallySpecified_Invalid_RegisteredDomain_Member) {
  PreloadedFirstPartySets sets;
  sets.SetManuallySpecifiedSet(
      "https://www.example.test,https://www.member.test..");
  EXPECT_THAT(sets.ParseAndSet("[]"), Pointee(IsEmpty()));
}

TEST(PreloadedFirstPartySets, SetsManuallySpecified_Valid_SingleMember) {
  PreloadedFirstPartySets sets;
  sets.SetManuallySpecifiedSet("https://example.test,https://member.test");
  EXPECT_THAT(sets.ParseAndSet("[]"),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://member.test"),
                       SerializesTo("https://example.test")))));
}

TEST(PreloadedFirstPartySets,
     SetsManuallySpecified_Valid_SingleMember_RegisteredDomain) {
  PreloadedFirstPartySets sets;
  sets.SetManuallySpecifiedSet(
      "https://www.example.test,https://www.member.test");
  EXPECT_THAT(sets.ParseAndSet("[]"),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://member.test"),
                       SerializesTo("https://example.test")))));
}

TEST(PreloadedFirstPartySets, SetsManuallySpecified_Valid_MultipleMembers) {
  PreloadedFirstPartySets sets;
  sets.SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member2.test");
  EXPECT_THAT(sets.ParseAndSet("[]"),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://member1.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://member2.test"),
                       SerializesTo("https://example.test")))));
}

TEST(PreloadedFirstPartySets, SetsManuallySpecified_Valid_OwnerIsOnlyMember) {
  PreloadedFirstPartySets sets;
  sets.SetManuallySpecifiedSet("https://example.test,https://example.test");
  EXPECT_THAT(sets.ParseAndSet("[]"), Pointee(IsEmpty()));
}

TEST(PreloadedFirstPartySets, SetsManuallySpecified_Valid_OwnerIsMember) {
  PreloadedFirstPartySets sets;
  sets.SetManuallySpecifiedSet(
      "https://example.test,https://example.test,https://member1.test");
  EXPECT_THAT(sets.ParseAndSet("[]"),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://member1.test"),
                       SerializesTo("https://example.test")))));
}

TEST(PreloadedFirstPartySets, SetsManuallySpecified_Valid_RepeatedMember) {
  PreloadedFirstPartySets sets;
  sets.SetManuallySpecifiedSet(
      R"(https://example.test,
       https://member1.test,
       https://member2.test,
       https://member1.test)");
  EXPECT_THAT(sets.ParseAndSet("[]"),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://member1.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://member2.test"),
                       SerializesTo("https://example.test")))));
}

TEST(PreloadedFirstPartySets, SetsManuallySpecified_DeduplicatesOwnerOwner) {
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

  PreloadedFirstPartySets sets;
  sets.SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member2.test");
  EXPECT_THAT(
      sets.ParseAndSet(input),
      Pointee(UnorderedElementsAre(Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member4.test"),
                                        SerializesTo("https://bar.test")))));
}

TEST(PreloadedFirstPartySets, SetsManuallySpecified_DeduplicatesOwnerMember) {
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

  PreloadedFirstPartySets sets;
  sets.SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member3.test");
  EXPECT_THAT(sets.ParseAndSet(input),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://member1.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://member2.test"),
                       SerializesTo("https://bar.test")),
                  Pair(SerializesTo("https://member3.test"),
                       SerializesTo("https://example.test")))));
}

TEST(PreloadedFirstPartySets, SetsManuallySpecified_DeduplicatesMemberOwner) {
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

  PreloadedFirstPartySets sets;
  sets.SetManuallySpecifiedSet("https://example.test,https://member3.test");
  EXPECT_THAT(sets.ParseAndSet(input),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://member1.test"),
                       SerializesTo("https://foo.test")),
                  Pair(SerializesTo("https://member2.test"),
                       SerializesTo("https://foo.test")),
                  Pair(SerializesTo("https://member3.test"),
                       SerializesTo("https://example.test")))));
}

TEST(PreloadedFirstPartySets, SetsManuallySpecified_DeduplicatesMemberMember) {
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

  PreloadedFirstPartySets sets;
  sets.SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member2.test");
  EXPECT_THAT(
      sets.ParseAndSet(input),
      Pointee(UnorderedElementsAre(Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member3.test"),
                                        SerializesTo("https://foo.test")),
                                   Pair(SerializesTo("https://member4.test"),
                                        SerializesTo("https://bar.test")))));
}

TEST(PreloadedFirstPartySets, SetsManuallySpecified_ClearsPreloadedOnError) {
  const std::string input = R"(
  [
    {
      "owner": "https://bar.test",
      "members": ["https://member3.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));

  PreloadedFirstPartySets sets;
  sets.SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member2.test");
  EXPECT_THAT(
      sets.ParseAndSet(input),
      Pointee(UnorderedElementsAre(Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member3.test"),
                                        SerializesTo("https://bar.test")))));

  EXPECT_THAT(sets.ParseAndSet("{}"),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://member1.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://member2.test"),
                       SerializesTo("https://example.test")))));
}

}  // namespace network

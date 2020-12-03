// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_set_parser.h"

#include "base/json/json_reader.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

namespace network {

MATCHER_P(SerializesTo, want, "") {
  const std::string got = arg.Serialize();
  return testing::ExplainMatchResult(testing::Eq(want), got, result_listener);
}

TEST(FirstPartySetParser_Preloaded, RejectsEmpty) {
  // If the input isn't valid JSON, we should
  // reject it. In particular, we should reject
  // empty input.

  EXPECT_FALSE(FirstPartySetParser::ParsePreloadedSets(""));
}

TEST(FirstPartySetParser_Preloaded, RejectsNonemptyMalformed) {
  // If the input isn't valid JSON, we should
  // reject it.
  const char input[] = "certainly not valid JSON";

  // Sanity check that the input is not valid JSON.
  ASSERT_FALSE(base::JSONReader::Read(input));

  EXPECT_FALSE(FirstPartySetParser::ParsePreloadedSets(input));
}

TEST(FirstPartySetParser, RejectsNonListInput) {
  // The outermost value should be a list.
  const std::string input = "{}";
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_FALSE(FirstPartySetParser::ParsePreloadedSets(input));
}

TEST(FirstPartySetParser, AcceptsTrivial) {
  const std::string input = "[]";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input),
              Pointee(IsEmpty()));
}

TEST(FirstPartySetParser, RejectsSingletonSet) {
  const std::string input =
      R"([{
        "owner": "https://example.test",
        "members": []
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_FALSE(FirstPartySetParser::ParsePreloadedSets(input));
}

TEST(FirstPartySetParser, AcceptsMinimal) {
  const std::string input =
      R"([{
        "owner": "https://example.test",
        "members": ["https://aaaa.test"]
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://aaaa.test"),
                       SerializesTo("https://example.test")))));
}

TEST(FirstPartySetParser, RejectsMissingOwner) {
  const std::string input = R"( [ { "members": ["https://aaaa.test"] } ] )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input), IsNull());
}

TEST(FirstPartySetParser, RejectsTypeUnsafeOwner) {
  const std::string input =
      R"( [ { "owner": 3, "members": ["https://aaaa.test"] } ] )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input), IsNull());
}

TEST(FirstPartySetParser, RejectsNonHTTPSOwner) {
  const std::string input =
      R"([{
        "owner": "http://example.test",
        "members": ["https://aaaa.test"]
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input), IsNull());
}

TEST(FirstPartySetParser, RejectsNonOriginOwner) {
  const std::string input =
      R"([{
        "owner": "example",
        "members": ["https://aaaa.test"]
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input), IsNull());
}

TEST(FirstPartySetParser, RejectsOwnerWithoutRegisteredDomain) {
  const std::string input =
      R"([{
        "owner": "https://example.test..",
        "members": ["https://aaaa.test"]
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input), IsNull());
}

TEST(FirstPartySetParser, RejectsMissingMembers) {
  const std::string input = R"( [ { "owner": "https://example.test" } ] )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input), IsNull());
}

TEST(FirstPartySetParser, RejectsTypeUnsafeMembers) {
  const std::string input =
      R"([{
        "owner": "https://example.test",
        "members": ["https://aaaa.test", 4]
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input), IsNull());
}

TEST(FirstPartySetParser, RejectsNonHTTPSMember) {
  const std::string input =
      R"([{
        "owner": "https://example.test",
        "members": ["http://aaaa.test"]
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input), IsNull());
}

TEST(FirstPartySetParser, RejectsNonOriginMember) {
  const std::string input =
      R"([{
        "owner": "https://example.test",
        "members": ["aaaa"]
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input), IsNull());
}

TEST(FirstPartySetParser, RejectsMemberWithoutRegisteredDomain) {
  const std::string input =
      R"([{
        "owner": "https://example.test",
        "members": ["https://aaaa.test.."]
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input), IsNull());
}

TEST(FirstPartySetParser, TruncatesSubdomain_Owner) {
  const std::string input =
      R"([{
        "owner": "https://subdomain.example.test",
        "members": ["https://aaaa.test"]
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://aaaa.test"),
                       SerializesTo("https://example.test")))));
}

TEST(FirstPartySetParser, TruncatesSubdomain_Member) {
  const std::string input =
      R"([{
        "owner": "https://example.test",
        "members": ["https://subdomain.aaaa.test"]
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://aaaa.test"),
                       SerializesTo("https://example.test")))));
}

TEST(FirstPartySetParser, AcceptsMultipleSets) {
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

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(
      FirstPartySetParser::ParsePreloadedSets(input),
      Pointee(UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://foo.test"),
                                        SerializesTo("https://foo.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://foo.test")))));
}

TEST(FirstPartySetParser, RejectsInvalidSets_InvalidOwner) {
  const std::string input = R"(
  [
    {
      "owner": 3,
      "members": ["https://member1.test"]
    },
    {
      "owner": "https://foo.test",
      "members": ["https://member2.test"]
    }
  ]
  )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input), IsNull());
}

TEST(FirstPartySetParser, RejectsInvalidSets_InvalidMember) {
  const std::string input = R"(
  [
    {
      "owner": "https://example.test",
      "members": [3]
    },
    {
      "owner": "https://foo.test",
      "members": ["https://member2.test"]
    }
  ]
  )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input), IsNull());
}

TEST(FirstPartySetParser, AllowsTrailingCommas) {
  const std::string input = R"(
  [
    {
      "owner": "https://example.test",
      "members": ["https://member1.test"],
    },
  ]
  )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(
      input, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://member1.test"),
                       SerializesTo("https://example.test")))));
}

TEST(FirstPartySetParser, Rejects_SameOwner) {
  const std::string input = R"(
  [
    {
      "owner": "https://example.test",
      "members": ["https://member1.test"]
    },
    {
      "owner": "https://example.test",
      "members": ["https://member2.test"]
    }
  ]
  )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input), IsNull());
}

TEST(FirstPartySetParser, Rejects_MemberAsOwner) {
  const std::string input = R"(
  [
    {
      "owner": "https://example.test",
      "members": ["https://member1.test"]
    },
    {
      "owner": "https://member1.test",
      "members": ["https://member2.test"]
    }
  ]
  )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input), IsNull());
}

TEST(FirstPartySetParser, Rejects_SameMember) {
  const std::string input = R"(
  [
    {
      "owner": "https://example.test",
      "members": ["https://member1.test"]
    },
    {
      "owner": "https://foo.test",
      "members": ["https://member1.test", "https://member2.test"]
    }
  ]
  )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input), IsNull());
}

TEST(FirstPartySetParser, Rejects_OwnerAsMember) {
  const std::string input = R"(
  [
    {
      "owner": "https://example.test",
      "members": ["https://member1.test"]
    },
    {
      "owner": "https://example2.test",
      "members": ["https://example.test", "https://member2.test"]
    }
  ]
  )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input), IsNull());
}

}  // namespace network

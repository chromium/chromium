// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_set_parser.h"

#include "base/json/json_reader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::Pointee;
using ::testing::UnorderedPointwise;

namespace network {

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
              Pointee(UnorderedPointwise(
                  Eq(), base::flat_map<std::string, std::string>({}))));
}

TEST(FirstPartySetParser, AcceptsMinimal) {
  const std::string input =
      R"( [ { "owner": "example.com", "members": ["aaaa"] } ] )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(
      FirstPartySetParser::ParsePreloadedSets(input),
      Pointee(UnorderedPointwise(Eq(), base::flat_map<std::string, std::string>(
                                           {{"aaaa", "example.com"}}))));
}

TEST(FirstPartySetParser, RejectsMissingOwner) {
  const std::string input = R"( [ { "members": ["aaaa"] } ] )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input),
              Pointee(UnorderedPointwise(
                  Eq(), base::flat_map<std::string, std::string>({}))));
}

TEST(FirstPartySetParser, RejectsTypeUnsafeOwner) {
  const std::string input = R"( [ { "owner": 3, "members": ["aaaa"] } ] )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input),
              Pointee(UnorderedPointwise(
                  Eq(), base::flat_map<std::string, std::string>({}))));
}

TEST(FirstPartySetParser, RejectsMissingMembers) {
  const std::string input = R"( [ { "owner": "example.com" } ] )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input),
              Pointee(UnorderedPointwise(
                  Eq(), base::flat_map<std::string, std::string>({}))));
}

TEST(FirstPartySetParser, RejectsTypeUnsafeMembers) {
  const std::string input =
      R"( [ { "owner": "example.com", "members": ["aaaa", 4] } ] )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(
      FirstPartySetParser::ParsePreloadedSets(input),
      Pointee(UnorderedPointwise(Eq(), base::flat_map<std::string, std::string>(
                                           {{"aaaa", "example.com"}}))));
}

TEST(FirstPartySetParser, AcceptsMultipleSets) {
  const std::string input = R"(
  [
    {
      "owner": "example.com",
      "members": ["member1"]
    },
    {
      "owner": "foo.com",
      "members": ["member2"]
    }
  ]
  )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input),
              Pointee(UnorderedPointwise(
                  Eq(), base::flat_map<std::string, std::string>({
                            {"member1", "example.com"},
                            {"member2", "foo.com"},
                        }))));
}

TEST(FirstPartySetParser, AllowsTrailingCommas) {
  const std::string input = R"(
  [
    {
      "owner": "example.com",
      "members": ["member1"],
    },
  ]
  )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(
      input, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input),
              Pointee(UnorderedPointwise(
                  Eq(), base::flat_map<std::string, std::string>({
                            {"member1", "example.com"},
                        }))));
}

TEST(FirstPartySetParser, IgnoresSubsequent_SameOwner) {
  const std::string input = R"(
  [
    {
      "owner": "example.com",
      "members": ["member1"]
    },
    {
      "owner": "example.com",
      "members": ["member2"]
    }
  ]
  )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input),
              Pointee(UnorderedPointwise(
                  Eq(), base::flat_map<std::string, std::string>({
                            {"member1", "example.com"},
                        }))));
}

TEST(FirstPartySetParser, IgnoresSubsequent_MemberAsOwner) {
  const std::string input = R"(
  [
    {
      "owner": "example.com",
      "members": ["member1"]
    },
    {
      "owner": "member1",
      "members": ["member2"]
    }
  ]
  )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input),
              Pointee(UnorderedPointwise(
                  Eq(), base::flat_map<std::string, std::string>({
                            {"member1", "example.com"},
                        }))));
}

TEST(FirstPartySetParser, IgnoresSubsequent_SameMember) {
  const std::string input = R"(
  [
    {
      "owner": "example.com",
      "members": ["member1"]
    },
    {
      "owner": "foo.com",
      "members": ["member1", "member2"]
    }
  ]
  )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input),
              Pointee(UnorderedPointwise(
                  Eq(), base::flat_map<std::string, std::string>({
                            {"member1", "example.com"},
                            {"member2", "foo.com"},
                        }))));
}

TEST(FirstPartySetParser, IgnoresSubsequent_OwnerAsMember) {
  const std::string input = R"(
  [
    {
      "owner": "example.com",
      "members": ["member1"]
    },
    {
      "owner": "example2.com",
      "members": ["example.com", "member2"]
    }
  ]
  )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParsePreloadedSets(input),
              Pointee(UnorderedPointwise(
                  Eq(), base::flat_map<std::string, std::string>({
                            {"member1", "example.com"},
                            {"member2", "example2.com"},
                        }))));
}

}  // namespace network

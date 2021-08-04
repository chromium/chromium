// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_set_parser.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace network {

MATCHER_P(SerializesTo, want, "") {
  const std::string got = arg.Serialize();
  return testing::ExplainMatchResult(testing::Eq(want), got, result_listener);
}

TEST(FirstPartySetParser, RejectsEmpty) {
  // If the input isn't valid JSON, we should
  // reject it. In particular, we should reject
  // empty input.

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(""),
              IsEmpty());
}

TEST(FirstPartySetParser, RejectsNonemptyMalformed) {
  // If the input isn't valid JSON, we should
  // reject it.
  const char input[] = "certainly not valid JSON";

  // Sanity check that the input is not valid JSON.
  ASSERT_FALSE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              IsEmpty());
}

TEST(FirstPartySetParser, RejectsNonListInput) {
  // The outermost value should be a list.
  const std::string input = "{}";
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              IsEmpty());
}

TEST(FirstPartySetParser, AcceptsTrivial) {
  const std::string input = "[]";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              IsEmpty());
}

TEST(FirstPartySetParser, RejectsSingletonSet) {
  const std::string input =
      R"([{
        "owner": "https://example.test",
        "members": []
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              IsEmpty());
}

TEST(FirstPartySetParser, AcceptsMinimal) {
  const std::string input =
      R"([{
        "owner": "https://example.test",
        "members": ["https://aaaa.test"]
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://aaaa.test"),
                                        SerializesTo("https://example.test"))));
}

TEST(FirstPartySetParser, RejectsMissingOwner) {
  const std::string input = R"( [ { "members": ["https://aaaa.test"] } ] )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              IsEmpty());
}

TEST(FirstPartySetParser, RejectsTypeUnsafeOwner) {
  const std::string input =
      R"( [ { "owner": 3, "members": ["https://aaaa.test"] } ] )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              IsEmpty());
}

TEST(FirstPartySetParser, RejectsNonHTTPSOwner) {
  const std::string input =
      R"([{
        "owner": "http://example.test",
        "members": ["https://aaaa.test"]
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              IsEmpty());
}

TEST(FirstPartySetParser, RejectsNonOriginOwner) {
  const std::string input =
      R"([{
        "owner": "example",
        "members": ["https://aaaa.test"]
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              IsEmpty());
}

TEST(FirstPartySetParser, RejectsOwnerWithoutRegisteredDomain) {
  const std::string input =
      R"([{
        "owner": "https://example.test..",
        "members": ["https://aaaa.test"]
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              IsEmpty());
}

TEST(FirstPartySetParser, RejectsMissingMembers) {
  const std::string input = R"( [ { "owner": "https://example.test" } ] )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              IsEmpty());
}

TEST(FirstPartySetParser, RejectsTypeUnsafeMembers) {
  const std::string input =
      R"([{
        "owner": "https://example.test",
        "members": ["https://aaaa.test", 4]
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              IsEmpty());
}

TEST(FirstPartySetParser, RejectsNonHTTPSMember) {
  const std::string input =
      R"([{
        "owner": "https://example.test",
        "members": ["http://aaaa.test"]
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              IsEmpty());
}

TEST(FirstPartySetParser, RejectsNonOriginMember) {
  const std::string input =
      R"([{
        "owner": "https://example.test",
        "members": ["aaaa"]
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              IsEmpty());
}

TEST(FirstPartySetParser, RejectsMemberWithoutRegisteredDomain) {
  const std::string input =
      R"([{
        "owner": "https://example.test",
        "members": ["https://aaaa.test.."]
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              IsEmpty());
}

TEST(FirstPartySetParser, TruncatesSubdomain_Owner) {
  const std::string input =
      R"([{
        "owner": "https://subdomain.example.test",
        "members": ["https://aaaa.test"]
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://aaaa.test"),
                                        SerializesTo("https://example.test"))));
}

TEST(FirstPartySetParser, TruncatesSubdomain_Member) {
  const std::string input =
      R"([{
        "owner": "https://example.test",
        "members": ["https://subdomain.aaaa.test"]
        }])";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://aaaa.test"),
                                        SerializesTo("https://example.test"))));
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

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://foo.test"),
                                        SerializesTo("https://foo.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://foo.test"))));
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

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              IsEmpty());
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

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              IsEmpty());
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

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test"))));
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

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              IsEmpty());
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

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              IsEmpty());
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

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              IsEmpty());
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

  EXPECT_THAT(FirstPartySetParser::ParseSetsFromComponentUpdater(input),
              IsEmpty());
}

TEST(FirstPartySetParser, SerializeFirstPartySets) {
  EXPECT_EQ(R"({"https://member1.test":"https://example1.test"})",
            FirstPartySetParser::SerializeFirstPartySets(
                {{net::SchemefulSite(GURL("https://member1.test")),
                  net::SchemefulSite(GURL("https://example1.test"))},
                 {net::SchemefulSite(GURL("https://example1.test")),
                  net::SchemefulSite(GURL("https://example1.test"))}}));
}

TEST(FirstPartySetParser, SerializeFirstPartySetsWithOpaqueOrigin) {
  EXPECT_EQ(R"({"https://member1.test":"null"})",
            FirstPartySetParser::SerializeFirstPartySets(
                {{net::SchemefulSite(GURL("https://member1.test")),
                  net::SchemefulSite(GURL(""))}}));
}

TEST(FirstPartySetParser, SerializeFirstPartySetsEmptySet) {
  EXPECT_EQ("{}", FirstPartySetParser::SerializeFirstPartySets({}));
}

TEST(FirstPartySetParser, DeserializeFirstPartySets) {
  const std::string input =
      R"({"https://member1.test":"https://example1.test",
          "https://member3.test":"https://example1.test",
          "https://member2.test":"https://example2.test"})";
  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(
      FirstPartySetParser::DeserializeFirstPartySets(input),
      UnorderedElementsAre(Pair(SerializesTo("https://member1.test"),
                                SerializesTo("https://example1.test")),
                           Pair(SerializesTo("https://member3.test"),
                                SerializesTo("https://example1.test")),
                           Pair(SerializesTo("https://example1.test"),
                                SerializesTo("https://example1.test")),
                           Pair(SerializesTo("https://member2.test"),
                                SerializesTo("https://example2.test")),
                           Pair(SerializesTo("https://example2.test"),
                                SerializesTo("https://example2.test"))));
}

TEST(FirstPartySetParser, DeserializeFirstPartySetsEmptySet) {
  EXPECT_THAT(FirstPartySetParser::DeserializeFirstPartySets("{}"), IsEmpty());
}

// Same member appear twice with different owner is not considered invalid
// content and wouldn't end up returning an empty map, since
// base::DictionaryValue automatically handles duplicated keys.
TEST(FirstPartySetParser, DeserializeFirstPartySetsDuplicatedKey) {
  const std::string input =
      R"({"https://member1.test":"https://example1.test",
          "https://member1.test":"https://example2.test"})";
  ASSERT_TRUE(base::JSONReader::Read(input));
  EXPECT_THAT(
      FirstPartySetParser::DeserializeFirstPartySets(input),
      UnorderedElementsAre(Pair(SerializesTo("https://member1.test"),
                                SerializesTo("https://example2.test")),
                           Pair(SerializesTo("https://example2.test"),
                                SerializesTo("https://example2.test"))));
}

// Singleton set is ignored.
TEST(FirstPartySetParser, DeserializeFirstPartySetsSingletonSet) {
  const std::string input =
      R"({"https://example1.test":"https://example1.test",
          "https://member1.test":"https://example2.test",
          "https://example2.test":"https://example2.test"})";
  ASSERT_TRUE(base::JSONReader::Read(input));
  EXPECT_THAT(
      FirstPartySetParser::DeserializeFirstPartySets(input),
      UnorderedElementsAre(Pair(SerializesTo("https://member1.test"),
                                SerializesTo("https://example2.test")),
                           Pair(SerializesTo("https://example2.test"),
                                SerializesTo("https://example2.test"))));
}

class FirstPartySetParserInvalidContentTest
    : public testing::Test,
      public testing::WithParamInterface<std::tuple<bool, std::string>> {
 public:
  FirstPartySetParserInvalidContentTest() {
    valid_json_ = std::get<0>(GetParam());
    input_ = std::get<1>(GetParam());
  }
  bool is_valid_json() { return valid_json_; }
  const std::string& input() { return input_; }

 private:
  bool valid_json_;
  std::string input_;
};

TEST_P(FirstPartySetParserInvalidContentTest, DeserializeFirstPartySets) {
  if (is_valid_json())
    ASSERT_TRUE(base::JSONReader::Read(input()));

  EXPECT_THAT(FirstPartySetParser::DeserializeFirstPartySets(input()),
              IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(
    InvalidContent,
    FirstPartySetParserInvalidContentTest,
    testing::Values(
        // The input is not valid JSON.
        std::make_tuple(false, "//"),
        // The serialized object is type of array.
        std::make_tuple(true,
                        R"(["https://member1.test","https://example1.test"])"),
        // The serialized string is type of map that contains non-URL key.
        std::make_tuple(true, R"({"member1":"https://example1.test"})"),
        // The serialized string is type of map that contains non-URL value.
        std::make_tuple(true, R"({"https://member1.test":"example1"})"),
        // The serialized string is type of map that contains opaque origin.
        std::make_tuple(true, R"({"https://member1.test":""})"),
        std::make_tuple(true, R"({"":"https://example1.test"})"),
        // The serialized string is type of map that contains non-string value.
        std::make_tuple(true, R"({"https://member1.test":1})"),
        // Nondisjoint set. The same site shows up both as member and owner.
        std::make_tuple(true,
                        R"({"https://member1.test":"https://example1.test",
            "https://member2.test":"https://member1.test"})"),
        std::make_tuple(true,
                        R"({"https://member1.test":"https://example1.test",
            "https://example1.test":"https://example2.test"})")));

}  // namespace network

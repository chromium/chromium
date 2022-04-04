// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_set_parser.h"

#include <sstream>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace content {

MATCHER_P(SerializesTo, want, "") {
  const std::string got = arg.Serialize();
  return testing::ExplainMatchResult(testing::Eq(want), got, result_listener);
}

base::flat_map<net::SchemefulSite, net::SchemefulSite> ParseSets(
    const std::string& sets) {
  std::istringstream stream(sets);
  return FirstPartySetParser::ParseSetsFromStream(stream);
}

TEST(FirstPartySetParser, RejectsNonemptyMalformed) {
  // If the input isn't valid JSON, we should
  // reject it.
  const std::string input = "certainly not valid JSON";

  ASSERT_FALSE(base::JSONReader::Read(input));
  std::istringstream stream(input);
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromStream(stream), IsEmpty());
}

TEST(FirstPartySetParser, AcceptsTrivial) {
  const std::string input = "";

  EXPECT_THAT(ParseSets(input), IsEmpty());
}

TEST(FirstPartySetParser, RejectsSingletonSet) {
  const std::string input =
      R"({"owner": "https://example.test", "members": []})";

  EXPECT_THAT(ParseSets(input), IsEmpty());
}

TEST(FirstPartySetParser, AcceptsMinimal) {
  const std::string input =
      R"({"owner": "https://example.test", "members": ["https://aaaa.test"]})";

  std::istringstream stream(input);
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromStream(stream),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://aaaa.test"),
                                        SerializesTo("https://example.test"))));
}

TEST(FirstPartySetParser, RejectsMissingOwner) {
  const std::string input = R"({"members": ["https://aaaa.test"]})";

  EXPECT_THAT(ParseSets(input), IsEmpty());
}

TEST(FirstPartySetParser, RejectsTypeUnsafeOwner) {
  const std::string input =
      R"({ "owner": 3, "members": ["https://aaaa.test"]})";

  EXPECT_THAT(ParseSets(input), IsEmpty());
}

TEST(FirstPartySetParser, RejectsNonHTTPSOwner) {
  const std::string input =
      R"({"owner": "http://example.test", "members": ["https://aaaa.test"]})";

  EXPECT_THAT(ParseSets(input), IsEmpty());
}

TEST(FirstPartySetParser, RejectsNonOriginOwner) {
  const std::string input =
      R"({"owner": "example", "members": ["https://aaaa.test"]})";

  EXPECT_THAT(ParseSets(input), IsEmpty());
}

TEST(FirstPartySetParser, SkipsSetOnNonOriginOwner) {
  const std::string input =
      R"({"owner": "example", "members": ["https://aaaa.test"]})"
      "\n"
      R"({"owner": "https://example2.test", "members": )"
      R"(["https://member2.test"]})"
      "\n"
      R"({"owner": "https://example.test", "members": ["https://aaaa.test"]})";

  EXPECT_THAT(ParseSets(input),
              UnorderedElementsAre(Pair(SerializesTo("https://example2.test"),
                                        SerializesTo("https://example2.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://example2.test")),
                                   Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://aaaa.test"),
                                        SerializesTo("https://example.test"))));
}

TEST(FirstPartySetParser, RejectsOwnerWithoutRegisteredDomain) {
  const std::string input = R"({"owner": "https://example.test..", )"
                            R"("members": ["https://aaaa.test"]})";

  EXPECT_THAT(ParseSets(input), IsEmpty());
}

TEST(FirstPartySetParser, RejectsMissingMembers) {
  const std::string input = R"({"owner": "https://example.test" })";

  EXPECT_THAT(ParseSets(input), IsEmpty());
}

TEST(FirstPartySetParser, RejectsTypeUnsafeMembers) {
  const std::string input = R"({"owner": "https://example.test", )"
                            R"("members": ["https://aaaa.test", 4]})";

  EXPECT_THAT(ParseSets(input), IsEmpty());
}

TEST(FirstPartySetParser, RejectsNonHTTPSMember) {
  const std::string input =
      R"({"owner": "https://example.test", "members": ["http://aaaa.test"]})";

  EXPECT_THAT(ParseSets(input), IsEmpty());
}

TEST(FirstPartySetParser, RejectsNonOriginMember) {
  const std::string input =
      R"({"owner": "https://example.test", "members": ["aaaa"]})";

  EXPECT_THAT(ParseSets(input), IsEmpty());
}

TEST(FirstPartySetParser, SkipsSetOnNonOriginMember) {
  const std::string input =
      R"({"owner": "https://example.test", "members": ["aaaa"]})"
      "\n"
      R"({"owner": "https://example2.test", "members": )"
      R"(["https://member2.test"]})"
      "\n"
      R"({"owner": "https://example.test", "members": )"
      R"(["https://member3.test"]})";

  EXPECT_THAT(ParseSets(input),
              UnorderedElementsAre(Pair(SerializesTo("https://example2.test"),
                                        SerializesTo("https://example2.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://example2.test")),
                                   Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member3.test"),
                                        SerializesTo("https://example.test"))));
}

TEST(FirstPartySetParser, RejectsMemberWithoutRegisteredDomain) {
  const std::string input = R"({"owner": "https://example.test", )"
                            R"("members": ["https://aaaa.test.."]})";

  EXPECT_THAT(ParseSets(input), IsEmpty());
}

TEST(FirstPartySetParser, TruncatesSubdomain_Owner) {
  const std::string input = R"({"owner": "https://subdomain.example.test", )"
                            R"("members": ["https://aaaa.test"]})";

  EXPECT_THAT(ParseSets(input),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://aaaa.test"),
                                        SerializesTo("https://example.test"))));
}

TEST(FirstPartySetParser, TruncatesSubdomain_Member) {
  const std::string input = R"({"owner": "https://example.test", )"
                            R"("members": ["https://subdomain.aaaa.test"]})";

  EXPECT_THAT(ParseSets(input),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://aaaa.test"),
                                        SerializesTo("https://example.test"))));
}

TEST(FirstPartySetParser, AcceptsMultipleSets) {
  const std::string input =
      "{\"owner\": \"https://example.test\", \"members\": "
      "[\"https://member1.test\"]}\n"
      "{\"owner\": \"https://foo.test\", \"members\": "
      "[\"https://member2.test\"]}";

  std::istringstream stream(input);
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromStream(stream),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://foo.test"),
                                        SerializesTo("https://foo.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://foo.test"))));
}

TEST(FirstPartySetParser, AcceptsMultipleSetsWithWhitespace) {
  // Note the leading blank line, middle blank line, trailing blank line, and
  // leading whitespace on each line.
  const std::string input = R"(
      {"owner": "https://example.test", "members": ["https://member1.test"]}

      {"owner": "https://foo.test", "members": ["https://member2.test"]}
    )";

  std::istringstream stream(input);
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromStream(stream),
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
  const std::string input = R"({"owner": 3, "members": ["https://member1.test"]}
    {"owner": "https://foo.test", "members": ["https://member2.test"]})";

  EXPECT_THAT(ParseSets(input), IsEmpty());
}

TEST(FirstPartySetParser, RejectsInvalidSets_InvalidMember) {
  const std::string input = R"({"owner": "https://example.test", "members": [3]}
    {"owner": "https://foo.test", "members": ["https://member2.test"]})";

  EXPECT_THAT(ParseSets(input), IsEmpty());
}

TEST(FirstPartySetParser, AllowsTrailingCommas) {
  const std::string input = R"({"owner": "https://example.test", )"
                            R"("members": ["https://member1.test"],})";

  EXPECT_THAT(ParseSets(input),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test"))));
}

TEST(FirstPartySetParser, Rejects_SameOwner) {
  const std::string input =
      R"({"owner": "https://example.test", "members": ["https://member1.test"]}
    {"owner": "https://example.test", "members": ["https://member2.test"]})";

  EXPECT_THAT(ParseSets(input), IsEmpty());
}

TEST(FirstPartySetParser, Rejects_MemberAsOwner) {
  const std::string input =
      R"({"owner": "https://example.test", "members": ["https://member1.test"]}
    {"owner": "https://member1.test", "members": ["https://member2.test"]})";

  EXPECT_THAT(ParseSets(input), IsEmpty());
}

TEST(FirstPartySetParser, Rejects_SameMember) {
  const std::string input =
      R"({"owner": "https://example.test", "members": ["https://member1.test"]}
    {"owner": "https://foo.test", "members": )"
      R"(["https://member1.test", "https://member2.test"]})";

  EXPECT_THAT(ParseSets(input), IsEmpty());
}

TEST(FirstPartySetParser, Rejects_OwnerAsMember) {
  const std::string input =
      R"({"owner": "https://example.test", "members": ["https://member1.test"]}
    {"owner": "https://example2.test", )"
      R"("members": ["https://example.test", "https://member2.test"]})";

  EXPECT_THAT(ParseSets(input), IsEmpty());
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

TEST(ParseSetsFromEnterprisePolicyTest, Accepts_MissingSetLists) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
              }
            )")
                                 .value();
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), &out_sets),
              Eq(absl::nullopt));
  EXPECT_THAT(out_sets, FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(ParseSetsFromEnterprisePolicyTest, Accepts_EmptyLists) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
                "replacements": [],
                "additions": []
              }
            )")
                                 .value();
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), &out_sets),
              Eq(absl::nullopt));
  EXPECT_THAT(out_sets, FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(ParseSetsFromEnterprisePolicyTest, ValidPolicy_NullOutParam) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
                "replacements": [],
                "additions": []
              }
            )")
                                 .value();
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), nullptr),
              Eq(absl::nullopt));
}

TEST(ParseSetsFromEnterprisePolicyTest, InvalidPolicy_NullOutParam) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
                "replacements": [],
                "additions": [
                  {
                    "owner": "https://owner1.test",
                    "members": ["https://owner1.test"]
                  }
                ]
              }
            )")
                                 .value();
  FirstPartySetParser::PolicyParsingError expected_error{
      FirstPartySetParser::ParseError::kRepeatedDomain,
      FirstPartySetParser::PolicySetType::kAddition, 0};
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), nullptr),
              expected_error);
}

TEST(ParseSetsFromEnterprisePolicyTest, InvalidTypeError_MissingOwner) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
                "replacements": [
                  {
                    "members": ["https://member1.test"]
                  }
                ],
                "additions": []
              }
            )")
                                 .value();
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), &out_sets),
              FirstPartySetParser::PolicyParsingError(
                  {FirstPartySetParser::ParseError::kInvalidType,
                   FirstPartySetParser::PolicySetType::kReplacement, 0}));
  EXPECT_THAT(out_sets, FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(ParseSetsFromEnterprisePolicyTest, InvalidTypeError_MissingMembers) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
                "replacements": [
                  {
                    "owner": "https://owner1.test"
                  }
                ],
                "additions": []
              }
            )")
                                 .value();
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), &out_sets),
              FirstPartySetParser::PolicyParsingError(
                  {FirstPartySetParser::ParseError::kInvalidType,
                   FirstPartySetParser::PolicySetType::kReplacement, 0}));
  EXPECT_THAT(out_sets, FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(ParseSetsFromEnterprisePolicyTest, InvalidTypeError_WrongOwnerType) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
                "replacements": [
                  {
                    "owner": 123,
                    "members": ["https://member1.test"]
                  }
                ],
                "additions": []
              }
            )")
                                 .value();
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), &out_sets),
              FirstPartySetParser::PolicyParsingError(
                  {FirstPartySetParser::ParseError::kInvalidType,
                   FirstPartySetParser::PolicySetType::kReplacement, 0}));
  EXPECT_THAT(out_sets, FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(ParseSetsFromEnterprisePolicyTest,
     InvalidTypeError_WrongMembersFieldType) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
                "replacements": [
                  {
                    "owner": "https://owner1.test",
                    "members": 123
                  }
                ],
                "additions": []
              }
            )")
                                 .value();
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), &out_sets),
              FirstPartySetParser::PolicyParsingError(
                  {FirstPartySetParser::ParseError::kInvalidType,
                   FirstPartySetParser::PolicySetType::kReplacement, 0}));
  EXPECT_THAT(out_sets, FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(ParseSetsFromEnterprisePolicyTest, InvalidTypeError_WrongMemberType) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
          "replacements": [
            {
              "owner": "https://owner1.test",
              "members": ["https://member1.test", 123,
              "https://member2.test"]
            }
          ],
          "additions": []
        }
            )")
                                 .value();
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), &out_sets),
              FirstPartySetParser::PolicyParsingError(
                  {FirstPartySetParser::ParseError::kInvalidType,
                   FirstPartySetParser::PolicySetType::kReplacement, 0}));
  EXPECT_THAT(out_sets, FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(ParseSetsFromEnterprisePolicyTest, InvalidOriginError_OwnerOpaque) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
                "replacements": [
                  {
                    "owner": "",
                    "members": ["https://member1.test"]
                  }
                ],
                "additions": []
              }
            )")
                                 .value();
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), &out_sets),
              FirstPartySetParser::PolicyParsingError(
                  {FirstPartySetParser::ParseError::kInvalidOrigin,
                   FirstPartySetParser::PolicySetType::kReplacement, 0}));
  EXPECT_THAT(out_sets, FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(ParseSetsFromEnterprisePolicyTest, InvalidOriginError_MemberOpaque) {
  base::Value policy_value = base::JSONReader::Read(R"(
               {
                "replacements": [
                  {
                    "owner": "https://owner1.test",
                    "members": [""]
                  }
                ],
                "additions": []
              }
            )")
                                 .value();
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), &out_sets),
              FirstPartySetParser::PolicyParsingError(
                  {FirstPartySetParser::ParseError::kInvalidOrigin,
                   FirstPartySetParser::PolicySetType::kReplacement, 0}));
  EXPECT_THAT(out_sets, FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(ParseSetsFromEnterprisePolicyTest, InvalidOriginError_OwnerNonHttps) {
  base::Value policy_value = base::JSONReader::Read(R"(
                 {
                "replacements": [
                  {
                    "owner": "http://owner1.test",
                    "members": ["https://member1.test"]
                  }
                ],
                "additions": []
              }
            )")
                                 .value();
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), &out_sets),
              FirstPartySetParser::PolicyParsingError(
                  {FirstPartySetParser::ParseError::kInvalidOrigin,
                   FirstPartySetParser::PolicySetType::kReplacement, 0}));
  EXPECT_THAT(out_sets, FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(ParseSetsFromEnterprisePolicyTest, InvalidOriginError_MemberNonHttps) {
  base::Value policy_value = base::JSONReader::Read(R"(
               {
                "replacements": [
                  {
                    "owner": "https://owner1.test",
                    "members": ["http://member1.test"]
                  }
                ],
                "additions": []
              }
            )")
                                 .value();
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), &out_sets),
              FirstPartySetParser::PolicyParsingError(
                  {FirstPartySetParser::ParseError::kInvalidOrigin,
                   FirstPartySetParser::PolicySetType::kReplacement, 0}));
  EXPECT_THAT(out_sets, FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(ParseSetsFromEnterprisePolicyTest,
     InvalidOriginError_OwnerNonRegisteredDomain) {
  base::Value policy_value = base::JSONReader::Read(R"(
                {
                "replacements": [
                  {
                    "owner": "https://owner1.test..",
                    "members": ["https://member1.test"]
                  }
                ],
                "additions": []
              }
            )")
                                 .value();
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), &out_sets),
              FirstPartySetParser::PolicyParsingError(
                  {FirstPartySetParser::ParseError::kInvalidOrigin,
                   FirstPartySetParser::PolicySetType::kReplacement, 0}));
  EXPECT_THAT(out_sets, FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(ParseSetsFromEnterprisePolicyTest,
     InvalidOriginError_MemberNonRegisteredDomain) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
                "replacements": [
                  {
                    "owner": "https://owner1.test",
                    "members": ["https://member1.test.."]
                  }
                ],
                "additions": []
              }
            )")
                                 .value();
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), &out_sets),
              FirstPartySetParser::PolicyParsingError(
                  {FirstPartySetParser::ParseError::kInvalidOrigin,
                   FirstPartySetParser::PolicySetType::kReplacement, 0}));
  EXPECT_THAT(out_sets, FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(ParseSetsFromEnterprisePolicyTest, SingletonSetError_EmptyMembers) {
  base::Value policy_value = base::JSONReader::Read(R"(
             {
                "replacements": [
                  {
                    "owner": "https://owner1.test",
                    "members": []
                  }
                ],
                "additions": []
              }
            )")
                                 .value();
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), &out_sets),
              FirstPartySetParser::PolicyParsingError(
                  {FirstPartySetParser::ParseError::kSingletonSet,
                   FirstPartySetParser::PolicySetType::kReplacement, 0}));
  EXPECT_THAT(out_sets, FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(ParseSetsFromEnterprisePolicyTest,
     RepeatedDomainError_WithinReplacements) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
                "replacements": [
                  {
                    "owner": "https://owner1.test",
                    "members": ["https://owner1.test"]
                  }
                ],
                "additions": []
              }
            )")
                                 .value();
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), &out_sets),
              FirstPartySetParser::PolicyParsingError(
                  {FirstPartySetParser::ParseError::kRepeatedDomain,
                   FirstPartySetParser::PolicySetType::kReplacement, 0}));
  EXPECT_THAT(out_sets, FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(ParseSetsFromEnterprisePolicyTest, NonDisjointError_WithinReplacements) {
  base::Value policy_value = base::JSONReader::Read(R"(
                   {
                "replacements": [
                  {
                    "owner": "https://owner1.test",
                    "members": ["https://member1.test"]
                  },
                  {
                    "owner": "https://owner2.test",
                    "members": ["https://member1.test"]
                  }
                ],
                "additions": []
              }
            )")
                                 .value();
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), &out_sets),
              FirstPartySetParser::PolicyParsingError(
                  {FirstPartySetParser::ParseError::kNonDisjointSets,
                   FirstPartySetParser::PolicySetType::kReplacement, 1}));
  EXPECT_THAT(out_sets, FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(ParseSetsFromEnterprisePolicyTest, NonDisjointError_WithinAdditions) {
  base::Value policy_value = base::JSONReader::Read(R"(
                   {
                "replacements": [],
                "additions": [
                  {
                    "owner": "https://owner1.test",
                    "members": ["https://member1.test"]
                  },
                  {
                    "owner": "https://owner2.test",
                    "members": ["https://member1.test"]
                  }
                ]
              }
            )")
                                 .value();
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), &out_sets),
              FirstPartySetParser::PolicyParsingError(
                  {FirstPartySetParser::ParseError::kNonDisjointSets,
                   FirstPartySetParser::PolicySetType::kAddition, 1}));
  EXPECT_THAT(out_sets, FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(ParseSetsFromEnterprisePolicyTest, NonDisjointError_AcrossBothLists) {
  base::Value policy_value = base::JSONReader::Read(R"(
               {
                "replacements": [
                  {
                    "owner": "https://owner1.test",
                    "members": ["https://member1.test"]
                  }
                ],
                "additions": [
                  {
                    "owner": "https://owner2.test",
                    "members": ["https://member1.test"]
                  }
                ]
              }
            )")
                                 .value();
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), &out_sets),
              FirstPartySetParser::PolicyParsingError(
                  {FirstPartySetParser::ParseError::kNonDisjointSets,
                   FirstPartySetParser::PolicySetType::kAddition, 0}));
  EXPECT_THAT(out_sets, FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(ParseSetsFromEnterprisePolicyTest, SuccessfulMapping_SameList) {
  base::Value policy_value = base::JSONReader::Read(R"(
             {
                "replacements": [
                  {
                    "owner": "https://owner1.test",
                    "members": ["https://member1.test"]
                  },
                  {
                    "owner": "https://owner2.test",
                    "members": ["https://member2.test"]
                  }
                ]
              }
            )")
                                 .value();
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), &out_sets),
              Eq(absl::nullopt));
  EXPECT_THAT(
      out_sets.replacements,
      ElementsAre(
          Pair(SerializesTo("https://owner1.test"),
               UnorderedElementsAre(SerializesTo("https://member1.test"))),
          Pair(SerializesTo("https://owner2.test"),
               UnorderedElementsAre(SerializesTo("https://member2.test")))));
  EXPECT_THAT(out_sets.additions, IsEmpty());
}

TEST(ParseSetsFromEnterprisePolicyTest, SuccessfulMapping_CrossList) {
  base::Value policy_value = base::JSONReader::Read(R"(
                {
                "replacements": [
                  {
                    "owner": "https://owner1.test",
                    "members": ["https://member1.test"]
                  },
                  {
                    "owner": "https://owner2.test",
                    "members": ["https://member2.test"]
                  }
                ],
                "additions": [
                  {
                    "owner": "https://owner3.test",
                    "members": ["https://member3.test"]
                  }
                ]
              }
            )")
                                 .value();
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromEnterprisePolicy(
                  policy_value.GetDict(), &out_sets),
              Eq(absl::nullopt));
  EXPECT_THAT(
      out_sets.replacements,
      ElementsAre(
          Pair(SerializesTo("https://owner1.test"),
               UnorderedElementsAre(SerializesTo("https://member1.test"))),
          Pair(SerializesTo("https://owner2.test"),
               UnorderedElementsAre(SerializesTo("https://member2.test")))));

  EXPECT_THAT(out_sets.additions,
              ElementsAre(Pair(
                  SerializesTo("https://owner3.test"),
                  UnorderedElementsAre(SerializesTo("https://member3.test")))));
}

}  // namespace content

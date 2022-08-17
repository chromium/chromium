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
#include "net/cookies/first_party_set_entry.h"
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

FirstPartySetParser::SetsAndAliases ParseSets(const std::string& sets) {
  std::istringstream stream(sets);
  return FirstPartySetParser::ParseSetsFromStream(stream);
}

TEST(FirstPartySetParser, RejectsNonemptyMalformed) {
  // If the input isn't valid JSON, we should
  // reject it.
  const std::string input = "certainly not valid JSON";

  ASSERT_FALSE(base::JSONReader::Read(input));
  std::istringstream stream(input);
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromStream(stream),
              Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, AcceptsTrivial) {
  const std::string input = "";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsSingletonSet) {
  const std::string input =
      R"({"owner": "https://example.test", "members": []})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, AcceptsMinimal) {
  const std::string input =
      R"({"owner": "https://example.test", "members": ["https://aaaa.test"]})";

  std::istringstream stream(input);
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromStream(stream),
      Pair(UnorderedElementsAre(
               Pair(SerializesTo("https://example.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://aaaa.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0))),
           IsEmpty()));
}

TEST(FirstPartySetParser, RejectsMissingOwner) {
  const std::string input = R"({"members": ["https://aaaa.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsTypeUnsafeOwner) {
  const std::string input =
      R"({ "owner": 3, "members": ["https://aaaa.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsNonHTTPSOwner) {
  const std::string input =
      R"({"owner": "http://example.test", "members": ["https://aaaa.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsNonOriginOwner) {
  const std::string input =
      R"({"owner": "example", "members": ["https://aaaa.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, SkipsSetOnNonOriginOwner) {
  const std::string input =
      R"({"owner": "example", "members": ["https://aaaa.test"]})"
      "\n"
      R"({"owner": "https://example2.test", "members": )"
      R"(["https://member2.test"]})"
      "\n"
      R"({"owner": "https://example.test", "members": ["https://aaaa.test"]})";

  EXPECT_THAT(
      ParseSets(input),
      Pair(UnorderedElementsAre(
               Pair(SerializesTo("https://example2.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example2.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://member2.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example2.test")),
                        net::SiteType::kAssociated, 0)),
               Pair(SerializesTo("https://example.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://aaaa.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0))),
           IsEmpty()));
}

TEST(FirstPartySetParser, RejectsOwnerWithoutRegisteredDomain) {
  const std::string input = R"({"owner": "https://example.test..", )"
                            R"("members": ["https://aaaa.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsMissingMembers) {
  const std::string input = R"({"owner": "https://example.test" })";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsTypeUnsafeMembers) {
  const std::string input = R"({"owner": "https://example.test", )"
                            R"("members": ["https://aaaa.test", 4]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsNonHTTPSMember) {
  const std::string input =
      R"({"owner": "https://example.test", "members": ["http://aaaa.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsNonOriginMember) {
  const std::string input =
      R"({"owner": "https://example.test", "members": ["aaaa"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
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

  EXPECT_THAT(
      ParseSets(input),
      Pair(UnorderedElementsAre(
               Pair(SerializesTo("https://example2.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example2.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://member2.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example2.test")),
                        net::SiteType::kAssociated, 0)),
               Pair(SerializesTo("https://example.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://member3.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0))),
           IsEmpty()));
}

TEST(FirstPartySetParser, RejectsMemberWithoutRegisteredDomain) {
  const std::string input = R"({"owner": "https://example.test", )"
                            R"("members": ["https://aaaa.test.."]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, TruncatesSubdomain_Owner) {
  const std::string input = R"({"owner": "https://subdomain.example.test", )"
                            R"("members": ["https://aaaa.test"]})";

  EXPECT_THAT(
      ParseSets(input),
      Pair(UnorderedElementsAre(
               Pair(SerializesTo("https://example.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://aaaa.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0))),
           IsEmpty()));
}

TEST(FirstPartySetParser, TruncatesSubdomain_Member) {
  const std::string input = R"({"owner": "https://example.test", )"
                            R"("members": ["https://subdomain.aaaa.test"]})";

  EXPECT_THAT(
      ParseSets(input),
      Pair(UnorderedElementsAre(
               Pair(SerializesTo("https://example.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://aaaa.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0))),
           IsEmpty()));
}

TEST(FirstPartySetParser, AcceptsMultipleSets) {
  const std::string input =
      "{\"owner\": \"https://example.test\", \"members\": "
      "[\"https://member1.test\"]}\n"
      "{\"owner\": \"https://foo.test\", \"members\": "
      "[\"https://member2.test\"]}";

  std::istringstream stream(input);
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromStream(stream),
      Pair(UnorderedElementsAre(
               Pair(SerializesTo("https://example.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://member1.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0)),
               Pair(SerializesTo("https://foo.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://member2.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kAssociated, 0))),
           IsEmpty()));
}

TEST(FirstPartySetParser, AcceptsMultipleSetsWithWhitespace) {
  // Note the leading blank line, middle blank line, trailing blank line, and
  // leading whitespace on each line.
  const std::string input = R"(
      {"owner": "https://example.test", "members": ["https://member1.test"]}

      {"owner": "https://foo.test", "members": ["https://member2.test"]}
    )";

  std::istringstream stream(input);
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromStream(stream),
      Pair(UnorderedElementsAre(
               Pair(SerializesTo("https://example.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://member1.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0)),
               Pair(SerializesTo("https://foo.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://member2.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kAssociated, 0))),
           IsEmpty()));
}

TEST(FirstPartySetParser, RejectsInvalidSets_InvalidOwner) {
  const std::string input = R"({"owner": 3, "members": ["https://member1.test"]}
    {"owner": "https://foo.test", "members": ["https://member2.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsInvalidSets_InvalidMember) {
  const std::string input = R"({"owner": "https://example.test", "members": [3]}
    {"owner": "https://foo.test", "members": ["https://member2.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, AllowsTrailingCommas) {
  const std::string input = R"({"owner": "https://example.test", )"
                            R"("members": ["https://member1.test"],})";

  EXPECT_THAT(
      ParseSets(input),
      Pair(UnorderedElementsAre(
               Pair(SerializesTo("https://example.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://member1.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0))),
           IsEmpty()));
}

TEST(FirstPartySetParser, Rejects_SameOwner) {
  const std::string input =
      R"({"owner": "https://example.test", "members": ["https://member1.test"]}
    {"owner": "https://example.test", "members": ["https://member2.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, Rejects_MemberAsOwner) {
  const std::string input =
      R"({"owner": "https://example.test", "members": ["https://member1.test"]}
    {"owner": "https://member1.test", "members": ["https://member2.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, Rejects_SameMember) {
  const std::string input =
      R"({"owner": "https://example.test", "members": ["https://member1.test"]}
    {"owner": "https://foo.test", "members": )"
      R"(["https://member1.test", "https://member2.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, Rejects_OwnerAsMember) {
  const std::string input =
      R"({"owner": "https://example.test", "members": ["https://member1.test"]}
    {"owner": "https://example2.test", )"
      R"("members": ["https://example.test", "https://member2.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, Accepts_ccTLDAliases) {
  const std::string input =
      "{"                                         //
      "\"owner\": \"https://example.test\","      //
      "\"members\": [\"https://member1.test\"],"  //
      "\"ccTLDs\": {"                             //
      "\"https://member1.test\": [\"https://member1.cctld1\", "
      "\"https://member1.cctld2\"],"                                    //
      "\"https://not_in_set.test\": [\"https://not_in_set.cctld\"],"    //
      "\"https://example.test\": \"https://not_a_list.test\""           //
      "}"                                                               //
      "}\n"                                                             //
      "{"                                                               //
      "\"owner\": \"https://foo.test\","                                //
      "\"members\": [\"https://member2.test\"],"                        //
      "\"ccTLDs\": {"                                                   //
      "\"https://foo.test\": [\"https://foo.cctld\"],"                  //
      "\"https://member2.test\": [\"https://different_prefix.cctld\"]"  //
      "}"                                                               //
      "}";

  std::istringstream stream(input);
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromStream(stream),
      Pair(UnorderedElementsAre(
               Pair(SerializesTo("https://example.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://member1.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0)),
               Pair(SerializesTo("https://foo.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://member2.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kAssociated, 0))),
           UnorderedElementsAre(Pair(SerializesTo("https://member1.cctld1"),
                                     SerializesTo("https://member1.test")),
                                Pair(SerializesTo("https://member1.cctld2"),
                                     SerializesTo("https://member1.test")),
                                Pair(SerializesTo("https://foo.cctld"),
                                     SerializesTo("https://foo.test")))));
}

TEST(FirstPartySetParser, Rejects_NonSchemefulSiteCcTLDAliases) {
  const std::string input =
      "{"                                               //
      "\"owner\": \"https://example.test\","            //
      "\"members\": [\"https://member1.test\"],"        //
      "\"ccTLDs\": {"                                   //
      "\"https://member1.test\": [\"member1.cctld1\"]"  //
      "}"                                               //
      "}";

  std::istringstream stream(input);
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromStream(stream),
              Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, SerializeFirstPartySets) {
  EXPECT_EQ(R"({"https://member1.test":"https://example1.test"})",
            FirstPartySetParser::SerializeFirstPartySets(
                {{net::SchemefulSite(GURL("https://member1.test")),
                  net::FirstPartySetEntry(
                      net::SchemefulSite(GURL("https://example1.test")),
                      net::SiteType::kAssociated, 0)},
                 {net::SchemefulSite(GURL("https://example1.test")),
                  net::FirstPartySetEntry(
                      net::SchemefulSite(GURL("https://example1.test")),
                      net::SiteType::kPrimary, absl::nullopt)}}));
}

TEST(FirstPartySetParser, SerializeFirstPartySetsWithOpaqueOrigin) {
  EXPECT_EQ(
      R"({"https://member1.test":"null"})",
      FirstPartySetParser::SerializeFirstPartySets(
          {{net::SchemefulSite(GURL("https://member1.test")),
            net::FirstPartySetEntry(net::SchemefulSite(GURL("")),
                                    net::SiteType::kPrimary, absl::nullopt)}}));
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

  EXPECT_THAT(FirstPartySetParser::DeserializeFirstPartySets(input),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member1.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example1.test")),
                           net::SiteType::kAssociated, absl::nullopt)),
                  Pair(SerializesTo("https://member3.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example1.test")),
                           net::SiteType::kAssociated, absl::nullopt)),
                  Pair(SerializesTo("https://example1.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example1.test")),
                           net::SiteType::kPrimary, absl::nullopt)),
                  Pair(SerializesTo("https://member2.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example2.test")),
                           net::SiteType::kAssociated, absl::nullopt)),
                  Pair(SerializesTo("https://example2.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example2.test")),
                           net::SiteType::kPrimary, absl::nullopt))));
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
  EXPECT_THAT(FirstPartySetParser::DeserializeFirstPartySets(input),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member1.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example2.test")),
                           net::SiteType::kAssociated, absl::nullopt)),
                  Pair(SerializesTo("https://example2.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example2.test")),
                           net::SiteType::kPrimary, absl::nullopt))));
}

// Singleton set is ignored.
TEST(FirstPartySetParser, DeserializeFirstPartySetsSingletonSet) {
  const std::string input =
      R"({"https://example1.test":"https://example1.test",
          "https://member1.test":"https://example2.test",
          "https://example2.test":"https://example2.test"})";
  ASSERT_TRUE(base::JSONReader::Read(input));
  EXPECT_THAT(FirstPartySetParser::DeserializeFirstPartySets(input),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member1.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example2.test")),
                           net::SiteType::kAssociated, absl::nullopt)),
                  Pair(SerializesTo("https://example2.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example2.test")),
                           net::SiteType::kPrimary, absl::nullopt))));
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

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
     Accepts_MissingSetLists) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
              }
            )")
                                 .value();
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .value(),
      FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest, Accepts_EmptyLists) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
                "replacements": [],
                "additions": []
              }
            )")
                                 .value();
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .value(),
      FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
     InvalidTypeError_MissingOwner) {
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
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .error(),
      FirstPartySetParser::PolicyParsingError(
          {FirstPartySetParser::ParseError::kInvalidType,
           FirstPartySetParser::PolicySetType::kReplacement, 0}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
     InvalidTypeError_MissingMembers) {
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
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .error(),
      FirstPartySetParser::PolicyParsingError(
          {FirstPartySetParser::ParseError::kInvalidType,
           FirstPartySetParser::PolicySetType::kReplacement, 0}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
     InvalidTypeError_WrongOwnerType) {
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
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .error(),
      FirstPartySetParser::PolicyParsingError(
          {FirstPartySetParser::ParseError::kInvalidType,
           FirstPartySetParser::PolicySetType::kReplacement, 0}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
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
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .error(),
      FirstPartySetParser::PolicyParsingError(
          {FirstPartySetParser::ParseError::kInvalidType,
           FirstPartySetParser::PolicySetType::kReplacement, 0}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
     InvalidTypeError_WrongMemberType) {
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
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .error(),
      FirstPartySetParser::PolicyParsingError(
          {FirstPartySetParser::ParseError::kInvalidType,
           FirstPartySetParser::PolicySetType::kReplacement, 0}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
     InvalidOriginError_OwnerOpaque) {
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
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .error(),
      FirstPartySetParser::PolicyParsingError(
          {FirstPartySetParser::ParseError::kInvalidOrigin,
           FirstPartySetParser::PolicySetType::kReplacement, 0}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
     InvalidOriginError_MemberOpaque) {
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
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .error(),
      FirstPartySetParser::PolicyParsingError(
          {FirstPartySetParser::ParseError::kInvalidOrigin,
           FirstPartySetParser::PolicySetType::kReplacement, 0}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
     InvalidOriginError_OwnerNonHttps) {
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
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .error(),
      FirstPartySetParser::PolicyParsingError(
          {FirstPartySetParser::ParseError::kInvalidOrigin,
           FirstPartySetParser::PolicySetType::kReplacement, 0}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
     InvalidOriginError_MemberNonHttps) {
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
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .error(),
      FirstPartySetParser::PolicyParsingError(
          {FirstPartySetParser::ParseError::kInvalidOrigin,
           FirstPartySetParser::PolicySetType::kReplacement, 0}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
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
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .error(),
      FirstPartySetParser::PolicyParsingError(
          {FirstPartySetParser::ParseError::kInvalidOrigin,
           FirstPartySetParser::PolicySetType::kReplacement, 0}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
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
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .error(),
      FirstPartySetParser::PolicyParsingError(
          {FirstPartySetParser::ParseError::kInvalidOrigin,
           FirstPartySetParser::PolicySetType::kReplacement, 0}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
     SingletonSetError_EmptyMembers) {
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
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .error(),
      FirstPartySetParser::PolicyParsingError(
          {FirstPartySetParser::ParseError::kSingletonSet,
           FirstPartySetParser::PolicySetType::kReplacement, 0}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
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
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .error(),
      FirstPartySetParser::PolicyParsingError(
          {FirstPartySetParser::ParseError::kRepeatedDomain,
           FirstPartySetParser::PolicySetType::kReplacement, 0}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
     NonDisjointError_WithinReplacements) {
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
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .error(),
      FirstPartySetParser::PolicyParsingError(
          {FirstPartySetParser::ParseError::kNonDisjointSets,
           FirstPartySetParser::PolicySetType::kReplacement, 1}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
     NonDisjointError_WithinAdditions) {
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
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .error(),
      FirstPartySetParser::PolicyParsingError(
          {FirstPartySetParser::ParseError::kNonDisjointSets,
           FirstPartySetParser::PolicySetType::kAddition, 1}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
     NonDisjointError_AcrossBothLists) {
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
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .error(),
      FirstPartySetParser::PolicyParsingError(
          {FirstPartySetParser::ParseError::kNonDisjointSets,
           FirstPartySetParser::PolicySetType::kAddition, 0}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
     SuccessfulMapping_SameList) {
  net::SchemefulSite owner1(GURL("https://owner1.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));
  net::SchemefulSite owner2(GURL("https://owner2.test"));
  net::SchemefulSite member2(GURL("https://member2.test"));

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
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .value(),
      FirstPartySetParser::ParsedPolicySetLists(
          {FirstPartySetParser::SetsMap({
               {owner1, net::FirstPartySetEntry(owner1, net::SiteType::kPrimary,
                                                absl::nullopt)},
               {member1,
                net::FirstPartySetEntry(owner1, net::SiteType::kAssociated,
                                        absl::nullopt)},
           }),
           FirstPartySetParser::SetsMap({
               {owner2, net::FirstPartySetEntry(owner2, net::SiteType::kPrimary,
                                                absl::nullopt)},
               {member2,
                net::FirstPartySetEntry(owner2, net::SiteType::kAssociated,
                                        absl::nullopt)},
           })},
          {}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
     SuccessfulMapping_CrossList) {
  net::SchemefulSite owner1(GURL("https://owner1.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));
  net::SchemefulSite owner2(GURL("https://owner2.test"));
  net::SchemefulSite member2(GURL("https://member2.test"));
  net::SchemefulSite owner3(GURL("https://owner3.test"));
  net::SchemefulSite member3(GURL("https://member3.test"));

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
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .value(),
      FirstPartySetParser::ParsedPolicySetLists(
          {FirstPartySetParser::SetsMap({
               {owner1, net::FirstPartySetEntry(owner1, net::SiteType::kPrimary,
                                                absl::nullopt)},
               {member1,
                net::FirstPartySetEntry(owner1, net::SiteType::kAssociated,
                                        absl::nullopt)},
           }),
           FirstPartySetParser::SetsMap({
               {owner2, net::FirstPartySetEntry(owner2, net::SiteType::kPrimary,
                                                absl::nullopt)},
               {member2,
                net::FirstPartySetEntry(owner2, net::SiteType::kAssociated,
                                        absl::nullopt)},
           })},
          {FirstPartySetParser::SetsMap({
              {owner3, net::FirstPartySetEntry(owner3, net::SiteType::kPrimary,
                                               absl::nullopt)},
              {member3, net::FirstPartySetEntry(
                            owner3, net::SiteType::kAssociated, absl::nullopt)},
          })}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
     SuccessfulMapping_CCTLDs) {
  net::SchemefulSite owner1(GURL("https://owner1.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));
  net::SchemefulSite member1_cctld(GURL("https://member1.cctld"));
  net::SchemefulSite owner2(GURL("https://owner2.test"));
  net::SchemefulSite owner2_cctld(GURL("https://owner2.cctld"));
  net::SchemefulSite member2(GURL("https://member2.test"));

  base::Value policy_value = base::JSONReader::Read(R"(
             {
                "replacements": [
                  {
                    "owner": "https://owner1.test",
                    "members": ["https://member1.test"],
                    "ccTLDs": {
                      "https://member1.test": ["https://member1.cctld"],
                      "https://not_in_set.test": ["https://not_in_set.cctld"]
                    }
                  },
                  {
                    "owner": "https://owner2.test",
                    "members": ["https://member2.test"],
                    "ccTLDs": {
                      "https://owner2.test": ["https://owner2.cctld"]
                    }
                  }
                ]
              }
            )")
                                 .value();
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .value(),
      FirstPartySetParser::ParsedPolicySetLists(
          {FirstPartySetParser::SetsMap({
               {owner1, net::FirstPartySetEntry(owner1, net::SiteType::kPrimary,
                                                absl::nullopt)},
               {member1,
                net::FirstPartySetEntry(owner1, net::SiteType::kAssociated,
                                        absl::nullopt)},
               {member1_cctld,
                net::FirstPartySetEntry(owner1, net::SiteType::kAssociated,
                                        absl::nullopt)},
           }),
           FirstPartySetParser::SetsMap({
               {owner2, net::FirstPartySetEntry(owner2, net::SiteType::kPrimary,
                                                absl::nullopt)},
               {owner2_cctld,
                net::FirstPartySetEntry(owner2, net::SiteType::kPrimary,
                                        absl::nullopt)},
               {member2,
                net::FirstPartySetEntry(owner2, net::SiteType::kAssociated,
                                        absl::nullopt)},
           })},
          {}));
}

}  // namespace content

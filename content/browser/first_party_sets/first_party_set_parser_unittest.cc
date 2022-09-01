// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_set_parser.h"

#include <sstream>

#include "base/json/json_reader.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
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
      R"({"primary": "https://example.test", "associatedSites": []})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, AcceptsMinimal) {
  const std::string input =
      R"({"primary": "https://example.test", "associatedSites": ["https://aaaa.test"]})";

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

TEST(FirstPartySetParser, RejectsMissingPrimary) {
  const std::string input = R"({"associatedSites": ["https://aaaa.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsTypeUnsafePrimary) {
  const std::string input =
      R"({ "primary": 3, "associatedSites": ["https://aaaa.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsNonHTTPSPrimary) {
  const std::string input =
      R"({"primary": "http://example.test", "associatedSites": ["https://aaaa.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsNonOriginPrimary) {
  const std::string input =
      R"({"primary": "example", "associatedSites": ["https://aaaa.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, SkipsSetOnNonOriginPrimary) {
  const std::string input =
      R"({"primary": "example", "associatedSites": ["https://aaaa.test"]})"
      "\n"
      R"({"primary": "https://example2.test", "associatedSites": )"
      R"(["https://associatedsite2.test"]})"
      "\n"
      R"({"primary": "https://example.test", "associatedSites": ["https://aaaa.test"]})";

  EXPECT_THAT(
      ParseSets(input),
      Pair(UnorderedElementsAre(
               Pair(SerializesTo("https://example2.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example2.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://associatedsite2.test"),
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

TEST(FirstPartySetParser, RejectsPrimaryWithoutRegisteredDomain) {
  const std::string input = R"({"primary": "https://example.test..", )"
                            R"("associatedSites": ["https://aaaa.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsMissingAssociatedSites) {
  const std::string input = R"({"primary": "https://example.test" })";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsTypeUnsafeAssociatedSites) {
  const std::string input = R"({"primary": "https://example.test", )"
                            R"("associatedSites": ["https://aaaa.test", 4]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsNonHTTPSAssociatedSite) {
  const std::string input =
      R"({"primary": "https://example.test", "associatedSites": ["http://aaaa.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsNonOriginAssociatedSite) {
  const std::string input =
      R"({"primary": "https://example.test", "associatedSites": ["aaaa"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, SkipsSetOnNonOriginAssociatedSite) {
  const std::string input =
      R"({"primary": "https://example.test", "associatedSites": ["aaaa"]})"
      "\n"
      R"({"primary": "https://example2.test", "associatedSites": )"
      R"(["https://associatedsite2.test"]})"
      "\n"
      R"({"primary": "https://example.test", "associatedSites": )"
      R"(["https://associatedsite3.test"]})";

  EXPECT_THAT(
      ParseSets(input),
      Pair(UnorderedElementsAre(
               Pair(SerializesTo("https://example2.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example2.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://associatedsite2.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example2.test")),
                        net::SiteType::kAssociated, 0)),
               Pair(SerializesTo("https://example.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://associatedsite3.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0))),
           IsEmpty()));
}

TEST(FirstPartySetParser, RejectsAssociatedSiteWithoutRegisteredDomain) {
  const std::string input = R"({"primary": "https://example.test", )"
                            R"("associatedSites": ["https://aaaa.test.."]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, TruncatesSubdomain_Primary) {
  const std::string input = R"({"primary": "https://subdomain.example.test", )"
                            R"("associatedSites": ["https://aaaa.test"]})";

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

TEST(FirstPartySetParser, TruncatesSubdomain_AssociatedSite) {
  const std::string input =
      R"({"primary": "https://example.test", )"
      R"("associatedSites": ["https://subdomain.aaaa.test"]})";

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
      "{\"primary\": \"https://example.test\", \"associatedSites\": "
      "[\"https://associatedsite1.test\"]}\n"
      "{\"primary\": \"https://foo.test\", \"associatedSites\": "
      "[\"https://associatedsite2.test\"]}";

  std::istringstream stream(input);
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromStream(stream),
      Pair(UnorderedElementsAre(
               Pair(SerializesTo("https://example.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://associatedsite1.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0)),
               Pair(SerializesTo("https://foo.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://associatedsite2.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kAssociated, 0))),
           IsEmpty()));
}

TEST(FirstPartySetParser, AcceptsMultipleSetsWithWhitespace) {
  // Note the leading blank line, middle blank line, trailing blank line, and
  // leading whitespace on each line.
  const std::string input = R"(
      {"primary": "https://example.test", "associatedSites": ["https://associatedsite1.test"]}

      {"primary": "https://foo.test", "associatedSites": ["https://associatedsite2.test"]}
    )";

  std::istringstream stream(input);
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromStream(stream),
      Pair(UnorderedElementsAre(
               Pair(SerializesTo("https://example.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://associatedsite1.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0)),
               Pair(SerializesTo("https://foo.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://associatedsite2.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kAssociated, 0))),
           IsEmpty()));
}

TEST(FirstPartySetParser, RejectsInvalidSets_InvalidPrimary) {
  const std::string input =
      R"({"primary": 3, "associatedSites": ["https://associatedsite1.test"]}
    {"primary": "https://foo.test", "associatedSites": ["https://associatedsite2.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsInvalidSets_InvalidAssociatedSite) {
  const std::string input =
      R"({"primary": "https://example.test", "associatedSites": [3]}
    {"primary": "https://foo.test", "associatedSites": ["https://associatedsite2.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, AllowsTrailingCommas) {
  const std::string input =
      R"({"primary": "https://example.test", )"
      R"("associatedSites": ["https://associatedsite1.test"],})";

  EXPECT_THAT(
      ParseSets(input),
      Pair(UnorderedElementsAre(
               Pair(SerializesTo("https://example.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://associatedsite1.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0))),
           IsEmpty()));
}

TEST(FirstPartySetParser, Rejects_SamePrimary) {
  const std::string input =
      R"({"primary": "https://example.test", "associatedSites": ["https://associatedsite1.test"]}
    {"primary": "https://example.test", "associatedSites": ["https://associatedsite2.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, Rejects_AssociatedSiteAsPrimary) {
  const std::string input =
      R"({"primary": "https://example.test", "associatedSites": ["https://associatedsite1.test"]}
    {"primary": "https://associatedsite1.test", "associatedSites": ["https://associatedsite2.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, Rejects_SameAssociatedSite) {
  const std::string input =
      R"({"primary": "https://example.test", "associatedSites": ["https://associatedsite1.test"]}
    {"primary": "https://foo.test", "associatedSites": )"
      R"(["https://associatedsite1.test", "https://associatedsite2.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, Rejects_PrimaryAsAssociatedSite) {
  const std::string input =
      R"({"primary": "https://example.test", "associatedSites": ["https://associatedsite1.test"]}
    {"primary": "https://example2.test", )"
      R"("associatedSites": ["https://example.test", "https://associatedsite2.test"]})";

  EXPECT_THAT(ParseSets(input), Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, Accepts_ccTLDAliases) {
  const std::string input =
      "{"                                                         //
      "\"primary\": \"https://example.test\","                    //
      "\"associatedSites\": [\"https://associatedsite1.test\"],"  //
      "\"ccTLDs\": {"                                             //
      "\"https://associatedsite1.test\": "
      "[\"https://associatedsite1.cctld1\", "
      "\"https://associatedsite1.cctld2\"],"                          //
      "\"https://not_in_set.test\": [\"https://not_in_set.cctld\"],"  //
      "\"https://example.test\": \"https://not_a_list.test\""         //
      "}"                                                             //
      "}\n"                                                           //
      "{"                                                             //
      "\"primary\": \"https://foo.test\","                            //
      "\"associatedSites\": [\"https://associatedsite2.test\"],"      //
      "\"ccTLDs\": {"                                                 //
      "\"https://foo.test\": [\"https://foo.cctld\"],"                //
      "\"https://associatedsite2.test\": "
      "[\"https://different_prefix.cctld\"]"  //
      "}"                                     //
      "}";

  std::istringstream stream(input);
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromStream(stream),
      Pair(UnorderedElementsAre(
               Pair(SerializesTo("https://example.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://associatedsite1.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0)),
               Pair(SerializesTo("https://foo.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kPrimary, absl::nullopt)),
               Pair(SerializesTo("https://associatedsite2.test"),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kAssociated, 0))),
           UnorderedElementsAre(
               Pair(SerializesTo("https://associatedsite1.cctld1"),
                    SerializesTo("https://associatedsite1.test")),
               Pair(SerializesTo("https://associatedsite1.cctld2"),
                    SerializesTo("https://associatedsite1.test")),
               Pair(SerializesTo("https://foo.cctld"),
                    SerializesTo("https://foo.test")))));
}

TEST(FirstPartySetParser, Rejects_NonSchemefulSiteCcTLDAliases) {
  const std::string input =
      "{"                                                               //
      "\"primary\": \"https://example.test\","                          //
      "\"associatedSites\": [\"https://associatedsite1.test\"],"        //
      "\"ccTLDs\": {"                                                   //
      "\"https://associatedsite1.test\": [\"associatedsite1.cctld1\"]"  //
      "}"                                                               //
      "}";

  std::istringstream stream(input);
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromStream(stream),
              Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, Rejects_NondisjointCcTLDAliases) {
  // These two sets overlap only via a ccTLD variant.
  std::istringstream stream(
      "{"                                         //
      "\"owner\": \"https://example.test\","      //
      "\"members\": [\"https://member.test1\"],"  //
      "\"ccTLDs\": {"                             //
      "\"https://member.test1\": [\"https://member.cctld\"],"
      "}"                                                     //
      "}\n"                                                   //
      "{"                                                     //
      "\"owner\": \"https://foo.test\","                      //
      "\"members\": [\"https://member.test2\"],"              //
      "\"ccTLDs\": {"                                         //
      "\"https://member.test2\": [\"https://member.cctld\"]"  //
      "}"                                                     //
      "}");
  EXPECT_THAT(FirstPartySetParser::ParseSetsFromStream(stream),
              Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, SerializeFirstPartySets) {
  EXPECT_EQ(R"({"https://associatedsite1.test":"https://example1.test"})",
            FirstPartySetParser::SerializeFirstPartySets(
                {{net::SchemefulSite(GURL("https://associatedsite1.test")),
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
      R"({"https://associatedsite1.test":"null"})",
      FirstPartySetParser::SerializeFirstPartySets(
          {{net::SchemefulSite(GURL("https://associatedsite1.test")),
            net::FirstPartySetEntry(net::SchemefulSite(GURL("")),
                                    net::SiteType::kPrimary, absl::nullopt)}}));
}

TEST(FirstPartySetParser, SerializeFirstPartySetsEmptySet) {
  EXPECT_EQ("{}", FirstPartySetParser::SerializeFirstPartySets({}));
}

TEST(FirstPartySetParser, DeserializeFirstPartySets) {
  const std::string input =
      R"({"https://associatedsite1.test":"https://example1.test",
          "https://associatedsite3.test":"https://example1.test",
          "https://associatedsite2.test":"https://example2.test"})";
  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySetParser::DeserializeFirstPartySets(input),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://associatedsite1.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example1.test")),
                           net::SiteType::kAssociated, absl::nullopt)),
                  Pair(SerializesTo("https://associatedsite3.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example1.test")),
                           net::SiteType::kAssociated, absl::nullopt)),
                  Pair(SerializesTo("https://example1.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example1.test")),
                           net::SiteType::kPrimary, absl::nullopt)),
                  Pair(SerializesTo("https://associatedsite2.test"),
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

// if the same associated site appears twice with different primaries,
// it is not considered invalid content and wouldn't end up returning
// an empty map, since base::DictionaryValue automatically handles
// duplicated keys.
TEST(FirstPartySetParser, DeserializeFirstPartySetsDuplicatedKey) {
  const std::string input =
      R"({"https://associatedsite1.test":"https://example1.test",
          "https://associatedsite1.test":"https://example2.test"})";
  ASSERT_TRUE(base::JSONReader::Read(input));
  EXPECT_THAT(FirstPartySetParser::DeserializeFirstPartySets(input),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://associatedsite1.test"),
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
          "https://associatedsite1.test":"https://example2.test",
          "https://example2.test":"https://example2.test"})";
  ASSERT_TRUE(base::JSONReader::Read(input));
  EXPECT_THAT(FirstPartySetParser::DeserializeFirstPartySets(input),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://associatedsite1.test"),
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
        std::make_tuple(
            true,
            R"(["https://associatedsite1.test","https://example1.test"])"),
        // The serialized string is type of map that contains non-URL key.
        std::make_tuple(true, R"({"associatedSite1":"https://example1.test"})"),
        // The serialized string is type of map that contains non-URL value.
        std::make_tuple(true, R"({"https://associatedsite1.test":"example1"})"),
        // The serialized string is type of map that contains opaque origin.
        std::make_tuple(true, R"({"https://associatedsite1.test":""})"),
        std::make_tuple(true, R"({"":"https://example1.test"})"),
        // The serialized string is type of map that contains non-string value.
        std::make_tuple(true, R"({"https://associatedsite1.test":1})"),
        // Nondisjoint set. The same site shows up both as associated site and
        // primary.
        std::make_tuple(
            true,
            R"({"https://associatedsite1.test":"https://example1.test",
            "https://associatedsite2.test":"https://associatedsite1.test"})"),
        std::make_tuple(
            true,
            R"({"https://associatedsite1.test":"https://example1.test",
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
     InvalidTypeError_MissingPrimary) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
                "replacements": [
                  {
                    "associatedSites": ["https://associatedsite1.test"]
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
     InvalidTypeError_MissingAssociatedSites) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
                "replacements": [
                  {
                    "primary": "https://primary1.test"
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
     InvalidTypeError_WrongPrimaryType) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
                "replacements": [
                  {
                    "primary": 123,
                    "associatedSites": ["https://associatedsite1.test"]
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
     InvalidTypeError_WrongAssociatedSitesFieldType) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": 123
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
     InvalidTypeError_WrongAssociatedSiteType) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
          "replacements": [
            {
              "primary": "https://primary1.test",
              "associatedSites": ["https://associatedsite1.test", 123,
              "https://associatedsite2.test"]
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
     InvalidOriginError_PrimaryOpaque) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
                "replacements": [
                  {
                    "primary": "",
                    "associatedSites": ["https://associatedsite1.test"]
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
     InvalidOriginError_AssociatedSiteOpaque) {
  base::Value policy_value = base::JSONReader::Read(R"(
               {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": [""]
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
     InvalidOriginError_PrimaryNonHttps) {
  base::Value policy_value = base::JSONReader::Read(R"(
                 {
                "replacements": [
                  {
                    "primary": "http://primary1.test",
                    "associatedSites": ["https://associatedsite1.test"]
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
     InvalidOriginError_AssociatedSiteNonHttps) {
  base::Value policy_value = base::JSONReader::Read(R"(
               {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["http://associatedsite1.test"]
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
     InvalidOriginError_PrimaryNonRegisteredDomain) {
  base::Value policy_value = base::JSONReader::Read(R"(
                {
                "replacements": [
                  {
                    "primary": "https://primary1.test..",
                    "associatedSites": ["https://associatedsite1.test"]
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
     InvalidOriginError_AssociatedSiteNonRegisteredDomain) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite1.test.."]
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
     SingletonSetError_EmptyAssociatedSites) {
  base::Value policy_value = base::JSONReader::Read(R"(
             {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": []
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
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://primary1.test"]
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
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite1.test"]
                  },
                  {
                    "primary": "https://primary2.test",
                    "associatedSites": ["https://associatedsite1.test"]
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
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite1.test"]
                  },
                  {
                    "primary": "https://primary2.test",
                    "associatedSites": ["https://associatedsite1.test"]
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
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite1.test"]
                  }
                ],
                "additions": [
                  {
                    "primary": "https://primary2.test",
                    "associatedSites": ["https://associatedsite1.test"]
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
  net::SchemefulSite primary1(GURL("https://primary1.test"));
  net::SchemefulSite associated_site1(GURL("https://associatedsite1.test"));
  net::SchemefulSite primary2(GURL("https://primary2.test"));
  net::SchemefulSite associated_site2(GURL("https://associatedsite2.test"));

  base::Value policy_value = base::JSONReader::Read(R"(
             {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite1.test"]
                  },
                  {
                    "primary": "https://primary2.test",
                    "associatedSites": ["https://associatedsite2.test"]
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
               {primary1,
                net::FirstPartySetEntry(primary1, net::SiteType::kPrimary,
                                        absl::nullopt)},
               {associated_site1,
                net::FirstPartySetEntry(primary1, net::SiteType::kAssociated,
                                        absl::nullopt)},
           }),
           FirstPartySetParser::SetsMap({
               {primary2,
                net::FirstPartySetEntry(primary2, net::SiteType::kPrimary,
                                        absl::nullopt)},
               {associated_site2,
                net::FirstPartySetEntry(primary2, net::SiteType::kAssociated,
                                        absl::nullopt)},
           })},
          {}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
     SuccessfulMapping_CrossList) {
  net::SchemefulSite primary1(GURL("https://primary1.test"));
  net::SchemefulSite associated_site1(GURL("https://associatedsite1.test"));
  net::SchemefulSite primary2(GURL("https://primary2.test"));
  net::SchemefulSite associated_site2(GURL("https://associatedsite2.test"));
  net::SchemefulSite primary3(GURL("https://primary3.test"));
  net::SchemefulSite associatedSite3(GURL("https://associatedsite3.test"));

  base::Value policy_value = base::JSONReader::Read(R"(
                {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite1.test"]
                  },
                  {
                    "primary": "https://primary2.test",
                    "associatedSites": ["https://associatedsite2.test"]
                  }
                ],
                "additions": [
                  {
                    "primary": "https://primary3.test",
                    "associatedSites": ["https://associatedsite3.test"]
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
               {primary1,
                net::FirstPartySetEntry(primary1, net::SiteType::kPrimary,
                                        absl::nullopt)},
               {associated_site1,
                net::FirstPartySetEntry(primary1, net::SiteType::kAssociated,
                                        absl::nullopt)},
           }),
           FirstPartySetParser::SetsMap({
               {primary2,
                net::FirstPartySetEntry(primary2, net::SiteType::kPrimary,
                                        absl::nullopt)},
               {associated_site2,
                net::FirstPartySetEntry(primary2, net::SiteType::kAssociated,
                                        absl::nullopt)},
           })},
          {FirstPartySetParser::SetsMap({
              {primary3, net::FirstPartySetEntry(
                             primary3, net::SiteType::kPrimary, absl::nullopt)},
              {associatedSite3,
               net::FirstPartySetEntry(primary3, net::SiteType::kAssociated,
                                       absl::nullopt)},
          })}));
}

TEST(FirstPartySets_ParseSetsFromEnterprisePolicyTest,
     SuccessfulMapping_CCTLDs) {
  net::SchemefulSite primary1(GURL("https://primary1.test"));
  net::SchemefulSite associated_site1(GURL("https://associatedsite1.test"));
  net::SchemefulSite associated_site1_cctld(
      GURL("https://associatedsite1.cctld"));
  net::SchemefulSite primary2(GURL("https://primary2.test"));
  net::SchemefulSite primary2_cctld(GURL("https://primary2.cctld"));
  net::SchemefulSite associated_site2(GURL("https://associatedsite2.test"));

  base::Value policy_value = base::JSONReader::Read(R"(
             {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite1.test"],
                    "ccTLDs": {
                      "https://associatedsite1.test": ["https://associatedsite1.cctld"],
                      "https://not_in_set.test": ["https://not_in_set.cctld"]
                    }
                  },
                  {
                    "primary": "https://primary2.test",
                    "associatedSites": ["https://associatedsite2.test"],
                    "ccTLDs": {
                      "https://primary2.test": ["https://primary2.cctld"]
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
               {primary1,
                net::FirstPartySetEntry(primary1, net::SiteType::kPrimary,
                                        absl::nullopt)},
               {associated_site1,
                net::FirstPartySetEntry(primary1, net::SiteType::kAssociated,
                                        absl::nullopt)},
               {associated_site1_cctld,
                net::FirstPartySetEntry(primary1, net::SiteType::kAssociated,
                                        absl::nullopt)},
           }),
           FirstPartySetParser::SetsMap({
               {primary2,
                net::FirstPartySetEntry(primary2, net::SiteType::kPrimary,
                                        absl::nullopt)},
               {primary2_cctld,
                net::FirstPartySetEntry(primary2, net::SiteType::kPrimary,
                                        absl::nullopt)},
               {associated_site2,
                net::FirstPartySetEntry(primary2, net::SiteType::kAssociated,
                                        absl::nullopt)},
           })},
          {}));
}

}  // namespace content

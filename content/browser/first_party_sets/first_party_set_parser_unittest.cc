// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_set_parser.h"

#include <sstream>

#include "base/json/json_reader.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/common/content_features.h"
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

namespace {

using ParseErrorType = FirstPartySetsHandler::ParseErrorType;
using ParseError = FirstPartySetsHandler::ParseError;
using ParseWarningType = FirstPartySetsHandler::ParseWarningType;
using ParseWarning = FirstPartySetsHandler::ParseWarning;

const char kPrimaryField[] = "primary";
const char kAssociatedSitesField[] = "associatedSites";
const char kCctldsField[] = "ccTLDs";
const char kReplacementsField[] = "replacements";
const char kAdditionsField[] = "additions";
constexpr char kParsedSuccessfullyHistogram[] =
    "Cookie.FirstPartySets.ComponentSetsParsedSuccessfully";
constexpr char kNonfatalErrorsHistogram[] =
    "Cookie.FirstPartySets.ComponentSetsNonfatalErrors";
constexpr char kProcessedComponentHistogram[] =
    "Cookie.FirstPartySets.ProcessedEntireComponent";

}  // namespace

FirstPartySetParser::SetsAndAliases ParseSets(const std::string& sets) {
  std::istringstream stream(sets);
  return FirstPartySetParser::ParseSetsFromStream(stream, false, true);
}

TEST(FirstPartySetParser, RejectsNonemptyMalformed) {
  // If the input isn't valid JSON, we should
  // reject it.
  base::HistogramTester histogram_tester;
  EXPECT_THAT(ParseSets("certainly not valid JSON"),
              Pair(IsEmpty(), IsEmpty()));
  EXPECT_EQ(histogram_tester.GetTotalSum(kParsedSuccessfullyHistogram), 0);
  EXPECT_EQ(histogram_tester.GetTotalSum(kNonfatalErrorsHistogram), 0);
  histogram_tester.ExpectUniqueSample(kProcessedComponentHistogram,
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/1);
}

TEST(FirstPartySetParser, AcceptsTrivial) {
  base::HistogramTester histogram_tester;
  EXPECT_THAT(ParseSets(""), Pair(IsEmpty(), IsEmpty()));
  histogram_tester.ExpectUniqueSample(kProcessedComponentHistogram,
                                      /*sample=*/1,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      kParsedSuccessfullyHistogram, /*sample=*/0, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kProcessedComponentHistogram,
                                      /*sample=*/1,
                                      /*expected_bucket_count=*/1);
}

TEST(FirstPartySetParser, RejectsSingletonSet) {
  EXPECT_THAT(
      ParseSets(
          R"({"primary": "https://example.test", "associatedSites": []})"),
      Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, AcceptsMinimal_Associated) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite aaaa(GURL("https://aaaa.test"));

  EXPECT_THAT(ParseSets(R"({"primary": "https://example.test",)"
                        R"("associatedSites": ["https://aaaa.test"]})"),
              Pair(UnorderedElementsAre(
                       Pair(example, net::FirstPartySetEntry(
                                         example, net::SiteType::kPrimary,
                                         absl::nullopt)),
                       Pair(aaaa, net::FirstPartySetEntry(
                                      example, net::SiteType::kAssociated, 0))),
                   IsEmpty()));
}

TEST(FirstPartySetParser, AcceptsMinimal_Service) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite aaaa(GURL("https://aaaa.test"));

  EXPECT_THAT(
      ParseSets(R"({"primary": "https://example.test",)"
                R"("serviceSites": ["https://aaaa.test"]})"),
      Pair(
          UnorderedElementsAre(
              Pair(example,
                   net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                           absl::nullopt)),
              Pair(aaaa, net::FirstPartySetEntry(
                             example, net::SiteType::kService, absl::nullopt))),
          IsEmpty()));
}

TEST(FirstPartySetParser, AcceptsMinimal_AllSubsets_WithCcTLDs) {
  base::HistogramTester histogram_tester;
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite example_cctld(GURL("https://example.cctld"));
  net::SchemefulSite a(GURL("https://a.test"));
  net::SchemefulSite a_cctld(GURL("https://a.cctld"));
  net::SchemefulSite b(GURL("https://b.test"));
  net::SchemefulSite b_cctld(GURL("https://b.cctld"));

  EXPECT_THAT(
      ParseSets(R"({"primary": "https://example.test",)"
                R"("associatedSites": ["https://a.test"],)"
                R"("serviceSites": ["https://b.test"],)"
                R"("ccTLDs": {)"
                R"("https://example.test": ["https://example.cctld"],)"
                R"("https://a.test": ["https://a.cctld"],)"
                R"("https://b.test": ["https://b.cctld"])"
                R"(})"
                R"(})"),
      Pair(UnorderedElementsAre(
               Pair(example,
                    net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                            absl::nullopt)),
               Pair(a, net::FirstPartySetEntry(example,
                                               net::SiteType::kAssociated, 0)),
               Pair(b, net::FirstPartySetEntry(example, net::SiteType::kService,
                                               absl::nullopt))),
           UnorderedElementsAre(Pair(example_cctld, example), Pair(a_cctld, a),
                                Pair(b_cctld, b))));
  histogram_tester.ExpectUniqueSample(
      kParsedSuccessfullyHistogram, /*sample=*/1, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kNonfatalErrorsHistogram, /*sample=*/0,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kProcessedComponentHistogram,
                                      /*sample=*/1,
                                      /*expected_bucket_count=*/1);
}

TEST(FirstPartySetParser, RejectsMissingPrimary) {
  base::HistogramTester histogram_tester;
  EXPECT_THAT(ParseSets(R"({"associatedSites": ["https://aaaa.test"]})"),
              Pair(IsEmpty(), IsEmpty()));
  histogram_tester.ExpectUniqueSample(kProcessedComponentHistogram,
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/1);
}

TEST(FirstPartySetParser, RejectsTypeUnsafePrimary) {
  EXPECT_THAT(
      ParseSets(R"({ "primary": 3, "associatedSites": ["https://aaaa.test"]})"),
      Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsNonHTTPSPrimary) {
  EXPECT_THAT(ParseSets(R"({"primary": "http://example.test",)"
                        R"("associatedSites": ["https://aaaa.test"]})"),
              Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsNonOriginPrimary) {
  EXPECT_THAT(
      ParseSets(
          R"({"primary": "example", "associatedSites": ["https://aaaa.test"]})"),
      Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, SkipsSetOnNonOriginPrimary) {
  base::HistogramTester histogram_tester;
  net::SchemefulSite example2(GURL("https://example2.test"));
  net::SchemefulSite associated2(GURL("https://associatedsite2.test"));
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite aaaa(GURL("https://aaaa.test"));

  EXPECT_THAT(
      ParseSets(
          R"({"primary": "https://127.0.0.1:1234", "associatedSites": ["https://aaaa.test"]})"
          "\n"
          R"({"primary": "https://example2.test", "associatedSites": )"
          R"(["https://associatedsite2.test"]})"
          "\n"
          R"({"primary": "https://example.test",)"
          R"("associatedSites": ["https://aaaa.test"]})"),
      Pair(UnorderedElementsAre(
               Pair(example2,
                    net::FirstPartySetEntry(example2, net::SiteType::kPrimary,
                                            absl::nullopt)),
               Pair(associated2, net::FirstPartySetEntry(
                                     example2, net::SiteType::kAssociated, 0)),
               Pair(example,
                    net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                            absl::nullopt)),
               Pair(aaaa, net::FirstPartySetEntry(
                              example, net::SiteType::kAssociated, 0))),
           IsEmpty()));
  histogram_tester.ExpectUniqueSample(
      kParsedSuccessfullyHistogram, /*sample=*/2, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kNonfatalErrorsHistogram, /*sample=*/1,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kProcessedComponentHistogram,
                                      /*sample=*/1,
                                      /*expected_bucket_count=*/1);
}

TEST(FirstPartySetParser, RejectsPrimaryWithoutRegisteredDomain) {
  base::HistogramTester histogram_tester;
  EXPECT_THAT(ParseSets(R"({"primary": "https://example.test..", )"
                        R"("associatedSites": ["https://aaaa.test"]})"),
              Pair(IsEmpty(), IsEmpty()));
  histogram_tester.ExpectUniqueSample(
      kParsedSuccessfullyHistogram, /*sample=*/0, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kNonfatalErrorsHistogram, /*sample=*/1,
                                      /*expected_bucket_count=*/1);
}

TEST(FirstPartySetParser, RejectsMissingAssociatedSites) {
  EXPECT_THAT(ParseSets(R"({"primary": "https://example.test" })"),
              Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsTypeUnsafeAssociatedSites) {
  EXPECT_THAT(ParseSets(R"({"primary": "https://example.test", )"
                        R"("associatedSites": ["https://aaaa.test", 4]})"),
              Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsNonHTTPSAssociatedSite) {
  EXPECT_THAT(ParseSets(R"({"primary": "https://example.test",)"
                        R"("associatedSites": ["http://aaaa.test"]})"),
              Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, RejectsNonOriginAssociatedSite) {
  EXPECT_THAT(ParseSets(R"({"primary": "https://example.test",)"
                        R"("associatedSites": ["aaaa"]})"),
              Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, SkipsSetOnNonOriginAssociatedSite) {
  net::SchemefulSite example2(GURL("https://example2.test"));
  net::SchemefulSite associated2(GURL("https://associatedsite2.test"));
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated3(GURL("https://associatedsite3.test"));

  EXPECT_THAT(
      ParseSets(
          R"({"primary": "https://example.test", "associatedSites": ["https://127.0.0.1:1234"]})"
          "\n"
          R"({"primary": "https://example2.test", "associatedSites": )"
          R"(["https://associatedsite2.test"]})"
          "\n"
          R"({"primary": "https://example.test", "associatedSites": )"
          R"(["https://associatedsite3.test"]})"),
      Pair(UnorderedElementsAre(
               Pair(example2,
                    net::FirstPartySetEntry(example2, net::SiteType::kPrimary,
                                            absl::nullopt)),
               Pair(associated2, net::FirstPartySetEntry(
                                     example2, net::SiteType::kAssociated, 0)),
               Pair(example,
                    net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                            absl::nullopt)),
               Pair(associated3, net::FirstPartySetEntry(
                                     example, net::SiteType::kAssociated, 0))),
           IsEmpty()));
}

TEST(FirstPartySetParser, RejectsAssociatedSiteWithoutRegisteredDomain) {
  EXPECT_THAT(ParseSets(R"({"primary": "https://example.test", )"
                        R"("associatedSites": ["https://aaaa.test.."]})"),
              Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, TruncatesSubdomain_Primary) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite aaaa(GURL("https://aaaa.test"));

  EXPECT_THAT(ParseSets(R"({"primary": "https://subdomain.example.test", )"
                        R"("associatedSites": ["https://aaaa.test"]})"),
              Pair(UnorderedElementsAre(
                       Pair(example, net::FirstPartySetEntry(
                                         example, net::SiteType::kPrimary,
                                         absl::nullopt)),
                       Pair(aaaa, net::FirstPartySetEntry(
                                      example, net::SiteType::kAssociated, 0))),
                   IsEmpty()));
}

TEST(FirstPartySetParser, TruncatesSubdomain_AssociatedSite) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite aaaa(GURL("https://aaaa.test"));

  EXPECT_THAT(
      ParseSets(R"({"primary": "https://example.test", )"
                R"("associatedSites": ["https://subdomain.aaaa.test"]})"),
      Pair(UnorderedElementsAre(
               Pair(example,
                    net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                            absl::nullopt)),
               Pair(aaaa, net::FirstPartySetEntry(
                              example, net::SiteType::kAssociated, 0))),
           IsEmpty()));
}

TEST(FirstPartySetParser, AcceptsMultipleSets) {
  base::HistogramTester histogram_tester;
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite associated2(GURL("https://associatedsite2.test"));
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated1(GURL("https://associatedsite1.test"));

  EXPECT_THAT(
      ParseSets("{\"primary\": \"https://example.test\", \"associatedSites\": "
                "[\"https://associatedsite1.test\"]}\n"
                "{\"primary\": \"https://foo.test\", \"associatedSites\": "
                "[\"https://associatedsite2.test\"]}"),
      Pair(UnorderedElementsAre(
               Pair(example,
                    net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                            absl::nullopt)),
               Pair(associated1, net::FirstPartySetEntry(
                                     example, net::SiteType::kAssociated, 0)),
               Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                                 absl::nullopt)),
               Pair(associated2, net::FirstPartySetEntry(
                                     foo, net::SiteType::kAssociated, 0))),
           IsEmpty()));
  histogram_tester.ExpectUniqueSample(
      kParsedSuccessfullyHistogram, /*sample=*/2, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kNonfatalErrorsHistogram, /*sample=*/0,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kProcessedComponentHistogram,
                                      /*sample=*/1,
                                      /*expected_bucket_count=*/1);
}

TEST(FirstPartySetParser, AcceptsMultipleSetsWithWhitespace) {
  base::HistogramTester histogram_tester;
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite associated1(GURL("https://associatedsite1.test"));
  net::SchemefulSite associated2(GURL("https://associatedsite2.test"));
  net::SchemefulSite example(GURL("https://example.test"));
  // Note the leading blank line, middle blank line, trailing blank line, and
  // leading whitespace on each line.
  EXPECT_THAT(
      ParseSets(R"(
      {"primary": "https://example.test", "associatedSites": ["https://associatedsite1.test"]}

      {"primary": "https://foo.test", "associatedSites": ["https://associatedsite2.test"]}
    )"),
      Pair(UnorderedElementsAre(
               Pair(example,
                    net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                            absl::nullopt)),
               Pair(associated1, net::FirstPartySetEntry(
                                     example, net::SiteType::kAssociated, 0)),
               Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                                 absl::nullopt)),
               Pair(associated2, net::FirstPartySetEntry(
                                     foo, net::SiteType::kAssociated, 0))),
           IsEmpty()));
  histogram_tester.ExpectUniqueSample(
      kParsedSuccessfullyHistogram, /*sample=*/2, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kNonfatalErrorsHistogram, /*sample=*/0,
                                      /*expected_bucket_count=*/1);
}

TEST(FirstPartySetParser, RejectsInvalidSets_InvalidPrimary) {
  base::HistogramTester histogram_tester;
  EXPECT_THAT(
      ParseSets(
          R"({"primary": 3, "associatedSites": ["https://associatedsite1.test"]}
    {"primary": "https://foo.test",)"
          R"("associatedSites": ["https://associatedsite2.test"]})"),
      Pair(IsEmpty(), IsEmpty()));
  EXPECT_EQ(histogram_tester.GetTotalSum(kParsedSuccessfullyHistogram), 0);
}

TEST(FirstPartySetParser, RejectsInvalidSets_InvalidAssociatedSite) {
  EXPECT_THAT(
      ParseSets(R"({"primary": "https://example.test", "associatedSites": [3]}
    {"primary": "https://foo.test",)"
                R"("associatedSites": ["https://associatedsite2.test"]})"),
      Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, AllowsTrailingCommas) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated1(GURL("https://associatedsite1.test"));
  EXPECT_THAT(
      ParseSets(R"({"primary": "https://example.test", )"
                R"("associatedSites": ["https://associatedsite1.test"],})"),
      Pair(UnorderedElementsAre(
               Pair(example,
                    net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                            absl::nullopt)),
               Pair(associated1, net::FirstPartySetEntry(
                                     example, net::SiteType::kAssociated, 0))),
           IsEmpty()));
}

TEST(FirstPartySetParser, Rejects_SamePrimary) {
  EXPECT_THAT(
      ParseSets(R"({"primary": "https://example.test",)"
                R"("associatedSites": ["https://associatedsite1.test"]}
    {"primary": "https://example.test",)"
                R"("associatedSites": ["https://associatedsite2.test"]})"),
      Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, Rejects_AssociatedSiteAsPrimary) {
  EXPECT_THAT(
      ParseSets(R"({"primary": "https://example.test",)"
                R"("associatedSites": ["https://associatedsite1.test"]}
    {"primary": "https://associatedsite1.test",)"
                R"("associatedSites": ["https://associatedsite2.test"]})"),
      Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, Rejects_SameAssociatedSite) {
  EXPECT_THAT(
      ParseSets(
          R"({"primary": "https://example.test",)"
          R"("associatedSites": ["https://associatedsite1.test"]}
    {"primary": "https://foo.test",)"
          R"("associatedSites": )"
          R"(["https://associatedsite1.test", "https://associatedsite2.test"]})"),
      Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, Rejects_PrimaryAsAssociatedSite) {
  EXPECT_THAT(
      ParseSets(R"({"primary": "https://example.test",)"
                R"("associatedSites": ["https://associatedsite1.test"]}
    {"primary": "https://example2.test", )"
                R"("associatedSites":)"
                R"(["https://example.test", "https://associatedsite2.test"]})"),
      Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, Accepts_ccTLDAliases) {
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite foo_cctld(GURL("https://foo.cctld"));
  net::SchemefulSite associated1(GURL("https://associatedsite1.test"));
  net::SchemefulSite associated1_cctld1(GURL("https://associatedsite1.cctld1"));
  net::SchemefulSite associated1_cctld2(GURL("https://associatedsite1.cctld2"));
  net::SchemefulSite associated2(GURL("https://associatedsite2.test"));
  net::SchemefulSite example(GURL("https://example.test"));

  EXPECT_THAT(
      ParseSets(
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
          "}"),
      Pair(UnorderedElementsAre(
               Pair(example,
                    net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                            absl::nullopt)),
               Pair(associated1, net::FirstPartySetEntry(
                                     example, net::SiteType::kAssociated, 0)),
               Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                                 absl::nullopt)),
               Pair(associated2, net::FirstPartySetEntry(
                                     foo, net::SiteType::kAssociated, 0))),
           UnorderedElementsAre(Pair(associated1_cctld1, associated1),
                                Pair(associated1_cctld2, associated1),
                                Pair(foo_cctld, foo))));
}

TEST(FirstPartySetParser, Rejects_NonSchemefulSiteCcTLDAliases) {
  EXPECT_THAT(
      ParseSets(
          "{"                                                               //
          "\"primary\": \"https://example.test\","                          //
          "\"associatedSites\": [\"https://associatedsite1.test\"],"        //
          "\"ccTLDs\": {"                                                   //
          "\"https://associatedsite1.test\": [\"associatedsite1.cctld1\"]"  //
          "}"                                                               //
          "}"),
      Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, Rejects_NondisjointCcTLDAliases) {
  // These two sets overlap only via a ccTLD variant.
  EXPECT_THAT(
      ParseSets("{"                                                 //
                "\"primary\": \"https://example.test\","            //
                "\"associatedSites\": [\"https://member.test1\"],"  //
                "\"ccTLDs\": {"                                     //
                "\"https://member.test1\": [\"https://member.cctld\"],"
                "}"                                                     //
                "}\n"                                                   //
                "{"                                                     //
                "\"primary\": \"https://foo.test\","                    //
                "\"associatedSites\": [\"https://member.test2\"],"      //
                "\"ccTLDs\": {"                                         //
                "\"https://member.test2\": [\"https://member.cctld\"]"  //
                "}"                                                     //
                "}"),
      Pair(IsEmpty(), IsEmpty()));
}

TEST(FirstPartySetParser, Logs_MultipleRejections) {
  // 2 rejections should show up on the histogram as separate instances
  base::HistogramTester histogram_tester;
  EXPECT_THAT(ParseSets("certainly not valid JSON"),
              Pair(IsEmpty(), IsEmpty()));
  EXPECT_THAT(ParseSets("also not valid JSON"), Pair(IsEmpty(), IsEmpty()));
  histogram_tester.ExpectUniqueSample(kProcessedComponentHistogram,
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/2);
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
     Accepts_MissingSetLists) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
              }
            )")
                                 .value();
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .first.value(),
      FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
     Accepts_EmptyLists) {
  base::Value policy_value = base::JSONReader::Read(R"(
              {
                "replacements": [],
                "additions": []
              }
            )")
                                 .value();
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .first.value(),
      FirstPartySetParser::ParsedPolicySetLists({}, {}));
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
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
          .first.error(),
      ParseError(ParseErrorType::kInvalidType,
                 {kReplacementsField, 0, kPrimaryField}));
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
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
          .first.error(),
      ParseError(ParseErrorType::kInvalidType,
                 {kReplacementsField, 0, kPrimaryField}));
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
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
          .first.error(),
      ParseError(ParseErrorType::kInvalidType,
                 {kReplacementsField, 0, kAssociatedSitesField}));
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
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
          .first.error(),
      ParseError(ParseErrorType::kInvalidType,
                 {kReplacementsField, 0, kAssociatedSitesField, 1}));
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
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
          .first.error(),
      ParseError(ParseErrorType::kInvalidOrigin,
                 {kReplacementsField, 0, kPrimaryField}));
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
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
          .first.error(),
      ParseError(ParseErrorType::kInvalidOrigin,
                 {kReplacementsField, 0, kAssociatedSitesField, 0}));
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest, PrimaryNonHttps) {
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
          .first.error(),
      ParseError(ParseErrorType::kNonHttpsScheme,
                 {kReplacementsField, 0, kPrimaryField}));
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
     AssociatedSiteNonHttps) {
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
          .first.error(),
      ParseError(ParseErrorType::kNonHttpsScheme,
                 {kReplacementsField, 0, kAssociatedSitesField, 0}));
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
     PrimaryNonRegisteredDomain) {
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
          .first.error(),
      ParseError(ParseErrorType::kInvalidDomain,
                 {kReplacementsField, 0, kPrimaryField}));
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
     AssociatedSiteNonRegisteredDomain) {
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
          .first.error(),
      ParseError(ParseErrorType::kInvalidDomain,
                 {kReplacementsField, 0, kAssociatedSitesField, 0}));
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
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
          .first.error(),
      ParseError(ParseErrorType::kSingletonSet,
                 {kReplacementsField, 0, kAssociatedSitesField}));
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
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
          .first.error(),
      ParseError(ParseErrorType::kRepeatedDomain,
                 {kReplacementsField, 0, kAssociatedSitesField, 0}));
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
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
          .first.error(),
      ParseError(ParseErrorType::kNonDisjointSets,
                 {kReplacementsField, 1, kAssociatedSitesField, 0}));
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
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
          .first.error(),
      ParseError(ParseErrorType::kNonDisjointSets,
                 {kAdditionsField, 1, kAssociatedSitesField, 0}));
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
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
          .first.error(),
      ParseError(ParseErrorType::kNonDisjointSets,
                 {kAdditionsField, 0, kAssociatedSitesField, 0}));
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest, WarnsUntilError) {
  base::Value policy_value = base::JSONReader::Read(R"(
               {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite1.test"],
                    "ccTLDs": {
                      "https://associatedsite1.cctld": ["https://associatedsite1.test"],
                      "https://primary1.test": ["https://primary1-diff.cctld"]
                    }
                  }
                ],
                "additions": [
                  {
                    "primary": "https://primary2.",
                    "associatedSites": ["https://associatedsite2.test"],
                    "ccTLDs": {
                      "https://associatedsite2.test": ["https://associatedsite2-diff.cctld"]
                    }
                  }
                ]
              }
            )")
                                 .value();
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .first.error(),
      ParseError(ParseErrorType::kInvalidDomain,
                 {kAdditionsField, 0, kPrimaryField}));

  // The ParseWarning in the ccTLDs field of "additions[0]" isn't added since
  // the InvalidOrigin error arises first.
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .second,
      ElementsAre(ParseWarning(ParseWarningType::kCctldKeyNotCanonical,
                               {kReplacementsField, 0, kCctldsField,
                                "https://associatedsite1.cctld"}),
                  ParseWarning(ParseWarningType::kAliasNotCctldVariant,
                               {kReplacementsField, 0, kCctldsField,
                                "https://primary1.test", 0})));
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
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
          .first.value(),
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
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .second,
      IsEmpty());
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
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
          .first.value(),
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
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .second,
      IsEmpty());
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
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
          .first.value(),
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

  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .second,
      std::vector<ParseWarning>{ParseWarning(
          ParseWarningType::kCctldKeyNotCanonical,
          {kReplacementsField, 0, kCctldsField, "https://not_in_set.test"})});
}

class FirstPartySetParserTest : public ::testing::Test {
 public:
  FirstPartySetParserTest() {
    features_.InitWithFeaturesAndParameters(
        {{features::kFirstPartySets,
          {{features::kFirstPartySetsMaxAssociatedSites.name, "1"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(FirstPartySetParserTest, RespectsAssociatedSiteLimit) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite a(GURL("https://a.test"));

  EXPECT_THAT(
      ParseSets(R"({"primary": "https://example.test",)"
                R"("associatedSites": ["https://a.test", "https://b.test"],)"
                R"(})"),
      Pair(UnorderedElementsAre(
               Pair(example,
                    net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                            absl::nullopt)),
               Pair(a, net::FirstPartySetEntry(example,
                                               net::SiteType::kAssociated, 0))),
           IsEmpty()));
}

TEST_F(FirstPartySetParserTest, DetectsErrorsPastAssociatedSiteLimit) {
  EXPECT_THAT(
      ParseSets(R"({"primary": "https://example.test",)"
                R"("associatedSites": ["https://a.test", "not a domain"],)"
                R"(})"),
      Pair(IsEmpty(), IsEmpty()));
}

TEST_F(FirstPartySetParserTest,
       ServiceSitesAreNotCountedAgainstAssociatedSiteLimit) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite a(GURL("https://a.test"));
  net::SchemefulSite b(GURL("https://b.test"));
  net::SchemefulSite c(GURL("https://c.test"));

  EXPECT_THAT(
      ParseSets(R"({)"
                R"("primary": "https://example.test",)"
                R"("associatedSites": ["https://a.test"],)"
                R"("serviceSites": ["https://b.test", "https://c.test"],)"
                R"(})"),
      Pair(UnorderedElementsAre(
               Pair(example,
                    net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                            absl::nullopt)),
               Pair(a, net::FirstPartySetEntry(example,
                                               net::SiteType::kAssociated, 0)),
               Pair(b, net::FirstPartySetEntry(example, net::SiteType::kService,
                                               absl::nullopt)),
               Pair(c, net::FirstPartySetEntry(example, net::SiteType::kService,
                                               absl::nullopt))),
           IsEmpty()));
}

TEST_F(FirstPartySetParserTest,
       AliasesAreNotCountedAgainstAssociatedSiteLimit) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite a(GURL("https://a.test"));
  net::SchemefulSite a_cctld1(GURL("https://a.cctld1"));
  net::SchemefulSite a_cctld2(GURL("https://a.cctld2"));

  EXPECT_THAT(
      ParseSets(
          R"({)"
          R"("primary": "https://example.test",)"
          R"("associatedSites": ["https://a.test", "https://b.test"],)"
          R"("ccTLDs": {)"
          R"(  "https://a.test": ["https://a.cctld1", "https://a.cctld2"])"
          R"(})"
          R"(})"),
      Pair(UnorderedElementsAre(
               Pair(example,
                    net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                            absl::nullopt)),
               Pair(a, net::FirstPartySetEntry(example,
                                               net::SiteType::kAssociated, 0))),
           UnorderedElementsAre(Pair(a_cctld1, a), Pair(a_cctld2, a))));
}

TEST_F(FirstPartySetParserTest,
       EnterprisePolicies_ExemptFromAssociatedSiteLimit) {
  net::SchemefulSite primary1(GURL("https://primary1.test"));
  net::SchemefulSite associated1(GURL("https://associated1.test"));
  net::SchemefulSite associated2(GURL("https://associated2.test"));

  base::Value policy_value = base::JSONReader::Read(R"(
             {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associated1.test", "https://associated2.test"]
                  }
                ]
              }
            )")
                                 .value();
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .first.value(),
      FirstPartySetParser::ParsedPolicySetLists(
          {FirstPartySetParser::SetsMap({
              {primary1, net::FirstPartySetEntry(
                             primary1, net::SiteType::kPrimary, absl::nullopt)},
              {associated1,
               net::FirstPartySetEntry(primary1, net::SiteType::kAssociated,
                                       absl::nullopt)},
              {associated2,
               net::FirstPartySetEntry(primary1, net::SiteType::kAssociated,
                                       absl::nullopt)},
          })},
          {}));
}

}  // namespace content

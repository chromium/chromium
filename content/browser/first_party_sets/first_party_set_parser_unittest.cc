// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_set_parser.h"

#include <optional>
#include <sstream>

#include "base/json/json_reader.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/version.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/common/content_features.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "net/first_party_sets/local_set_declaration.h"
#include "net/first_party_sets/sets_mutation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace content {

namespace {

using ParseErrorType = FirstPartySetsHandler::ParseErrorType;
using ParseError = FirstPartySetsHandler::ParseError;
using ParseWarningType = FirstPartySetsHandler::ParseWarningType;
using ParseWarning = FirstPartySetsHandler::ParseWarning;

constexpr char kPrimaryField[] = "primary";
constexpr char kAssociatedSitesField[] = "associatedSites";
constexpr char kCctldsField[] = "ccTLDs";
constexpr char kReplacementsField[] = "replacements";
constexpr char kAdditionsField[] = "additions";
constexpr char kParsedSuccessfullyHistogram[] =
    "Cookie.FirstPartySets.ComponentSetsParsedSuccessfully";
constexpr char kNonfatalErrorsHistogram[] =
    "Cookie.FirstPartySets.ComponentSetsNonfatalErrors";
constexpr char kProcessedComponentHistogram[] =
    "Cookie.FirstPartySets.ProcessedEntireComponent";

const base::Version kVersion("1.0");

const net::GlobalFirstPartySets kEmptySets =
    net::GlobalFirstPartySets(kVersion, /*entries=*/{}, /*aliases=*/{});

}  // namespace

net::GlobalFirstPartySets ParseSets(const std::string& sets) {
  std::istringstream stream(sets);
  return FirstPartySetParser::ParseSetsFromStream(stream, kVersion,
                                                  /*emit_errors=*/false,
                                                  /*emit_metrics=*/true);
}

TEST(FirstPartySetParser, RejectsNonemptyMalformed) {
  // If the input isn't valid JSON, we should
  // reject it.
  base::HistogramTester histogram_tester;
  EXPECT_EQ(ParseSets("certainly not valid JSON"), kEmptySets);
  EXPECT_EQ(histogram_tester.GetTotalSum(kParsedSuccessfullyHistogram), 0);
  EXPECT_EQ(histogram_tester.GetTotalSum(kNonfatalErrorsHistogram), 0);
  histogram_tester.ExpectUniqueSample(kProcessedComponentHistogram,
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/1);
}

TEST(FirstPartySetParser, AcceptsTrivial) {
  base::HistogramTester histogram_tester;
  EXPECT_EQ(ParseSets(""), kEmptySets);
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
  EXPECT_EQ(
      ParseSets(
          R"({"primary": "https://example.test", "associatedSites": []})"),
      kEmptySets);
}

TEST(FirstPartySetParser, AcceptsMinimal_Associated) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite aaaa(GURL("https://aaaa.test"));

  EXPECT_EQ(
      ParseSets(R"({"primary": "https://example.test",)"
                R"("associatedSites": ["https://aaaa.test"]})"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)},
              {aaaa,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)},
          },
          {}));
}

TEST(FirstPartySetParser, AcceptsMinimal_Service) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite aaaa(GURL("https://aaaa.test"));

  EXPECT_EQ(
      ParseSets(R"({"primary": "https://example.test",)"
                R"("serviceSites": ["https://aaaa.test"]})"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)},
              {aaaa, net::FirstPartySetEntry(example, net::SiteType::kService,
                                             std::nullopt)},
          },
          {}));
}

TEST(FirstPartySetParser, AcceptsMinimal_AllSubsets_WithCcTLDs) {
  base::HistogramTester histogram_tester;
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite example_cctld(GURL("https://example.cctld"));
  net::SchemefulSite a(GURL("https://a.test"));
  net::SchemefulSite a_cctld(GURL("https://a.cctld"));
  net::SchemefulSite b(GURL("https://b.test"));
  net::SchemefulSite b_cctld(GURL("https://b.cctld"));

  EXPECT_EQ(
      ParseSets(R"({"primary": "https://example.test",)"
                R"("associatedSites": ["https://a.test"],)"
                R"("serviceSites": ["https://b.test"],)"
                R"("ccTLDs": {)"
                R"("https://example.test": ["https://example.cctld"],)"
                R"("https://a.test": ["https://a.cctld"],)"
                R"("https://b.test": ["https://b.cctld"])"
                R"(})"
                R"(})"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)},
              {a,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)},
              {b, net::FirstPartySetEntry(example, net::SiteType::kService,
                                          std::nullopt)},
          },
          {{example_cctld, example}, {a_cctld, a}, {b_cctld, b}}));
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
  EXPECT_EQ(ParseSets(R"({"associatedSites": ["https://aaaa.test"]})"),
            kEmptySets);
  histogram_tester.ExpectUniqueSample(kProcessedComponentHistogram,
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/1);
}

TEST(FirstPartySetParser, RejectsTypeUnsafePrimary) {
  EXPECT_EQ(
      ParseSets(R"({ "primary": 3, "associatedSites": ["https://aaaa.test"]})"),
      kEmptySets);
}

TEST(FirstPartySetParser, RejectsNonHTTPSPrimary) {
  EXPECT_EQ(ParseSets(R"({"primary": "http://example.test",)"
                      R"("associatedSites": ["https://aaaa.test"]})"),
            kEmptySets);
}

TEST(FirstPartySetParser, NonOriginPrimary) {
  EXPECT_EQ(
      ParseSets(R"({"primary": "example", "associatedSites": )"
                R"(["https://associatedsite1.test"]})"
                "\n"
                R"({"primary": "https://example2.test", "associatedSites": )"
                R"(["https://associatedsite2.test"]})"),
      kEmptySets);
}

TEST(FirstPartySetParser, PrimaryIsTLD) {
  const net::SchemefulSite example2(GURL("https://example2.test"));
  const net::SchemefulSite associated2(GURL("https://associatedsite2.test"));

  EXPECT_EQ(
      ParseSets(R"({"primary": "https://example", "associatedSites": )"
                R"(["https://associatedsite1.test"]})"
                "\n"
                R"({"primary": "https://example2.test", "associatedSites": )"
                R"(["https://associatedsite2.test"]})"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example2, net::FirstPartySetEntry(
                             example2, net::SiteType::kPrimary, std::nullopt)},
              {associated2, net::FirstPartySetEntry(
                                example2, net::SiteType::kAssociated, 0)},
          },
          {}));
}

TEST(FirstPartySetParser, PrimaryIsIPAddress) {
  base::HistogramTester histogram_tester;
  net::SchemefulSite example2(GURL("https://example2.test"));
  net::SchemefulSite associated2(GURL("https://associatedsite2.test"));
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite aaaa(GURL("https://aaaa.test"));

  EXPECT_EQ(
      ParseSets(
          R"({"primary": "https://127.0.0.1:1234", "associatedSites": ["https://aaaa.test"]})"
          "\n"
          R"({"primary": "https://example2.test", "associatedSites": )"
          R"(["https://associatedsite2.test"]})"
          "\n"
          R"({"primary": "https://example.test",)"
          R"("associatedSites": ["https://aaaa.test"]})"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example2, net::FirstPartySetEntry(
                             example2, net::SiteType::kPrimary, std::nullopt)},
              {associated2, net::FirstPartySetEntry(
                                example2, net::SiteType::kAssociated, 0)},
              {example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)},
              {aaaa,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)},
          },
          {}));
  histogram_tester.ExpectUniqueSample(
      kParsedSuccessfullyHistogram, /*sample=*/2, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kNonfatalErrorsHistogram, /*sample=*/1,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kProcessedComponentHistogram,
                                      /*sample=*/1,
                                      /*expected_bucket_count=*/1);
}

TEST(FirstPartySetParser, PrimaryHasNoTLD) {
  const net::SchemefulSite example2(GURL("https://example2.test"));
  const net::SchemefulSite associated2(GURL("https://associatedsite2.test"));

  base::HistogramTester histogram_tester;
  EXPECT_EQ(
      ParseSets(R"({"primary": "https://example.test..", "associatedSites": )"
                R"(["https://associatedsite1.test"]})"
                "\n"
                R"({"primary": "https://example2.test", "associatedSites": )"
                R"(["https://associatedsite2.test"]})"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example2, net::FirstPartySetEntry(
                             example2, net::SiteType::kPrimary, std::nullopt)},
              {associated2, net::FirstPartySetEntry(
                                example2, net::SiteType::kAssociated, 0)},
          },
          {}));
  histogram_tester.ExpectUniqueSample(
      kParsedSuccessfullyHistogram, /*sample=*/1, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kNonfatalErrorsHistogram, /*sample=*/1,
                                      /*expected_bucket_count=*/1);
}

TEST(FirstPartySetParser, RejectsMissingAssociatedSites) {
  EXPECT_EQ(ParseSets(R"({"primary": "https://example.test" })"), kEmptySets);
}

TEST(FirstPartySetParser, RejectsTypeUnsafeAssociatedSites) {
  EXPECT_EQ(ParseSets(R"({"primary": "https://example.test", )"
                      R"("associatedSites": ["https://aaaa.test", 4]})"),
            kEmptySets);
}

TEST(FirstPartySetParser, RejectsNonHTTPSAssociatedSite) {
  EXPECT_EQ(ParseSets(R"({"primary": "https://example.test",)"
                      R"("associatedSites": ["http://aaaa.test"]})"),
            kEmptySets);
}

TEST(FirstPartySetParser, NonOriginAssociatedSite) {
  EXPECT_EQ(
      ParseSets(R"({"primary": "https://example1.test", "associatedSites": )"
                R"(["associatedsite1"]})"
                "\n"
                R"({"primary": "https://example2.test", "associatedSites": )"
                R"(["https://associatedsite2.test"]})"),
      kEmptySets);
}

TEST(FirstPartySetParser, AssociatedSiteIsTLD) {
  const net::SchemefulSite example(GURL("https://example.test"));
  const net::SchemefulSite example2(GURL("https://example2.test"));
  const net::SchemefulSite associated1(GURL("https://associatedsite1.test"));
  const net::SchemefulSite associated2(GURL("https://associatedsite2.test"));

  EXPECT_EQ(
      ParseSets(R"({"primary": "https://example.test", "associatedSites": [)"
                R"("https://associated", "https://associatedsite1.test"]})"
                "\n"
                R"({"primary": "https://example2.test", "associatedSites": )"
                R"(["https://associatedsite2.test"]})"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)},
              {associated1,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 1)},
              {example2, net::FirstPartySetEntry(
                             example2, net::SiteType::kPrimary, std::nullopt)},
              {associated2, net::FirstPartySetEntry(
                                example2, net::SiteType::kAssociated, 0)},
          },
          {}));
}

TEST(FirstPartySetParser, AssociatedSiteIsIPAddress) {
  const net::SchemefulSite example(GURL("https://example.test"));
  const net::SchemefulSite example2(GURL("https://example2.test"));
  const net::SchemefulSite associated1(GURL("https://associatedsite1.test"));
  const net::SchemefulSite associated2(GURL("https://associatedsite2.test"));

  EXPECT_EQ(
      ParseSets(R"({"primary": "https://example.test", "associatedSites": [)"
                R"("https://127.0.0.1:1234", "https://associatedsite1.test"]})"
                "\n"
                R"({"primary": "https://example2.test", "associatedSites": )"
                R"(["https://associatedsite2.test"]})"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)},
              {associated1,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 1)},
              {example2, net::FirstPartySetEntry(
                             example2, net::SiteType::kPrimary, std::nullopt)},
              {associated2, net::FirstPartySetEntry(
                                example2, net::SiteType::kAssociated, 0)},
          },
          {}));
}

TEST(FirstPartySetParser, AssociatedSiteHasNoTLD) {
  const net::SchemefulSite example(GURL("https://example.test"));
  const net::SchemefulSite example2(GURL("https://example2.test"));
  const net::SchemefulSite example3(GURL("https://example3.test"));
  const net::SchemefulSite associated1(GURL("https://associatedsite1.test"));
  const net::SchemefulSite associated2(GURL("https://associatedsite2.test"));
  const net::SchemefulSite associated3(GURL("https://associatedsite3.test"));

  EXPECT_EQ(
      ParseSets(R"({"primary": "https://example.test", "associatedSites": [)"
                R"("https://aaaa.test..", "https://associatedsite1.test"]})"
                "\n"
                R"({"primary": "https://example2.test", "associatedSites": )"
                R"(["https://associatedsite2.test"]})"
                "\n"
                R"({"primary": "https://example3.test", "associatedSites": )"
                R"(["https://associatedsite3.test"]})"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)},
              {associated1,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 1)},
              {example2, net::FirstPartySetEntry(
                             example2, net::SiteType::kPrimary, std::nullopt)},
              {associated2, net::FirstPartySetEntry(
                                example2, net::SiteType::kAssociated, 0)},
              {example3, net::FirstPartySetEntry(
                             example3, net::SiteType::kPrimary, std::nullopt)},
              {associated3, net::FirstPartySetEntry(
                                example3, net::SiteType::kAssociated, 0)},
          },
          {}));
}

TEST(FirstPartySetParser, TruncatesSubdomain_Primary) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite aaaa(GURL("https://aaaa.test"));

  EXPECT_EQ(
      ParseSets(R"({"primary": "https://subdomain.example.test", )"
                R"("associatedSites": ["https://aaaa.test"]})"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)},
              {aaaa,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)},
          },
          {}));
}

TEST(FirstPartySetParser, TruncatesPrimaryInvalidWithAlias) {
  // Regression test for https://crbug.com/1510152.
  //
  // The primary and first service site get truncated down to the same TLD, so
  // they get marked as invalid. Since the primary is invalid, that means we
  // have to delete the whole set (including any aliases).
  EXPECT_EQ(
      ParseSets(
          R"({"primary": "https://subdomain1..test",)"
          R"("serviceSites": ["https://subdomain2..test","https://foo.test"],)"
          R"("ccTLDs": {"https://foo.test": ["https://foo.cctld"]}})"),
      net::GlobalFirstPartySets(kVersion, {}, {}));
}

TEST(FirstPartySetParser, TruncatesSubdomain_AssociatedSite) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite aaaa(GURL("https://aaaa.test"));

  EXPECT_EQ(
      ParseSets(R"({"primary": "https://example.test", )"
                R"("associatedSites": ["https://subdomain.aaaa.test"]})"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)},
              {aaaa,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)},
          },
          {}));
}

TEST(FirstPartySetParser, TruncatesSubdomain_RepeatedDomain) {
  const net::SchemefulSite example(GURL("https://example.test"));
  const net::SchemefulSite example2(GURL("https://example2.test"));
  const net::SchemefulSite bbbb(GURL("https://bbbb.test"));
  const net::SchemefulSite cccc(GURL("https://cccc.test"));

  // The first set is valid iff aaaa.test is on the Public Suffix List. Since
  // that invariant is not under the control of whatever provides the
  // First-Party Sets data, the whole list of sets should not be invalidated if
  // that invariant fails as a result of PSL changes.
  EXPECT_EQ(
      ParseSets(R"({"primary": "https://example.test", )"
                R"("associatedSites": [)"
                R"("https://subdomain1.aaaa.test", )"
                R"("https://subdomain2.aaaa.test", )"
                R"("https://bbbb.test"]})"
                "\n"
                R"({"primary": "https://example2.test", )"
                R"("associatedSites": [)"
                R"("https://cccc.test"]})"
                "\n"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)},
              {bbbb,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 2)},
              {example2, net::FirstPartySetEntry(
                             example2, net::SiteType::kPrimary, std::nullopt)},
              {cccc, net::FirstPartySetEntry(example2,
                                             net::SiteType::kAssociated, 0)},
          },
          {}));
}

TEST(FirstPartySetParser, TruncatesSubdomain_NondisjointSets) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite example2(GURL("https://example2.test"));
  net::SchemefulSite example3(GURL("https://example3.test"));
  net::SchemefulSite example3_cctld(GURL("https://example3.cctld"));
  net::SchemefulSite aaaa(GURL("https://aaaa.test"));
  net::SchemefulSite bbbb(GURL("https://bbbb.test"));
  net::SchemefulSite cccc(GURL("https://cccc.test"));

  // These sets are disjoint iff aaaa.test is on the Public Suffix List. Since
  // that invariant is not under the control of whatever provides the
  // First-Party Sets data, the whole list of sets should not be invalidated if
  // that invariant fails as a result of PSL changes.
  //
  // Note that when "invalid" domains are removed from sets, we have to re-scan
  // to find and delete singleton sets.
  //
  // Note also that if the sets are nondisjoint for reasons unrelated to the
  // PSL, then the whole list should be considered invalid; see other test
  // cases.
  EXPECT_EQ(
      ParseSets(R"({"primary": "https://example3.test", )"
                R"("associatedSites": [)"
                R"("https://subdomain3.aaaa.test"]})"
                "\n"
                R"({"primary": "https://example.test", )"
                R"("associatedSites": [)"
                R"("https://subdomain.aaaa.test", "https://bbbb.test"]})"
                "\n"
                R"({"primary": "https://example2.test", )"
                R"("associatedSites": [)"
                R"("https://subdomain2.aaaa.test", "https://cccc.test"]})"
                "\n"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)},
              {bbbb,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 1)},
              {example2, net::FirstPartySetEntry(
                             example2, net::SiteType::kPrimary, std::nullopt)},
              {cccc, net::FirstPartySetEntry(example2,
                                             net::SiteType::kAssociated, 1)},
          },
          {}));

  // example3.test's set should lose its associated site and alias, but should
  // not be removed (since it does not become a singleton).
  EXPECT_EQ(
      ParseSets(R"({"primary": "https://example3.test", )"
                R"("associatedSites": [)"
                R"("https://subdomain3.aaaa.test"],)"
                R"("ccTLDs": {)"
                R"("https://aaaa.test": ["https://subdomain3.aaaa.cctld"],)"
                R"("https://example3.test": ["https://example3.cctld"])"
                "}"
                "}"
                "\n"
                R"({"primary": "https://example.test", )"
                R"("associatedSites": [)"
                R"("https://subdomain.aaaa.test", "https://bbbb.test"]})"
                "\n"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)},
              {bbbb,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 1)},
              {example3, net::FirstPartySetEntry(
                             example3, net::SiteType::kPrimary, std::nullopt)},
          },
          {{example3_cctld, example3}}));

  // This is a scenario where an invalid alias in one set gets removed, and that
  // set becomes a singleton and is removed as a result.
  EXPECT_EQ(
      ParseSets(
          R"({"primary": "https://example3.test", )"
          R"("ccTLDs": {)"
          R"("https://example3.test": ["https://subdomain1.example3.cctld"])"
          "}"
          "}"
          "\n"
          R"({"primary": "https://example.test", )"
          R"("associatedSites": [)"
          R"("https://subdomain2.example3.cctld", "https://bbbb.test"]})"
          "\n"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)},
              {bbbb,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 1)},
          },
          {}));
}

TEST(FirstPartySetParser, AcceptsMultipleSets) {
  base::HistogramTester histogram_tester;
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite associated2(GURL("https://associatedsite2.test"));
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated1(GURL("https://associatedsite1.test"));

  EXPECT_EQ(
      ParseSets("{\"primary\": \"https://example.test\", \"associatedSites\": "
                "[\"https://associatedsite1.test\"]}\n"
                "{\"primary\": \"https://foo.test\", \"associatedSites\": "
                "[\"https://associatedsite2.test\"]}"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)},
              {associated1,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)},
              {foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                            std::nullopt)},
              {associated2,
               net::FirstPartySetEntry(foo, net::SiteType::kAssociated, 0)},
          },
          {}));
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
  EXPECT_EQ(
      ParseSets(R"(
      {"primary": "https://example.test", "associatedSites": ["https://associatedsite1.test"]}

      {"primary": "https://foo.test", "associatedSites": ["https://associatedsite2.test"]}
    )"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)},
              {associated1,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)},
              {foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                            std::nullopt)},
              {associated2,
               net::FirstPartySetEntry(foo, net::SiteType::kAssociated, 0)},
          },
          {}));
  histogram_tester.ExpectUniqueSample(
      kParsedSuccessfullyHistogram, /*sample=*/2, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kNonfatalErrorsHistogram, /*sample=*/0,
                                      /*expected_bucket_count=*/1);
}

TEST(FirstPartySetParser, RejectsInvalidSets_InvalidPrimary) {
  base::HistogramTester histogram_tester;
  EXPECT_EQ(
      ParseSets(
          R"({"primary": 3, "associatedSites": ["https://associatedsite1.test"]}
    {"primary": "https://foo.test",)"
          R"("associatedSites": ["https://associatedsite2.test"]})"),
      kEmptySets);
  EXPECT_EQ(histogram_tester.GetTotalSum(kParsedSuccessfullyHistogram), 0);
}

TEST(FirstPartySetParser, RejectsInvalidSets_InvalidAssociatedSite) {
  EXPECT_EQ(
      ParseSets(R"({"primary": "https://example.test", "associatedSites": [3]}
    {"primary": "https://foo.test",)"
                R"("associatedSites": ["https://associatedsite2.test"]})"),
      kEmptySets);
}

TEST(FirstPartySetParser, AllowsTrailingCommas) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated1(GURL("https://associatedsite1.test"));
  EXPECT_EQ(
      ParseSets(R"({"primary": "https://example.test", )"
                R"("associatedSites": ["https://associatedsite1.test"],})"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)},
              {associated1,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)},
          },
          {}));
}

TEST(FirstPartySetParser, Rejects_SamePrimary) {
  EXPECT_EQ(
      ParseSets(R"({"primary": "https://example.test",)"
                R"("associatedSites": ["https://associatedsite1.test"]}
    {"primary": "https://example.test",)"
                R"("associatedSites": ["https://associatedsite2.test"]})"),
      kEmptySets);
}

TEST(FirstPartySetParser, Rejects_AssociatedSiteAsPrimary) {
  EXPECT_EQ(
      ParseSets(R"({"primary": "https://example.test",)"
                R"("associatedSites": ["https://associatedsite1.test"]}
    {"primary": "https://associatedsite1.test",)"
                R"("associatedSites": ["https://associatedsite2.test"]})"),
      kEmptySets);
}

TEST(FirstPartySetParser, Rejects_SameAssociatedSite) {
  EXPECT_EQ(
      ParseSets(
          R"({"primary": "https://example.test",)"
          R"("associatedSites": ["https://associatedsite1.test"]}
    {"primary": "https://foo.test",)"
          R"("associatedSites": )"
          R"(["https://associatedsite1.test", "https://associatedsite2.test"]})"),
      kEmptySets);
}

TEST(FirstPartySetParser, Rejects_PrimaryAsAssociatedSite) {
  EXPECT_EQ(
      ParseSets(R"({"primary": "https://example.test",)"
                R"("associatedSites": ["https://associatedsite1.test"]}
    {"primary": "https://example2.test", )"
                R"("associatedSites":)"
                R"(["https://example.test", "https://associatedsite2.test"]})"),
      kEmptySets);
}

TEST(FirstPartySetParser, Accepts_ccTLDAliases) {
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite foo_cctld(GURL("https://foo.cctld"));
  net::SchemefulSite associated1(GURL("https://associatedsite1.test"));
  net::SchemefulSite associated1_cctld1(GURL("https://associatedsite1.cctld1"));
  net::SchemefulSite associated1_cctld2(GURL("https://associatedsite1.cctld2"));
  net::SchemefulSite associated2(GURL("https://associatedsite2.test"));
  net::SchemefulSite example(GURL("https://example.test"));

  EXPECT_EQ(
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
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)},
              {associated1,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)},
              {foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                            std::nullopt)},
              {associated2,
               net::FirstPartySetEntry(foo, net::SiteType::kAssociated, 0)},
          },
          {
              {associated1_cctld1, associated1},
              {associated1_cctld2, associated1},
              {foo_cctld, foo},
          }));
}

TEST(FirstPartySetParser, Rejects_NonSchemefulSiteCcTLDAliases) {
  EXPECT_EQ(
      ParseSets(
          "{"                                                               //
          "\"primary\": \"https://example.test\","                          //
          "\"associatedSites\": [\"https://associatedsite1.test\"],"        //
          "\"ccTLDs\": {"                                                   //
          "\"https://associatedsite1.test\": [\"associatedsite1.cctld1\"]"  //
          "}"                                                               //
          "}"),
      kEmptySets);
}

TEST(FirstPartySetParser, Rejects_NondisjointCcTLDAliases) {
  // These two sets overlap only via a ccTLD variant.
  EXPECT_EQ(ParseSets("{"                                                 //
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
            kEmptySets);
}

TEST(FirstPartySetParser, Logs_MultipleRejections) {
  // 2 rejections should show up on the histogram as separate instances
  base::HistogramTester histogram_tester;
  EXPECT_EQ(ParseSets("certainly not valid JSON"), kEmptySets);
  EXPECT_EQ(ParseSets("also not valid JSON"), kEmptySets);
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
  EXPECT_EQ(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .first.value(),
      FirstPartySetsOverridesPolicy(net::SetsMutation({}, {})));
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
  EXPECT_EQ(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .first.value(),
      FirstPartySetsOverridesPolicy(net::SetsMutation({}, {})));
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
  const net::SchemefulSite primary2(GURL("https://primary2.test"));
  const net::SchemefulSite primary3(GURL("https://primary3.test"));
  const net::SchemefulSite associated2(GURL("https://associatedsite2.test"));
  const net::SchemefulSite associated3(GURL("https://associatedsite3.test"));

  // The invalid primary invalidates the set, but other sets are still parsed.
  base::Value policy_value = base::JSONReader::Read(R"(
                {
                "replacements": [
                  {
                    "primary": "https://primary1.test..",
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
  EXPECT_EQ(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .first.value(),
      FirstPartySetsOverridesPolicy(net::SetsMutation(
          {
              {
                  {primary2,
                   net::FirstPartySetEntry(primary2, net::SiteType::kPrimary,
                                           std::nullopt)},
                  {associated2,
                   net::FirstPartySetEntry(primary2, net::SiteType::kAssociated,
                                           std::nullopt)},
              },
          },
          {
              {
                  {primary3,
                   net::FirstPartySetEntry(primary3, net::SiteType::kPrimary,
                                           std::nullopt)},
                  {associated3,
                   net::FirstPartySetEntry(primary3, net::SiteType::kAssociated,
                                           std::nullopt)},
              },
          })));
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
     AssociatedSiteNonRegisteredDomain) {
  const net::SchemefulSite primary1(GURL("https://primary1.test"));
  const net::SchemefulSite primary2(GURL("https://primary2.test"));
  const net::SchemefulSite associated1(GURL("https://associatedsite1.test"));
  const net::SchemefulSite associated2(GURL("https://associatedsite2.test"));
  const net::SchemefulSite associated3(GURL("https://associatedsite3.test"));

  base::Value policy_value = base::JSONReader::Read(R"(
              {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": [
                      "https://associatedsite1.test..",
                      "https://associatedsite2.test"
                    ]
                  }
                ],
                "additions": [
                  {
                    "primary": "https://primary2.test",
                    "associatedSites": ["https://associatedsite3"]
                  }
                ]
              }
            )")
                                 .value();

  // The invalid associated site is ignored, but the rest of the set is still
  // processed. If the set becomes a singleton as a result of ignoring a member
  // site, the set is ignored entirely.
  EXPECT_EQ(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .first.value(),
      FirstPartySetsOverridesPolicy(net::SetsMutation(
          {
              {
                  {primary1,
                   net::FirstPartySetEntry(primary1, net::SiteType::kPrimary,
                                           std::nullopt)},
                  {associated2,
                   net::FirstPartySetEntry(primary1, net::SiteType::kAssociated,
                                           std::nullopt)},
              },
          },
          {})));
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
     SingletonSetError_EmptyAssociatedSites) {
  const net::SchemefulSite primary2(GURL("https://primary2.test"));
  const net::SchemefulSite associated2(GURL("https://associatedsite2.test"));

  base::Value policy_value = base::JSONReader::Read(R"(
             {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": []
                  },
                  {
                    "primary": "https://primary2.test",
                    "associatedSites": ["https://associatedsite2.test"]
                  }
                ],
                "additions": []
              }
            )")
                                 .value();
  EXPECT_EQ(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .first.value(),
      FirstPartySetsOverridesPolicy(net::SetsMutation(
          {
              {
                  {primary2,
                   net::FirstPartySetEntry(primary2, net::SiteType::kPrimary,
                                           std::nullopt)},
                  {associated2,
                   net::FirstPartySetEntry(primary2, net::SiteType::kAssociated,
                                           std::nullopt)},
              },
          },
          {})));
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
                    "primary": "http://primary2.test",
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
      ParseError(ParseErrorType::kNonHttpsScheme,
                 {kAdditionsField, 0, kPrimaryField}));

  // The ParseWarning in the ccTLDs field of "additions[0]" isn't added since
  // the error arises first.
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
  EXPECT_EQ(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .first.value(),
      FirstPartySetsOverridesPolicy(net::SetsMutation(
          {{
               {primary1, net::FirstPartySetEntry(
                              primary1, net::SiteType::kPrimary, std::nullopt)},
               {associated_site1,
                net::FirstPartySetEntry(primary1, net::SiteType::kAssociated,
                                        std::nullopt)},
           },
           {
               {primary2, net::FirstPartySetEntry(
                              primary2, net::SiteType::kPrimary, std::nullopt)},
               {associated_site2,
                net::FirstPartySetEntry(primary2, net::SiteType::kAssociated,
                                        std::nullopt)},
           }},
          {})));
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
  EXPECT_EQ(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .first.value(),
      FirstPartySetsOverridesPolicy(net::SetsMutation(
          {{
               {primary1, net::FirstPartySetEntry(
                              primary1, net::SiteType::kPrimary, std::nullopt)},
               {associated_site1,
                net::FirstPartySetEntry(primary1, net::SiteType::kAssociated,
                                        std::nullopt)},
           },
           {
               {primary2, net::FirstPartySetEntry(
                              primary2, net::SiteType::kPrimary, std::nullopt)},
               {associated_site2,
                net::FirstPartySetEntry(primary2, net::SiteType::kAssociated,
                                        std::nullopt)},
           }},
          {{
              {primary3, net::FirstPartySetEntry(
                             primary3, net::SiteType::kPrimary, std::nullopt)},
              {associatedSite3,
               net::FirstPartySetEntry(primary3, net::SiteType::kAssociated,
                                       std::nullopt)},
          }})));
  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .second,
      IsEmpty());
}

TEST(FirstPartySetParser_ParseSetsFromEnterprisePolicyTest,
     SuccessfulMapping_CrossList_TruncatesNonDisjoint) {
  net::SchemefulSite primary1(GURL("https://primary1.test"));
  net::SchemefulSite associated1(GURL("https://associated1.test"));
  net::SchemefulSite primary2(GURL("https://primary2.test"));
  net::SchemefulSite associated2(GURL("https://associated2.test"));
  net::SchemefulSite primary3(GURL("https://primary3.test"));
  net::SchemefulSite associated3(GURL("https://associated3.test"));

  // The following sets are disjoint iff aaaa.test is on the Public Suffix List.
  base::Value policy_value = base::JSONReader::Read(R"(
                {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": [
                      "https://associated1.test",
                      "https://subdomain1.aaaa.test"
                    ]
                  },
                  {
                    "primary": "https://primary2.test",
                    "associatedSites": [
                      "https://associated2.test",
                      "https://subdomain2.aaaa.test"
                    ]
                  }
                ],
                "additions": [
                  {
                    "primary": "https://primary3.test",
                    "associatedSites": [
                      "https://associated3.test",
                      "https://subdomain3.aaaa.test"
                    ]
                  }
                ]
              }
            )")
                                 .value();
  EXPECT_EQ(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .first.value(),
      FirstPartySetsOverridesPolicy(net::SetsMutation(
          {{
               {primary1, net::FirstPartySetEntry(
                              primary1, net::SiteType::kPrimary, std::nullopt)},
               {associated1,
                net::FirstPartySetEntry(primary1, net::SiteType::kAssociated,
                                        std::nullopt)},
           },
           {
               {primary2, net::FirstPartySetEntry(
                              primary2, net::SiteType::kPrimary, std::nullopt)},
               {associated2,
                net::FirstPartySetEntry(primary2, net::SiteType::kAssociated,
                                        std::nullopt)},
           }},
          {{
              {primary3, net::FirstPartySetEntry(
                             primary3, net::SiteType::kPrimary, std::nullopt)},
              {associated3,
               net::FirstPartySetEntry(primary3, net::SiteType::kAssociated,
                                       std::nullopt)},
          }})));
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
  EXPECT_EQ(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .first.value(),
      FirstPartySetsOverridesPolicy(net::SetsMutation(
          {{
               {primary1, net::FirstPartySetEntry(
                              primary1, net::SiteType::kPrimary, std::nullopt)},
               {associated_site1,
                net::FirstPartySetEntry(primary1, net::SiteType::kAssociated,
                                        std::nullopt)},
               {associated_site1_cctld,
                net::FirstPartySetEntry(primary1, net::SiteType::kAssociated,
                                        std::nullopt)},
           },
           {
               {primary2, net::FirstPartySetEntry(
                              primary2, net::SiteType::kPrimary, std::nullopt)},
               {primary2_cctld,
                net::FirstPartySetEntry(primary2, net::SiteType::kPrimary,
                                        std::nullopt)},
               {associated_site2,
                net::FirstPartySetEntry(primary2, net::SiteType::kAssociated,
                                        std::nullopt)},
           }},
          {})));

  EXPECT_THAT(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .second,
      std::vector<ParseWarning>{ParseWarning(
          ParseWarningType::kCctldKeyNotCanonical,
          {kReplacementsField, 0, kCctldsField, "https://not_in_set.test"})});
}

TEST(FirstPartySetParser, RespectsAssociatedSiteLimit) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite a(GURL("https://a.test"));
  net::SchemefulSite b(GURL("https://b.test"));
  net::SchemefulSite c(GURL("https://c.test"));
  net::SchemefulSite d(GURL("https://d.test"));
  net::SchemefulSite e(GURL("https://e.test"));

  EXPECT_EQ(
      ParseSets(
          R"({"primary": "https://example.test",)"
          R"("associatedSites": ["https://a.test", "https://b.test",)"
          R"("https://c.test", "https://d.test", "https://e.test", "https://f.test"],)"
          R"(})"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)},
              {a,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)},
              {b,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 1)},
              {c,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 2)},
              {d,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 3)},
              {e,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 4)},
          },
          {}));
}

TEST(FirstPartySetParser, DetectsErrorsPastAssociatedSiteLimit) {
  EXPECT_EQ(
      ParseSets(R"({"primary": "https://example.test",)"
                R"("associatedSites": ["https://a.test", "not a domain"],)"
                R"(})"),
      kEmptySets);
}

TEST(FirstPartySetParser, ServiceSitesAreNotCountedAgainstAssociatedSiteLimit) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite a(GURL("https://a.test"));
  net::SchemefulSite b(GURL("https://b.test"));
  net::SchemefulSite c(GURL("https://c.test"));
  net::SchemefulSite d(GURL("https://d.test"));
  net::SchemefulSite e(GURL("https://e.test"));
  net::SchemefulSite f(GURL("https://f.test"));
  net::SchemefulSite g(GURL("https://g.test"));

  EXPECT_EQ(
      ParseSets(R"({)"
                R"("primary": "https://example.test",)"
                R"("associatedSites": ["https://a.test", "https://d.test",)"
                R"("https://e.test", "https://f.test", "https://g.test"],)"
                R"("serviceSites": ["https://b.test", "https://c.test"],)"
                R"(})"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)},
              {a,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)},
              {d,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 1)},
              {e,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 2)},
              {f,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 3)},
              {g,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 4)},
              {b, net::FirstPartySetEntry(example, net::SiteType::kService,
                                          std::nullopt)},
              {c, net::FirstPartySetEntry(example, net::SiteType::kService,
                                          std::nullopt)},
          },
          {}));
}

TEST(FirstPartySetParser, AliasesAreNotCountedAgainstAssociatedSiteLimit) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite a(GURL("https://a.test"));
  net::SchemefulSite b(GURL("https://b.test"));
  net::SchemefulSite c(GURL("https://c.test"));
  net::SchemefulSite d(GURL("https://d.test"));
  net::SchemefulSite e(GURL("https://e.test"));
  net::SchemefulSite a_cctld1(GURL("https://a.cctld1"));
  net::SchemefulSite a_cctld2(GURL("https://a.cctld2"));

  EXPECT_EQ(
      ParseSets(
          R"({)"
          R"("primary": "https://example.test",)"
          R"("associatedSites": ["https://a.test", "https://b.test",)"
          R"("https://c.test", "https://d.test", "https://e.test"],)"
          R"("ccTLDs": {)"
          R"(  "https://a.test": ["https://a.cctld1", "https://a.cctld2"])"
          R"(})"
          R"(})"),
      net::GlobalFirstPartySets(
          kVersion,
          {
              {example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)},
              {a,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)},
              {b,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 1)},
              {c,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 2)},
              {d,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 3)},
              {e,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 4)},
          },
          {{a_cctld1, a}, {a_cctld2, a}}));
}

TEST(FirstPartySetParser, EnterprisePolicies_ExemptFromAssociatedSiteLimit) {
  net::SchemefulSite primary1(GURL("https://primary1.test"));
  net::SchemefulSite associated1(GURL("https://associated1.test"));
  net::SchemefulSite associated2(GURL("https://associated2.test"));
  net::SchemefulSite associated3(GURL("https://associated3.test"));
  net::SchemefulSite associated4(GURL("https://associated4.test"));
  net::SchemefulSite associated5(GURL("https://associated5.test"));
  net::SchemefulSite associated6(GURL("https://associated6.test"));

  base::Value policy_value = base::JSONReader::Read(R"(
             {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": [
                      "https://associated1.test",
                      "https://associated2.test",
                      "https://associated3.test",
                      "https://associated4.test",
                      "https://associated5.test",
                      "https://associated6.test"
                    ]
                  }
                ]
              }
            )")
                                 .value();
  EXPECT_EQ(
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy_value.GetDict())
          .first.value(),
      FirstPartySetsOverridesPolicy(net::SetsMutation(
          {{
              {primary1, net::FirstPartySetEntry(
                             primary1, net::SiteType::kPrimary, std::nullopt)},
              {associated1,
               net::FirstPartySetEntry(primary1, net::SiteType::kAssociated,
                                       std::nullopt)},
              {associated2,
               net::FirstPartySetEntry(primary1, net::SiteType::kAssociated,
                                       std::nullopt)},
              {associated3,
               net::FirstPartySetEntry(primary1, net::SiteType::kAssociated,
                                       std::nullopt)},
              {associated4,
               net::FirstPartySetEntry(primary1, net::SiteType::kAssociated,
                                       std::nullopt)},
              {associated5,
               net::FirstPartySetEntry(primary1, net::SiteType::kAssociated,
                                       std::nullopt)},
              {associated6,
               net::FirstPartySetEntry(primary1, net::SiteType::kAssociated,
                                       std::nullopt)},
          }},
          {})));
}

TEST(FirstPartySetParser, ParseFromCommandLine_Invalid_MultipleSets) {
  EXPECT_THAT(FirstPartySetParser::ParseFromCommandLine(
                  R"({"primary": "https://primary1.test",)"
                  R"("associatedSites": ["https://associated1.test"]})"
                  "\n"
                  R"({"primary": "https://primary2.test",)"
                  R"("associatedSites": ["https://associated2.test"]})"),
              IsEmpty());
}

TEST(FirstPartySetParser, ParseFromCommandLine_Invalid_Singleton) {
  EXPECT_THAT(FirstPartySetParser::ParseFromCommandLine(
                  R"({"primary": "https://primary1.test"})"),
              IsEmpty());
}

TEST(FirstPartySetParser,
     ParseFromCommandLine_Valid_MultipleSubsetsAndAliases) {
  net::SchemefulSite primary(GURL("https://primary.test"));
  net::SchemefulSite associated1(GURL("https://associated1.test"));
  net::SchemefulSite associated2(GURL("https://associated2.test"));
  net::SchemefulSite associated2_cctld(GURL("https://associated2.cctld"));
  net::SchemefulSite service(GURL("https://service.test"));

  net::LocalSetDeclaration local_set =
      FirstPartySetParser::ParseFromCommandLine(
          R"({"primary": "https://primary.test",)"
          R"("associatedSites":)"
          R"(["https://associated1.test", "https://associated2.test"],)"
          R"("serviceSites": ["https://service.test"],)"
          R"("ccTLDs": {)"
          R"(  "https://associated2.test": ["https://associated2.cctld"])"
          R"(})"
          R"(})");

  EXPECT_THAT(
      local_set.entries(),
      UnorderedElementsAre(
          Pair(primary, net::FirstPartySetEntry(
                            primary, net::SiteType::kPrimary, std::nullopt)),
          Pair(associated1,
               net::FirstPartySetEntry(primary, net::SiteType::kAssociated, 0)),
          Pair(associated2,
               net::FirstPartySetEntry(primary, net::SiteType::kAssociated, 1)),
          Pair(service, net::FirstPartySetEntry(
                            primary, net::SiteType::kService, std::nullopt))));

  EXPECT_THAT(local_set.aliases(),
              UnorderedElementsAre(Pair(associated2_cctld, associated2)));
}

}  // namespace content

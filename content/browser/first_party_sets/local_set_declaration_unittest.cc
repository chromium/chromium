// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/local_set_declaration.h"

#include <string>

#include "base/containers/flat_map.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/browser/first_party_sets/local_set_declaration.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace content {

MATCHER_P3(SetIs, primary_matcher, set_matcher, aliases_matcher, "") {
  const LocalSetDeclaration& local_set = arg;
  const net::SchemefulSite& primary = local_set.GetPrimary();
  const FirstPartySetParser::SingleSet& set = local_set.GetSet();
  const FirstPartySetParser::Aliases& aliases = local_set.GetAliases();
  return testing::ExplainMatchResult(primary_matcher, primary,
                                     result_listener) &&
         testing::ExplainMatchResult(set_matcher, set, result_listener) &&
         testing::ExplainMatchResult(aliases_matcher, aliases, result_listener);
}

TEST(LocalSetDeclarationTest, Invalid_EmptyString) {
  EXPECT_THAT(LocalSetDeclaration(""), IsEmpty());
}

TEST(LocalSetDeclarationTest, Invalid_MultipleSets) {
  EXPECT_THAT(LocalSetDeclaration(
                  R"({"primary": "https://primary1.test",)"
                  R"("associatedSites": ["https://associated1.test"]})"
                  "\n"
                  R"({"primary": "https://primary2.test",)"
                  R"("associatedSites": ["https://associated2.test"]})"),
              IsEmpty());
}

TEST(LocalSetDeclarationTest, Valid_Basic) {
  net::SchemefulSite primary(GURL("https://primary.test"));
  net::SchemefulSite associated(GURL("https://associated.test"));

  EXPECT_THAT(
      LocalSetDeclaration(R"({"primary": "https://primary.test",)"
                          R"("associatedSites": ["https://associated.test"]})"),
      SetIs(primary,
            UnorderedElementsAre(
                Pair(primary,
                     net::FirstPartySetEntry(primary, net::SiteType::kPrimary,
                                             absl::nullopt)),
                Pair(associated, net::FirstPartySetEntry(
                                     primary, net::SiteType::kAssociated, 0))),
            IsEmpty()));
}

TEST(LocalSetDeclarationTest, Valid_MultipleSubsetsAndAliases) {
  net::SchemefulSite primary(GURL("https://primary.test"));
  net::SchemefulSite associated1(GURL("https://associated1.test"));
  net::SchemefulSite associated2(GURL("https://associated2.test"));
  net::SchemefulSite associated2_cctld(GURL("https://associated2.cctld"));
  net::SchemefulSite service(GURL("https://service.test"));

  EXPECT_THAT(
      LocalSetDeclaration(
          R"({"primary": "https://primary.test",)"
          R"("associatedSites":)"
          R"(["https://associated1.test", "https://associated2.test"],)"
          R"("serviceSites": ["https://service.test"],)"
          R"("ccTLDs": {)"
          R"(  "https://associated2.test": ["https://associated2.cctld"])"
          R"(})"
          R"(})"),
      SetIs(primary,
            UnorderedElementsAre(
                Pair(primary,
                     net::FirstPartySetEntry(primary, net::SiteType::kPrimary,
                                             absl::nullopt)),
                Pair(associated1, net::FirstPartySetEntry(
                                      primary, net::SiteType::kAssociated, 0)),
                Pair(associated2, net::FirstPartySetEntry(
                                      primary, net::SiteType::kAssociated, 1)),
                Pair(service,
                     net::FirstPartySetEntry(primary, net::SiteType::kService,
                                             absl::nullopt))),
            UnorderedElementsAre(Pair(associated2_cctld, associated2))));
}

}  // namespace content

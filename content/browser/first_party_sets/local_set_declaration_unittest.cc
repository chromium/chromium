// Copyright 2022 The Chromium Authors. All rights reserved.
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

MATCHER_P2(SetIs, primary_matcher, set_matcher, "") {
  const LocalSetDeclaration& local_set = arg;
  const net::SchemefulSite& primary = local_set.GetPrimary();
  const FirstPartySetParser::SingleSet& set = local_set.GetSet();
  return testing::ExplainMatchResult(primary_matcher, primary,
                                     result_listener) &&
         testing::ExplainMatchResult(set_matcher, set, result_listener);
}

TEST(LocalSetDeclarationTest, Invalid_Singleton) {
  EXPECT_THAT(LocalSetDeclaration("https://primary.test"), IsEmpty());
}

TEST(LocalSetDeclarationTest, Invalid_NotOrigins) {
  EXPECT_THAT(LocalSetDeclaration("https://primary.test,associated"),
              IsEmpty());
}

TEST(LocalSetDeclarationTest, Invalid_NotHTTPS) {
  EXPECT_THAT(
      LocalSetDeclaration("https://primary.test,http://associated.test"),
      IsEmpty());
}

TEST(LocalSetDeclarationTest, Invalid_RegisteredDomain_Primary) {
  EXPECT_THAT(LocalSetDeclaration(
                  "https://www.primary.test..,https://www.associated.test"),
              IsEmpty());
}

TEST(LocalSetDeclarationTest, Invalid_RegisteredDomain_Associated) {
  EXPECT_THAT(LocalSetDeclaration(
                  "https://www.primary.test,https://www.associated.test.."),
              IsEmpty());
}

TEST(LocalSetDeclarationTest, Valid_SingleAssociatedSite) {
  net::SchemefulSite primary(GURL("https://primary.test"));
  net::SchemefulSite associated(GURL("https://associated.test"));

  EXPECT_THAT(
      LocalSetDeclaration("https://primary.test,https://associated.test"),
      SetIs(primary, UnorderedElementsAre(
                         Pair(primary, net::FirstPartySetEntry(
                                           primary, net::SiteType::kPrimary,
                                           absl::nullopt)),
                         Pair(associated,
                              net::FirstPartySetEntry(
                                  primary, net::SiteType::kAssociated, 0)))));
}

TEST(LocalSetDeclarationTest, Valid_SingleAssociatedSite_RegisteredDomain) {
  net::SchemefulSite primary(GURL("https://primary.test"));
  net::SchemefulSite associated(GURL("https://associated.test"));

  EXPECT_THAT(
      LocalSetDeclaration(
          "https://www.primary.test,https://www.associated.test"),
      SetIs(primary, UnorderedElementsAre(
                         Pair(primary, net::FirstPartySetEntry(
                                           primary, net::SiteType::kPrimary,
                                           absl::nullopt)),
                         Pair(associated,
                              net::FirstPartySetEntry(
                                  primary, net::SiteType::kAssociated, 0)))));
}

TEST(LocalSetDeclarationTest, Valid_MultipleAssociatedSites) {
  net::SchemefulSite primary(GURL("https://primary.test"));
  net::SchemefulSite associated1(GURL("https://associated1.test"));
  net::SchemefulSite associated2(GURL("https://associated2.test"));

  EXPECT_THAT(
      LocalSetDeclaration("https://primary.test,https://"
                          "associated1.test,https://associated2.test"),
      SetIs(
          primary,
          UnorderedElementsAre(
              Pair(primary,
                   net::FirstPartySetEntry(primary, net::SiteType::kPrimary,
                                           absl::nullopt)),
              Pair(associated1, net::FirstPartySetEntry(
                                    primary, net::SiteType::kAssociated, 0)),
              Pair(associated2, net::FirstPartySetEntry(
                                    primary, net::SiteType::kAssociated, 1)))));
}

TEST(LocalSetDeclarationTest, Invalid_PrimaryIsOnlyAssociatedSite) {
  EXPECT_THAT(LocalSetDeclaration("https://primary.test,https://primary.test"),
              IsEmpty());
}

TEST(LocalSetDeclarationTest, Valid_RepeatedPrimary) {
  net::SchemefulSite primary(GURL("https://primary.test"));
  net::SchemefulSite associated(GURL("https://associated.test"));

  EXPECT_THAT(
      LocalSetDeclaration(
          "https://primary.test,https://primary.test,https://associated.test"),
      SetIs(primary, UnorderedElementsAre(
                         Pair(primary, net::FirstPartySetEntry(
                                           primary, net::SiteType::kPrimary,
                                           absl::nullopt)),
                         Pair(associated,
                              net::FirstPartySetEntry(
                                  primary, net::SiteType::kAssociated, 0)))));
}

TEST(LocalSetDeclarationTest, Valid_RepeatedAssociatedSite) {
  net::SchemefulSite primary(GURL("https://primary.test"));
  net::SchemefulSite associated1(GURL("https://associated1.test"));
  net::SchemefulSite associated2(GURL("https://associated2.test"));

  EXPECT_THAT(
      LocalSetDeclaration(R"(https://primary.test,
https://associated1.test,
https://associated2.test,
https://associated1.test)"),
      SetIs(
          primary,
          UnorderedElementsAre(
              Pair(primary,
                   net::FirstPartySetEntry(primary, net::SiteType::kPrimary,
                                           absl::nullopt)),
              Pair(associated1, net::FirstPartySetEntry(
                                    primary, net::SiteType::kAssociated, 0)),
              Pair(associated2, net::FirstPartySetEntry(
                                    primary, net::SiteType::kAssociated, 1)))));
}

}  // namespace content

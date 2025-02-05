// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/local_set_declaration.h"

#include <optional>

#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/sets_mutation.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace net {

TEST(LocalSetDeclarationTest, Valid_EmptySet) {
  EXPECT_THAT(LocalSetDeclaration(), IsEmpty());
}

TEST(LocalSetDeclarationTest, Valid_Basic) {
  SchemefulSite primary(GURL("https://primary.test"));
  SchemefulSite associated(GURL("https://associated.test"));

  base::flat_map<SchemefulSite, FirstPartySetEntry> entries({
      {primary, FirstPartySetEntry(primary, SiteType::kPrimary, std::nullopt)},
      {associated, FirstPartySetEntry(primary, SiteType::kAssociated, 0)},
  });

  EXPECT_THAT(
      LocalSetDeclaration(entries, /*aliases=*/{}).ComputeMutation(),
      SetsMutation(
          /*replacement_sets=*/
          {
              {
                  {primary, FirstPartySetEntry(primary, SiteType::kPrimary,
                                               std::nullopt)},
                  {associated,
                   FirstPartySetEntry(primary, SiteType::kAssociated, 0)},
              },
          },
          /*addition_sets=*/{}, /*aliases=*/{}));
}

TEST(LocalSetDeclarationTest, Valid_BasicWithAliases) {
  SchemefulSite primary(GURL("https://primary.test"));
  SchemefulSite primary_cctld(GURL("https://primary.cctld"));
  SchemefulSite associated(GURL("https://associated.test"));
  SchemefulSite associated_cctld(GURL("https://associated.cctld"));

  base::flat_map<SchemefulSite, FirstPartySetEntry> entries({
      {primary, FirstPartySetEntry(primary, SiteType::kPrimary, std::nullopt)},
      {associated, FirstPartySetEntry(primary, SiteType::kAssociated, 0)},
  });

  base::flat_map<SchemefulSite, SchemefulSite> aliases(
      {{primary_cctld, primary}, {associated_cctld, associated}});

  LocalSetDeclaration local_set(entries, aliases);

  // LocalSetDeclaration should allow these to pass through, after passing
  // validation.
  EXPECT_THAT(
      local_set.ComputeMutation(),
      SetsMutation(
          /*replacement_sets=*/
          {
              {
                  {primary, FirstPartySetEntry(primary, SiteType::kPrimary,
                                               std::nullopt)},
                  {primary_cctld,
                   FirstPartySetEntry(primary, SiteType::kPrimary,
                                      std::nullopt)},
                  {associated,
                   FirstPartySetEntry(primary, SiteType::kAssociated, 0)},
                  {associated_cctld,
                   FirstPartySetEntry(primary, SiteType::kAssociated, 0)},
              },
          },
          /*addition_sets=*/{}, /*aliases=*/
          {
              {associated_cctld, associated},
              {primary_cctld, primary},
          }));
}

}  // namespace net

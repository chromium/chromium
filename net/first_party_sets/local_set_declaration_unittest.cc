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
using ::testing::Optional;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

namespace net {

TEST(LocalSetDeclarationTest, Valid_EmptySet) {
  EXPECT_THAT(LocalSetDeclaration(), IsEmpty());
  EXPECT_THAT(LocalSetDeclaration::Create({}, {}), Optional(IsEmpty()));
}

TEST(LocalSetDeclarationTest, Valid_Basic) {
  SchemefulSite primary(GURL("https://primary.test"));
  SchemefulSite associated(GURL("https://associated.test"));

  base::flat_map<SchemefulSite, FirstPartySetEntry> entries({
      {primary, FirstPartySetEntry(primary, SiteType::kPrimary)},
      {associated, FirstPartySetEntry(primary, SiteType::kAssociated)},
  });

  LocalSetDeclaration local_set_declaration =
      LocalSetDeclaration::Create(entries, /*aliases=*/{}).value();

  EXPECT_THAT(
      local_set_declaration.ComputeMutation(),
      SetsMutation(
          /*replacement_sets=*/
          {
              {
                  {primary, FirstPartySetEntry(primary, SiteType::kPrimary)},
                  {associated,
                   FirstPartySetEntry(primary, SiteType::kAssociated)},
              },
          },
          /*addition_sets=*/{}, /*aliases=*/{}));
  EXPECT_THAT(local_set_declaration, SizeIs(2));
}

TEST(LocalSetDeclarationTest, Valid_BasicWithAliases) {
  SchemefulSite primary(GURL("https://primary.test"));
  SchemefulSite primary_cctld(GURL("https://primary.cctld"));
  SchemefulSite associated(GURL("https://associated.test"));
  SchemefulSite associated_cctld(GURL("https://associated.cctld"));

  base::flat_map<SchemefulSite, FirstPartySetEntry> entries({
      {primary, FirstPartySetEntry(primary, SiteType::kPrimary)},
      {associated, FirstPartySetEntry(primary, SiteType::kAssociated)},
  });

  base::flat_map<SchemefulSite, SchemefulSite> aliases(
      {{primary_cctld, primary}, {associated_cctld, associated}});

  LocalSetDeclaration local_set_declaration =
      LocalSetDeclaration::Create(entries, aliases).value();

  // LocalSetDeclaration should allow these to pass through, after passing
  // validation.
  EXPECT_THAT(
      local_set_declaration.ComputeMutation(),
      SetsMutation(
          /*replacement_sets=*/
          {
              {
                  {primary, FirstPartySetEntry(primary, SiteType::kPrimary)},
                  {primary_cctld,
                   FirstPartySetEntry(primary, SiteType::kPrimary)},
                  {associated,
                   FirstPartySetEntry(primary, SiteType::kAssociated)},
                  {associated_cctld,
                   FirstPartySetEntry(primary, SiteType::kAssociated)},
              },
          },
          /*addition_sets=*/{}, /*aliases=*/
          {
              {associated_cctld, associated},
              {primary_cctld, primary},
          }));

  EXPECT_THAT(local_set_declaration, SizeIs(4));
}

TEST(LocalSetDeclarationTest, Invalid) {
  SchemefulSite primary(GURL("https://primary.test"));
  SchemefulSite primary_cctld(GURL("https://primary.cctld"));
  SchemefulSite primary2(GURL("https://primary2.test"));
  SchemefulSite associated(GURL("https://associated.test"));
  SchemefulSite associated_cctld(GURL("https://associated.cctld"));
  SchemefulSite associated2(GURL("https://associated2.test"));
  SchemefulSite associated2_cctld(GURL("https://associated2.cctld"));

  // All aliases must refer to a canonical site that has an entry in the set.
  EXPECT_FALSE(LocalSetDeclaration::Create(
      {
          {primary, FirstPartySetEntry(primary, SiteType::kPrimary)},
          {associated, FirstPartySetEntry(primary, SiteType::kAssociated)},
      },
      {{associated2_cctld, associated2}}));

  // An alias must not have an explicit entry, even one that matches the
  // canonical's entry.
  FirstPartySetEntry associated_entry(primary, SiteType::kAssociated);
  EXPECT_FALSE(LocalSetDeclaration::Create(
      {
          {primary, FirstPartySetEntry(primary, SiteType::kPrimary)},
          {associated, associated_entry},
          {associated_cctld, associated_entry},
      },
      {{associated_cctld, associated}}));

  // No singleton sets.
  EXPECT_FALSE(LocalSetDeclaration::Create(
      {
          {primary, FirstPartySetEntry(primary, SiteType::kPrimary)},
      },
      {}));

  // Multiple sets aren't supported.
  EXPECT_FALSE(LocalSetDeclaration::Create(
      {
          {primary, FirstPartySetEntry(primary, SiteType::kPrimary)},
          {primary2, FirstPartySetEntry(primary2, SiteType::kPrimary)},
          {associated, FirstPartySetEntry(primary, SiteType::kAssociated)},
          {associated2, FirstPartySetEntry(primary2, SiteType::kAssociated)},
      },
      {}));
}

}  // namespace net

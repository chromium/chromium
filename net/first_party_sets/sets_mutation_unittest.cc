// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/sets_mutation.h"

#include <optional>

#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace net {

TEST(SetsMutationTest, Valid) {
  const SchemefulSite primary1(GURL("https://primary1.test"));
  const SchemefulSite primary1_cctld(GURL("https://primary1.cctld"));
  const SchemefulSite associated1(GURL("https://associated1.test"));
  const SchemefulSite associated1_cctld(GURL("https://associated1.ccltd"));
  const SchemefulSite primary2(GURL("https://primary2.test"));
  const SchemefulSite associated2(GURL("https://associated2.test"));
  const SchemefulSite associated2_cctld(GURL("https://associated2.ccltd"));

  std::ignore = SetsMutation(
      /*replacement_sets=*/
      {
          {
              {primary1, FirstPartySetEntry(primary1, SiteType::kPrimary)},
              {associated1,
               FirstPartySetEntry(primary1, SiteType::kAssociated)},
              {primary1_cctld,
               FirstPartySetEntry(primary1, SiteType::kPrimary)},
              {associated1_cctld,
               FirstPartySetEntry(primary1, SiteType::kAssociated)},
          },
          {
              {primary2, FirstPartySetEntry(primary2, SiteType::kPrimary)},
              {associated2,
               FirstPartySetEntry(primary2, SiteType::kAssociated)},
              {associated2_cctld,
               FirstPartySetEntry(primary2, SiteType::kAssociated)},
          },
      },
      /*addition_sets=*/{}, /*aliases=*/
      {
          {primary1_cctld, primary1},
          {associated1_cctld, associated1},
          {associated2_cctld, associated2},
      });

  std::ignore = SetsMutation(
      /*replacement_sets=*/{},
      /*addition_sets=*/
      {
          {
              {primary1, FirstPartySetEntry(primary1, SiteType::kPrimary)},
              {associated1,
               FirstPartySetEntry(primary1, SiteType::kAssociated)},
              {primary1_cctld,
               FirstPartySetEntry(primary1, SiteType::kPrimary)},
              {associated1_cctld,
               FirstPartySetEntry(primary1, SiteType::kAssociated)},
          },
          {
              {primary2, FirstPartySetEntry(primary2, SiteType::kPrimary)},
              {associated2,
               FirstPartySetEntry(primary2, SiteType::kAssociated)},
              {associated2_cctld,
               FirstPartySetEntry(primary2, SiteType::kAssociated)},
          },
      },
      /*aliases=*/
      {
          {primary1_cctld, primary1},
          {associated1_cctld, associated1},
          {associated2_cctld, associated2},
      });

  std::ignore = SetsMutation(
      /*replacement_sets=*/
      {
          {
              {primary1, FirstPartySetEntry(primary1, SiteType::kPrimary)},
              {associated1,
               FirstPartySetEntry(primary1, SiteType::kAssociated)},
              {primary1_cctld,
               FirstPartySetEntry(primary1, SiteType::kPrimary)},
              {associated1_cctld,
               FirstPartySetEntry(primary1, SiteType::kAssociated)},
          },
      },
      /*addition_sets=*/
      {
          {
              {primary2, FirstPartySetEntry(primary2, SiteType::kPrimary)},
              {associated2,
               FirstPartySetEntry(primary2, SiteType::kAssociated)},
              {associated2_cctld,
               FirstPartySetEntry(primary2, SiteType::kAssociated)},
          },
      },
      /*aliases=*/
      {
          {primary1_cctld, primary1},
          {associated1_cctld, associated1},
          {associated2_cctld, associated2},
      });
}

#if defined(GTEST_HAS_DEATH_TEST)
TEST(SetsMutationTest, Nondisjoint_death) {
  const SchemefulSite primary1(GURL("https://primary1.test"));
  const SchemefulSite associated1(GURL("https://associated1.test"));
  const SchemefulSite primary2(GURL("https://primary2.test"));
  const SchemefulSite associated2(GURL("https://associated2.test"));

  EXPECT_DEATH(
      {
        SetsMutation(
            /*replacement_sets=*/
            {
                {
                    {primary1,
                     FirstPartySetEntry(primary1, SiteType::kPrimary)},
                    {associated1,
                     FirstPartySetEntry(primary1, SiteType::kAssociated)},
                },
                {
                    {primary2,
                     FirstPartySetEntry(primary2, SiteType::kPrimary)},
                    {associated1,
                     FirstPartySetEntry(primary2, SiteType::kAssociated)},
                    {associated2,
                     FirstPartySetEntry(primary2, SiteType::kAssociated)},
                },
            },
            /*addition_sets=*/{}, /*aliases=*/{});
      },
      "");
}
#endif  // defined(GTEST_HAS_DEATH_TEST)

}  // namespace net

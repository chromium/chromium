// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/settings/password/model/password_list_sorting_filtering.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/ui/affiliated_group.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::password_manager::AffiliatedGroup;
using ::testing::ElementsAre;

// Creates an `AffiliatedGroup` with the given `display_name`.
AffiliatedGroup CreateAffiliatedGroup(std::string display_name) {
  return AffiliatedGroup(
      /*credentials=*/{},
      affiliations::FacetBrandingInfo{.name = std::move(display_name)});
}

// Returns the display names of `groups` in order.
std::vector<std::string> GetDisplayNames(
    base::span<const AffiliatedGroup* const> groups) {
  std::vector<std::string> names;
  names.reserve(groups.size());
  for (const AffiliatedGroup* group : groups) {
    names.push_back(group->GetDisplayName());
  }
  return names;
}

// Tests that `SortAffiliatedGroups()` sorts mixed-case display names in
// ascending and descending alphabetical order without mutating the input.
TEST(PasswordListSortingFilteringTest, SortAffiliatedGroupsByName) {
  const std::vector<AffiliatedGroup> groups = {
      CreateAffiliatedGroup("Spotify"),
      CreateAffiliatedGroup("apple"),
      CreateAffiliatedGroup("Banana"),
  };

  EXPECT_THAT(GetDisplayNames(SortAffiliatedGroups(
                  groups, PasswordSortOrder::kNameAscending)),
              ElementsAre("apple", "Banana", "Spotify"));

  EXPECT_THAT(GetDisplayNames(SortAffiliatedGroups(
                  groups, PasswordSortOrder::kNameDescending)),
              ElementsAre("Spotify", "Banana", "apple"));

  EXPECT_TRUE(SortAffiliatedGroups(base::span<const AffiliatedGroup>(),
                                   PasswordSortOrder::kNameAscending)
                  .empty());
}

// Tests that `SortAffiliatedGroups()` strips URL schemes before comparing
// display names and preserves relative order of ties in both sort orders.
TEST(PasswordListSortingFilteringTest, SortAffiliatedGroupsStripsURLScheme) {
  const std::vector<AffiliatedGroup> groups = {
      CreateAffiliatedGroup("http://zebra.com"),
      CreateAffiliatedGroup("https://alpha.org"),
      CreateAffiliatedGroup("beta.net"),
      CreateAffiliatedGroup("http://alpha.org"),
  };

  EXPECT_THAT(GetDisplayNames(SortAffiliatedGroups(
                  groups, PasswordSortOrder::kNameAscending)),
              ElementsAre("https://alpha.org", "http://alpha.org", "beta.net",
                          "http://zebra.com"));

  EXPECT_THAT(GetDisplayNames(SortAffiliatedGroups(
                  groups, PasswordSortOrder::kNameDescending)),
              ElementsAre("http://zebra.com", "beta.net", "https://alpha.org",
                          "http://alpha.org"));
}

}  // namespace

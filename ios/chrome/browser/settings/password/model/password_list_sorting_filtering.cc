// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/settings/password/model/password_list_sorting_filtering.h"

#include <algorithm>
#include <cstddef>
#include <string_view>

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "components/password_manager/core/browser/ui/affiliated_group.h"
#include "url/url_constants.h"

namespace {

// Returns `display_name` with any URL scheme prefix (up to and including
// `url::kStandardSchemeSeparator`) removed.
std::string_view DisplayNameWithoutScheme(std::string_view display_name) {
  constexpr std::string_view kSeparator(url::kStandardSchemeSeparator);
  const size_t position = display_name.find(kSeparator);
  if (position == std::string_view::npos) {
    return display_name;
  }
  return display_name.substr(position + kSeparator.size());
}

}  // namespace

std::vector<const password_manager::AffiliatedGroup*> SortAffiliatedGroups(
    base::span<const password_manager::AffiliatedGroup> affiliated_groups,
    PasswordSortOrder sort_order) {
  std::vector<const password_manager::AffiliatedGroup*> sorted_groups;
  sorted_groups.reserve(affiliated_groups.size());
  for (const password_manager::AffiliatedGroup& group : affiliated_groups) {
    sorted_groups.push_back(&group);
  }

  // Mirrors `PasswordsGrouper::GetAffiliatedGroupsWithGroupingInfo()` (omitting
  // fallback URI schemes and comparing ASCII case-insensitively) so
  // `PasswordSortOrder::kNameAscending` matches `SavedPasswordsPresenter`'s
  // default ordering.
  std::ranges::stable_sort(
      sorted_groups,
      [sort_order](const password_manager::AffiliatedGroup* lhs,
                   const password_manager::AffiliatedGroup* rhs) {
        const int comparison = base::CompareCaseInsensitiveASCII(
            DisplayNameWithoutScheme(lhs->GetDisplayName()),
            DisplayNameWithoutScheme(rhs->GetDisplayName()));
        switch (sort_order) {
          case PasswordSortOrder::kNameAscending:
            return comparison < 0;
          case PasswordSortOrder::kNameDescending:
            return comparison > 0;
        }
        NOTREACHED();
      });
  return sorted_groups;
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_PASSWORD_MODEL_PASSWORD_LIST_SORTING_FILTERING_H_
#define IOS_CHROME_BROWSER_SETTINGS_PASSWORD_MODEL_PASSWORD_LIST_SORTING_FILTERING_H_

#include <vector>

#include "base/containers/span.h"

namespace password_manager {
class AffiliatedGroup;
}  // namespace password_manager

// Sort orders supported by the Password Manager credential list.
enum class PasswordSortOrder {
  // Sorts credentials alphabetically by display name (A to Z).
  kNameAscending,
  // Sorts credentials in reverse alphabetical order by display name (Z to A).
  kNameDescending,
};

// Returns pointers into `affiliated_groups` sorted according to `sort_order`.
// Returned pointers remain valid only while the underlying buffer of
// `affiliated_groups` is alive and unmodified.
std::vector<const password_manager::AffiliatedGroup*> SortAffiliatedGroups(
    base::span<const password_manager::AffiliatedGroup> affiliated_groups,
    PasswordSortOrder sort_order);

#endif  // IOS_CHROME_BROWSER_SETTINGS_PASSWORD_MODEL_PASSWORD_LIST_SORTING_FILTERING_H_

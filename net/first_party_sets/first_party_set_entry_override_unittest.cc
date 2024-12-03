// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/first_party_set_entry_override.h"

#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

TEST(FirstPartySetEntryOverrideTest, IsDeletion_true) {
  EXPECT_TRUE(FirstPartySetEntryOverride().IsDeletion());
}

TEST(FirstPartySetEntryOverrideTest, IsDeletion_false) {
  EXPECT_FALSE(
      FirstPartySetEntryOverride(
          FirstPartySetEntry(SchemefulSite(GURL("https://example.test")),
                             SiteType::kPrimary, std::nullopt))
          .IsDeletion());
}

TEST(FirstPartySetEntryOverrideTest, GetEntry) {
  FirstPartySetEntry entry(SchemefulSite(GURL("https://example.test")),
                           SiteType::kPrimary, std::nullopt);
  EXPECT_EQ(FirstPartySetEntryOverride(entry).GetEntry(), entry);
}

}  // namespace net

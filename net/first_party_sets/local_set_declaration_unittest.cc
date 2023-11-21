// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/local_set_declaration.h"

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

namespace net {

TEST(LocalSetDeclarationTest, Valid_EmptySet) {
  EXPECT_THAT(LocalSetDeclaration(), IsEmpty());
}

TEST(LocalSetDeclarationTest, Valid_Basic) {
  SchemefulSite primary(GURL("https://primary.test"));
  SchemefulSite associated(GURL("https://associated.test"));

  base::flat_map<SchemefulSite, FirstPartySetEntry> entries({
      {primary, FirstPartySetEntry(primary, SiteType::kPrimary, absl::nullopt)},
      {associated, FirstPartySetEntry(primary, SiteType::kAssociated, 0)},
  });

  EXPECT_THAT(LocalSetDeclaration(entries).entries(),
              UnorderedElementsAre(
                  Pair(primary, FirstPartySetEntry(primary, SiteType::kPrimary,
                                                   absl::nullopt)),
                  Pair(associated,
                       FirstPartySetEntry(primary, SiteType::kAssociated, 0))));
}

}  // namespace net

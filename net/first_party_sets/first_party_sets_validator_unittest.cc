// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/first_party_sets_validator.h"

#include <utility>
#include <vector>

#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

const SchemefulSite kPrimary(GURL("https://primary.test"));
const SchemefulSite kPrimaryCctld(GURL("https://primary.ccltd"));
const SchemefulSite kPrimary2(GURL("https://primary2.test"));
const SchemefulSite kAssociated1(GURL("https://associated1.test"));
const SchemefulSite kAssociated2(GURL("https://associated2.test"));
const SchemefulSite kService(GURL("https://service.test"));

}  // namespace

TEST(FirstPartySetsValidator, Default) {
  FirstPartySetsValidator validator;
  EXPECT_TRUE(validator.IsValid());
  EXPECT_FALSE(validator.IsSitePrimaryValid(kPrimary));
}

TEST(FirstPartySetsValidator, Valid) {
  // This is a valid RWSs.
  std::vector<std::pair<SchemefulSite, SchemefulSite>> sets({
      {kAssociated1, kPrimary},
      {kPrimary, kPrimary},
      {kService, kPrimary2},
      {kPrimary2, kPrimary2},
  });
  FirstPartySetsValidator validator;
  for (const auto& [site, primary] : sets) {
    validator.Update(site, primary);
  }

  EXPECT_TRUE(validator.IsValid());
  EXPECT_TRUE(validator.IsSitePrimaryValid(kPrimary));
  EXPECT_TRUE(validator.IsSitePrimaryValid(kPrimary2));
}

TEST(FirstPartySetsValidator, Invalid_Singleton) {
  // `kPrimary` is a singleton.
  std::vector<std::pair<SchemefulSite, SchemefulSite>> sets({
      {kPrimary, kPrimary},
      {kService, kPrimary2},
      {kPrimary2, kPrimary2},
  });
  FirstPartySetsValidator validator;
  for (const auto& [site, primary] : sets) {
    validator.Update(site, primary);
  }

  EXPECT_FALSE(validator.IsValid());
  EXPECT_FALSE(validator.IsSitePrimaryValid(kPrimary));
  EXPECT_TRUE(validator.IsSitePrimaryValid(kPrimary2));
}

TEST(FirstPartySetsValidator, Invalid_Orphan) {
  // `kAssociated1` is an orphan.
  std::vector<std::pair<SchemefulSite, SchemefulSite>> sets({
      {kAssociated1, kPrimary},
      {kService, kPrimary2},
      {kPrimary2, kPrimary2},
  });
  FirstPartySetsValidator validator;
  for (const auto& [site, primary] : sets) {
    validator.Update(site, primary);
  }

  EXPECT_FALSE(validator.IsValid());
  EXPECT_FALSE(validator.IsSitePrimaryValid(kPrimary));
  EXPECT_TRUE(validator.IsSitePrimaryValid(kPrimary2));
}

}  // namespace net

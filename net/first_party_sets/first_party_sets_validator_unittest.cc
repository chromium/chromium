// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/first_party_sets_validator.h"

#include <initializer_list>
#include <utility>
#include <vector>

#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

const SchemefulSite kPrimary1(GURL("https://primary1.test"));
const SchemefulSite kPrimary1Cctld(GURL("https://primary1.ccltd"));
const SchemefulSite kPrimary2(GURL("https://primary2.test"));
const SchemefulSite kPrimary3(GURL("https://primary3.test"));
const SchemefulSite kAssociated1(GURL("https://associated1.test"));
const SchemefulSite kAssociated2(GURL("https://associated2.test"));
const SchemefulSite kAssociated3(GURL("https://associated3.test"));
const SchemefulSite kService1(GURL("https://service1.test"));
const SchemefulSite kService2(GURL("https://service2.test"));

struct SiteEntry {
  SchemefulSite site;
  SchemefulSite primary;
};

FirstPartySetsValidator ValidateSets(std::initializer_list<SiteEntry> sites) {
  FirstPartySetsValidator validator;
  for (const auto& site_entry : sites) {
    validator.Update(site_entry.site, site_entry.primary);
  }
  return validator;
}

}  // namespace

TEST(FirstPartySetsValidator, Default) {
  FirstPartySetsValidator validator;
  EXPECT_TRUE(validator.IsValid());
  EXPECT_FALSE(validator.IsSitePrimaryValid(kPrimary1));
}

TEST(FirstPartySetsValidator, Valid) {
  // This is a valid RWSs.
  FirstPartySetsValidator validator = ValidateSets({
      {kAssociated1, kPrimary1},
      {kPrimary1, kPrimary1},

      {kService1, kPrimary2},
      {kPrimary2, kPrimary2},
  });

  EXPECT_TRUE(validator.IsValid());
  EXPECT_TRUE(validator.IsSitePrimaryValid(kPrimary1));
  EXPECT_TRUE(validator.IsSitePrimaryValid(kPrimary2));
}

TEST(FirstPartySetsValidator, Invalid_Singleton) {
  // `kPrimary1` is a singleton.
  FirstPartySetsValidator validator = ValidateSets({
      {kPrimary1, kPrimary1},

      {kService1, kPrimary2},
      {kPrimary2, kPrimary2},
  });

  EXPECT_FALSE(validator.IsValid());
  EXPECT_FALSE(validator.IsSitePrimaryValid(kPrimary1));
  EXPECT_TRUE(validator.IsSitePrimaryValid(kPrimary2));
}

TEST(FirstPartySetsValidator, Invalid_Orphan) {
  // `kAssociated1` is an orphan.
  FirstPartySetsValidator validator = ValidateSets({
      {kAssociated1, kPrimary1},

      {kService1, kPrimary2},
      {kPrimary2, kPrimary2},
  });

  EXPECT_FALSE(validator.IsValid());
  EXPECT_FALSE(validator.IsSitePrimaryValid(kPrimary1));
  EXPECT_TRUE(validator.IsSitePrimaryValid(kPrimary2));
}

TEST(FirstPartySetsValidator, Invalid_Nondisjoint) {
  // `kAssociated1` is listed in more than one set.
  FirstPartySetsValidator validator = ValidateSets({
      {kAssociated1, kPrimary1},
      {kService1, kPrimary1},
      {kPrimary1, kPrimary1},

      {kAssociated1, kPrimary2},
      {kService2, kPrimary2},
      {kPrimary2, kPrimary2},

      {kAssociated3, kPrimary3},
      {kPrimary3, kPrimary3},
  });

  EXPECT_FALSE(validator.IsValid());
  EXPECT_FALSE(validator.IsSitePrimaryValid(kPrimary1));
  EXPECT_FALSE(validator.IsSitePrimaryValid(kPrimary2));
  EXPECT_TRUE(validator.IsSitePrimaryValid(kPrimary3));
}

}  // namespace net

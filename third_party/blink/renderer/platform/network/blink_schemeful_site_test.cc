// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/network/blink_schemeful_site.h"

#include "base/memory/scoped_refptr.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

// BlinkSchemefulSites created from the same "site" should match. Different
// sites should not.
TEST(BlinkSchemefulSiteTest, SameSiteEquality) {
  scoped_refptr<SecurityOrigin> origin1_site1 =
      SecurityOrigin::CreateFromString("https://example.com");
  scoped_refptr<SecurityOrigin> origin2_site1 =
      SecurityOrigin::CreateFromString("https://sub.example.com");
  scoped_refptr<SecurityOrigin> origin1_site2 =
      SecurityOrigin::CreateFromString("https://other.com");

  BlinkSchemefulSite schemeful_site_1(origin1_site1);
  BlinkSchemefulSite schemeful_site_1_2(origin1_site1);

  EXPECT_EQ(schemeful_site_1, schemeful_site_1_2);

  BlinkSchemefulSite schemeful_site_2(origin2_site1);

  EXPECT_EQ(schemeful_site_1, schemeful_site_2);

  BlinkSchemefulSite schemeful_site_3(origin1_site2);

  EXPECT_NE(schemeful_site_3, schemeful_site_1);

  scoped_refptr<SecurityOrigin> opaque_origin1 =
      SecurityOrigin::CreateUniqueOpaque();
  scoped_refptr<SecurityOrigin> opaque_origin2 =
      SecurityOrigin::CreateUniqueOpaque();

  EXPECT_EQ(opaque_origin1, opaque_origin1);
  EXPECT_NE(opaque_origin1, opaque_origin2);
}

// BlinkSchemefulSites created from the  different origin constructors (for the
// same site) should match.
TEST(BlinkSchemefulSiteTest, ConstructorEquality) {
  scoped_refptr<SecurityOrigin> security_origin_1 =
      SecurityOrigin::CreateFromString("https://www.example.com");
  url::Origin origin_1 = url::Origin::Create(GURL("https://www.example.com"));

  BlinkSchemefulSite security_origin_site_1 =
      BlinkSchemefulSite(security_origin_1);
  BlinkSchemefulSite origin_site_1 = BlinkSchemefulSite(origin_1);

  EXPECT_EQ(security_origin_site_1, origin_site_1);

  scoped_refptr<SecurityOrigin> security_origin_2 =
      SecurityOrigin::CreateFromString("https://www.other.com");
  url::Origin origin_2 = url::Origin::Create(GURL("https://www.other.com"));

  BlinkSchemefulSite security_origin_site_2 =
      BlinkSchemefulSite(security_origin_2);
  BlinkSchemefulSite origin_site_2 = BlinkSchemefulSite(origin_2);

  EXPECT_EQ(security_origin_site_2, origin_site_2);

  EXPECT_NE(security_origin_site_2, origin_site_1);
  EXPECT_NE(security_origin_site_1, origin_site_2);
}

TEST(BlinkSchemefulSiteTest, SchemefulSiteConstructor) {
  url::Origin origin_1 = url::Origin::Create(GURL("https://www.example.com"));

  net::SchemefulSite net_site = net::SchemefulSite(origin_1);
  BlinkSchemefulSite blink_site_from_origin = BlinkSchemefulSite(origin_1);

  BlinkSchemefulSite blink_site_from_net_site = BlinkSchemefulSite(net_site);

  EXPECT_EQ(blink_site_from_origin, blink_site_from_net_site);
}

TEST(BlinkSchemefulSiteTest, TypecastOperator) {
  url::Origin origin_1 = url::Origin::Create(GURL("https://www.example.com"));

  BlinkSchemefulSite blink_site = BlinkSchemefulSite(origin_1);
  net::SchemefulSite net_site_from_origin = net::SchemefulSite(origin_1);

  net::SchemefulSite net_site_from_blink_site =
      static_cast<net::SchemefulSite>(blink_site);

  EXPECT_EQ(net_site_from_origin, net_site_from_blink_site);
}

// Should construct a BlinkSchemeful site for a valid input but should fail for
// invalid inputs.
TEST(BlinkSchemefulSiteTest, FromWire) {
  url::Origin valid = url::Origin::Create(GURL("https://example.com"));
  url::Origin invalid = url::Origin::Create(GURL("https://sub.example.com"));

  BlinkSchemefulSite out;
  BlinkSchemefulSite valid_site(valid);

  EXPECT_FALSE(BlinkSchemefulSite::FromWire(invalid, &out));

  EXPECT_TRUE(BlinkSchemefulSite::FromWire(valid, &out));
  EXPECT_EQ(out, valid_site);
}

TEST(BlinkSchemefulSiteTest, HashBlinkSchemefulSite) {
  WTF::HashMap<BlinkSchemefulSite, int> blink_schemeful_site_map_;

  BlinkSchemefulSite blink_site_1(
      SecurityOrigin::CreateFromString("https://example.com"));
  BlinkSchemefulSite blink_site_2(
      SecurityOrigin::CreateFromString("https://other.com"));
  BlinkSchemefulSite opaque_site_1;
  BlinkSchemefulSite opaque_site_2;

  blink_schemeful_site_map_.insert(blink_site_1, 1);
  EXPECT_EQ(blink_schemeful_site_map_.size(), 1u);
  EXPECT_TRUE(blink_schemeful_site_map_.Contains(blink_site_1));
  EXPECT_EQ(blink_schemeful_site_map_.at(blink_site_1), 1);

  blink_schemeful_site_map_.insert(blink_site_2, 2);
  blink_schemeful_site_map_.insert(opaque_site_1, 3);
  blink_schemeful_site_map_.insert(opaque_site_2, 4);
  EXPECT_EQ(blink_schemeful_site_map_.size(), 4u);

  blink_schemeful_site_map_.erase(blink_site_1);
  blink_schemeful_site_map_.erase(opaque_site_1);
  EXPECT_FALSE(blink_schemeful_site_map_.Contains(blink_site_1));
  EXPECT_FALSE(blink_schemeful_site_map_.Contains(opaque_site_1));
}

TEST(BlinkSchemefulSiteTest, IsOpaque) {
  BlinkSchemefulSite site(
      SecurityOrigin::CreateFromString("https://example.com"));
  EXPECT_FALSE(site.IsOpaque());
  BlinkSchemefulSite opaque_site;
  EXPECT_TRUE(opaque_site.IsOpaque());
}

}  // namespace blink

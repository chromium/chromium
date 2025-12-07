// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ev_root_ca_metadata.h"

#include "build/build_config.h"
#include "net/base/hash_value.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/input.h"

namespace net {

namespace {

#if defined(PLATFORM_USES_CHROMIUM_EV_METADATA)
const char kFakePolicyStr[] = "2.16.840.1.42";

// DER OID values (no tag or length).
const uint8_t kFakePolicy[] = {0x60, 0x86, 0x48, 0x01, 0x2a};
const uint8_t kCabEvPolicy[] = {0x67, 0x81, 0x0c, 0x01, 0x01};

const SHA256HashValue kFakeFingerprint = {
    {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa,
     0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
     0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}};
// This root must be in the Chrome Root Store. This must be kept in sync with
// the test in google3, and with the contents of the Chrome Root Store.
//
// Failure to update this test first before removing the below root from the
// Chrome Root Store wil break the sync of the Chrome Root Store between google3
// and the chromium repository.
const SHA256HashValue kAmazonFingerprint = {
    {0x1b, 0xa5, 0xb2, 0xaa, 0x8c, 0x65, 0x40, 0x1a, 0x82, 0x96, 0x01,
     0x18, 0xf8, 0x0b, 0xec, 0x4f, 0x62, 0x30, 0x4d, 0x83, 0xce, 0xc4,
     0x71, 0x3a, 0x19, 0xc3, 0x9c, 0x01, 0x1e, 0xa4, 0x6d, 0xb4}};

TEST(EVRootCAMetadataTest, Basic) {
  EVRootCAMetadata* ev_metadata(EVRootCAMetadata::GetInstance());

  // Contains an expected policy.
  EXPECT_TRUE(ev_metadata->IsEVPolicyOID(bssl::der::Input(kCabEvPolicy)));

  // Does not contain an unregistered policy.
  EXPECT_FALSE(ev_metadata->IsEVPolicyOID(bssl::der::Input(kFakePolicy)));

  // The policy is correct for the right root.
  EXPECT_TRUE(ev_metadata->HasEVPolicyOID(kAmazonFingerprint,
                                          bssl::der::Input(kCabEvPolicy)));

  // The policy does not match if the root does not match.
  EXPECT_FALSE(ev_metadata->HasEVPolicyOID(kFakeFingerprint,
                                           bssl::der::Input(kCabEvPolicy)));

  // The expected root only has the expected policies; it should fail to match
  // the root against unknown policies.
  EXPECT_FALSE(ev_metadata->HasEVPolicyOID(kAmazonFingerprint,
                                           bssl::der::Input(kFakePolicy)));

  // Test a completely bogus OID.
  const uint8_t bad_oid[] = {0};
  EXPECT_FALSE(ev_metadata->HasEVPolicyOID(kAmazonFingerprint,
                                           bssl::der::Input(bad_oid)));
}

TEST(EVRootCAMetadataTest, AddRemove) {
  EVRootCAMetadata* ev_metadata(EVRootCAMetadata::GetInstance());

  // An unregistered/junk policy should not work.
  EXPECT_FALSE(ev_metadata->IsEVPolicyOID(bssl::der::Input(kFakePolicy)));

  EXPECT_FALSE(ev_metadata->HasEVPolicyOID(kFakeFingerprint,
                                           bssl::der::Input(kFakePolicy)));

  {
    // However, this unregistered/junk policy can be temporarily registered
    // and made to work.
    ScopedTestEVPolicy test_ev_policy(ev_metadata, kFakeFingerprint,
                                      kFakePolicyStr);

    EXPECT_TRUE(ev_metadata->IsEVPolicyOID(bssl::der::Input(kFakePolicy)));

    EXPECT_TRUE(ev_metadata->HasEVPolicyOID(kFakeFingerprint,
                                            bssl::der::Input(kFakePolicy)));
  }

  // It should go out of scope when the ScopedTestEVPolicy goes out of scope.
  EXPECT_FALSE(ev_metadata->IsEVPolicyOID(bssl::der::Input(kFakePolicy)));

  EXPECT_FALSE(ev_metadata->HasEVPolicyOID(kFakeFingerprint,
                                           bssl::der::Input(kFakePolicy)));
}

#endif  // defined(PLATFORM_USES_CHROMIUM_EV_METADATA)

}  // namespace

}  // namespace net

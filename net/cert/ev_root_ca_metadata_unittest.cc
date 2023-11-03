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
const SHA256HashValue kStarfieldFingerprint = {
    {0x14, 0x65, 0xfa, 0x20, 0x53, 0x97, 0xb8, 0x76, 0xfa, 0xa6, 0xf0,
     0xa9, 0x95, 0x8e, 0x55, 0x90, 0xe4, 0x0f, 0xcc, 0x7f, 0xaa, 0x4f,
     0xb7, 0xc2, 0xc8, 0x67, 0x75, 0x21, 0xfb, 0x5f, 0xb6, 0x58}};

TEST(EVRootCAMetadataTest, Basic) {
  EVRootCAMetadata* ev_metadata(EVRootCAMetadata::GetInstance());

  // Contains an expected policy.
  EXPECT_TRUE(ev_metadata->IsEVPolicyOID(bssl::der::Input(kCabEvPolicy)));

  // Does not contain an unregistered policy.
  EXPECT_FALSE(ev_metadata->IsEVPolicyOID(bssl::der::Input(kFakePolicy)));

  // The policy is correct for the right root.
  EXPECT_TRUE(ev_metadata->HasEVPolicyOID(kStarfieldFingerprint,
                                          bssl::der::Input(kCabEvPolicy)));

  // The policy does not match if the root does not match.
  EXPECT_FALSE(ev_metadata->HasEVPolicyOID(kFakeFingerprint,
                                           bssl::der::Input(kCabEvPolicy)));

  // The expected root only has the expected policies; it should fail to match
  // the root against unknown policies.
  EXPECT_FALSE(ev_metadata->HasEVPolicyOID(kStarfieldFingerprint,
                                           bssl::der::Input(kFakePolicy)));

  // Test a completely bogus OID.
  const uint8_t bad_oid[] = {0};
  EXPECT_FALSE(ev_metadata->HasEVPolicyOID(kStarfieldFingerprint,
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

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ev_root_ca_metadata.h"

#include "build/build_config.h"
#include "net/cert/x509_cert_types.h"
#include "net/der/input.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

#if defined(OS_WIN)
const char kFakePolicyStr[] = "2.16.840.1.42";
const char kCabEvPolicyStr[] = "2.23.140.1.1";
const char kStarfieldPolicyStr[] = "2.16.840.1.114414.1.7.23.3";
#elif defined(PLATFORM_USES_CHROMIUM_EV_METADATA)
const char kFakePolicyStr[] = "2.16.840.1.42";
#endif

#if defined(PLATFORM_USES_CHROMIUM_EV_METADATA)
// DER OID values (no tag or length).
const uint8_t kFakePolicyBytes[] = {0x60, 0x86, 0x48, 0x01, 0x2a};
const uint8_t kCabEvPolicyBytes[] = {0x67, 0x81, 0x0c, 0x01, 0x01};
const uint8_t kStarfieldPolicyBytes[] = {0x60, 0x86, 0x48, 0x01, 0x86, 0xFD,
                                         0x6E, 0x01, 0x07, 0x17, 0x03};

const SHA256HashValue kFakeFingerprint = {
    {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa,
     0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
     0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}};
const SHA256HashValue kStarfieldFingerprint = {
    {0x14, 0x65, 0xfa, 0x20, 0x53, 0x97, 0xb8, 0x76, 0xfa, 0xa6, 0xf0,
     0xa9, 0x95, 0x8e, 0x55, 0x90, 0xe4, 0x0f, 0xcc, 0x7f, 0xaa, 0x4f,
     0xb7, 0xc2, 0xc8, 0x67, 0x75, 0x21, 0xfb, 0x5f, 0xb6, 0x58}};

class EVOidData {
 public:
  EVOidData();
  bool Init();

  EVRootCAMetadata::PolicyOID fake_policy;
  der::Input fake_policy_bytes;

  EVRootCAMetadata::PolicyOID cab_ev_policy;
  der::Input cab_ev_policy_bytes;

  EVRootCAMetadata::PolicyOID starfield_policy;
  der::Input starfield_policy_bytes;
};

#endif  // defined(PLATFORM_USES_CHROMIUM_EV_METADATA)

#if defined(OS_WIN)

EVOidData::EVOidData()
    : fake_policy(kFakePolicyStr),
      fake_policy_bytes(kFakePolicyBytes),
      cab_ev_policy(kCabEvPolicyStr),
      cab_ev_policy_bytes(kCabEvPolicyBytes),
      starfield_policy(kStarfieldPolicyStr),
      starfield_policy_bytes(kStarfieldPolicyBytes) {}

bool EVOidData::Init() {
  return true;
}

#elif defined(PLATFORM_USES_CHROMIUM_EV_METADATA)

EVOidData::EVOidData()
    : fake_policy(kFakePolicyBytes),
      fake_policy_bytes(kFakePolicyBytes),
      cab_ev_policy(kCabEvPolicyBytes),
      cab_ev_policy_bytes(kCabEvPolicyBytes),
      starfield_policy(kStarfieldPolicyBytes),
      starfield_policy_bytes(kStarfieldPolicyBytes) {}

bool EVOidData::Init() {
  return true;
}

#endif

#if defined(PLATFORM_USES_CHROMIUM_EV_METADATA)

class EVRootCAMetadataTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(ev_oid_data.Init()); }

  EVOidData ev_oid_data;
};

TEST_F(EVRootCAMetadataTest, Basic) {
  EVRootCAMetadata* ev_metadata(EVRootCAMetadata::GetInstance());

  // Contains an expected policy.
  EXPECT_TRUE(ev_metadata->IsEVPolicyOID(ev_oid_data.starfield_policy));
  EXPECT_TRUE(
      ev_metadata->IsEVPolicyOIDGivenBytes(ev_oid_data.starfield_policy_bytes));

  // Does not contain an unregistered policy.
  EXPECT_FALSE(ev_metadata->IsEVPolicyOID(ev_oid_data.fake_policy));
  EXPECT_FALSE(
      ev_metadata->IsEVPolicyOIDGivenBytes(ev_oid_data.fake_policy_bytes));

  // The policy is correct for the right root.
  EXPECT_TRUE(ev_metadata->HasEVPolicyOID(kStarfieldFingerprint,
                                          ev_oid_data.starfield_policy));
  EXPECT_TRUE(ev_metadata->HasEVPolicyOIDGivenBytes(
      kStarfieldFingerprint, ev_oid_data.starfield_policy_bytes));

  // The policy does not match if the root does not match.
  EXPECT_FALSE(ev_metadata->HasEVPolicyOID(kFakeFingerprint,
                                           ev_oid_data.starfield_policy));
  EXPECT_FALSE(ev_metadata->HasEVPolicyOIDGivenBytes(
      kFakeFingerprint, ev_oid_data.starfield_policy_bytes));

  // The expected root only has the expected policies; it should fail to match
  // the root against both unknown policies as well as policies associated
  // with other roots.
  EXPECT_FALSE(ev_metadata->HasEVPolicyOID(kStarfieldFingerprint,
                                           ev_oid_data.fake_policy));
  EXPECT_FALSE(ev_metadata->HasEVPolicyOIDGivenBytes(
      kStarfieldFingerprint, ev_oid_data.fake_policy_bytes));

  EXPECT_FALSE(ev_metadata->HasEVPolicyOID(kStarfieldFingerprint,
                                           ev_oid_data.cab_ev_policy));
  EXPECT_FALSE(ev_metadata->HasEVPolicyOIDGivenBytes(
      kStarfieldFingerprint, ev_oid_data.cab_ev_policy_bytes));

  // Test a completely bogus OID given bytes.
  const uint8_t bad_oid[] = {0};
  EXPECT_FALSE(ev_metadata->HasEVPolicyOIDGivenBytes(kStarfieldFingerprint,
                                                     der::Input(bad_oid)));
}

TEST_F(EVRootCAMetadataTest, AddRemove) {
  EVRootCAMetadata* ev_metadata(EVRootCAMetadata::GetInstance());

  // An unregistered/junk policy should not work.
  EXPECT_FALSE(ev_metadata->IsEVPolicyOID(ev_oid_data.fake_policy));
  EXPECT_FALSE(
      ev_metadata->IsEVPolicyOIDGivenBytes(ev_oid_data.fake_policy_bytes));

  EXPECT_FALSE(
      ev_metadata->HasEVPolicyOID(kFakeFingerprint, ev_oid_data.fake_policy));
  EXPECT_FALSE(ev_metadata->HasEVPolicyOIDGivenBytes(
      kFakeFingerprint, ev_oid_data.fake_policy_bytes));

  {
    // However, this unregistered/junk policy can be temporarily registered
    // and made to work.
    ScopedTestEVPolicy test_ev_policy(ev_metadata, kFakeFingerprint,
                                      kFakePolicyStr);

    EXPECT_TRUE(ev_metadata->IsEVPolicyOID(ev_oid_data.fake_policy));
    EXPECT_TRUE(
        ev_metadata->IsEVPolicyOIDGivenBytes(ev_oid_data.fake_policy_bytes));

    EXPECT_TRUE(
        ev_metadata->HasEVPolicyOID(kFakeFingerprint, ev_oid_data.fake_policy));
    EXPECT_TRUE(ev_metadata->HasEVPolicyOIDGivenBytes(
        kFakeFingerprint, ev_oid_data.fake_policy_bytes));
  }

  // It should go out of scope when the ScopedTestEVPolicy goes out of scope.
  EXPECT_FALSE(ev_metadata->IsEVPolicyOID(ev_oid_data.fake_policy));
  EXPECT_FALSE(
      ev_metadata->IsEVPolicyOIDGivenBytes(ev_oid_data.fake_policy_bytes));

  EXPECT_FALSE(
      ev_metadata->HasEVPolicyOID(kFakeFingerprint, ev_oid_data.fake_policy));
  EXPECT_FALSE(ev_metadata->HasEVPolicyOIDGivenBytes(
      kFakeFingerprint, ev_oid_data.fake_policy_bytes));
}

TEST_F(EVRootCAMetadataTest, IsCaBrowserForumEvOid) {
  EXPECT_TRUE(
      EVRootCAMetadata::IsCaBrowserForumEvOid(ev_oid_data.cab_ev_policy));

  EXPECT_FALSE(
      EVRootCAMetadata::IsCaBrowserForumEvOid(ev_oid_data.fake_policy));
  EXPECT_FALSE(
      EVRootCAMetadata::IsCaBrowserForumEvOid(ev_oid_data.starfield_policy));
}

#endif  // defined(PLATFORM_USES_CHROMIUM_EV_METADATA)

}  // namespace

}  // namespace net

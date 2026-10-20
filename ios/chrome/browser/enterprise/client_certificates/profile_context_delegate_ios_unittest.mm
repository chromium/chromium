// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/client_certificates/profile_context_delegate_ios.h"

#import <memory>

#import "components/enterprise/browser/identifiers/profile_id_service.h"
#import "components/enterprise/client_certificates/core/constants.h"
#import "components/enterprise/client_certificates/core/prefs.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace client_certificates {

namespace {
constexpr char kFakeProfileId[] = "fake-ios-profile-id";
}  // namespace

class ProfileContextDelegateIOSTest : public PlatformTest {
 protected:
  enterprise::ProfileIdService profile_id_service_{kFakeProfileId};
  ProfileContextDelegateIOS delegate_{&profile_id_service_};
};

TEST_F(ProfileContextDelegateIOSTest, GetExpectedCommonName) {
  EXPECT_EQ(kFakeProfileId, delegate_.GetExpectedCommonName());
}

TEST_F(ProfileContextDelegateIOSTest, GetIdentityName) {
  EXPECT_EQ(kManagedProfileIdentityName, delegate_.GetIdentityName());
}

TEST_F(ProfileContextDelegateIOSTest, GetTemporaryIdentityName) {
  EXPECT_EQ(kTemporaryManagedProfileIdentityName,
            delegate_.GetTemporaryIdentityName());
}

TEST_F(ProfileContextDelegateIOSTest, GetPolicyPref) {
  EXPECT_EQ(prefs::kProvisionManagedClientCertificateForUserPrefs,
            delegate_.GetPolicyPref());
}

TEST_F(ProfileContextDelegateIOSTest, GetLoggingContext) {
  EXPECT_EQ("Profile", delegate_.GetLoggingContext());
}

}  // namespace client_certificates

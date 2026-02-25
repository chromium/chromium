// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/client_certificates/profile_context_delegate_ios.h"

#import <memory>

#import "components/enterprise/client_certificates/core/constants.h"
#import "components/enterprise/client_certificates/core/prefs.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace client_certificates {

using ProfileContextDelegateIOSTest = PlatformTest;

TEST_F(ProfileContextDelegateIOSTest, GetIdentityName) {
  EXPECT_EQ(kManagedProfileIdentityName,
            ProfileContextDelegateIOS().GetIdentityName());
}

TEST_F(ProfileContextDelegateIOSTest, GetTemporaryIdentityName) {
  EXPECT_EQ(kTemporaryManagedProfileIdentityName,
            ProfileContextDelegateIOS().GetTemporaryIdentityName());
}

TEST_F(ProfileContextDelegateIOSTest, GetPolicyPref) {
  EXPECT_EQ(prefs::kProvisionManagedClientCertificateForUserPrefs,
            ProfileContextDelegateIOS().GetPolicyPref());
}

TEST_F(ProfileContextDelegateIOSTest, GetLoggingContext) {
  EXPECT_EQ("Profile", ProfileContextDelegateIOS().GetLoggingContext());
}

}  // namespace client_certificates

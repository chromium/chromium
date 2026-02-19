// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/client_certificates/browser_context_delegate_ios.h"

#import "components/enterprise/client_certificates/core/constants.h"
#import "components/enterprise/client_certificates/core/prefs.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace client_certificates {

using BrowserContextDelegateIOSTest = PlatformTest;

TEST_F(BrowserContextDelegateIOSTest, GetIdentityName) {
  EXPECT_EQ(kManagedBrowserIdentityName,
            BrowserContextDelegateIOS().GetIdentityName());
}

TEST_F(BrowserContextDelegateIOSTest, GetTemporaryIdentityName) {
  EXPECT_EQ(kTemporaryManagedBrowserIdentityName,
            BrowserContextDelegateIOS().GetTemporaryIdentityName());
}

TEST_F(BrowserContextDelegateIOSTest, GetPolicyPref) {
  EXPECT_EQ(prefs::kProvisionManagedClientCertificateForBrowserPrefs,
            BrowserContextDelegateIOS().GetPolicyPref());
}

TEST_F(BrowserContextDelegateIOSTest, GetLoggingContext) {
  EXPECT_EQ("Browser", BrowserContextDelegateIOS().GetLoggingContext());
}

}  // namespace client_certificates

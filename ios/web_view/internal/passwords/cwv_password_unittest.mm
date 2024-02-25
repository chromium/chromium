// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/cwv_password_internal.h"

#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace ios_web_view {

using CWVPasswordTest = PlatformTest;

// Tests CWVPassword initialization for a blocked site.
TEST_F(CWVPasswordTest, Blocked) {
  password_manager::PasswordForm password_form;
  password_form.url = GURL("http://www.example.com/accounts/LoginAuth");
  password_form.action = GURL("http://www.example.com/accounts/Login");
  password_form.username_element = base::SysNSStringToUTF16(@"Email");
  password_form.username_value = base::SysNSStringToUTF16(@"test@egmail.com");
  password_form.password_element = base::SysNSStringToUTF16(@"Passwd");
  password_form.password_value = base::SysNSStringToUTF16(@"test");
  password_form.submit_element = base::SysNSStringToUTF16(@"signIn");
  password_form.signon_realm = "http://www.example.com/";
  password_form.scheme = password_manager::PasswordForm::Scheme::kHtml;
  password_form.blocked_by_user = true;
  password_form.keychain_identifier = "test-encrypted-password";

  CWVPassword* password =
      [[CWVPassword alloc] initWithPasswordForm:password_form];

  EXPECT_EQ(password_form, *[password internalPasswordForm]);
  EXPECT_NSEQ(base::SysUTF8ToNSString(password_manager::GetShownOrigin(
                  password_manager::CredentialUIEntry(password_form))),
              password.title);
  EXPECT_NSEQ(base::SysUTF8ToNSString(
                  password_manager::GetShownUrl(
                      password_manager::CredentialUIEntry(password_form))
                      .spec()),
              password.site);
  EXPECT_TRUE(password.blocked);
  EXPECT_FALSE(password.username);
  EXPECT_FALSE(password.password);
  EXPECT_FALSE(password.keychainIdentifier);
}

// Tests CWVPassword initialization for a non-blocked site.
TEST_F(CWVPasswordTest, NonBlocked) {
  password_manager::PasswordForm password_form;
  password_form.url = GURL("http://www.example.com/accounts/LoginAuth");
  password_form.action = GURL("http://www.example.com/accounts/Login");
  password_form.username_element = base::SysNSStringToUTF16(@"Email");
  password_form.username_value = base::SysNSStringToUTF16(@"test@egmail.com");
  password_form.password_element = base::SysNSStringToUTF16(@"Passwd");
  password_form.password_value = base::SysNSStringToUTF16(@"test");
  password_form.submit_element = base::SysNSStringToUTF16(@"signIn");
  password_form.signon_realm = "http://www.example.com/";
  password_form.scheme = password_manager::PasswordForm::Scheme::kHtml;
  password_form.blocked_by_user = false;
  password_form.keychain_identifier = "test-encrypted-password";

  CWVPassword* password =
      [[CWVPassword alloc] initWithPasswordForm:password_form];

  EXPECT_EQ(password_form, *[password internalPasswordForm]);
  EXPECT_NSEQ(base::SysUTF8ToNSString(password_manager::GetShownOrigin(
                  password_manager::CredentialUIEntry(password_form))),
              password.title);
  EXPECT_NSEQ(base::SysUTF8ToNSString(
                  password_manager::GetShownUrl(
                      password_manager::CredentialUIEntry(password_form))
                      .spec()),
              password.site);
  EXPECT_FALSE(password.blocked);
  EXPECT_NSEQ(@"test@egmail.com", password.username);
  EXPECT_NSEQ(@"test", password.password);
  EXPECT_NSEQ(@"test-encrypted-password", password.keychainIdentifier);
}

}  // namespace ios_web_view

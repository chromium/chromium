// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/cwv_password_internal.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

using CWVPasswordTest = PlatformTest;

// Tests CWVPassword initialization for a blacklisted site.
TEST_F(CWVPasswordTest, Blacklisted) {
  autofill::PasswordForm password_form;
  password_form.origin = GURL("http://www.example.com/accounts/LoginAuth");
  password_form.action = GURL("http://www.example.com/accounts/Login");
  password_form.username_element = base::SysNSStringToUTF16(@"Email");
  password_form.username_value = base::SysNSStringToUTF16(@"test@egmail.com");
  password_form.password_element = base::SysNSStringToUTF16(@"Passwd");
  password_form.password_value = base::SysNSStringToUTF16(@"test");
  password_form.submit_element = base::SysNSStringToUTF16(@"signIn");
  password_form.signon_realm = "http://www.example.com/";
  password_form.preferred = false;
  password_form.scheme = autofill::PasswordForm::Scheme::kHtml;
  password_form.blacklisted_by_user = true;
  auto name_and_link =
      password_manager::GetShownOriginAndLinkUrl(password_form);

  CWVPassword* password =
      [[CWVPassword alloc] initWithPasswordForm:password_form];

  EXPECT_EQ(password_form, *[password internalPasswordForm]);
  EXPECT_NSEQ(base::SysUTF8ToNSString(name_and_link.first), password.title);
  EXPECT_NSEQ(base::SysUTF8ToNSString(name_and_link.second.spec()),
              password.site);
  EXPECT_TRUE(password.blacklisted);
  EXPECT_FALSE(password.username);
  EXPECT_FALSE(password.password);
}

// Tests CWVPassword initialization for a non-blacklisted site.
TEST_F(CWVPasswordTest, NonBlacklisted) {
  autofill::PasswordForm password_form;
  password_form.origin = GURL("http://www.example.com/accounts/LoginAuth");
  password_form.action = GURL("http://www.example.com/accounts/Login");
  password_form.username_element = base::SysNSStringToUTF16(@"Email");
  password_form.username_value = base::SysNSStringToUTF16(@"test@egmail.com");
  password_form.password_element = base::SysNSStringToUTF16(@"Passwd");
  password_form.password_value = base::SysNSStringToUTF16(@"test");
  password_form.submit_element = base::SysNSStringToUTF16(@"signIn");
  password_form.signon_realm = "http://www.example.com/";
  password_form.preferred = false;
  password_form.scheme = autofill::PasswordForm::Scheme::kHtml;
  password_form.blacklisted_by_user = false;
  auto name_and_link =
      password_manager::GetShownOriginAndLinkUrl(password_form);

  CWVPassword* password =
      [[CWVPassword alloc] initWithPasswordForm:password_form];

  EXPECT_EQ(password_form, *[password internalPasswordForm]);
  EXPECT_NSEQ(base::SysUTF8ToNSString(name_and_link.first), password.title);
  EXPECT_NSEQ(base::SysUTF8ToNSString(name_and_link.second.spec()),
              password.site);
  EXPECT_FALSE(password.blacklisted);
  EXPECT_NSEQ(@"test@egmail.com", password.username);
  EXPECT_NSEQ(@"test", password.password);
}

}  // namespace ios_web_view

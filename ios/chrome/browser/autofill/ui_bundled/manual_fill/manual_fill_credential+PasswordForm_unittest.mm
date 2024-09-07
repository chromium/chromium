// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_credential+PasswordForm.h"

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_form.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using password_manager::PasswordForm;
using ManualFillCredentialFormPasswordiOSTest = PlatformTest;

// Tests the creation of a credential from a password form.
TEST_F(ManualFillCredentialFormPasswordiOSTest, CreationHTTPURL) {
  NSString* password = @"password_value";
  NSString* username = @"username_value";
  NSString* url = @"http://www.alpha.example.com/path/";

  PasswordForm passwordForm = PasswordForm();
  passwordForm.password_value = base::SysNSStringToUTF16(password);
  passwordForm.username_value = base::SysNSStringToUTF16(username);
  passwordForm.url = GURL(base::SysNSStringToUTF16(url));
  ManualFillCredential* credential =
      [[ManualFillCredential alloc] initWithPasswordForm:passwordForm];

  EXPECT_TRUE(credential);
  EXPECT_NSEQ(username, credential.username);
  EXPECT_NSEQ(password, credential.password);
  EXPECT_NSEQ(@"example.com", credential.siteName);
  EXPECT_NSEQ(@"alpha.example.com", credential.host);
}

// Tests the creation of a credential from a password form.
TEST_F(ManualFillCredentialFormPasswordiOSTest, CreationHTTPSURL) {
  NSString* password = @"password_value";
  NSString* username = @"username_value";
  NSString* url = @"https://www.alpha.example.com/path/";

  PasswordForm passwordForm = PasswordForm();
  passwordForm.password_value = base::SysNSStringToUTF16(password);
  passwordForm.username_value = base::SysNSStringToUTF16(username);
  passwordForm.url = GURL(base::SysNSStringToUTF16(url));
  ManualFillCredential* credential =
      [[ManualFillCredential alloc] initWithPasswordForm:passwordForm];

  EXPECT_TRUE(credential);
  EXPECT_NSEQ(username, credential.username);
  EXPECT_NSEQ(password, credential.password);
  EXPECT_NSEQ(@"example.com", credential.siteName);
  EXPECT_NSEQ(@"alpha.example.com", credential.host);
}

// Tests the creation of a credential from a password form.
TEST_F(ManualFillCredentialFormPasswordiOSTest, CreationNoWWW) {
  NSString* password = @"password_value";
  NSString* username = @"username_value";
  NSString* url = @"http://alpha.example.com/path/";

  PasswordForm passwordForm = PasswordForm();
  passwordForm.password_value = base::SysNSStringToUTF16(password);
  passwordForm.username_value = base::SysNSStringToUTF16(username);
  passwordForm.url = GURL(base::SysNSStringToUTF16(url));
  ManualFillCredential* credential =
      [[ManualFillCredential alloc] initWithPasswordForm:passwordForm];

  EXPECT_TRUE(credential);
  EXPECT_NSEQ(username, credential.username);
  EXPECT_NSEQ(password, credential.password);
  EXPECT_NSEQ(@"example.com", credential.siteName);
  EXPECT_NSEQ(@"alpha.example.com", credential.host);
}

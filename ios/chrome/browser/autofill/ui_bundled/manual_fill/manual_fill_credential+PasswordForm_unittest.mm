// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_form.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_credential+PasswordForm.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using password_manager::PasswordForm;
using ManualFillCredentialFormPasswordiOSTest = PlatformTest;

namespace {

constexpr NSString* kPassword = @"password_value";
constexpr NSString* kUsername = @"username_value";
constexpr NSString* kUrl = @"http://www.alpha.example.com/path/";

// Creates a password form for the given `url`.
PasswordForm CreatePasswordForm(NSString* url) {
  PasswordForm password_form = PasswordForm();
  password_form.password_value = base::SysNSStringToUTF16(kPassword);
  password_form.username_value = base::SysNSStringToUTF16(kUsername);
  password_form.url = GURL(base::SysNSStringToUTF16(url));
  return password_form;
}

}  // namespace

// Tests the creation of a credential from a password form.
TEST_F(ManualFillCredentialFormPasswordiOSTest, CreationHTTPURL) {
  NSString* url = @"http://www.alpha.example.com/path/";

  PasswordForm password_form = CreatePasswordForm(url);
  ManualFillCredential* credential =
      [[ManualFillCredential alloc] initWithPasswordForm:password_form
                                                isBackup:NO];

  EXPECT_TRUE(credential);
  EXPECT_NSEQ(kUsername, credential.username);
  EXPECT_NSEQ(kPassword, credential.password);
  EXPECT_NSEQ(@"example.com", credential.siteName);
  EXPECT_NSEQ(@"alpha.example.com", credential.host);
  EXPECT_FALSE(credential.isBackupCredential);
}

// Tests the creation of a credential from a password form.
TEST_F(ManualFillCredentialFormPasswordiOSTest, CreationHTTPSURL) {
  NSString* url = @"https://www.alpha.example.com/path/";

  PasswordForm password_form = CreatePasswordForm(url);
  ManualFillCredential* credential =
      [[ManualFillCredential alloc] initWithPasswordForm:password_form
                                                isBackup:NO];

  EXPECT_TRUE(credential);
  EXPECT_NSEQ(kUsername, credential.username);
  EXPECT_NSEQ(kPassword, credential.password);
  EXPECT_NSEQ(@"example.com", credential.siteName);
  EXPECT_NSEQ(@"alpha.example.com", credential.host);
  EXPECT_FALSE(credential.isBackupCredential);
}

// Tests the creation of a credential from a password form.
TEST_F(ManualFillCredentialFormPasswordiOSTest, CreationNoWWW) {
  NSString* url = @"http://alpha.example.com/path/";

  PasswordForm password_form = CreatePasswordForm(url);
  ManualFillCredential* credential =
      [[ManualFillCredential alloc] initWithPasswordForm:password_form
                                                isBackup:NO];

  EXPECT_TRUE(credential);
  EXPECT_NSEQ(kUsername, credential.username);
  EXPECT_NSEQ(kPassword, credential.password);
  EXPECT_NSEQ(@"example.com", credential.siteName);
  EXPECT_NSEQ(@"alpha.example.com", credential.host);
  EXPECT_FALSE(credential.isBackupCredential);
}

// Tests the creation of a backup credential.
TEST_F(ManualFillCredentialFormPasswordiOSTest, CreationBackupCredential) {
  NSString* url = @"https://www.alpha.example.com/path/";

  PasswordForm password_form = CreatePasswordForm(url);
  ManualFillCredential* credential =
      [[ManualFillCredential alloc] initWithPasswordForm:password_form
                                                isBackup:YES];

  EXPECT_TRUE(credential);
  EXPECT_NSEQ(kUsername, credential.username);
  EXPECT_NSEQ(kPassword, credential.password);
  EXPECT_NSEQ(@"example.com", credential.siteName);
  EXPECT_NSEQ(@"alpha.example.com", credential.host);
  EXPECT_TRUE(credential.isBackupCredential);
}

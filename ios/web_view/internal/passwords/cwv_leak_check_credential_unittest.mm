// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/cwv_leak_check_credential_internal.h"

#import "base/strings/utf_string_conversions.h"
#import "components/password_manager/core/browser/password_form.h"
#import "ios/web_view/internal/passwords/cwv_password_internal.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using password_manager::LeakCheckCredential;

namespace ios_web_view {

class CWVLeakCheckCredentialTest : public PlatformTest {
 protected:
  // Creates a CWVPassword for testing its conversion to a
  // CWVLeakCheckCredential.
  CWVPassword* CreateCWVPassword(std::u16string username,
                                 std::u16string password) {
    password_manager::PasswordForm password_form;
    password_form.url = GURL("http://www.example.com/accounts/LoginAuth");
    password_form.action = GURL("http://www.example.com/accounts/Login");
    password_form.username_element = u"Email";
    password_form.username_value = username;
    password_form.password_element = u"Passwd";
    password_form.password_value = password;
    password_form.submit_element = u"signIn";
    password_form.signon_realm = "http://www.example.com/";
    password_form.scheme = password_manager::PasswordForm::Scheme::kHtml;
    password_form.blocked_by_user = false;
    password_form.keychain_identifier = base::UTF16ToUTF8(password);

    return [[CWVPassword alloc] initWithPasswordForm:password_form];
  }
};

// Tests that the internal credential matches the username/password fields of
// the CWVPassword used to initialize it.
TEST_F(CWVLeakCheckCredentialTest, CWVPasswordInitialization) {
  CWVLeakCheckCredential* credential = [CWVLeakCheckCredential
      canonicalLeakCheckCredentialWithPassword:CreateCWVPassword(u"username",
                                                                 u"password")];

  EXPECT_EQ(credential.internalCredential.username(), u"username");
  EXPECT_EQ(credential.internalCredential.password(), u"password");
}

// Test that canonicalization is applied by seeing if a username with a domain
// and without are regarded as equivalent.
TEST_F(CWVLeakCheckCredentialTest, CWVPasswordInitializationCanonical) {
  CWVPassword* password = CreateCWVPassword(u"username", u"password");
  CWVPassword* password_with_domain =
      CreateCWVPassword(u"username@google.com", u"password");

  EXPECT_NSEQ(
      [CWVLeakCheckCredential
          canonicalLeakCheckCredentialWithPassword:password],
      [CWVLeakCheckCredential
          canonicalLeakCheckCredentialWithPassword:password_with_domain]);
}

// Tests that isEquals and hash methods are equivalent for different objects
// with equivalent credentials.
TEST_F(CWVLeakCheckCredentialTest, Equals) {
  CWVLeakCheckCredential* credential = [[CWVLeakCheckCredential alloc]
      initWithCredential:std::make_unique<LeakCheckCredential>(u"username",
                                                               u"password")];
  CWVLeakCheckCredential* same_credential = [[CWVLeakCheckCredential alloc]
      initWithCredential:std::make_unique<LeakCheckCredential>(u"username",
                                                               u"password")];

  EXPECT_NSEQ(credential, same_credential);
  EXPECT_EQ(credential.hash, same_credential.hash);
}

// Tests that isEquals and hash methods are not equivalent for different objects
// with different credentials.
TEST_F(CWVLeakCheckCredentialTest, NotEquals) {
  CWVLeakCheckCredential* credential = [[CWVLeakCheckCredential alloc]
      initWithCredential:std::make_unique<LeakCheckCredential>(u"username",
                                                               u"password")];
  CWVLeakCheckCredential* different_password = [[CWVLeakCheckCredential alloc]
      initWithCredential:std::make_unique<LeakCheckCredential>(u"username",
                                                               u"secret")];
  CWVLeakCheckCredential* different_username = [[CWVLeakCheckCredential alloc]
      initWithCredential:std::make_unique<LeakCheckCredential>(u"bob",
                                                               u"password")];

  EXPECT_NSNE(credential, different_password);
  EXPECT_NE(credential.hash, different_password.hash);
  EXPECT_NSNE(credential, different_username);
  EXPECT_NE(credential.hash, different_username.hash);
}

}  // namespace ios_web_view

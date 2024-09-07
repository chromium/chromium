// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_credential.h"

#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using ManualFillCredentialiOSTest = PlatformTest;

// Tests that a credential is correctly created.
TEST_F(ManualFillCredentialiOSTest, Creation) {
  NSString* username = @"username@example.com";
  NSString* password = @"password";
  NSString* siteName = @"example.com";
  NSString* host = @"sub.example.com";
  GURL URL("https://www.sub.example.com");
  ManualFillCredential* credential =
      [[ManualFillCredential alloc] initWithUsername:username
                                            password:password
                                            siteName:siteName
                                                host:host
                                                 URL:URL];
  EXPECT_TRUE(credential);
  EXPECT_NSEQ(username, credential.username);
  EXPECT_NSEQ(password, credential.password);
  EXPECT_NSEQ(siteName, credential.siteName);
  EXPECT_NSEQ(host, credential.host);
  EXPECT_EQ(URL, credential.URL);
}

// Test equality between credentials.
TEST_F(ManualFillCredentialiOSTest, Equality) {
  NSString* username = @"username@example.com";
  NSString* password = @"password";
  NSString* siteName = @"example.com";
  NSString* host = @"sub.example.com";
  GURL URL("https://www.sub.example.com");
  ManualFillCredential* credential =
      [[ManualFillCredential alloc] initWithUsername:username
                                            password:password
                                            siteName:siteName
                                                host:host
                                                 URL:URL];
  ManualFillCredential* equalCredential =
      [[ManualFillCredential alloc] initWithUsername:username
                                            password:password
                                            siteName:siteName
                                                host:host
                                                 URL:URL];
  EXPECT_TRUE([credential isEqual:equalCredential]);

  ManualFillCredential* differentUsernameCredential =
      [[ManualFillCredential alloc] initWithUsername:@"username2"
                                            password:password
                                            siteName:siteName
                                                host:host
                                                 URL:URL];
  EXPECT_FALSE([credential isEqual:differentUsernameCredential]);

  ManualFillCredential* differentPasswordCredential =
      [[ManualFillCredential alloc] initWithUsername:username
                                            password:@"psswd"
                                            siteName:siteName
                                                host:host
                                                 URL:URL];
  EXPECT_FALSE([credential isEqual:differentPasswordCredential]);

  ManualFillCredential* differentSiteNameCredential =
      [[ManualFillCredential alloc] initWithUsername:username
                                            password:password
                                            siteName:@"notexample.com"
                                                host:host
                                                 URL:URL];
  EXPECT_FALSE([credential isEqual:differentSiteNameCredential]);

  ManualFillCredential* differentHostCredential =
      [[ManualFillCredential alloc] initWithUsername:username
                                            password:password
                                            siteName:siteName
                                                host:@"other.example.com"
                                                 URL:URL];
  EXPECT_FALSE([credential isEqual:differentHostCredential]);

  ManualFillCredential* differentURLCredential = [[ManualFillCredential alloc]
      initWithUsername:username
              password:password
              siteName:siteName
                  host:host
                   URL:GURL("https://www.other.example.com")];
  EXPECT_FALSE([credential isEqual:differentURLCredential]);
}

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_error_page_helper.h"

#import "base/apple/bundle_locations.h"
#import "base/strings/sys_string_conversions.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using CRWErrorPageHelperTest = PlatformTest;

// Tests that the failed navigation URL is correctly extracted from the error.
TEST_F(CRWErrorPageHelperTest, FailedNavigationURL) {
  NSString* url_string = @"https://test-error-page.com";
  NSError* error = [NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorBadURL
             userInfo:@{NSURLErrorFailingURLStringErrorKey : url_string}];
  CRWErrorPageHelper* helper = [[CRWErrorPageHelper alloc] initWithError:error];
  NSURL* url = [NSURL URLWithString:url_string];
  EXPECT_NSEQ(url, helper.failedNavigationURL);
}

// Tests that the original URL is correctly extracted from the file error URL
// created by the helper.
TEST_F(CRWErrorPageHelperTest, ExtractOriginalURLFromErrorPageURL) {
  NSString* url_string = @"https://test-error-page.com";
  NSError* error = [NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorBadURL
             userInfo:@{NSURLErrorFailingURLStringErrorKey : url_string}];
  CRWErrorPageHelper* helper = [[CRWErrorPageHelper alloc] initWithError:error];
  GURL url_from_helper = net::GURLWithNSURL(helper.errorPageFileURL);
  GURL result_original_url = [CRWErrorPageHelper
      failedNavigationURLFromErrorPageFileURL:url_from_helper];
  EXPECT_EQ(GURL(base::SysNSStringToUTF8(url_string)), result_original_url);
  EXPECT_TRUE([CRWErrorPageHelper isErrorPageFileURL:url_from_helper]);
}

// Tests that the error page is correctly identified as error page.
TEST_F(CRWErrorPageHelperTest, IsErrorPageFileURL) {
  NSString* url_string = @"https://test-error-page.com";
  NSError* error = [NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorBadURL
             userInfo:@{NSURLErrorFailingURLStringErrorKey : url_string}];
  CRWErrorPageHelper* helper = [[CRWErrorPageHelper alloc] initWithError:error];
  EXPECT_TRUE([helper
      isErrorPageFileURLForFailedNavigationURL:helper.errorPageFileURL]);
}

// Tests that a normal URL isn't identified as error page.
TEST_F(CRWErrorPageHelperTest, IsErrorPageFileURLWrong) {
  NSString* url_string = @"file://test-error-page.com";
  NSError* error =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorBadURL
                      userInfo:@{
                        NSURLErrorFailingURLStringErrorKey : @"http://fake.com"
                      }];
  CRWErrorPageHelper* helper = [[CRWErrorPageHelper alloc] initWithError:error];
  EXPECT_FALSE([helper
      isErrorPageFileURLForFailedNavigationURL:[NSURL
                                                   URLWithString:url_string]]);
}

// Tests that the failed navigation URL is correctly extracted from the page
// URL.
TEST_F(CRWErrorPageHelperTest, FailedNavigationURLFromErrorPageFileURLCorrect) {
  std::string expected_url = "http://expected-url.com";
  std::string path = base::SysNSStringToUTF8([base::apple::FrameworkBundle()
      pathForResource:@"error_page_loaded"
               ofType:@"html"]);

  GURL url = GURL("file://" + path + "?file=http://not-that-url.com&url=" +
                  expected_url + "&garbage=http://still-not-that-one.com");
  GURL result_url =
      [CRWErrorPageHelper failedNavigationURLFromErrorPageFileURL:url];
  EXPECT_EQ(GURL(expected_url), result_url);
  EXPECT_TRUE([CRWErrorPageHelper isErrorPageFileURL:url]);
}

// Tests that the extract failed navigation URL is empty if the `url` query
// isn't present in the page URL.
TEST_F(CRWErrorPageHelperTest, FailedNavigationURLFromErrorPageFileURLNoQuery) {
  std::string expected_url = "http://expected-url.com";
  std::string path = base::SysNSStringToUTF8([base::apple::FrameworkBundle()
      pathForResource:@"error_page_loaded"
               ofType:@"html"]);

  GURL url = GURL("file://" + path + "?file=" + expected_url +
                  "&garbage=http://still-not-that-one.com");
  GURL result_url =
      [CRWErrorPageHelper failedNavigationURLFromErrorPageFileURL:url];
  EXPECT_TRUE(result_url.is_empty());
  EXPECT_FALSE([CRWErrorPageHelper isErrorPageFileURL:url]);
}

// Tests that the extracted failed navigation URL is empty if the path of the
// current page isn't correct.
TEST_F(CRWErrorPageHelperTest,
       FailedNavigationURLFromErrorPageFileURLWrongPath) {
  GURL url =
      GURL("file://not-the-correct-path.com?url=http://potential-url.com");
  GURL result_url =
      [CRWErrorPageHelper failedNavigationURLFromErrorPageFileURL:url];
  EXPECT_TRUE(result_url.is_empty());
  EXPECT_FALSE([CRWErrorPageHelper isErrorPageFileURL:url]);
}

// Tests that the extracted failed navigation URL is empty if the scheme of the
// current page isn't file://.
TEST_F(CRWErrorPageHelperTest,
       FailedNavigationURLFromErrorPageFileURLWrongScheme) {
  std::string path = base::SysNSStringToUTF8([base::apple::FrameworkBundle()
      pathForResource:@"error_page_loaded"
               ofType:@"html"]);

  GURL url = GURL("http://" + path + "?url=http://potential-url.com");
  GURL result_url =
      [CRWErrorPageHelper failedNavigationURLFromErrorPageFileURL:url];
  EXPECT_TRUE(result_url.is_empty());
  EXPECT_FALSE([CRWErrorPageHelper isErrorPageFileURL:url]);
}

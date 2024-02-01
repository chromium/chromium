// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/error_page_util.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/web/public/test/error_test_util.h"
#import "net/base/net_errors.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using l10n_util::GetNSString;
using net::ERR_CONNECTION_TIMED_OUT;

namespace {

// URL string passed from ios/web layer to ios/chrome and rendered on the error
// page.
NSString* kTestUrl = @"https://chromium.test/";

// Returns string for the given error code.
NSString* ErrorAsString(int net_error) {
  return base::SysUTF8ToNSString(net::ErrorToShortString(net_error));
}

// Returns error in the same format as passed from ios/web layer to ios/chrome.
NSError* CreateTestError(NSInteger url_error) {
  NSDictionary* info = @{
    NSURLErrorFailingURLStringErrorKey : kTestUrl,
  };
  return web::testing::CreateTestNetError(
      [NSError errorWithDomain:NSURLErrorDomain code:url_error userInfo:info]);
}

}  // namespace

// Tests error page rendering by inspecting substrings of the resulting html.
// This test does not actualy load html, because it is an expencive operation.
using ErrorPageUtilTest = PlatformTest;

// Tests error page for non-POST and non-OTR error. The expected strings are:
// error code, failing url, reload button.
TEST_F(ErrorPageUtilTest, NonPostNonOtrError) {
  NSString* html = GetErrorPage(GURL(base::SysNSStringToUTF8(kTestUrl)),
                                CreateTestError(NSURLErrorTimedOut),
                                /*is_post=*/false,
                                /*is_off_the_record=*/false);

  // Make sure gzipped HTML is successfully decompressed.
  EXPECT_TRUE([html containsString:@"<head>"]);

  EXPECT_TRUE([html containsString:ErrorAsString(ERR_CONNECTION_TIMED_OUT)]);
  EXPECT_TRUE([html containsString:kTestUrl]);
  EXPECT_TRUE([html containsString:GetNSString(IDS_ERRORPAGES_BUTTON_RELOAD)]);
}

// Tests error page for POST and non-OTR error. Error pages for POST requests do
// not have Reload button. The expected strings are: error code and failing
// url.
TEST_F(ErrorPageUtilTest, PostNonOtrError) {
  NSString* html = GetErrorPage(GURL(base::SysNSStringToUTF8(kTestUrl)),
                                CreateTestError(NSURLErrorTimedOut),
                                /*is_post=*/true,
                                /*is_off_the_record=*/false);

  // Make sure gzipped HTML is successfully decompressed.
  EXPECT_TRUE([html containsString:@"<head>"]);

  EXPECT_TRUE([html containsString:ErrorAsString(ERR_CONNECTION_TIMED_OUT)]);
  EXPECT_TRUE([html containsString:kTestUrl]);
  EXPECT_FALSE([html containsString:GetNSString(IDS_ERRORPAGES_BUTTON_RELOAD)]);
}

// Tests error page for non-POST and OTR error. On iOS OTR error pages are the
// same as non-OTR. The expected strings are: error code, failing url, reload
// button.
TEST_F(ErrorPageUtilTest, NonPostOtrError) {
  NSString* html = GetErrorPage(GURL(base::SysNSStringToUTF8(kTestUrl)),
                                CreateTestError(NSURLErrorTimedOut),
                                /*is_post=*/false,
                                /*is_off_the_record=*/true);

  // Make sure gzipped HTML is successfully decompressed.
  EXPECT_TRUE([html containsString:@"<head>"]);

  EXPECT_TRUE([html containsString:ErrorAsString(ERR_CONNECTION_TIMED_OUT)]);
  EXPECT_TRUE([html containsString:kTestUrl]);
  EXPECT_TRUE([html containsString:GetNSString(IDS_ERRORPAGES_BUTTON_RELOAD)]);
}

// Tests error page for OST and OTR error. On iOS OTR error pages are the same
// as non-OTR. Error pages for POST requests do not have Reload button. The
// expected strings are: error code and failing url.
TEST_F(ErrorPageUtilTest, PostOtrError) {
  NSString* html = GetErrorPage(GURL(base::SysNSStringToUTF8(kTestUrl)),
                                CreateTestError(NSURLErrorTimedOut),
                                /*is_post=*/true,
                                /*is_off_the_record=*/true);

  // Make sure gzipped HTML is successfully decompressed.
  EXPECT_TRUE([html containsString:@"<head>"]);

  EXPECT_TRUE([html containsString:ErrorAsString(ERR_CONNECTION_TIMED_OUT)]);
  EXPECT_TRUE([html containsString:kTestUrl]);
  EXPECT_FALSE([html containsString:GetNSString(IDS_ERRORPAGES_BUTTON_RELOAD)]);
}

// Tests error page for error without NSURLErrorFailingURLStringErrorKey key.
// This test only makes sure that absence of the spec is handled gracefully.
TEST_F(ErrorPageUtilTest, NoUrlSpec) {
  NSError* error = web::testing::CreateTestNetError([NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorTimedOut
             userInfo:nil]);
  NSString* html = GetErrorPage(GURL(), error,
                                /*is_post=*/false,
                                /*is_off_the_record=*/false);

  // Make sure gzipped HTML is successfully decompressed.
  EXPECT_TRUE([html containsString:@"<head>"]);

  EXPECT_TRUE([html containsString:ErrorAsString(ERR_CONNECTION_TIMED_OUT)]);
  EXPECT_TRUE([html containsString:GetNSString(IDS_ERRORPAGES_BUTTON_RELOAD)]);
}

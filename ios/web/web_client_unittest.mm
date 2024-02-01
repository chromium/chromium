// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_client.h"

#import <Foundation/Foundation.h>

#import "net/ssl/ssl_info.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using WebClientTest = PlatformTest;

// Tests WebClient::PrepareErrorPage method.
TEST_F(WebClientTest, PrepareErrorPage) {
  web::WebClient web_client;

  NSString* const description = @"a pretty bad error";
  NSError* error =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorNotConnectedToInternet
                      userInfo:@{NSLocalizedDescriptionKey : description}];

  std::optional<net::SSLInfo> info = std::nullopt;
  __block bool callback_called = false;
  __block NSString* html = nil;
  web_client.PrepareErrorPage(/*web_state*/ nullptr, GURL(), error,
                              /*is_post=*/false,
                              /*is_off_the_record=*/false,
                              /*info=*/info,
                              /*navigation_id=*/0,
                              base::BindOnce(^(NSString* error_html) {
                                html = error_html;
                                callback_called = true;
                              }));
  EXPECT_TRUE(callback_called);
  EXPECT_NSEQ(description, html);
}

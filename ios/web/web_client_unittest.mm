// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_client.h"

#import <Foundation/Foundation.h>

#include "net/ssl/ssl_info.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using WebClientTest = PlatformTest;

// Tests WebClient::PrepareErrorPage method.
TEST_F(WebClientTest, PrepareErrorPage) {
  web::WebClient web_client;

  NSString* const description = @"a pretty bad error";
  NSError* error =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorNotConnectedToInternet
                      userInfo:@{NSLocalizedDescriptionKey : description}];

  base::Optional<net::SSLInfo> info = base::nullopt;
  __block bool callback_called = false;
  __block NSString* html = nil;
  web_client.PrepareErrorPage(/*web_state*/ nullptr, GURL::EmptyGURL(), error,
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

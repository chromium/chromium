// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/url_opener_params.h"

#import <Foundation/Foundation.h>

#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

typedef PlatformTest URLOpenerParamsTest;

// Simple test for initWithUIOpenURLContext:.
// TODO(crbug.com/40166681): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_initWithUIOpenURLContext initWithUIOpenURLContext
#else
#define MAYBE_initWithUIOpenURLContext DISABLED_initWithUIOpenURLContext
#endif
TEST_F(URLOpenerParamsTest, MAYBE_initWithUIOpenURLContext) {
  NSURL* url = [NSURL URLWithString:@"https://url.test"];
  NSString* source = @"source";
  id open_url_context = [OCMockObject mockForClass:[UIOpenURLContext class]];
  id open_url_context_options =
      [OCMockObject mockForClass:[UISceneOpenURLOptions class]];
  OCMStub([open_url_context_options sourceApplication]).andReturn(source);
  OCMStub([open_url_context URL]).andReturn(url);
  [(UIOpenURLContext*)[[open_url_context stub]
      andReturn:open_url_context_options] options];

  URLOpenerParams* params =
      [[URLOpenerParams alloc] initWithUIOpenURLContext:open_url_context];

  EXPECT_NSEQ(url, params.URL);
  EXPECT_NSEQ(source, params.sourceApplication);
}

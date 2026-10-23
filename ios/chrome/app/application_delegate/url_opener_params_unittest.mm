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
TEST_F(URLOpenerParamsTest, initWithUIOpenURLContext) {
  NSURL* url = [NSURL URLWithString:@"https://url.test"];
  NSString* source = @"source";
  // `UIOpenURLContext` and `UISceneOpenURLOptions` cannot be instantiated
  // directly because UIKit marks `+new` and `-init` as `NS_UNAVAILABLE` and
  // exposes only read-only properties, so mocks are required. Use nice mocks
  // (`OCMClassMock`) rather than strict mocks so the test is not brittle to
  // harmless unstubbed calls (such as `-description` or internal UIKit
  // methods).
  UIOpenURLContext* open_url_context = OCMClassMock([UIOpenURLContext class]);
  UISceneOpenURLOptions* open_url_context_options =
      OCMClassMock([UISceneOpenURLOptions class]);
  OCMStub([open_url_context_options sourceApplication]).andReturn(source);
  OCMStub([open_url_context URL]).andReturn(url);
  OCMStub([open_url_context options]).andReturn(open_url_context_options);

  URLOpenerParams* params =
      [[URLOpenerParams alloc] initWithUIOpenURLContext:open_url_context];

  EXPECT_NSEQ(url, params.URL);
  EXPECT_NSEQ(source, params.sourceApplication);
}

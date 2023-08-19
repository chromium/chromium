// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_html_element_fetch_request.h"

#import "base/time/time.h"
#import "ios/web/js_features/context_menu/context_menu_constants.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace web {

using CRWHTMLElementFetchRequestTest = PlatformTest;

// Tests that `creationTime` is set at CRWHTMLElementFetchRequest object
// creation.
TEST_F(CRWHTMLElementFetchRequestTest, CreationTime) {
  CRWHTMLElementFetchRequest* request =
      [[CRWHTMLElementFetchRequest alloc] initWithFoundElementHandler:nil];
  base::TimeDelta delta = base::TimeTicks::Now() - request.creationTime;
  // Validate that `request.creationTime` is "now", but only use second
  // precision to avoid performance induced test flake.
  EXPECT_GT(1, delta.InSeconds());
}

// Tests that `runHandlerWithResponse:` runs the handler from the object's
// initializer with the expected `response`.
TEST_F(CRWHTMLElementFetchRequestTest, RunHandler) {
  __block bool handler_called = false;
  __block web::ContextMenuParams received_params;
  void (^handler)(const web::ContextMenuParams&) =
      ^(const web::ContextMenuParams& params) {
        handler_called = true;
        received_params = params;
      };
  CRWHTMLElementFetchRequest* request =
      [[CRWHTMLElementFetchRequest alloc] initWithFoundElementHandler:handler];
  web::ContextMenuParams params = web::ContextMenuParams();
  params.text = @"text";
  [request runHandlerWithResponse:params];
  EXPECT_TRUE(handler_called);
  EXPECT_NSEQ(params.text, received_params.text);
}

// Tests that `runHandlerWithResponse:` does not run the handler from the
// object's initializer if `invalidate` has been called.
TEST_F(CRWHTMLElementFetchRequestTest, Invalidate) {
  __block bool handler_called = false;
  void (^handler)(const web::ContextMenuParams&) =
      ^(const web::ContextMenuParams& params) {
        handler_called = true;
      };
  CRWHTMLElementFetchRequest* request =
      [[CRWHTMLElementFetchRequest alloc] initWithFoundElementHandler:handler];
  [request invalidate];
  [request runHandlerWithResponse:web::ContextMenuParams()];
  EXPECT_FALSE(handler_called);
}

}  // namespace web

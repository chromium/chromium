// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_view/wk_web_view_util.h"

#import "base/functional/bind.h"
#import "base/test/ios/wait_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class WKWebViewUtilTest : public PlatformTest {};

namespace web {

// Tests that CreateFullPagePDF calls createPDFWithConfiguration and it invokes
// the callback with NSData.
TEST_F(WKWebViewUtilTest, EnsureCallbackIsCalledWithData) {
  // Expected: callback is called with valid NSData.
  // Mock the web_view and make sure createPDFWithConfiguration's
  // completionHandler is invoked with NSData and no errors.
  id web_view_mock = OCMClassMock([WKWebView class]);
  OCMStub([web_view_mock createPDFWithConfiguration:[OCMArg any]
                                  completionHandler:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        void (^completion_block)(NSData* pdf_document_data, NSError* error);
        [invocation getArgument:&completion_block atIndex:3];
        completion_block([[NSData alloc] init], nil);
      });

  __block bool callback_called = false;
  __block NSData* callback_data = nil;

  CreateFullPagePdf(web_view_mock, base::BindOnce(^(NSData* data) {
                      callback_called = true;
                      callback_data = [data copy];
                    }));

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^bool {
        return callback_called;
      }));
  EXPECT_TRUE(callback_data);
}

// Tests that CreateFullPagePDF calls createPDFWithConfiguration and it
// generates an NSError.
TEST_F(WKWebViewUtilTest, EnsureCallbackIsCalledWithNil) {
  // Expected: callback is called with nil.
  // Mock the web_view and make sure createPDFWithConfiguration's
  // completionHandler is invoked with NSData and an error.
  id web_view_mock = OCMClassMock([WKWebView class]);
  NSError* error =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorServerCertificateHasUnknownRoot
                      userInfo:nil];
  OCMStub([web_view_mock createPDFWithConfiguration:[OCMArg any]
                                  completionHandler:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        void (^completion_block)(NSData* pdf_document_data, NSError* error);
        [invocation getArgument:&completion_block atIndex:3];
        completion_block(nil, error);
      });

  __block bool callback_called = false;
  __block NSData* callback_data = nil;

  CreateFullPagePdf(web_view_mock, base::BindOnce(^(NSData* data) {
                      callback_called = true;
                      callback_data = [data copy];
                    }));

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^bool {
        return callback_called;
      }));
  EXPECT_FALSE(callback_data);
}

// Tests that CreateFullPagePDF invokes the callback with NULL if
// its given a NULL WKWebView.
TEST_F(WKWebViewUtilTest, NULLWebView) {
  // Expected: callback is called with nil.
  __block bool callback_called = false;
  __block NSData* callback_data = nil;

  CreateFullPagePdf(nil, base::BindOnce(^(NSData* data) {
                      callback_called = true;
                      callback_data = [data copy];
                    }));

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^bool {
        return callback_called;
      }));
  EXPECT_FALSE(callback_data);
}
}  // namespace web

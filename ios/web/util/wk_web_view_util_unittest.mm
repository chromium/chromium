// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/util/wk_web_view_util.h"

#import "base/functional/bind.h"
#import "base/test/ios/wait_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class WKWebViewUtilTest : public PlatformTest {};

namespace web {

// Tests that `CreatePdf()` with default parameters leaves the configuration
// rect null and invokes the callback with the generated `NSData`.
TEST_F(WKWebViewUtilTest, EnsureCallbackIsCalledWithData) {
  NSData* expected_data = [@"Foo" dataUsingEncoding:NSUTF8StringEncoding];
  id web_view_mock = OCMClassMock([WKWebView class]);
  OCMStub([web_view_mock
      createPDFWithConfiguration:[OCMArg checkWithBlock:^BOOL(
                                             WKPDFConfiguration* config) {
        return CGRectIsNull(config.rect);
      }]
               completionHandler:[OCMArg checkWithBlock:^BOOL(
                                             void (^completion_block)(
                                                 NSData* pdf_document_data,
                                                 NSError* error)) {
                 completion_block(expected_data, nil);
                 return YES;
               }]]);

  __block bool callback_called = false;
  __block NSData* callback_data = nil;

  CreatePdf(web_view_mock, {}, base::BindOnce(^(NSData* data) {
              callback_called = true;
              callback_data = [data copy];
            }));

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^bool {
        return callback_called;
      }));
  EXPECT_NSEQ(expected_data, callback_data);
}

// Tests that `CreatePdf()` invokes the callback with nil when WebKit produces
// an `NSError`.
TEST_F(WKWebViewUtilTest, EnsureCallbackIsCalledWithNil) {
  id web_view_mock = OCMClassMock([WKWebView class]);
  NSError* error =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorServerCertificateHasUnknownRoot
                      userInfo:nil];
  OCMStub([web_view_mock
      createPDFWithConfiguration:[OCMArg any]
               completionHandler:[OCMArg checkWithBlock:^BOOL(
                                             void (^completion_block)(
                                                 NSData* pdf_document_data,
                                                 NSError* error)) {
                 completion_block(nil, error);
                 return YES;
               }]]);

  __block bool callback_called = false;
  __block NSData* callback_data = nil;

  CreatePdf(web_view_mock, {}, base::BindOnce(^(NSData* data) {
              callback_called = true;
              callback_data = [data copy];
            }));

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^bool {
        return callback_called;
      }));
  EXPECT_FALSE(callback_data);
}

// Tests that `CreatePdf()` invokes the callback with nil when given a nil
// `WKWebView`.
TEST_F(WKWebViewUtilTest, NULLWebView) {
  __block bool callback_called = false;
  __block NSData* callback_data = nil;

  CreatePdf(nil, {}, base::BindOnce(^(NSData* data) {
              callback_called = true;
              callback_data = [data copy];
            }));

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^bool {
        return callback_called;
      }));
  EXPECT_FALSE(callback_data);
}

// Tests that `CreatePdf()` forwards valid document rects (including the 1x1
// minimum size boundary) to WebKit through the PDF configuration and invokes
// the callback with the generated `NSData`.
TEST_F(WKWebViewUtilTest, CreatePdfForwardsDocumentRect) {
  const CGRect valid_rects[] = {
      CGRectMake(10, 2000, 400, 700),
      CGRectMake(0, 0, 1, 1),
  };
  NSData* expected_data = [@"Foo" dataUsingEncoding:NSUTF8StringEncoding];
  for (CGRect document_rect : valid_rects) {
    id web_view_mock = OCMClassMock([WKWebView class]);
    OCMStub([web_view_mock
        createPDFWithConfiguration:[OCMArg checkWithBlock:^BOOL(
                                               WKPDFConfiguration* config) {
          return CGRectEqualToRect(config.rect, document_rect);
        }]
                 completionHandler:[OCMArg checkWithBlock:^BOOL(
                                               void (^completion_block)(
                                                   NSData* pdf_document_data,
                                                   NSError* error)) {
                   completion_block(expected_data, nil);
                   return YES;
                 }]]);

    __block bool callback_called = false;
    __block NSData* callback_data = nil;

    CreatePdf(web_view_mock, document_rect, base::BindOnce(^(NSData* data) {
                callback_called = true;
                callback_data = [data copy];
              }));

    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForPageLoadTimeout, ^bool {
          return callback_called;
        }));
    EXPECT_NSEQ(expected_data, callback_data);
  }
}

// Tests that `CreatePdf()` invokes the callback with nil without asking WebKit
// for a PDF when the document rect is empty, smaller than one CSS pixel, or
// non-finite.
TEST_F(WKWebViewUtilTest, CreatePdfReturnsNilForInvalidDocumentRect) {
  id web_view_mock = OCMClassMock([WKWebView class]);
  OCMReject([web_view_mock createPDFWithConfiguration:[OCMArg any]
                                    completionHandler:[OCMArg any]]);

  const CGRect invalid_rects[] = {
      CGRectZero,
      CGRectMake(0, 0, 400, 0),
      CGRectMake(0, 0, 0.5, 100),
      CGRectMake(0, 0, 100, 0.99),
      CGRectMake(0, 0, -10, 100),
      CGRectNull,
      CGRectInfinite,
  };
  for (CGRect invalid_rect : invalid_rects) {
    __block bool callback_called = false;
    __block NSData* callback_data = nil;

    CreatePdf(web_view_mock, invalid_rect, base::BindOnce(^(NSData* data) {
                callback_called = true;
                callback_data = [data copy];
              }));

    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForPageLoadTimeout, ^bool {
          return callback_called;
        }));
    EXPECT_FALSE(callback_data);
  }
  EXPECT_OCMOCK_VERIFY(web_view_mock);
}
}  // namespace web

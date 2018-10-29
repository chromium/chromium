// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/web_http_protocol_handler_delegate.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/web_client.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

// Test application specific scheme.
const char kAppSpecificScheme[] = "appspecific";

// URLs expected to be supported.
const char* kSupportedURLs[] = {
    "http://foo.com",
    "https://foo.com",
    "data:text/html;charset=utf-8,Hello",
};

// URLs expected to be unsupported.
const char* kUnsupportedURLs[] = {
    "foo:blank",          // Unknown scheme.
    "appspecific:blank",  // No main document URL.
};

// Test web client with an application specific scheme.
class AppSpecificURLTestWebClient : public WebClient {
 public:
  bool IsAppSpecificURL(const GURL& url) const override {
    return url.SchemeIs(kAppSpecificScheme);
  }
};

class WebHTTPProtocolHandlerDelegateTest : public PlatformTest {
 public:
  WebHTTPProtocolHandlerDelegateTest()
      : context_getter_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())),
        delegate_(new WebHTTPProtocolHandlerDelegate(context_getter_.get())),
        web_client_(base::WrapUnique(new AppSpecificURLTestWebClient)) {}

 protected:
  base::MessageLoop message_loop_;
  scoped_refptr<net::URLRequestContextGetter> context_getter_;
  std::unique_ptr<WebHTTPProtocolHandlerDelegate> delegate_;
  web::ScopedTestingWebClient web_client_;
};

}  // namespace

TEST_F(WebHTTPProtocolHandlerDelegateTest, IsRequestSupported) {
  NSMutableURLRequest* request;

  for (unsigned int i = 0; i < base::size(kSupportedURLs); ++i) {
    NSString* url_string =
        [[NSString alloc] initWithUTF8String:kSupportedURLs[i]];
    request = [[NSMutableURLRequest alloc]
        initWithURL:[NSURL URLWithString:url_string]];
    EXPECT_TRUE(delegate_->IsRequestSupported(request))
        << kSupportedURLs[i] << " should be supported.";
  }

  for (unsigned int i = 0; i < base::size(kUnsupportedURLs); ++i) {
    NSString* url_string =
        [[NSString alloc] initWithUTF8String:kUnsupportedURLs[i]];
    request = [[NSMutableURLRequest alloc]
        initWithURL:[NSURL URLWithString:url_string]];
    EXPECT_FALSE(delegate_->IsRequestSupported(request))
        << kUnsupportedURLs[i] << " should NOT be supported.";
  }

  // Application specific scheme with main document URL.
  request = [[NSMutableURLRequest alloc]
      initWithURL:[NSURL URLWithString:@"appspecific:blank"]];
  [request setMainDocumentURL:[NSURL URLWithString:@"http://foo"]];
  EXPECT_FALSE(delegate_->IsRequestSupported(request));
  [request setMainDocumentURL:[NSURL URLWithString:@"appspecific:main"]];
  EXPECT_TRUE(delegate_->IsRequestSupported(request));
  request = [[NSMutableURLRequest alloc]
      initWithURL:[NSURL URLWithString:@"foo:blank"]];
  [request setMainDocumentURL:[NSURL URLWithString:@"appspecific:main"]];
  EXPECT_FALSE(delegate_->IsRequestSupported(request));
}

TEST_F(WebHTTPProtocolHandlerDelegateTest, IsRequestSupportedMalformed) {
  NSURLRequest* request;

  // Null URL.
  request = [[NSMutableURLRequest alloc] init];
  ASSERT_FALSE([request URL]);
  EXPECT_FALSE(delegate_->IsRequestSupported(request));

  // URL with no scheme.
  request =
      [[NSMutableURLRequest alloc] initWithURL:[NSURL URLWithString:@"foo"]];
  ASSERT_TRUE([request URL]);
  ASSERT_FALSE([[request URL] scheme]);
  EXPECT_FALSE(delegate_->IsRequestSupported(request));

  // Empty scheme.
  request =
      [[NSMutableURLRequest alloc] initWithURL:[NSURL URLWithString:@":foo"]];
  ASSERT_TRUE([request URL]);
  ASSERT_TRUE([[request URL] scheme]);
  ASSERT_FALSE([[[request URL] scheme] length]);
  EXPECT_FALSE(delegate_->IsRequestSupported(request));
}

// Tests that requests for images are considered as static file requests,
// regardless of the user agent.
TEST_F(WebHTTPProtocolHandlerDelegateTest, TestIsStaticImageRequestTrue) {
  // Empty dictionary so User-Agent check fails.
  NSDictionary* headers = @{};
  NSURL* url = [NSURL URLWithString:@"file:///show/this.png"];
  id mock_request = [OCMockObject mockForClass:[NSURLRequest class]];
  [[[mock_request stub] andReturn:headers] allHTTPHeaderFields];
  [[[mock_request stub] andReturn:url] URL];
  EXPECT_TRUE(IsStaticFileRequest(mock_request));
}

// Tests that requests for files are considered as static file requests if they
// have the static file user agent.
TEST_F(WebHTTPProtocolHandlerDelegateTest, TestIsStaticFileRequestTrue) {
  NSDictionary* headers =
      @{ @"User-Agent" : @"UIWebViewForStaticFileContent foo" };
  NSURL* url = [NSURL URLWithString:@"file:///some/random/url.html"];
  id mock_request = [OCMockObject mockForClass:[NSURLRequest class]];
  [[[mock_request stub] andReturn:headers] allHTTPHeaderFields];
  [[[mock_request stub] andReturn:url] URL];
  EXPECT_TRUE(IsStaticFileRequest(mock_request));
}

// Tests that arbitrary files cannot be retrieved by a web view for
// static file content.
TEST_F(WebHTTPProtocolHandlerDelegateTest, TestIsStaticFileRequestFalse) {
  // Empty dictionary so User-Agent check fails.
  NSDictionary* headers = @{};
  NSURL* url = [NSURL URLWithString:@"file:///steal/this/file.html"];
  id mock_request = [OCMockObject mockForClass:[NSURLRequest class]];
  [[[mock_request stub] andReturn:headers] allHTTPHeaderFields];
  [[[mock_request stub] andReturn:url] URL];
  EXPECT_FALSE(IsStaticFileRequest(mock_request));
}

}  // namespace web

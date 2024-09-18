// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/protocol_handler_util.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/task_environment.h"
#import "net/base/apple/url_conversions.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

namespace net {
namespace {

const char* kTextHtml = "text/html";

class HeadersURLRequestJob : public URLRequestJob {
 public:
  explicit HeadersURLRequestJob(URLRequest* request) : URLRequestJob(request) {}

  ~HeadersURLRequestJob() override {}

  void Start() override {
    // Fills response headers and returns immediately.
    NotifyHeadersComplete();
  }

  bool GetMimeType(std::string* mime_type) const override {
    *mime_type = GetContentTypeValue();
    return true;
  }

  void GetResponseInfo(HttpResponseInfo* info) override {
    // This is called by NotifyHeadersComplete().
    std::string header_string("HTTP/1.0 200 OK");
    header_string.push_back('\0');
    header_string += std::string("Cache-Control: max-age=600");
    header_string.push_back('\0');
    if (request()->url().path_piece() == "/multiplecontenttype") {
      header_string += std::string(
          "coNteNt-tYPe: text/plain; charset=iso-8859-4, image/png");
      header_string.push_back('\0');
    }
    header_string += std::string("Content-Type: ") + GetContentTypeValue();
    header_string.push_back('\0');
    header_string += std::string("Foo: A");
    header_string.push_back('\0');
    header_string += std::string("Bar: B");
    header_string.push_back('\0');
    header_string += std::string("Baz: C");
    header_string.push_back('\0');
    header_string += std::string("Foo: D");
    header_string.push_back('\0');
    header_string += std::string("Foo: E");
    header_string.push_back('\0');
    header_string += std::string("Bar: F");
    header_string.push_back('\0');
    info->headers = new HttpResponseHeaders(header_string);
  }

 protected:
  std::string GetContentTypeValue() const {
    if (request()->url().path_piece() == "/badcontenttype")
      return "\xff";
    return kTextHtml;
  }
};

class NetURLRequestInterceptor : public URLRequestInterceptor {
 public:
  std::unique_ptr<URLRequestJob> MaybeInterceptRequest(
      URLRequest* request) const override {
    return std::make_unique<HeadersURLRequestJob>(request);
  }
};

class ProtocolHandlerUtilTest : public PlatformTest,
                                public URLRequest::Delegate {
 public:
  ProtocolHandlerUtilTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        request_context_(net::CreateTestURLRequestContextBuilder()->Build()) {
    URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        "http", "foo.test", std::make_unique<NetURLRequestInterceptor>());
  }

  ~ProtocolHandlerUtilTest() override {
    URLRequestFilter::GetInstance()->ClearHandlers();
  }

  void OnResponseStarted(URLRequest* request, int net_error) override {}
  void OnReadCompleted(URLRequest* request, int bytes_read) override {}

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<URLRequestContext> request_context_;
};

}  // namespace

TEST_F(ProtocolHandlerUtilTest, GetResponseHttpTest) {
  // Create a request.
  GURL url("http://foo.test/");
  std::unique_ptr<URLRequest> request(
      request_context_->CreateRequest(url, DEFAULT_PRIORITY, this));
  request->Start();
  // Create a response from the request.
  NSURLResponse* response = GetNSURLResponseForRequest(request.get());
  EXPECT_NSEQ([NSString stringWithUTF8String:kTextHtml], [response MIMEType]);
  ASSERT_TRUE([response isKindOfClass:[NSHTTPURLResponse class]]);
  NSHTTPURLResponse* http_response = (NSHTTPURLResponse*)response;
  NSDictionary* headers = [http_response allHeaderFields];
  // Check the headers, duplicates must be appended.
  EXPECT_EQ(5u, [headers count]);
  NSString* foo_header = [headers objectForKey:@"Foo"];
  EXPECT_NSEQ(@"A,D,E", foo_header);
  NSString* bar_header = [headers objectForKey:@"Bar"];
  EXPECT_NSEQ(@"B,F", bar_header);
  NSString* baz_header = [headers objectForKey:@"Baz"];
  EXPECT_NSEQ(@"C", baz_header);
  NSString* cache_header = [headers objectForKey:@"Cache-Control"];
  EXPECT_NSEQ(@"no-store", cache_header);  // Cache-Control is overridden.
  // Check the status.
  EXPECT_EQ(request->GetResponseCode(), [http_response statusCode]);
}

TEST_F(ProtocolHandlerUtilTest, BadHttpContentType) {
  // Create a request using the magic path that triggers a garbage
  // content-type in the test framework.
  GURL url("http://foo.test/badcontenttype");
  std::unique_ptr<URLRequest> request(
      request_context_->CreateRequest(url, DEFAULT_PRIORITY, this));
  request->Start();
  // Create a response from the request.
  @try {
    GetNSURLResponseForRequest(request.get());
  }
  @catch (id exception) {
    FAIL() << "Exception while creating response";
  }
}

TEST_F(ProtocolHandlerUtilTest, MultipleHttpContentType) {
  // Create a request using the magic path that triggers a garbage
  // content-type in the test framework.
  GURL url("http://foo.test/multiplecontenttype");
  std::unique_ptr<URLRequest> request(
      request_context_->CreateRequest(url, DEFAULT_PRIORITY, this));
  request->Start();
  // Create a response from the request.
  NSURLResponse* response = GetNSURLResponseForRequest(request.get());
  EXPECT_NSEQ(@"text/plain", [response MIMEType]);
  EXPECT_NSEQ(@"iso-8859-4", [response textEncodingName]);
  NSHTTPURLResponse* http_response = (NSHTTPURLResponse*)response;
  NSDictionary* headers = [http_response allHeaderFields];
  NSString* content_type_header = [headers objectForKey:@"Content-Type"];
  EXPECT_NSEQ(@"text/plain; charset=iso-8859-4", content_type_header);
}

TEST_F(ProtocolHandlerUtilTest, CopyHttpHeaders) {
  GURL url("http://foo.test/");
  NSMutableURLRequest* in_request =
      [[NSMutableURLRequest alloc] initWithURL:NSURLWithGURL(url)];
  [in_request setAllHTTPHeaderFields:@{
      @"Referer" : @"referrer",
      @"User-Agent" : @"secret",
      @"Accept" : @"money/cash",
      @"Foo" : @"bar",
  }];
  std::unique_ptr<URLRequest> out_request(
      request_context_->CreateRequest(url, DEFAULT_PRIORITY, nullptr));
  CopyHttpHeaders(in_request, out_request.get());

  EXPECT_EQ("referrer", out_request->referrer());
  const HttpRequestHeaders& headers = out_request->extra_request_headers();
  EXPECT_FALSE(headers.HasHeader("Content-Type"));  // Only in POST requests.
  EXPECT_EQ("money/cash", headers.GetHeader("Accept"));
  EXPECT_EQ("bar", headers.GetHeader("Foo"));
}

TEST_F(ProtocolHandlerUtilTest, AddMissingHeaders) {
  GURL url("http://foo.test/");
  NSMutableURLRequest* in_request =
      [[NSMutableURLRequest alloc] initWithURL:NSURLWithGURL(url)];
  std::unique_ptr<URLRequest> out_request(
      request_context_->CreateRequest(url, DEFAULT_PRIORITY, nullptr));
  out_request->set_method("POST");
  auto reader = std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring(""));
  out_request->set_upload(
      ElementsUploadDataStream::CreateWithReader(std::move(reader)));
  CopyHttpHeaders(in_request, out_request.get());

  // Some headers are added by default if missing.
  const HttpRequestHeaders& headers = out_request->extra_request_headers();
  EXPECT_EQ("*/*", headers.GetHeader("Accept"));
  EXPECT_EQ("application/x-www-form-urlencoded",
            headers.GetHeader("Content-Type"));
}

}  // namespace net

// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/protocol_handler_util.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/task_environment.h"
#include "net/base/elements_upload_data_stream.h"
#import "net/base/mac/url_conversions.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// When C++ exceptions are disabled, the C++ library defines |try| and
// |catch| so as to allow exception-expecting C++ code to build properly when
// language support for exceptions is not present.  These macros interfere
// with the use of |@try| and |@catch| in Objective-C files such as this one.
// Undefine these macros here, after everything has been #included, since
// there will be no C++ uses and only Objective-C uses from this point on.
#undef try
#undef catch

namespace net {
namespace {

const char* kTextHtml = "text/html";

class HeadersURLRequestJob : public URLRequestJob {
 public:
  HeadersURLRequestJob(URLRequest* request)
      : URLRequestJob(request, nullptr) {}

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
    if (request()->url().DomainIs("multiplecontenttype")) {
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
  ~HeadersURLRequestJob() override {}

  std::string GetContentTypeValue() const {
    if (request()->url().DomainIs("badcontenttype"))
      return "\xff";
    return kTextHtml;
  }
};

class NetProtocolHandler : public URLRequestJobFactory::ProtocolHandler {
 public:
  URLRequestJob* MaybeCreateJob(
      URLRequest* request,
      NetworkDelegate* network_delegate) const override {
    return new HeadersURLRequestJob(request);
  }
};

class ProtocolHandlerUtilTest : public PlatformTest,
                                public URLRequest::Delegate {
 public:
  ProtocolHandlerUtilTest() : request_context_(new TestURLRequestContext) {
    // Ownership of the protocol handlers is transferred to the factory.
    job_factory_.SetProtocolHandler("http",
                                    base::WrapUnique(new NetProtocolHandler));
    request_context_->set_job_factory(&job_factory_);
  }

  void OnResponseStarted(URLRequest* request, int net_error) override {}
  void OnReadCompleted(URLRequest* request, int bytes_read) override {}

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  URLRequestJobFactoryImpl job_factory_;
  std::unique_ptr<URLRequestContext> request_context_;
};

}  // namespace

TEST_F(ProtocolHandlerUtilTest, GetResponseHttpTest) {
  // Create a request.
  GURL url(std::string("http://url"));
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
  // Create a request using the magic domain that triggers a garbage
  // content-type in the test framework.
  GURL url(std::string("http://badcontenttype"));
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
  // Create a request using the magic domain that triggers a garbage
  // content-type in the test framework.
  GURL url(std::string("http://multiplecontenttype"));
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
  GURL url(std::string("http://url"));
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
  EXPECT_FALSE(headers.HasHeader("User-Agent"));    // User agent is not copied.
  EXPECT_FALSE(headers.HasHeader("Content-Type"));  // Only in POST requests.
  std::string header;
  EXPECT_TRUE(headers.GetHeader("Accept", &header));
  EXPECT_EQ("money/cash", header);
  EXPECT_TRUE(headers.GetHeader("Foo", &header));
  EXPECT_EQ("bar", header);
}

TEST_F(ProtocolHandlerUtilTest, AddMissingHeaders) {
  GURL url(std::string("http://url"));
  NSMutableURLRequest* in_request =
      [[NSMutableURLRequest alloc] initWithURL:NSURLWithGURL(url)];
  std::unique_ptr<URLRequest> out_request(
      request_context_->CreateRequest(url, DEFAULT_PRIORITY, nullptr));
  out_request->set_method("POST");
  std::unique_ptr<UploadElementReader> reader(
      new UploadBytesElementReader(nullptr, 0));
  out_request->set_upload(
      ElementsUploadDataStream::CreateWithReader(std::move(reader), 0));
  CopyHttpHeaders(in_request, out_request.get());

  // Some headers are added by default if missing.
  const HttpRequestHeaders& headers = out_request->extra_request_headers();
  std::string header;
  EXPECT_TRUE(headers.GetHeader("Accept", &header));
  EXPECT_EQ("*/*", header);
  EXPECT_TRUE(headers.GetHeader("Content-Type", &header));
  EXPECT_EQ("application/x-www-form-urlencoded", header);
}

}  // namespace net

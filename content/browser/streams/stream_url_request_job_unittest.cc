// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/streams/stream_url_request_job.h"

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_metadata.h"
#include "content/browser/streams/stream_registry.h"
#include "content/browser/streams/stream_write_observer.h"
#include "net/base/request_priority.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const int kBufferSize = 1024;
const char kTestData1[] = "Hello";
const char kTestData2[] = "Here it is data.";

const GURL kStreamURL("blob://stream");

}  // namespace

class StreamURLRequestJobTest : public testing::Test {
 public:
  // A simple ProtocolHandler implementation to create StreamURLRequestJob.
  class MockProtocolHandler :
      public net::URLRequestJobFactory::ProtocolHandler {
   public:
    MockProtocolHandler(StreamRegistry* registry) : registry_(registry) {}

    // net::URLRequestJobFactory::ProtocolHandler override.
    net::URLRequestJob* MaybeCreateJob(
        net::URLRequest* request,
        net::NetworkDelegate* network_delegate) const override {
      scoped_refptr<Stream> stream = registry_->GetStream(request->url());
      if (stream.get())
        return new StreamURLRequestJob(request, network_delegate, stream);
      return nullptr;
    }

   private:
    StreamRegistry* registry_;
  };

  StreamURLRequestJobTest() {}

  void SetUp() override {
    registry_.reset(new StreamRegistry());

    url_request_job_factory_.SetProtocolHandler(
        "blob", std::make_unique<MockProtocolHandler>(registry_.get()));
    url_request_context_.set_job_factory(&url_request_job_factory_);
  }

  void TearDown() override {}

  void TestSuccessRequest(const GURL& url,
                          const std::string& expected_response) {
    TestRequest("GET", url, net::HttpRequestHeaders(), 200, net::OK,
                expected_response);
  }

  std::unique_ptr<net::HttpResponseInfo> BuildResponseInfo() {
    auto response_info = std::make_unique<net::HttpResponseInfo>();
    response_info->headers = new net::HttpResponseHeaders("HTTP/1.1 200 OK");
    return response_info;
  }

  void TestRequest(const std::string& method,
                   const GURL& url,
                   const net::HttpRequestHeaders& extra_headers,
                   int expected_status_code,
                   int expected_error_code,
                   const std::string& expected_response) {
    net::TestDelegate delegate;
    request_ = url_request_context_.CreateRequest(
        url, net::DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
    request_->set_method(method);
    if (!extra_headers.IsEmpty())
      request_->SetExtraRequestHeaders(extra_headers);
    request_->Start();

    base::RunLoop().RunUntilIdle();

    // Verify response.
    if (expected_error_code)
      EXPECT_EQ(expected_error_code, request_->status().error());
    else
      EXPECT_TRUE(request_->status().is_success());
    ASSERT_TRUE(request_->response_headers());
    EXPECT_EQ(expected_status_code,
              request_->response_headers()->response_code());
    EXPECT_EQ(expected_response, delegate.data_received());
  }

 protected:
  base::test::ScopedTaskEnvironment task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<StreamRegistry> registry_;

  net::URLRequestContext url_request_context_;
  net::URLRequestJobFactoryImpl url_request_job_factory_;
  std::unique_ptr<net::URLRequest> request_;
};

TEST_F(StreamURLRequestJobTest, TestGetSimpleDataRequest) {
  scoped_refptr<Stream> stream(
      new Stream(registry_.get(), nullptr, kStreamURL));
  stream->OnResponseStarted(*BuildResponseInfo());

  scoped_refptr<net::StringIOBuffer> buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kTestData1);

  stream->AddData(buffer, buffer->size());
  stream->Finalize(net::OK);

  TestSuccessRequest(kStreamURL, kTestData1);
}

TEST_F(StreamURLRequestJobTest, TestGetLargeStreamRequest) {
  scoped_refptr<Stream> stream(
      new Stream(registry_.get(), nullptr, kStreamURL));
  stream->OnResponseStarted(*BuildResponseInfo());

  std::string large_data;
  large_data.reserve(kBufferSize * 5);
  for (int i = 0; i < kBufferSize * 5; ++i)
    large_data.append(1, static_cast<char>(i % 256));

  scoped_refptr<net::StringIOBuffer> buffer =
      base::MakeRefCounted<net::StringIOBuffer>(large_data);

  stream->AddData(buffer, buffer->size());
  stream->Finalize(net::OK);
  TestSuccessRequest(kStreamURL, large_data);
}

TEST_F(StreamURLRequestJobTest, TestGetNonExistentStreamRequest) {
  net::TestDelegate delegate;
  request_ = url_request_context_.CreateRequest(
      kStreamURL, net::DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  request_->set_method("GET");
  request_->Start();

  base::RunLoop().RunUntilIdle();

  // Verify response.
  EXPECT_FALSE(request_->status().is_success());
}

TEST_F(StreamURLRequestJobTest, TestRangeDataRequest) {
  scoped_refptr<Stream> stream(
      new Stream(registry_.get(), nullptr, kStreamURL));
  stream->OnResponseStarted(*BuildResponseInfo());

  scoped_refptr<net::StringIOBuffer> buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kTestData2);

  stream->AddData(buffer, buffer->size());
  stream->Finalize(net::OK);

  net::HttpRequestHeaders extra_headers;
  extra_headers.SetHeader(net::HttpRequestHeaders::kRange,
                          net::HttpByteRange::Bounded(0, 3).GetHeaderValue());
  TestRequest("GET", kStreamURL, extra_headers,
              200, net::OK, std::string(kTestData2, 4));
}

TEST_F(StreamURLRequestJobTest, TestInvalidRangeDataRequest) {
  scoped_refptr<Stream> stream(
      new Stream(registry_.get(), nullptr, kStreamURL));
  stream->OnResponseStarted(*BuildResponseInfo());
  scoped_refptr<net::StringIOBuffer> buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kTestData2);

  stream->AddData(buffer, buffer->size());
  stream->Finalize(net::OK);

  net::HttpRequestHeaders extra_headers;
  extra_headers.SetHeader(net::HttpRequestHeaders::kRange,
                          net::HttpByteRange::Bounded(1, 3).GetHeaderValue());
  TestRequest("GET", kStreamURL, extra_headers, 405,
              net::ERR_METHOD_NOT_SUPPORTED, std::string());
}

}  // namespace content

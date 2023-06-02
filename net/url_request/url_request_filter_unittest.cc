// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_filter.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "net/base/request_priority.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class TestURLRequestInterceptor : public URLRequestInterceptor {
 public:
  TestURLRequestInterceptor() = default;

  TestURLRequestInterceptor(const TestURLRequestInterceptor&) = delete;
  TestURLRequestInterceptor& operator=(const TestURLRequestInterceptor&) =
      delete;

  ~TestURLRequestInterceptor() override = default;

  // URLRequestInterceptor implementation:
  std::unique_ptr<URLRequestJob> MaybeInterceptRequest(
      URLRequest* request) const override {
    auto job = std::make_unique<URLRequestTestJob>(request);
    job_ = job.get();
    return job;
  }

  // Is |job| the URLRequestJob generated during interception?
  bool WasLastJobCreated(URLRequestJob* job) const {
    return job_ && job_ == job;
  }

 private:
  mutable raw_ptr<URLRequestTestJob, DanglingUntriaged> job_ = nullptr;
};

TEST(URLRequestFilter, BasicMatching) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
  TestDelegate delegate;
  auto context = CreateTestURLRequestContextBuilder()->Build();
  URLRequestFilter* filter = URLRequestFilter::GetInstance();

  const GURL kUrl1("http://foo.com/");
  std::unique_ptr<URLRequest> request1(context->CreateRequest(
      kUrl1, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));

  const GURL kUrl2("http://bar.com/");
  std::unique_ptr<URLRequest> request2(context->CreateRequest(
      kUrl2, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));

  // Check AddUrlInterceptor checks for invalid URLs.
  EXPECT_FALSE(filter->AddUrlInterceptor(
      GURL(), std::make_unique<TestURLRequestInterceptor>()));

  // Check URLRequestInterceptor URL matching.
  filter->ClearHandlers();
  auto interceptor1 = std::make_unique<TestURLRequestInterceptor>();
  auto* interceptor1_ptr = interceptor1.get();
  EXPECT_TRUE(filter->AddUrlInterceptor(kUrl1, std::move(interceptor1)));
  {
    std::unique_ptr<URLRequestJob> found =
        filter->MaybeInterceptRequest(request1.get());
    EXPECT_TRUE(interceptor1_ptr->WasLastJobCreated(found.get()));
  }
  EXPECT_EQ(filter->hit_count(), 1);

  // Check we don't match other URLs.
  EXPECT_FALSE(filter->MaybeInterceptRequest(request2.get()));
  EXPECT_EQ(1, filter->hit_count());

  // Check we can remove URL matching.
  filter->RemoveUrlHandler(kUrl1);
  EXPECT_FALSE(filter->MaybeInterceptRequest(request1.get()));
  EXPECT_EQ(1, filter->hit_count());

  // Check hostname matching.
  filter->ClearHandlers();
  EXPECT_EQ(0, filter->hit_count());
  auto interceptor2 = std::make_unique<TestURLRequestInterceptor>();
  auto* interceptor2_ptr = interceptor2.get();
  filter->AddHostnameInterceptor(kUrl1.scheme(), kUrl1.host(),
                                 std::move(interceptor2));
  {
    std::unique_ptr<URLRequestJob> found =
        filter->MaybeInterceptRequest(request1.get());
    EXPECT_TRUE(interceptor2_ptr->WasLastJobCreated(found.get()));
  }
  EXPECT_EQ(1, filter->hit_count());

  // Check we don't match other hostnames.
  EXPECT_FALSE(filter->MaybeInterceptRequest(request2.get()));
  EXPECT_EQ(1, filter->hit_count());

  // Check we can remove hostname matching.
  filter->RemoveHostnameHandler(kUrl1.scheme(), kUrl1.host());
  EXPECT_FALSE(filter->MaybeInterceptRequest(request1.get()));
  EXPECT_EQ(1, filter->hit_count());

  filter->ClearHandlers();
}

}  // namespace

}  // namespace net

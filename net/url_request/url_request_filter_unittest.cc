// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_filter.h"

#include <memory>

#include "base/macros.h"
#include "base/test/task_environment.h"
#include "net/base/request_priority.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class TestURLRequestInterceptor : public URLRequestInterceptor {
 public:
  TestURLRequestInterceptor() : job_(nullptr) {}
  ~TestURLRequestInterceptor() override = default;

  // URLRequestInterceptor implementation:
  URLRequestJob* MaybeInterceptRequest(
      URLRequest* request,
      NetworkDelegate* network_delegate) const override {
    job_ = new URLRequestTestJob(request, network_delegate);
    return job_;
  }

  // Is |job| the URLRequestJob generated during interception?
  bool WasLastJobCreated(URLRequestJob* job) const {
    return job_ && job_ == job;
  }

 private:
  mutable URLRequestTestJob* job_;

  DISALLOW_COPY_AND_ASSIGN(TestURLRequestInterceptor);
};

TEST(URLRequestFilter, BasicMatching) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
  TestDelegate delegate;
  TestURLRequestContext request_context;
  URLRequestFilter* filter = URLRequestFilter::GetInstance();

  const GURL kUrl1("http://foo.com/");
  std::unique_ptr<URLRequest> request1(request_context.CreateRequest(
      kUrl1, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));

  const GURL kUrl2("http://bar.com/");
  std::unique_ptr<URLRequest> request2(request_context.CreateRequest(
      kUrl2, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));

  // Check AddUrlInterceptor checks for invalid URLs.
  EXPECT_FALSE(filter->AddUrlInterceptor(
      GURL(),
      std::unique_ptr<URLRequestInterceptor>(new TestURLRequestInterceptor())));

  // Check URLRequestInterceptor URL matching.
  filter->ClearHandlers();
  TestURLRequestInterceptor* interceptor = new TestURLRequestInterceptor();
  EXPECT_TRUE(filter->AddUrlInterceptor(
      kUrl1, std::unique_ptr<URLRequestInterceptor>(interceptor)));
  {
    std::unique_ptr<URLRequestJob> found(
        filter->MaybeInterceptRequest(request1.get(), nullptr));
    EXPECT_TRUE(interceptor->WasLastJobCreated(found.get()));
  }
  EXPECT_EQ(filter->hit_count(), 1);

  // Check we don't match other URLs.
  EXPECT_TRUE(filter->MaybeInterceptRequest(request2.get(), nullptr) ==
              nullptr);
  EXPECT_EQ(1, filter->hit_count());

  // Check we can remove URL matching.
  filter->RemoveUrlHandler(kUrl1);
  EXPECT_TRUE(filter->MaybeInterceptRequest(request1.get(), nullptr) ==
              nullptr);
  EXPECT_EQ(1, filter->hit_count());

  // Check hostname matching.
  filter->ClearHandlers();
  EXPECT_EQ(0, filter->hit_count());
  interceptor = new TestURLRequestInterceptor();
  filter->AddHostnameInterceptor(
      kUrl1.scheme(), kUrl1.host(),
      std::unique_ptr<URLRequestInterceptor>(interceptor));
  {
    std::unique_ptr<URLRequestJob> found(
        filter->MaybeInterceptRequest(request1.get(), nullptr));
    EXPECT_TRUE(interceptor->WasLastJobCreated(found.get()));
  }
  EXPECT_EQ(1, filter->hit_count());

  // Check we don't match other hostnames.
  EXPECT_TRUE(filter->MaybeInterceptRequest(request2.get(), nullptr) ==
              nullptr);
  EXPECT_EQ(1, filter->hit_count());

  // Check we can remove hostname matching.
  filter->RemoveHostnameHandler(kUrl1.scheme(), kUrl1.host());
  EXPECT_TRUE(filter->MaybeInterceptRequest(request1.get(), nullptr) ==
              nullptr);
  EXPECT_EQ(1, filter->hit_count());

  filter->ClearHandlers();
}

}  // namespace

}  // namespace net

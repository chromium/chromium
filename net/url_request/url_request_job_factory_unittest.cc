// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job_factory.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "net/base/request_priority.h"
#include "net/test/gtest_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

class MockURLRequestJob : public URLRequestJob {
 public:
  explicit MockURLRequestJob(URLRequest* request) : URLRequestJob(request) {}

  ~MockURLRequestJob() override = default;

  void Start() override {
    // Start reading asynchronously so that all error reporting and data
    // callbacks happen as they would for network requests.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&MockURLRequestJob::StartAsync,
                                  weak_factory_.GetWeakPtr()));
  }

 private:
  void StartAsync() { NotifyHeadersComplete(); }

  base::WeakPtrFactory<MockURLRequestJob> weak_factory_{this};
};

class DummyProtocolHandler : public URLRequestJobFactory::ProtocolHandler {
 public:
  std::unique_ptr<URLRequestJob> CreateJob(URLRequest* request) const override {
    return std::make_unique<MockURLRequestJob>(request);
  }
};

TEST(URLRequestJobFactoryTest, NoProtocolHandler) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
  TestDelegate delegate;
  auto request_context = CreateTestURLRequestContextBuilder()->Build();
  std::unique_ptr<URLRequest> request(
      request_context->CreateRequest(GURL("foo://bar"), DEFAULT_PRIORITY,
                                     &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();

  delegate.RunUntilComplete();
  EXPECT_EQ(ERR_UNKNOWN_URL_SCHEME, delegate.request_status());
}

TEST(URLRequestJobFactoryTest, BasicProtocolHandler) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
  TestDelegate delegate;
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->SetProtocolHandler("foo",
                                      std::make_unique<DummyProtocolHandler>());
  auto request_context = context_builder->Build();
  std::unique_ptr<URLRequest> request(
      request_context->CreateRequest(GURL("foo://bar"), DEFAULT_PRIORITY,
                                     &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();

  delegate.RunUntilComplete();
  EXPECT_EQ(OK, delegate.request_status());
}

}  // namespace

}  // namespace net

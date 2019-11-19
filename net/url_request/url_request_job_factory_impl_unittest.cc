// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job_factory_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/request_priority.h"
#include "net/test/gtest_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
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
  MockURLRequestJob(URLRequest* request, NetworkDelegate* network_delegate)
      : URLRequestJob(request, network_delegate) {}

  void Start() override {
    // Start reading asynchronously so that all error reporting and data
    // callbacks happen as they would for network requests.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&MockURLRequestJob::StartAsync,
                                  weak_factory_.GetWeakPtr()));
  }

 protected:
  ~MockURLRequestJob() override = default;

 private:
  void StartAsync() {
    NotifyHeadersComplete();
  }

  base::WeakPtrFactory<MockURLRequestJob> weak_factory_{this};
};

class DummyProtocolHandler : public URLRequestJobFactory::ProtocolHandler {
 public:
  URLRequestJob* MaybeCreateJob(
      URLRequest* request,
      NetworkDelegate* network_delegate) const override {
    return new MockURLRequestJob(request, network_delegate);
  }
};

TEST(URLRequestJobFactoryTest, NoProtocolHandler) {
  base::test::TaskEnvironment task_environment;
  TestDelegate delegate;
  TestURLRequestContext request_context;
  std::unique_ptr<URLRequest> request(
      request_context.CreateRequest(GURL("foo://bar"), DEFAULT_PRIORITY,
                                    &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();

  base::RunLoop().Run();
  EXPECT_EQ(ERR_UNKNOWN_URL_SCHEME, delegate.request_status());
}

TEST(URLRequestJobFactoryTest, BasicProtocolHandler) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
  TestDelegate delegate;
  URLRequestJobFactoryImpl job_factory;
  TestURLRequestContext request_context;
  request_context.set_job_factory(&job_factory);
  job_factory.SetProtocolHandler("foo",
                                 std::make_unique<DummyProtocolHandler>());
  std::unique_ptr<URLRequest> request(
      request_context.CreateRequest(GURL("foo://bar"), DEFAULT_PRIORITY,
                                    &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();

  base::RunLoop().Run();
  EXPECT_EQ(OK, delegate.request_status());
}

TEST(URLRequestJobFactoryTest, DeleteProtocolHandler) {
  base::test::TaskEnvironment task_environment;
  URLRequestJobFactoryImpl job_factory;
  TestURLRequestContext request_context;
  request_context.set_job_factory(&job_factory);
  job_factory.SetProtocolHandler("foo",
                                 std::make_unique<DummyProtocolHandler>());
  job_factory.SetProtocolHandler("foo", nullptr);
}

}  // namespace

}  // namespace net

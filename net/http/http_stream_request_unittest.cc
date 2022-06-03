// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_request.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_stream_factory_job.h"
#include "net/http/http_stream_factory_job_controller.h"
#include "net/http/http_stream_factory_test_util.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/spdy/spdy_test_util_common.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace net {

// Make sure that Request passes on its priority updates to its jobs.
TEST(HttpStreamRequestTest, SetPriority) {
  base::test::TaskEnvironment task_environment;

  SequencedSocketData data;
  data.set_connect_data(MockConnect(ASYNC, OK));
  auto ssl_data = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());
  session_deps.socket_factory->AddSocketDataProvider(&data);
  session_deps.socket_factory->AddSSLSocketDataProvider(ssl_data.get());

  std::unique_ptr<HttpNetworkSession> session =
      SpdySessionDependencies::SpdyCreateSession(&session_deps);
  HttpStreamFactory* factory = session->http_stream_factory();
  MockHttpStreamRequestDelegate request_delegate;
  TestJobFactory job_factory;
  HttpRequestInfo request_info;
  request_info.url = GURL("http://www.example.com/");
  auto job_controller = std::make_unique<HttpStreamFactory::JobController>(
      factory, &request_delegate, session.get(), &job_factory, request_info,
      /* is_preconnect = */ false,
      /* is_websocket = */ false,
      /* enable_ip_based_pooling = */ true,
      /* enable_alternative_services = */ true, SSLConfig(), SSLConfig());
  HttpStreamFactory::JobController* job_controller_raw_ptr =
      job_controller.get();
  factory->job_controller_set_.insert(std::move(job_controller));

  std::unique_ptr<HttpStreamRequest> request(job_controller_raw_ptr->Start(
      &request_delegate, nullptr, NetLogWithSource(),
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY));
  EXPECT_TRUE(job_controller_raw_ptr->main_job());
  EXPECT_EQ(DEFAULT_PRIORITY, job_controller_raw_ptr->main_job()->priority());

  request->SetPriority(MEDIUM);
  EXPECT_EQ(MEDIUM, job_controller_raw_ptr->main_job()->priority());

  EXPECT_CALL(request_delegate, OnStreamFailed(_, _, _, _, _)).Times(1);
  job_controller_raw_ptr->OnStreamFailed(job_factory.main_job(), ERR_FAILED,
                                         SSLConfig());

  request->SetPriority(IDLE);
  EXPECT_EQ(IDLE, job_controller_raw_ptr->main_job()->priority());
  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}
}  // namespace net

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/gcd_state_updater.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringize_macros.h"
#include "base/test/bind_test_util.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "remoting/base/constants.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/host/gcd_rest_client.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace remoting {

class GcdStateUpdaterTest : public testing::Test {
 public:
  GcdStateUpdaterTest()
      : task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
        test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        token_getter_(OAuthTokenGetter::SUCCESS,
                      "<fake_user_email>",
                      "<fake_access_token>"),
        rest_client_(new GcdRestClient("http://gcd_base_url",
                                       "<gcd_device_id>",
                                       test_shared_url_loader_factory_,
                                       &token_getter_)),
        signal_strategy_(SignalingAddress("local_jid")) {
    rest_client_->SetClockForTest(&test_clock_);
  }

  void OnSuccess() { on_success_count_++; }

  void OnHostIdError() { on_host_id_error_count_++; }

  network::TestURLLoaderFactory::PendingRequest* GetPendingRequest(
      size_t index = 0) {
    if (index >= test_url_loader_factory_.pending_requests()->size())
      return nullptr;
    auto* request = &(*test_url_loader_factory_.pending_requests())[index];
    DCHECK(request);
    return request;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::SimpleTestClock test_clock_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  FakeOAuthTokenGetter token_getter_;
  std::unique_ptr<GcdRestClient> rest_client_;
  FakeSignalStrategy signal_strategy_;
  int on_success_count_ = 0;
  int on_host_id_error_count_ = 0;
};

TEST_F(GcdStateUpdaterTest, Success) {
  std::unique_ptr<GcdStateUpdater> updater(new GcdStateUpdater(
      base::Bind(&GcdStateUpdaterTest::OnSuccess, base::Unretained(this)),
      base::Bind(&GcdStateUpdaterTest::OnHostIdError, base::Unretained(this)),
      &signal_strategy_, std::move(rest_client_)));

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ("{\"patches\":[{\"patch\":{"
            "\"base\":{\"_hostVersion\":\"" STRINGIZE(VERSION) "\","
            "\"_jabberId\":\"local_jid\"}},"
            "\"timeMs\":0.0}],\"requestTimeMs\":0.0}",
            network::GetUploadData(request));

        test_url_loader_factory_.AddResponse(request.url.spec(), std::string(),
                                             net::HTTP_OK);
      }));

  signal_strategy_.Connect();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(1, on_success_count_);

  updater.reset();

  EXPECT_EQ(0, on_host_id_error_count_);
}

TEST_F(GcdStateUpdaterTest, QueuedRequests) {
  std::unique_ptr<GcdStateUpdater> updater(new GcdStateUpdater(
      base::Bind(&GcdStateUpdaterTest::OnSuccess, base::Unretained(this)),
      base::Bind(&GcdStateUpdaterTest::OnHostIdError, base::Unretained(this)),
      &signal_strategy_, std::move(rest_client_)));

  // Connect, then re-connect with a different JID while the status
  // update for the first connection is pending.
  signal_strategy_.Connect();
  task_environment_.RunUntilIdle();
  signal_strategy_.Disconnect();
  task_environment_.RunUntilIdle();
  signal_strategy_.SetLocalAddress(SignalingAddress("local_jid2"));
  signal_strategy_.Connect();
  task_environment_.RunUntilIdle();

  // Let the first status update finish.  This should be a no-op in
  // the updater because the local JID has changed since this request
  // was issued.
  auto* request = GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, std::string());

  EXPECT_EQ(0, on_success_count_);

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ("{\"patches\":[{\"patch\":{"
            "\"base\":{\"_hostVersion\":\"" STRINGIZE(VERSION) "\","
            "\"_jabberId\":\"local_jid2\"}},"
            "\"timeMs\":0.0}],\"requestTimeMs\":0.0}",
            network::GetUploadData(request));
        test_url_loader_factory_.AddResponse(request.url.spec(), std::string(),
                                             net::HTTP_OK);
        task_environment_.RunUntilIdle();
      }));

  // Wait for the next retry.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // There should be a new pending request now with the new local JID.
  // It will be caught and handled by the interceptor installed above.
  EXPECT_EQ(1, on_success_count_);

  updater.reset();

  EXPECT_EQ(0, on_host_id_error_count_);
}

TEST_F(GcdStateUpdaterTest, Retry) {
  std::unique_ptr<GcdStateUpdater> updater(new GcdStateUpdater(
      base::Bind(&GcdStateUpdaterTest::OnSuccess, base::Unretained(this)),
      base::Bind(&GcdStateUpdaterTest::OnHostIdError, base::Unretained(this)),
      &signal_strategy_, std::move(rest_client_)));

  signal_strategy_.Connect();
  task_environment_.RunUntilIdle();

  auto* request = GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, network::mojom::URLResponseHead::New(), std::string(),
      network::URLLoaderCompletionStatus(net::ERR_FAILED));

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  request = GetPendingRequest(1);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, network::CreateURLResponseHead(net::HTTP_OK), std::string(),
      network::URLLoaderCompletionStatus());
  EXPECT_EQ(1, on_success_count_);

  updater.reset();

  EXPECT_EQ(0, on_host_id_error_count_);
}

TEST_F(GcdStateUpdaterTest, UnknownHost) {
  std::unique_ptr<GcdStateUpdater> updater(new GcdStateUpdater(
      base::Bind(&GcdStateUpdaterTest::OnSuccess, base::Unretained(this)),
      base::Bind(&GcdStateUpdaterTest::OnHostIdError, base::Unretained(this)),
      &signal_strategy_, std::move(rest_client_)));

  signal_strategy_.Connect();
  task_environment_.RunUntilIdle();

  auto* request = GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, network::CreateURLResponseHead(net::HTTP_NOT_FOUND),
      std::string(), network::URLLoaderCompletionStatus(net::OK));
  EXPECT_EQ(0, on_success_count_);
  EXPECT_EQ(1, on_host_id_error_count_);
}

}  // namespace remoting

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/connect_job_test_util.h"

#include <utility>

#include "base/check.h"
#include "base/run_loop.h"
#include "net/socket/stream_socket.h"
#include "net/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TestConnectJobDelegate::TestConnectJobDelegate(SocketExpected socket_expected)
    : socket_expected_(socket_expected) {}

TestConnectJobDelegate::~TestConnectJobDelegate() = default;

void TestConnectJobDelegate::OnConnectJobComplete(int result, ConnectJob* job) {
  EXPECT_FALSE(has_result_);
  result_ = result;
  socket_ = job->PassSocket();
  EXPECT_EQ(socket_.get() != nullptr,
            result == OK || socket_expected_ == SocketExpected::ALWAYS);
  // On success, generally end up with a connected socket. Could theoretically
  // be racily disconnected before it was returned, but that case isn't tested
  // with this class.
  if (result == OK)
    EXPECT_TRUE(socket_->IsConnected());
  has_result_ = true;
  run_loop_.Quit();
}

void TestConnectJobDelegate::OnNeedsProxyAuth(
    const HttpResponseInfo& response,
    HttpAuthController* auth_controller,
    base::OnceClosure restart_with_auth_callback,
    ConnectJob* job) {
  EXPECT_TRUE(auth_controller);
  EXPECT_TRUE(restart_with_auth_callback);

  EXPECT_FALSE(has_result_);
  EXPECT_FALSE(auth_controller_);
  EXPECT_FALSE(restart_with_auth_callback_);

  num_auth_challenges_++;
  auth_response_info_ = response;
  auth_controller_ = auth_controller;
  restart_with_auth_callback_ = std::move(restart_with_auth_callback);
  if (auth_challenge_run_loop_)
    auth_challenge_run_loop_->Quit();
}

void TestConnectJobDelegate::WaitForAuthChallenge(
    int num_auth_challenges_to_wait_for) {
  // It a bit strange to call this after a job has already complete, and doing
  // so probably indicates a bug.
  EXPECT_FALSE(has_result_);

  while (num_auth_challenges_ < num_auth_challenges_to_wait_for) {
    auth_challenge_run_loop_ = std::make_unique<base::RunLoop>();
    auth_challenge_run_loop_->Run();
    auth_challenge_run_loop_.reset();
  }
  EXPECT_EQ(num_auth_challenges_to_wait_for, num_auth_challenges_);
}

void TestConnectJobDelegate::RunAuthCallback() {
  ASSERT_TRUE(restart_with_auth_callback_);
  auth_controller_ = nullptr;
  std::move(restart_with_auth_callback_).Run();
}

int TestConnectJobDelegate::WaitForResult() {
  run_loop_.Run();
  DCHECK(has_result_);
  return result_;
}

void TestConnectJobDelegate::StartJobExpectingResult(ConnectJob* connect_job,
                                                     net::Error expected_result,
                                                     bool expect_sync_result) {
  int rv = connect_job->Connect();
  if (rv == ERR_IO_PENDING) {
    EXPECT_FALSE(expect_sync_result);
    EXPECT_THAT(WaitForResult(), test::IsError(expected_result));
  } else {
    EXPECT_TRUE(expect_sync_result);
    // The callback should not have been invoked.
    ASSERT_FALSE(has_result_);
    OnConnectJobComplete(rv, connect_job);
    EXPECT_THAT(result_, test::IsError(expected_result));
  }
}

std::unique_ptr<StreamSocket> TestConnectJobDelegate::ReleaseSocket() {
  return std::move(socket_);
}

}  // namespace net

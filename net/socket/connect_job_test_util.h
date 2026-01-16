// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CONNECT_JOB_TEST_UTIL_H_
#define NET_SOCKET_CONNECT_JOB_TEST_UTIL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_response_info.h"
#include "net/socket/connect_job.h"

namespace net {

class StreamSocket;

class TestConnectJobDelegate : public ConnectJob::Delegate {
 public:
  // Whether a socket should be returned. In most cases, no socket is returned
  // on failure; however, on certain SSL errors, a socket is returned in the
  // case of error.
  enum class SocketExpected {
    ON_SUCCESS_ONLY,
    ALWAYS,
  };

  explicit TestConnectJobDelegate(
      SocketExpected socket_expected = SocketExpected::ON_SUCCESS_ONLY);

  TestConnectJobDelegate(const TestConnectJobDelegate&) = delete;
  TestConnectJobDelegate& operator=(const TestConnectJobDelegate&) = delete;

  ~TestConnectJobDelegate() override;

  // ConnectJob::Delegate implementation.
  void OnConnectJobComplete(int result, ConnectJob* job) override;
  void OnNeedsProxyAuth(const HttpResponseInfo& response,
                        HttpAuthController* auth_controller,
                        base::OnceClosure restart_with_auth_callback,
                        ConnectJob* job) override;

  // Waits for the specified number of total auth challenges to be seen. Number
  // includes auth challenges that have already been waited for. Fails the test
  // if more auth challenges are seen than expected.
  void WaitForAuthChallenge(int num_auth_challenges_to_wait_for);

  void RunAuthCallback();

  // Waits for the ConnectJob to complete if it hasn't already and returns the
  // resulting network error code.
  int WaitForResult();

  int num_auth_challenges() const { return num_auth_challenges_; }
  const HttpResponseInfo& auth_response_info() const {
    return auth_response_info_;
  }
  scoped_refptr<HttpAuthController> auth_controller() {
    return auth_controller_.get();
  }

  // Returns true if the ConnectJob has a result.
  bool has_result() const { return has_result_; }

  void StartJobExpectingResult(ConnectJob* connect_job,
                               net::Error expected_result,
                               bool expect_sync_result);

  StreamSocket* socket() { return socket_.get(); }

  std::unique_ptr<StreamSocket> ReleaseSocket();

 private:
  const SocketExpected socket_expected_;
  bool has_result_ = false;
  int result_ = ERR_IO_PENDING;
  std::unique_ptr<StreamSocket> socket_;

  // These values are all updated each time a proxy auth challenge is seen.
  int num_auth_challenges_ = 0;
  HttpResponseInfo auth_response_info_;
  scoped_refptr<HttpAuthController> auth_controller_;
  base::OnceClosure restart_with_auth_callback_;

  base::RunLoop run_loop_;
  std::unique_ptr<base::RunLoop> auth_challenge_run_loop_;
};

}  // namespace net

#endif  // NET_SOCKET_CONNECT_JOB_TEST_UTIL_H_

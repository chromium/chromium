// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_CONTROLLABLE_HTTP_RESPONSE_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_CONTROLLABLE_HTTP_RESPONSE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace net {

namespace test_server {

// A response that can be manually controlled on the current test thread. It is
// used for waiting for a connection, sending data and closing it. It will
// handle only **one** request with the matching |relative_url|. In the case of
// multiple ControllableHttpResponses for the same path, they're used in the
// order they were created.
//
// If |relative_url_is_prefix| is true, |relative_url| is only compared agaisnt
// the start of the URL being requested, which allows matching against (possibly
// variable) query strings, for instance.
class ControllableHttpResponse {
 public:
  ControllableHttpResponse(EmbeddedTestServer* embedded_test_server,
                           const std::string& relative_url,
                           bool relative_url_is_prefix = false);
  ~ControllableHttpResponse();

  // These method are intented to be used in order.
  // 1) Wait for the response to be requested.
  void WaitForRequest();

  // 2) Send raw response data in response to a request.
  //    May be called several time.
  void Send(const std::string& bytes);

  // Same as 2) but with more specific parameters.
  void Send(net::HttpStatusCode http_status,
            const std::string& content_type = std::string("text/html"),
            const std::string& content = std::string(),
            const std::vector<std::string>& cookies = {});

  // 3) Notify there are no more data to be sent and close the socket.
  void Done();

  // Returns the HttpRequest after a call to WaitForRequest.
  const HttpRequest* http_request() const { return http_request_.get(); }

 private:
  class Interceptor;

  enum class State { WAITING_FOR_REQUEST, READY_TO_SEND_DATA, DONE };

  void OnRequest(scoped_refptr<base::SingleThreadTaskRunner>
                     embedded_test_server_task_runner,
                 const SendBytesCallback& send,
                 const SendCompleteCallback& done,
                 std::unique_ptr<HttpRequest> http_request);

  static std::unique_ptr<HttpResponse> RequestHandler(
      base::WeakPtr<ControllableHttpResponse> controller,
      scoped_refptr<base::SingleThreadTaskRunner> controller_task_runner,
      bool* available,
      const std::string& relative_url,
      bool relative_url_is_prefix,
      const HttpRequest& request);

  State state_ = State::WAITING_FOR_REQUEST;
  base::RunLoop loop_;
  scoped_refptr<base::SingleThreadTaskRunner> embedded_test_server_task_runner_;
  SendBytesCallback send_;
  SendCompleteCallback done_;
  std::unique_ptr<HttpRequest> http_request_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ControllableHttpResponse> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ControllableHttpResponse);
};

}  // namespace test_server

}  // namespace net

#endif  //  NET_TEST_EMBEDDED_TEST_SERVER_CONTROLLABLE_HTTP_RESPONSE_H_

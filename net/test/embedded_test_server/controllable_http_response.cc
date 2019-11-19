// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/controllable_http_response.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"

namespace net {

namespace test_server {

class ControllableHttpResponse::Interceptor : public HttpResponse {
 public:
  explicit Interceptor(
      base::WeakPtr<ControllableHttpResponse> controller,
      scoped_refptr<base::SingleThreadTaskRunner> controller_task_runner,
      const HttpRequest& http_request)
      : controller_(controller),
        controller_task_runner_(controller_task_runner),
        http_request_(std::make_unique<HttpRequest>(http_request)) {}
  ~Interceptor() override {}

 private:
  void SendResponse(const SendBytesCallback& send,
                    const SendCompleteCallback& done) override {
    controller_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ControllableHttpResponse::OnRequest, controller_,
                       base::ThreadTaskRunnerHandle::Get(), send, done,
                       std::move(http_request_)));
  }

  base::WeakPtr<ControllableHttpResponse> controller_;
  scoped_refptr<base::SingleThreadTaskRunner> controller_task_runner_;

  std::unique_ptr<HttpRequest> http_request_;

  DISALLOW_COPY_AND_ASSIGN(Interceptor);
};

ControllableHttpResponse::ControllableHttpResponse(
    EmbeddedTestServer* embedded_test_server,
    const std::string& relative_url,
    bool relative_url_is_prefix) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  embedded_test_server->RegisterRequestHandler(base::BindRepeating(
      RequestHandler, weak_ptr_factory_.GetWeakPtr(),
      base::ThreadTaskRunnerHandle::Get(), base::Owned(new bool(true)),
      relative_url, relative_url_is_prefix));
}

ControllableHttpResponse::~ControllableHttpResponse() {}

void ControllableHttpResponse::WaitForRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::WAITING_FOR_REQUEST, state_)
      << "WaitForRequest() called twice.";
  loop_.Run();
  DCHECK(embedded_test_server_task_runner_);
  DCHECK(send_);
  DCHECK(done_);
  state_ = State::READY_TO_SEND_DATA;
}

void ControllableHttpResponse::Send(net::HttpStatusCode http_status,
                                    const std::string& content_type,
                                    const std::string& content,
                                    const std::vector<std::string>& cookies) {
  std::string content_data(base::StringPrintf(
      "HTTP/1.1 %d %s\nContent-type: %s\n", static_cast<int>(http_status),
      net::GetHttpReasonPhrase(http_status), content_type.c_str()));
  for (auto& cookie : cookies)
    content_data += "Set-Cookie: " + cookie + "\n";
  content_data += "\n";
  content_data += content;
  Send(content_data);
}

void ControllableHttpResponse::Send(const std::string& bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::READY_TO_SEND_DATA, state_) << "Send() called without any "
                                                  "opened connection. Did you "
                                                  "call WaitForRequest()?";
  base::RunLoop loop;
  embedded_test_server_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(send_, bytes, loop.QuitClosure()));
  loop.Run();
}

void ControllableHttpResponse::Done() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::READY_TO_SEND_DATA, state_) << "Done() called without any "
                                                  "opened connection. Did you "
                                                  "call WaitForRequest()?";
  embedded_test_server_task_runner_->PostTask(FROM_HERE, done_);
  state_ = State::DONE;
}

void ControllableHttpResponse::OnRequest(
    scoped_refptr<base::SingleThreadTaskRunner>
        embedded_test_server_task_runner,
    const SendBytesCallback& send,
    const SendCompleteCallback& done,
    std::unique_ptr<HttpRequest> http_request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!embedded_test_server_task_runner_)
      << "A ControllableHttpResponse can only handle one request at a time";
  embedded_test_server_task_runner_ = embedded_test_server_task_runner;
  send_ = send;
  done_ = done;
  http_request_ = std::move(http_request);
  loop_.Quit();
}

// Helper function used in the ControllableHttpResponse constructor.
// static
std::unique_ptr<HttpResponse> ControllableHttpResponse::RequestHandler(
    base::WeakPtr<ControllableHttpResponse> controller,
    scoped_refptr<base::SingleThreadTaskRunner> controller_task_runner,
    bool* available,
    const std::string& relative_url,
    bool relative_url_is_prefix,
    const HttpRequest& request) {
  if (!*available)
    return nullptr;

  if (request.relative_url == relative_url ||
      (relative_url_is_prefix &&
       base::StartsWith(request.relative_url, relative_url,
                        base::CompareCase::SENSITIVE))) {
    *available = false;
    return std::make_unique<ControllableHttpResponse::Interceptor>(
        controller, controller_task_runner, request);
  }

  return nullptr;
}

}  // namespace test_server

}  // namespace net

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/slow_http_response.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_post_task.h"
#include "base/callback_helpers.h"
#include "base/test/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace content {

const char SlowHttpResponse::kSlowResponseUrl[] = "/slow-response";
const char SlowHttpResponse::kFinishSlowResponseUrl[] = "/slow-response-finish";

const int SlowHttpResponse::kFirstResponsePartSize = 1024 * 35;
const int SlowHttpResponse::kSecondResponsePartSize = 1024 * 10;

// static
SlowHttpResponse::GotRequestCallback
SlowHttpResponse::FinishResponseImmediately() {
  return base::BindOnce(
      [](base::OnceClosure start_response, base::OnceClosure finish_response) {
        std::move(start_response).Run();
        std::move(finish_response).Run();
      });
}

// static
SlowHttpResponse::GotRequestCallback SlowHttpResponse::NoResponse() {
  return base::DoNothing();
}

SlowHttpResponse::SlowHttpResponse(GotRequestCallback got_request)
    : main_thread_(base::ThreadTaskRunnerHandle::Get()),
      got_request_(std::move(got_request)) {}

SlowHttpResponse::~SlowHttpResponse() = default;

bool SlowHttpResponse::IsHandledUrl() {
  //  return url_ == kSlowResponseUrl || url_ == kFinishSlowResponseUrl;
  return false;
}

void SlowHttpResponse::AddResponseHeaders(std::string* response) {
  response->append("Content-type: text/html\r\n");
}

void SlowHttpResponse::SetStatusLine(std::string* response) {
  response->append("HTTP/1.1 200 OK\r\n");
}

void SlowHttpResponse::SendResponse(
    const net::test_server::SendBytesCallback& send,
    net::test_server::SendCompleteCallback done) {
  // Construct the headers here so subclasses can override them. Then we will
  // bind them into the async task which sends them in the response.
  std::string header_response;
  SetStatusLine(&header_response);
  AddResponseHeaders(&header_response);
  header_response.append("Cache-Control: no-store\r\n");
  header_response.append("\r\n");

  // SendResponse() runs off the test's main thread so we must have these tasks
  // post back from the test's main thread to this thread.
  auto send_headers = base::BindPostTask(
      base::ThreadTaskRunnerHandle::Get(),
      base::BindOnce(
          [](const std::string& header_response,
             const net::test_server::SendBytesCallback& send) {
            send.Run(header_response, base::DoNothing());
          },
          header_response, send));
  auto send_first_part = base::BindPostTask(
      base::ThreadTaskRunnerHandle::Get(),
      base::BindOnce(
          [](const net::test_server::SendBytesCallback& send) {
            std::string response(kFirstResponsePartSize, '*');
            send.Run(response, base::DoNothing());
          },
          send));
  auto send_second_part = base::BindPostTask(
      base::ThreadTaskRunnerHandle::Get(),
      base::BindOnce(
          [](const net::test_server::SendBytesCallback& send,
             net::test_server::SendCompleteCallback done) {
            std::string response(kSecondResponsePartSize, '*');
            send.Run(response, std::move(done));
          },
          send, std::move(done)));

  // We run both `send_headers` and `send_first_part` when the test asks
  // us to start the response, but as separate tasks.
  base::OnceClosure start_response =
      std::move(send_headers).Then(std::move(send_first_part));
  base::OnceClosure finish_response = std::move(send_second_part);
  // SendResponse() runs off the test's main thread so we must post task the
  // test's `got_request_` callback to the main thread.
  main_thread_->PostTask(FROM_HERE, base::BindOnce(std::move(got_request_),
                                                   std::move(start_response),
                                                   std::move(finish_response)));
}

}  // namespace content

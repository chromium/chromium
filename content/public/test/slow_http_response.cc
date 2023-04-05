// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/slow_http_response.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_split.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_response.h"

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
    : main_thread_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      got_request_(std::move(got_request)) {}

SlowHttpResponse::~SlowHttpResponse() = default;

base::StringPairs SlowHttpResponse::ResponseHeaders() {
  return {{"Content-type", "text/html"}};
}

std::pair<net::HttpStatusCode, std::string> SlowHttpResponse::StatusLine() {
  return {net::HTTP_OK, "OK"};
}

void SlowHttpResponse::SendResponse(
    base::WeakPtr<HttpResponseDelegate> delegate) {
  // Construct the headers here so subclasses can override them. Then we will
  // bind them into the async task which sends them in the response.
  auto [status, status_reason] = StatusLine();

  base::StringPairs headers = ResponseHeaders();
  headers.emplace_back("Cache-Control", "no-store");

  // SendResponse() runs off the test's main thread so we must have these tasks
  // post back from the test's main thread to this thread.
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  auto send_headers = base::BindPostTask(
      task_runner, base::BindOnce(&HttpResponseDelegate::SendResponseHeaders,
                                  delegate, status, status_reason, headers));

  auto send_first_part = base::BindPostTask(
      task_runner, base::BindOnce(&HttpResponseDelegate::SendContents, delegate,
                                  std::string(kFirstResponsePartSize, '*'),
                                  base::DoNothing()));
  auto send_second_part = base::BindPostTask(
      task_runner,
      base::BindOnce(&HttpResponseDelegate::SendContentsAndFinish, delegate,
                     std::string(kSecondResponsePartSize, '*')));

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

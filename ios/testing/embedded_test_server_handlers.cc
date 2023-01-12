// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/testing/embedded_test_server_handlers.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace testing {

const char kTestFormPage[] = "ios.testing.HandleForm";
const char kTestFormFieldValue[] = "test-value";
const char kTestDownloadMimeType[] = "application/vnd.test";

namespace {
// Extracts and escapes url spec from the query.
std::string ExtractUlrSpecFromQuery(
    const net::test_server::HttpRequest& request) {
  GURL request_url = request.GetURL();
  std::string spec =
      base::UnescapeBinaryURLComponent(request_url.query_piece());

  // Escape the URL spec.
  GURL url(spec);
  return url.is_valid() ? base::EscapeForHTML(url.spec()) : spec;
}

// A HttpResponse that responds with |length| zeroes and kTestDownloadMimeType
// MIME Type.
class DownloadResponse : public net::test_server::BasicHttpResponse {
 public:
  DownloadResponse(int length) : length_(length) {}

  DownloadResponse(const DownloadResponse&) = delete;
  DownloadResponse& operator=(const DownloadResponse&) = delete;

  void SendResponse(
      base::WeakPtr<net::test_server::HttpResponseDelegate> delegate) override {
    base::StringPairs headers = {
        {"content-type", kTestDownloadMimeType},
        {"content-length", base::StringPrintf("%d", length_)}};

    delegate->SendResponseHeaders(net::HTTP_OK, "OK", headers);
    Send(delegate, length_);
  }

 private:
  // Sends "0" |count| times using 1KB blocks. Using blocks with smaller size is
  // performance inefficient and can cause unnecessary delays especially when
  // multiple tests run in parallel on a single machine.
  static void Send(
      base::WeakPtr<net::test_server::HttpResponseDelegate> delegate,
      int count) {
    if (!count) {
      if (delegate)
        delegate->FinishResponse();
      return;
    }
    int block_size = std::min(count, 1000);
    std::string content_block(block_size, 0);
    auto next_send =
        base::BindOnce(&DownloadResponse::Send, delegate, count - block_size);

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&net::test_server::HttpResponseDelegate::SendContents,
                       delegate, content_block, std::move(next_send)),
        base::Milliseconds(100));
  }

  int length_ = 0;
};

// A slow response that would take several hours to finish. This is useful for
// testing scenarios where a load is interrupted after it starts but before it
// finishes.
class SlowResponse : public net::test_server::BasicHttpResponse {
 public:
  SlowResponse() = default;

  SlowResponse(const SlowResponse&) = delete;
  SlowResponse& operator=(const SlowResponse&) = delete;

  void SendResponse(
      base::WeakPtr<net::test_server::HttpResponseDelegate> delegate) override {
    delegate_ = delegate;
    delegate_->SendResponseHeaders(
        net::HTTP_OK, "OK",
        {{"Content-Length",
          base::NumberToString(kilobytes_left_to_send_ * 1024)},
         {"Content-Type", "text/plain"}});
    SendKilobyte();
  }

 private:
  void SendKilobyte() {
    if (kilobytes_left_to_send_ == 0) {
      delegate_->FinishResponse();
      return;
    }

    DCHECK_GT(kilobytes_left_to_send_, 0);
    kilobytes_left_to_send_--;

    delegate_->SendContents(
        std::string(1024, 'a'),
        base::BindOnce(&SlowResponse::SendKilobyteAfterDelay,
                       weak_factory_.GetWeakPtr()));
  }

  void SendKilobyteAfterDelay() {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SlowResponse::SendKilobyte, weak_factory_.GetWeakPtr()),
        delay_between_kilobytes_);
  }

  base::WeakPtrFactory<SlowResponse> weak_factory_{this};
  base::WeakPtr<net::test_server::HttpResponseDelegate> delegate_ = nullptr;
  int kilobytes_left_to_send_ = 100000;
  base::TimeDelta delay_between_kilobytes_ = base::Seconds(1);
};

}  // namespace

std::unique_ptr<net::test_server::HttpResponse> HandleIFrame(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_content_type("text/html");
  http_response->set_content(
      base::StringPrintf("<html><head></head><body><iframe "
                         "src='%s'></iframe>Main frame text</body></html>",
                         ExtractUlrSpecFromQuery(request).c_str()));
  return std::move(http_response);
}

// Returns a page with |html|.
std::unique_ptr<net::test_server::HttpResponse> HandlePageWithHtml(
    const std::string& html,
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_content_type("text/html");
  http_response->set_content(html);
  return std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse> HandlePageWithContents(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_content_type("text/html");
  http_response->set_content(request.GetURL().query());
  return std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse> HandleEchoQueryOrCloseSocket(
    const bool& responds_with_content,
    const net::test_server::HttpRequest& request) {
  if (!responds_with_content) {
    return std::make_unique<net::test_server::RawHttpResponse>(
        /*headers=*/"", /*contents=*/"");
  }
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/html");
  response->set_content(request.GetURL().query());
  response->AddCustomHeader("Cache-Control", "no-store");
  return std::move(response);
}

std::unique_ptr<net::test_server::HttpResponse> HandleForm(
    const net::test_server::HttpRequest& request) {
  std::string form_action = ExtractUlrSpecFromQuery(request);
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/html");
  response->set_content(base::StringPrintf(
      "<form method='post' id='form' action='%s'>"
      "  <input type='text' name='test-name' value='%s'>"
      "</form>"
      "%s",
      form_action.c_str(), kTestFormFieldValue, kTestFormPage));

  return std::move(response);
}

std::unique_ptr<net::test_server::HttpResponse> HandleDownload(
    const net::test_server::HttpRequest& request) {
  int length = 0;
  if (!base::StringToInt(request.GetURL().query(), &length)) {
    length = 1;
  }

  return std::make_unique<DownloadResponse>(length);
}

std::unique_ptr<net::test_server::HttpResponse> HandleSlow(
    const net::test_server::HttpRequest& request) {
  return std::make_unique<SlowResponse>();
}

}  // namespace testing

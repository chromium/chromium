// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/testing/embedded_test_server_handlers.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/escape.h"
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
  std::string spec = net::UnescapeBinaryURLComponent(request_url.query_piece());

  // Escape the URL spec.
  GURL url(spec);
  return url.is_valid() ? net::EscapeForHTML(url.spec()) : spec;
}

// A HttpResponse that responds with |length| zeroes and kTestDownloadMimeType
// MIME Type.
class DownloadResponse : public net::test_server::BasicHttpResponse {
 public:
  DownloadResponse(int length) : length_(length) {}

  void SendResponse(
      const net::test_server::SendBytesCallback& send,
      const net::test_server::SendCompleteCallback& done) override {
    send.Run(base::StringPrintf("HTTP/1.1 200 OK\r\n"
                                "Content-Type:%s\r\n\r\n"
                                "Content-Length:%d\r\n\r\n",
                                kTestDownloadMimeType, length_),
             base::BindRepeating(&DownloadResponse::Send, send, done, length_));
  }

 private:
  // Sends "0" |count| times.
  static void Send(const net::test_server::SendBytesCallback& send,
                   const net::test_server::SendCompleteCallback& done,
                   int count) {
    if (!count) {
      done.Run();
      return;
    }

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindRepeating(send, "0",
                            base::BindRepeating(&DownloadResponse::Send, send,
                                                done, count - 1)));
  }

  int length_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DownloadResponse);
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

}  // namespace testing

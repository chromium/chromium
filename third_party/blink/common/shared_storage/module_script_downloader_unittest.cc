// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/shared_storage/module_script_downloader.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace blink {

namespace {

const char kAsciiResponseBody[] = "ASCII response body.";
const char kUtf8ResponseBody[] = "\xc3\x9f\xc3\x9e";
const char kNonUtf8ResponseBody[] = "\xc3";

const char kAsciiCharset[] = "us-ascii";
const char kUtf8Charset[] = "utf-8";

const char kJavascriptMimeType[] = "application/javascript";
const char kJsonMimeType[] = "application/json";

void AddResponse(network::TestURLLoaderFactory* url_loader_factory,
                 const GURL& url,
                 std::optional<std::string> mime_type,
                 std::optional<std::string> charset,
                 const std::string content,
                 net::HttpStatusCode http_status = net::HTTP_OK,
                 network::TestURLLoaderFactory::Redirects redirects =
                     network::TestURLLoaderFactory::Redirects()) {
  auto head = network::mojom::URLResponseHead::New();
  // Don't bother adding these as headers, since the script grabs headers from
  // URLResponseHead fields instead of the corresponding
  // net::HttpResponseHeaders fields.
  if (mime_type) {
    head->mime_type = *mime_type;
  }
  if (charset) {
    head->charset = *charset;
  }
  if (http_status != net::HTTP_OK) {
    std::string full_headers = base::StringPrintf(
        "HTTP/1.1 %d %s\r\n\r\n", static_cast<int>(http_status),
        net::GetHttpReasonPhrase(http_status));
    head->headers = net::HttpResponseHeaders::TryToCreate(full_headers);
    CHECK(head->headers);
  }
  url_loader_factory->AddResponse(url, std::move(head), content,
                                  network::URLLoaderCompletionStatus(),
                                  std::move(redirects));
}

}  // namespace

class ModuleScriptDownloaderTest : public testing::Test {
 public:
  ModuleScriptDownloaderTest() = default;
  ~ModuleScriptDownloaderTest() override = default;

  std::unique_ptr<std::string> RunRequest() {
    DCHECK(!run_loop_);

    ModuleScriptDownloader downloader(
        &url_loader_factory_, url_,
        base::BindOnce(&ModuleScriptDownloaderTest::DownloadCompleteCallback,
                       base::Unretained(this)));

    // Populate `run_loop_` after starting the download, since API guarantees
    // callback will not be invoked synchronously.
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
    return std::move(body_);
  }

 protected:
  void DownloadCompleteCallback(
      std::unique_ptr<std::string> body,
      std::string error,
      network::mojom::URLResponseHeadPtr response_head) {
    DCHECK(!body_);
    DCHECK(run_loop_);
    body_ = std::move(body);
    error_ = std::move(error);
    response_head_ = std::move(response_head);
    EXPECT_EQ(error_.empty(), !!body_);
    run_loop_->Quit();
  }

  base::test::TaskEnvironment task_environment_;

  const GURL url_ = GURL("https://url.test/script.js");

  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<std::string> body_;
  std::string error_;
  network::mojom::URLResponseHeadPtr response_head_;

  network::TestURLLoaderFactory url_loader_factory_;
};

TEST_F(ModuleScriptDownloaderTest, NetworkError) {
  network::URLLoaderCompletionStatus status;
  status.error_code = net::ERR_FAILED;
  url_loader_factory_.AddResponse(url_, /*head=*/nullptr, kAsciiResponseBody,
                                  status);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Failed to load https://url.test/script.js error = net::ERR_FAILED.",
      error_);
  EXPECT_FALSE(response_head_);
}

// HTTP 404 responses are treated as failures.
TEST_F(ModuleScriptDownloaderTest, HttpError) {
  // This is an unlikely response for an error case, but should fail if it ever
  // happens.
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody, net::HTTP_NOT_FOUND);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Failed to load https://url.test/script.js HTTP status = 404 Not Found.",
      error_);
  EXPECT_TRUE(response_head_);
  EXPECT_EQ(response_head_->mime_type, kJavascriptMimeType);
}

// Redirect responses are treated as failures.
TEST_F(ModuleScriptDownloaderTest, Redirect) {
  // None of these fields actually matter for this test, but a bit strange for
  // them not to be populated.
  net::RedirectInfo redirect_info;
  redirect_info.status_code = net::HTTP_MOVED_PERMANENTLY;
  redirect_info.new_url = url_;
  redirect_info.new_method = "GET";
  network::TestURLLoaderFactory::Redirects redirects;
  redirects.push_back(
      std::make_pair(redirect_info, network::mojom::URLResponseHead::New()));

  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody, net::HTTP_OK, std::move(redirects));
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ("Unexpected redirect on https://url.test/script.js.", error_);
  EXPECT_FALSE(response_head_);
}

TEST_F(ModuleScriptDownloaderTest, Success) {
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody);
  std::unique_ptr<std::string> body = RunRequest();
  ASSERT_TRUE(body);
  EXPECT_EQ(kAsciiResponseBody, *body);
}

// Test unexpected response mime type.
TEST_F(ModuleScriptDownloaderTest, UnexpectedMimeType) {
  // Javascript request, JSON response type.
  AddResponse(&url_loader_factory_, url_, kJsonMimeType, kUtf8Charset,
              kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected MIME "
      "type.",
      error_);

  // Javascript request, no response type.
  AddResponse(&url_loader_factory_, url_, std::nullopt, kUtf8Charset,
              kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected MIME "
      "type.",
      error_);

  // Javascript request, empty response type.
  AddResponse(&url_loader_factory_, url_, "", kUtf8Charset, kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected MIME "
      "type.",
      error_);

  // Javascript request, unknown response type.
  AddResponse(&url_loader_factory_, url_, "blobfish", kUtf8Charset,
              kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected MIME "
      "type.",
      error_);
}

// Test all Javscript type strings.
TEST_F(ModuleScriptDownloaderTest, JavscriptMimeTypeVariants) {
  // All supported Javscript MIME types, copied from blink's mime_util.cc.
  // TODO(yaoxia): find a way to keep the list in sync with the original list.

  const char* kJavascriptMimeTypes[] = {
      "application/ecmascript",
      "application/javascript",
      "application/x-ecmascript",
      "application/x-javascript",
      "text/ecmascript",
      "text/javascript",
      "text/javascript1.0",
      "text/javascript1.1",
      "text/javascript1.2",
      "text/javascript1.3",
      "text/javascript1.4",
      "text/javascript1.5",
      "text/jscript",
      "text/livescript",
      "text/x-ecmascript",
      "text/x-javascript",
  };

  for (const char* javascript_type : kJavascriptMimeTypes) {
    AddResponse(&url_loader_factory_, url_, javascript_type, kUtf8Charset,
                kAsciiResponseBody);
    std::unique_ptr<std::string> body = RunRequest();
    ASSERT_TRUE(body);
    EXPECT_EQ(kAsciiResponseBody, *body);
  }
}

TEST_F(ModuleScriptDownloaderTest, Charset) {
  // ASCII charset should restrict response bodies to ASCII characters.
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kAsciiCharset,
              kAsciiResponseBody);
  std::unique_ptr<std::string> body = RunRequest();
  ASSERT_TRUE(body);
  EXPECT_EQ(kAsciiResponseBody, *body);
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kAsciiCharset,
              kUtf8ResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected charset.",
      error_);
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kAsciiCharset,
              kNonUtf8ResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected charset.",
      error_);

  // UTF-8 charset should restrict response bodies to valid UTF-8 characters.
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody);
  body = RunRequest();
  ASSERT_TRUE(body);
  EXPECT_EQ(kAsciiResponseBody, *body);
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kUtf8ResponseBody);
  body = RunRequest();
  ASSERT_TRUE(body);
  EXPECT_EQ(kUtf8ResponseBody, *body);
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kNonUtf8ResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected charset.",
      error_);

  // Null charset should act like UTF-8.
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, std::nullopt,
              kAsciiResponseBody);
  body = RunRequest();
  ASSERT_TRUE(body);
  EXPECT_EQ(kAsciiResponseBody, *body);
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, std::nullopt,
              kUtf8ResponseBody);
  body = RunRequest();
  ASSERT_TRUE(body);
  EXPECT_EQ(kUtf8ResponseBody, *body);
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, std::nullopt,
              kNonUtf8ResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected charset.",
      error_);

  // Empty charset should act like UTF-8.
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, "",
              kAsciiResponseBody);
  body = RunRequest();
  ASSERT_TRUE(body);
  EXPECT_EQ(kAsciiResponseBody, *body);
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, "",
              kUtf8ResponseBody);
  body = RunRequest();
  ASSERT_TRUE(body);
  EXPECT_EQ(kUtf8ResponseBody, *body);
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, "",
              kNonUtf8ResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected charset.",
      error_);
}

}  // namespace blink

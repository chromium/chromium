// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_downloader.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace auction_worklet {
namespace {

const char kAsciiResponseBody[] = "ASCII response body.";
const char kUtf8ResponseBody[] = "\xc3\x9f\xc3\x9e";
const char kNonUtf8ResponseBody[] = "\xc3";

const char kAsciiCharset[] = "us-ascii";
const char kUtf8Charset[] = "utf-8";

class AuctionDownloaderTest : public testing::Test {
 public:
  AuctionDownloaderTest() = default;
  ~AuctionDownloaderTest() override = default;

  std::unique_ptr<std::string> RunRequest() {
    DCHECK(!run_loop_);

    AuctionDownloader downloader(
        &url_loader_factory_, url_, mime_type_,
        base::BindOnce(&AuctionDownloaderTest::DownloadCompleteCallback,
                       base::Unretained(this)));

    // Populate `run_loop_` after starting the download, since API guarantees
    // callback will not be invoked synchronously.
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
    return std::move(body_);
  }

 protected:
  void DownloadCompleteCallback(std::unique_ptr<std::string> body) {
    DCHECK(!body_);
    DCHECK(run_loop_);
    body_ = std::move(body);
    run_loop_->Quit();
  }

  base::test::TaskEnvironment task_environment_;

  const GURL url_ = GURL("https://url.test/");

  AuctionDownloader::MimeType mime_type_ =
      AuctionDownloader::MimeType::kJavascript;

  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<std::string> body_;

  network::TestURLLoaderFactory url_loader_factory_;
};

TEST_F(AuctionDownloaderTest, NetworkError) {
  network::URLLoaderCompletionStatus status;
  status.error_code = net::ERR_FAILED;
  url_loader_factory_.AddResponse(url_, nullptr /* head */, kAsciiResponseBody,
                                  status);
  EXPECT_FALSE(RunRequest());
}

// HTTP 404 responses are trested as failures.
TEST_F(AuctionDownloaderTest, HttpError) {
  // This is an unlikely response for an error case, but should fail if it ever
  // happens.
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody, kAllowFledgeHeader, net::HTTP_NOT_FOUND);
  EXPECT_FALSE(RunRequest());
}

TEST_F(AuctionDownloaderTest, AllowFledge) {
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody, "X-Allow-FLEDGE: true");
  EXPECT_TRUE(RunRequest());

  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody, "x-aLLow-fLeDgE: true");
  EXPECT_TRUE(RunRequest());

  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody, "X-Allow-FLEDGE: false");
  EXPECT_FALSE(RunRequest());

  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody, "X-Allow-FLEDGE: sometimes");
  EXPECT_FALSE(RunRequest());

  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody, "X-Allow-FLEDGE: ");
  EXPECT_FALSE(RunRequest());

  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody, "X-Allow-Hats: true");
  EXPECT_FALSE(RunRequest());

  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody, "");
  EXPECT_FALSE(RunRequest());

  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody, base::nullopt);
  EXPECT_FALSE(RunRequest());
}

// Redirect responses are treated as failures.
TEST_F(AuctionDownloaderTest, Redirect) {
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
              kAsciiResponseBody, kAllowFledgeHeader, net::HTTP_OK,
              std::move(redirects));
  EXPECT_FALSE(RunRequest());
}

TEST_F(AuctionDownloaderTest, Success) {
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody);
  std::unique_ptr<std::string> body = RunRequest();
  ASSERT_TRUE(body);
  EXPECT_EQ(kAsciiResponseBody, *body);
}

// Test `AuctionDownloader::MimeType` values work as expected.
TEST_F(AuctionDownloaderTest, MimeType) {
  // Javascript request, JSON response type.
  AddResponse(&url_loader_factory_, url_, kJsonMimeType, kUtf8Charset,
              kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());

  // Javascript request, no response type.
  AddResponse(&url_loader_factory_, url_, base::nullopt, kUtf8Charset,
              kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());

  // Javascript request, empty response type.
  AddResponse(&url_loader_factory_, url_, "", kUtf8Charset, kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());

  // Javascript request, unknown response type.
  AddResponse(&url_loader_factory_, url_, "blobfish", kUtf8Charset,
              kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());

  // JSON request, Javascript response type.
  mime_type_ = AuctionDownloader::MimeType::kJson;
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());

  // JSON request, no response type.
  AddResponse(&url_loader_factory_, url_, base::nullopt, kUtf8Charset,
              kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());

  // JSON request, empty response type.
  AddResponse(&url_loader_factory_, url_, "", kUtf8Charset, kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());

  // JSON request, unknown response type.
  AddResponse(&url_loader_factory_, url_, "blobfish", kUtf8Charset,
              kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());

  // JSON request, JSON response type.
  mime_type_ = AuctionDownloader::MimeType::kJson;
  AddResponse(&url_loader_factory_, url_, kJsonMimeType, kUtf8Charset,
              kAsciiResponseBody);
  std::unique_ptr<std::string> body = RunRequest();
  ASSERT_TRUE(body);
  EXPECT_EQ(kAsciiResponseBody, *body);
}

// Test all Javscript and JSON MIME type strings.
TEST_F(AuctionDownloaderTest, MimeTypeVariants) {
  // All supported Javscript MIME types, copied from blink's mime_util.cc.
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

  // Some MIME types (there are some wild cards in the matcher).
  const char* kJsonMimeTypes[] = {
      "application/json",      "text/json",
      "application/goat+json", "application/javascript+json",
      "application/+json",
  };

  for (const char* javascript_type : kJavascriptMimeTypes) {
    mime_type_ = AuctionDownloader::MimeType::kJavascript;
    AddResponse(&url_loader_factory_, url_, javascript_type, kUtf8Charset,
                kAsciiResponseBody);
    std::unique_ptr<std::string> body = RunRequest();
    ASSERT_TRUE(body);
    EXPECT_EQ(kAsciiResponseBody, *body);

    mime_type_ = AuctionDownloader::MimeType::kJson;
    AddResponse(&url_loader_factory_, url_, javascript_type, kUtf8Charset,
                kAsciiResponseBody);
    EXPECT_FALSE(RunRequest());
  }

  for (const char* json_type : kJsonMimeTypes) {
    mime_type_ = AuctionDownloader::MimeType::kJavascript;
    AddResponse(&url_loader_factory_, url_, json_type, kUtf8Charset,
                kAsciiResponseBody);
    EXPECT_FALSE(RunRequest());

    mime_type_ = AuctionDownloader::MimeType::kJson;
    AddResponse(&url_loader_factory_, url_, json_type, kUtf8Charset,
                kAsciiResponseBody);
    std::unique_ptr<std::string> body = RunRequest();
    ASSERT_TRUE(body);
    EXPECT_EQ(kAsciiResponseBody, *body);
  }
}

TEST_F(AuctionDownloaderTest, Charset) {
  // Unknown/unsupported charsets should result in failure.
  AddResponse(&url_loader_factory_, url_, kJsonMimeType, "fred",
              kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());
  AddResponse(&url_loader_factory_, url_, kJsonMimeType, "iso-8859-1",
              kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());

  // ASCII charset should restrict response bodies to ASCII characters.
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kAsciiCharset,
              kUtf8ResponseBody);
  EXPECT_FALSE(RunRequest());
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kAsciiCharset,
              kAsciiResponseBody);
  std::unique_ptr<std::string> body = RunRequest();
  ASSERT_TRUE(body);
  EXPECT_EQ(kAsciiResponseBody, *body);
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kAsciiCharset,
              kUtf8ResponseBody);
  EXPECT_FALSE(RunRequest());
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kAsciiCharset,
              kNonUtf8ResponseBody);
  EXPECT_FALSE(RunRequest());

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

  // Null charset should act like UTF-8.
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, base::nullopt,
              kAsciiResponseBody);
  body = RunRequest();
  ASSERT_TRUE(body);
  EXPECT_EQ(kAsciiResponseBody, *body);
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, base::nullopt,
              kUtf8ResponseBody);
  body = RunRequest();
  ASSERT_TRUE(body);
  EXPECT_EQ(kUtf8ResponseBody, *body);
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, base::nullopt,
              kNonUtf8ResponseBody);
  EXPECT_FALSE(RunRequest());

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
}

}  // namespace
}  // namespace auction_worklet

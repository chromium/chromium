// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_downloader.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

  // Helper to avoid checking has_value all over the place.
  std::string last_error_msg() const {
    return error_.value_or("Not an error.");
  }

 protected:
  void DownloadCompleteCallback(std::unique_ptr<std::string> body,
                                scoped_refptr<net::HttpResponseHeaders> headers,
                                absl::optional<std::string> error) {
    DCHECK(!body_);
    DCHECK(run_loop_);
    body_ = std::move(body);
    headers_ = std::move(headers);
    error_ = std::move(error);
    EXPECT_EQ(error_.has_value(), !body_);
    run_loop_->Quit();
  }

  base::test::TaskEnvironment task_environment_;

  const GURL url_ = GURL("https://url.test/script.js");

  AuctionDownloader::MimeType mime_type_ =
      AuctionDownloader::MimeType::kJavascript;

  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<std::string> body_;
  scoped_refptr<net::HttpResponseHeaders> headers_;
  absl::optional<std::string> error_;

  network::TestURLLoaderFactory url_loader_factory_;
};

TEST_F(AuctionDownloaderTest, NetworkError) {
  network::URLLoaderCompletionStatus status;
  status.error_code = net::ERR_FAILED;
  url_loader_factory_.AddResponse(url_, /*head=*/nullptr, kAsciiResponseBody,
                                  status);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Failed to load https://url.test/script.js error = net::ERR_FAILED.",
      last_error_msg());
}

// HTTP 404 responses are trested as failures.
TEST_F(AuctionDownloaderTest, HttpError) {
  // This is an unlikely response for an error case, but should fail if it ever
  // happens.
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody, kAllowFledgeHeader, net::HTTP_NOT_FOUND);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Failed to load https://url.test/script.js HTTP status = 404 Not Found.",
      last_error_msg());
}

TEST_F(AuctionDownloaderTest, Timeout) {
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody, kAllowFledgeHeader,
              net::HTTP_REQUEST_TIMEOUT);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Failed to load https://url.test/script.js HTTP status = 408 Request "
      "Timeout.",
      last_error_msg());
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
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to lack of "
      "X-Allow-FLEDGE: true.",
      last_error_msg());

  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody, "X-Allow-FLEDGE: sometimes");
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to lack of "
      "X-Allow-FLEDGE: true.",
      last_error_msg());

  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody, "X-Allow-FLEDGE: ");
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to lack of "
      "X-Allow-FLEDGE: true.",
      last_error_msg());

  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody, "X-Allow-Hats: true");
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to lack of "
      "X-Allow-FLEDGE: true.",
      last_error_msg());

  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody, "");
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to lack of "
      "X-Allow-FLEDGE: true.",
      last_error_msg());

  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody, absl::nullopt);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to lack of "
      "X-Allow-FLEDGE: true.",
      last_error_msg());
}

TEST_F(AuctionDownloaderTest, PassesHeaders) {
  std::string allow_fledge_string;
  std::string data_version_string;

  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody, "X-Allow-FLEDGE: true");
  EXPECT_TRUE(RunRequest()) << last_error_msg();
  EXPECT_TRUE(
      headers_->GetNormalizedHeader("X-Allow-FLEDGE", &allow_fledge_string));
  EXPECT_EQ("true", allow_fledge_string);
  EXPECT_FALSE(
      headers_->GetNormalizedHeader("Data-Version", &data_version_string));

  mime_type_ = AuctionDownloader::MimeType::kJson;
  AddVersionedJsonResponse(&url_loader_factory_, url_, kAsciiResponseBody, 10u);
  EXPECT_TRUE(RunRequest()) << last_error_msg();
  EXPECT_TRUE(
      headers_->GetNormalizedHeader("X-Allow-FLEDGE", &allow_fledge_string));
  EXPECT_EQ("true", allow_fledge_string);
  EXPECT_TRUE(
      headers_->GetNormalizedHeader("Data-Version", &data_version_string));
  EXPECT_EQ("10", data_version_string);

  AddVersionedJsonResponse(&url_loader_factory_, url_, kAsciiResponseBody, 5u);
  EXPECT_TRUE(RunRequest()) << last_error_msg();
  EXPECT_TRUE(
      headers_->GetNormalizedHeader("X-Allow-FLEDGE", &allow_fledge_string));
  EXPECT_EQ("true", allow_fledge_string);
  EXPECT_TRUE(
      headers_->GetNormalizedHeader("Data-Version", &data_version_string));
  EXPECT_EQ("5", data_version_string);

  AddJsonResponse(&url_loader_factory_, url_, kAsciiResponseBody);
  EXPECT_TRUE(RunRequest()) << last_error_msg();
  EXPECT_TRUE(
      headers_->GetNormalizedHeader("X-Allow-FLEDGE", &allow_fledge_string));
  EXPECT_EQ("true", allow_fledge_string);
  EXPECT_FALSE(
      headers_->GetNormalizedHeader("Data-Version", &data_version_string));
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
  EXPECT_EQ("Unexpected redirect on https://url.test/script.js.",
            last_error_msg());
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
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected MIME "
      "type.",
      last_error_msg());

  // Javascript request, no response type.
  AddResponse(&url_loader_factory_, url_, absl::nullopt, kUtf8Charset,
              kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected MIME "
      "type.",
      last_error_msg());

  // Javascript request, empty response type.
  AddResponse(&url_loader_factory_, url_, "", kUtf8Charset, kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected MIME "
      "type.",
      last_error_msg());

  // Javascript request, unknown response type.
  AddResponse(&url_loader_factory_, url_, "blobfish", kUtf8Charset,
              kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected MIME "
      "type.",
      last_error_msg());

  // JSON request, Javascript response type.
  mime_type_ = AuctionDownloader::MimeType::kJson;
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected MIME "
      "type.",
      last_error_msg());

  // JSON request, no response type.
  AddResponse(&url_loader_factory_, url_, absl::nullopt, kUtf8Charset,
              kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected MIME "
      "type.",
      last_error_msg());

  // JSON request, empty response type.
  AddResponse(&url_loader_factory_, url_, "", kUtf8Charset, kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected MIME "
      "type.",
      last_error_msg());

  // JSON request, unknown response type.
  AddResponse(&url_loader_factory_, url_, "blobfish", kUtf8Charset,
              kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected MIME "
      "type.",
      last_error_msg());

  // JSON request, JSON response type.
  mime_type_ = AuctionDownloader::MimeType::kJson;
  AddResponse(&url_loader_factory_, url_, kJsonMimeType, kUtf8Charset,
              kAsciiResponseBody);
  std::unique_ptr<std::string> body = RunRequest();
  ASSERT_TRUE(body);
  EXPECT_EQ(kAsciiResponseBody, *body);
}

TEST_F(AuctionDownloaderTest, MimeTypeWasm) {
  mime_type_ = AuctionDownloader::MimeType::kWebAssembly;

  // WASM request, Javascript response type.
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kUtf8Charset,
              kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected MIME "
      "type.",
      last_error_msg());

  // WASM request, no response type.
  AddResponse(&url_loader_factory_, url_, /*mime_type=*/absl::nullopt,
              /*charset=*/absl::nullopt, kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected MIME "
      "type.",
      last_error_msg());

  // WASM request, JSON response type.
  AddResponse(&url_loader_factory_, url_, kJsonMimeType, kUtf8Charset,
              kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected MIME "
      "type.",
      last_error_msg());

  // WASM request, WASM response type.
  AddResponse(&url_loader_factory_, url_, kWasmMimeType,
              /*charset=*/absl::nullopt, kNonUtf8ResponseBody);
  std::unique_ptr<std::string> body = RunRequest();
  ASSERT_TRUE(body);
  EXPECT_EQ(kNonUtf8ResponseBody, *body);

  // Mimetypes are case insensitive.
  AddResponse(&url_loader_factory_, url_, "Application/WasM",
              /*charset=*/absl::nullopt, kNonUtf8ResponseBody);
  body = RunRequest();
  ASSERT_TRUE(body);
  EXPECT_EQ(kNonUtf8ResponseBody, *body);

  // No charset should be used (it's a binary format, after all).
  AddResponse(&url_loader_factory_, url_, kWasmMimeType,
              /*charset=*/kUtf8Charset, kUtf8ResponseBody);
  body = RunRequest();
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected MIME "
      "type.",
      last_error_msg());

  // Even an empty parameter list is to be rejected.
  AddResponse(&url_loader_factory_, url_, "application/wasm;",
              /*charset=*/absl::nullopt, kNonUtf8ResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected MIME "
      "type.",
      last_error_msg());
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
    EXPECT_EQ(
        "Rejecting load of https://url.test/script.js due to unexpected MIME "
        "type.",
        last_error_msg());
  }

  for (const char* json_type : kJsonMimeTypes) {
    mime_type_ = AuctionDownloader::MimeType::kJavascript;
    AddResponse(&url_loader_factory_, url_, json_type, kUtf8Charset,
                kAsciiResponseBody);
    EXPECT_FALSE(RunRequest());
    EXPECT_EQ(
        "Rejecting load of https://url.test/script.js due to unexpected MIME "
        "type.",
        last_error_msg());

    mime_type_ = AuctionDownloader::MimeType::kJson;
    AddResponse(&url_loader_factory_, url_, json_type, kUtf8Charset,
                kAsciiResponseBody);
    std::unique_ptr<std::string> body = RunRequest();
    ASSERT_TRUE(body);
    EXPECT_EQ(kAsciiResponseBody, *body);
  }
}

TEST_F(AuctionDownloaderTest, Charset) {
  mime_type_ = AuctionDownloader::MimeType::kJson;
  // Unknown/unsupported charsets should result in failure.
  AddResponse(&url_loader_factory_, url_, kJsonMimeType, "fred",
              kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected charset.",
      last_error_msg());

  AddResponse(&url_loader_factory_, url_, kJsonMimeType, "iso-8859-1",
              kAsciiResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected charset.",
      last_error_msg());

  // ASCII charset should restrict response bodies to ASCII characters.
  mime_type_ = AuctionDownloader::MimeType::kJavascript;
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kAsciiCharset,
              kUtf8ResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected charset.",
      last_error_msg());

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
      last_error_msg());

  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, kAsciiCharset,
              kNonUtf8ResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected charset.",
      last_error_msg());

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
      last_error_msg());

  // Null charset should act like UTF-8.
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, absl::nullopt,
              kAsciiResponseBody);
  body = RunRequest();
  ASSERT_TRUE(body);
  EXPECT_EQ(kAsciiResponseBody, *body);
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, absl::nullopt,
              kUtf8ResponseBody);
  body = RunRequest();
  ASSERT_TRUE(body);
  EXPECT_EQ(kUtf8ResponseBody, *body);
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, absl::nullopt,
              kNonUtf8ResponseBody);
  EXPECT_FALSE(RunRequest());
  EXPECT_EQ(
      "Rejecting load of https://url.test/script.js due to unexpected charset.",
      last_error_msg());

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
      last_error_msg());
}

}  // namespace
}  // namespace auction_worklet

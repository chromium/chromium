// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_downloader.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace auction_worklet {
namespace {

const char kAsciiResponseBody[] = "ASCII response body.";

class AuctionDownloaderTest : public testing::Test {
 public:
  AuctionDownloaderTest() = default;
  ~AuctionDownloaderTest() override = default;

  std::unique_ptr<std::string> RunRequest() {
    AuctionDownloader downloader(
        &url_loader_factory_, url_,
        base::BindOnce(&AuctionDownloaderTest::DownloadCompleteCallback,
                       base::Unretained(this)));
    run_loop_.Run();
    return std::move(body_);
  }

 protected:
  void DownloadCompleteCallback(std::unique_ptr<std::string> body) {
    DCHECK(!body_);
    body_ = std::move(body);
    run_loop_.Quit();
  }

  base::test::TaskEnvironment task_environment_;

  const GURL url_ = GURL("https://url.test/");

  base::RunLoop run_loop_;
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
  url_loader_factory_.AddResponse(url_.spec(), kAsciiResponseBody,
                                  net::HTTP_NOT_FOUND);
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

  url_loader_factory_.AddResponse(
      url_, network::mojom::URLResponseHead::New(), kAsciiResponseBody,
      network::URLLoaderCompletionStatus(), std::move(redirects));
  EXPECT_FALSE(RunRequest());
}

TEST_F(AuctionDownloaderTest, Success) {
  url_loader_factory_.AddResponse(url_.spec(), kAsciiResponseBody);
  std::unique_ptr<std::string> body = RunRequest();
  ASSERT_TRUE(body);
  EXPECT_EQ(kAsciiResponseBody, *body);
}

}  // namespace
}  // namespace auction_worklet

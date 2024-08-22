// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/winhttp/network_fetcher.h"

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/winhttp/scoped_hinternet.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"
#include "url/gurl.h"

namespace winhttp {

TEST(WinHttpNetworkFetcher, InvalidUrlPost) {
  base::test::TaskEnvironment environment;
  base::RunLoop run_loop;
  auto network_fetcher = base::MakeRefCounted<NetworkFetcher>(
      base::MakeRefCounted<SharedHInternet>(
          CreateSessionHandle(L"WinHttpNetworkFetcher.InvalidUrlPost",
                              WINHTTP_ACCESS_TYPE_NO_PROXY)),
      base::MakeRefCounted<ProxyConfiguration>());
  network_fetcher->PostRequest(
      /*url=*/GURL("file://afile"),
      /*content_type=*/"text/plain",
      /*post_data=*/"a request body",
      /*post_additional_headers=*/{},
      /*fetch_started_callback=*/base::DoNothing(),
      /*fetch_progress_callback=*/base::DoNothing(),
      /*fetch_complete_callback=*/
      base::BindLambdaForTesting(
          [&run_loop](int response_code) { run_loop.Quit(); }));
  run_loop.Run();
  EXPECT_EQ(network_fetcher->GetNetError(), E_INVALIDARG);
}

TEST(WinHttpNetworkFetcher, InvalidUrlDownload) {
  base::test::TaskEnvironment environment;
  base::RunLoop run_loop;
  auto network_fetcher = base::MakeRefCounted<NetworkFetcher>(
      base::MakeRefCounted<SharedHInternet>(
          CreateSessionHandle(L"WinHttpNetworkFetcher.InvalidUrlDownload",
                              WINHTTP_ACCESS_TYPE_NO_PROXY)),
      base::MakeRefCounted<ProxyConfiguration>());
  network_fetcher->DownloadToFile(
      /*url=*/GURL("file://afile"),
      /*file_path=*/{},
      /*fetch_started_callback=*/base::DoNothing(),
      /*fetch_progress_callback=*/base::DoNothing(),
      /*fetch_complete_callback=*/
      base::BindLambdaForTesting(
          [&run_loop](int response_code) { run_loop.Quit(); }));
  run_loop.Run();
  EXPECT_EQ(network_fetcher->GetNetError(), E_INVALIDARG);
}

// Tests that the fetcher is not crashing when the session handle is null.
TEST(WinHttpNetworkFetcher, NullSession) {
  base::test::TaskEnvironment environment;
  base::RunLoop run_loop;
  auto network_fetcher = base::MakeRefCounted<NetworkFetcher>(
      base::MakeRefCounted<SharedHInternet>(ScopedHInternet(NULL)),
      base::MakeRefCounted<ProxyConfiguration>());
  network_fetcher->DownloadToFile(
      /*url=*/GURL("http://aurl"),
      /*file_path=*/{},
      /*fetch_started_callback=*/base::DoNothing(),
      /*fetch_progress_callback=*/base::DoNothing(),
      /*fetch_complete_callback=*/
      base::BindLambdaForTesting(
          [&run_loop](int response_code) { run_loop.Quit(); }));
  run_loop.Run();
  EXPECT_THAT(network_fetcher->GetNetError(),
              ::testing::AnyOf(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32,
                                            ERROR_WINHTTP_NOT_INITIALIZED),
                               E_HANDLE));
}

TEST(WinHttpNetworkFetcher, GZip) {
  static const std::string kResponse = "hello response";
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        if (!request.headers.contains("Accept-Encoding") ||
            request.headers.at("Accept-Encoding").find("gzip") ==
                std::string::npos) {
          ADD_FAILURE() << "gzip `Accept-Encoding` not found in request";
          response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
          return response;
        }

        std::string compressed_body;
        if (!compression::GzipCompress(kResponse, &compressed_body)) {
          ADD_FAILURE() << "gzip compression failed";
          response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
          return response;
        }
        response->AddCustomHeader("Content-Encoding", "gzip");
        response->set_content(compressed_body);
        return response;
      }));
  ASSERT_TRUE(test_server.Start());

  base::test::TaskEnvironment environment;
  base::RunLoop run_loop;
  auto network_fetcher = base::MakeRefCounted<NetworkFetcher>(
      base::MakeRefCounted<SharedHInternet>(CreateSessionHandle(
          L"WinHttpNetworkFetcherTest.GZip", WINHTTP_ACCESS_TYPE_NO_PROXY)),
      base::MakeRefCounted<ProxyConfiguration>());
  network_fetcher->PostRequest(
      /*url=*/test_server.GetURL("/"),
      /*content_type=*/"text/plain",
      /*post_data=*/"hello request",
      /*post_additional_headers=*/{},
      /*fetch_started_callback=*/base::DoNothing(),
      /*fetch_progress_callback=*/base::DoNothing(),
      /*fetch_complete_callback=*/
      base::BindLambdaForTesting(
          [&run_loop](int /*response_code*/) { run_loop.Quit(); }));
  run_loop.Run();
  ASSERT_HRESULT_SUCCEEDED(network_fetcher->GetNetError());
  ASSERT_EQ(network_fetcher->GetResponseBody(), kResponse);
}

}  // namespace winhttp

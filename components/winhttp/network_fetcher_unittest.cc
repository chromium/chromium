// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/winhttp/network_fetcher.h"

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/winhttp/scoped_hinternet.h"
#include "testing/gtest/include/gtest/gtest.h"
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
  EXPECT_EQ(network_fetcher->GetNetError(),
            MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32,
                         ERROR_WINHTTP_NOT_INITIALIZED));
}

}  // namespace winhttp

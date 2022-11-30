// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/winhttp/network_fetcher.h"

#include "base/bind.h"
#include "base/files/file.h"
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
  winhttp::ScopedHInternet session_handle = CreateSessionHandle(
      L"WinHttpNetworkFetcher.InvalidUrlPost", WINHTTP_ACCESS_TYPE_NO_PROXY);
  auto network_fetcher = base::MakeRefCounted<NetworkFetcher>(
      session_handle.get(), base::MakeRefCounted<ProxyConfiguration>());
  network_fetcher->PostRequest(
      /*url*/ GURL("file://afile"),
      /*content_type*/ "text/plain",
      /*post_data*/ "a request body",
      /*post_additional_headers*/ {},
      /*fetch_started_callback*/
      base::BindOnce([](int response_code, int64_t content_length) {}),
      /*fetch_progress_callback*/ base::BindRepeating([](int64_t current) {}),
      /*fetch_complete_callback*/
      base::BindLambdaForTesting(
          [&run_loop](int response_code) { run_loop.Quit(); }));
  run_loop.Run();
  EXPECT_EQ(network_fetcher->GetNetError(), E_INVALIDARG);
}

TEST(WinHttpNetworkFetcher, InvalidUrlDownload) {
  base::test::TaskEnvironment environment;
  base::RunLoop run_loop;
  winhttp::ScopedHInternet session_handle = CreateSessionHandle(
      L"WinHttpNetworkFetcher.InvalidUrlPost", WINHTTP_ACCESS_TYPE_NO_PROXY);
  auto network_fetcher = base::MakeRefCounted<NetworkFetcher>(
      session_handle.get(), base::MakeRefCounted<ProxyConfiguration>());
  network_fetcher->DownloadToFile(
      /*url*/ GURL("file://afile"),
      /*file_path*/ {},
      /*fetch_started_callback*/
      base::BindOnce([](int response_code, int64_t content_length) {}),
      /*fetch_progress_callback*/ base::BindRepeating([](int64_t current) {}),
      /*fetch_complete_callback*/
      base::BindLambdaForTesting(
          [&run_loop](int response_code) { run_loop.Quit(); }));
  run_loop.Run();
  EXPECT_EQ(network_fetcher->GetNetError(), E_INVALIDARG);
}
}  // namespace winhttp

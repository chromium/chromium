// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_script_fetcher.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

blink::mojom::WorkerMainScriptLoadParamsPtr CreateParams(
    const std::vector<GURL>& url_list_via_service_worker,
    const std::vector<GURL>& redirect_infos) {
  blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params =
      blink::mojom::WorkerMainScriptLoadParams::New();

  main_script_load_params->response_head =
      network::mojom::URLResponseHead::New();

  if (!url_list_via_service_worker.empty()) {
    main_script_load_params->response_head->was_fetched_via_service_worker =
        true;
    main_script_load_params->response_head->url_list_via_service_worker =
        url_list_via_service_worker;
  }

  for (const GURL& url : redirect_infos) {
    net::RedirectInfo redirect_info;
    redirect_info.new_url = url;
    main_script_load_params->redirect_infos.push_back(redirect_info);
  }

  return main_script_load_params;
}

}  // namespace

TEST(WorkerScriptFetcherTest, DetermineFinalResponseUrl) {
  struct TestCase {
    GURL initial_request_url;
    std::vector<GURL> url_list_via_service_worker;
    std::vector<GURL> redirect_infos;
    GURL expected_final_response_url;
  };

  static const std::vector<TestCase> kTestCases = {
      {
          GURL("https://initial.com"),
          {},
          {},
          GURL("https://initial.com"),
      },
      {
          GURL("https://initial.com"),
          {GURL("https://url_list_1.com"), GURL("https://url_list_2.com")},
          {},
          GURL("https://url_list_2.com"),
      },
      {
          GURL("https://initial.com"),
          {},
          {GURL("https://redirect_1.com"), GURL("https://redirect_2.com")},
          GURL("https://redirect_2.com"),
      },
      {
          GURL("https://initial.com"),
          {GURL("https://url_list_1.com"), GURL("https://url_list_2.com")},
          {GURL("https://redirect_1.com"), GURL("https://redirect_2.com")},
          GURL("https://url_list_2.com"),
      },
  };

  for (const auto& test_case : kTestCases) {
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params =
        CreateParams(test_case.url_list_via_service_worker,
                     test_case.redirect_infos);

    GURL final_response_url = WorkerScriptFetcher::DetermineFinalResponseUrl(
        test_case.initial_request_url, main_script_load_params.get());

    EXPECT_EQ(final_response_url, test_case.expected_final_response_url);
  }
}

}  // namespace content

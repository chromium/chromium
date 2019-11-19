// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "net/base/features.h"
#include "services/network/public/cpp/features.h"

namespace content {

namespace {

bool SupportsSharedWorker() {
#if defined(OS_ANDROID)
  // SharedWorkers are not enabled on Android. https://crbug.com/154571
  return false;
#else
  return true;
#endif
}

}  // namespace

enum class WorkerType {
  kServiceWorker,
  kSharedWorker,
};

class WorkerNetworkIsolationKeyBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Register a service/shared worker |main_script_file| in the scope of
  // |subframe_rfh|'s origin.
  void RegisterWorker(RenderFrameHost* subframe_rfh,
                      WorkerType worker_type,
                      const std::string& main_script_file) {
    RegisterWorkerWithUrlParameters(subframe_rfh, worker_type, main_script_file,
                                    {});
  }

  // Register a service/shared worker |main_script_file| in the scope of
  // |subframe_rfh|'s origin, that does
  // importScripts(|import_script_url|) and fetch(|fetch_url|).
  void RegisterWorkerThatDoesImportScriptsAndFetch(
      RenderFrameHost* subframe_rfh,
      WorkerType worker_type,
      const std::string& main_script_file,
      const GURL& import_script_url,
      const GURL& fetch_url) {
    RegisterWorkerWithUrlParameters(
        subframe_rfh, worker_type, main_script_file,
        {{"import_script_url", import_script_url.spec()},
         {"fetch_url", fetch_url.spec()}});
  }

  RenderFrameHost* CreateSubframe(const GURL& subframe_url) {
    DCHECK_EQ(shell()->web_contents()->GetURL().path(),
              "/workers/frame_factory.html");

    content::TestNavigationObserver navigation_observer(
        shell()->web_contents(), /*number_of_navigations*/ 1,
        content::MessageLoopRunner::QuitMode::DEFERRED);

    std::string subframe_name = GetUniqueSubframeName();
    EvalJsResult result = EvalJs(
        shell()->web_contents()->GetMainFrame(),
        JsReplace("createFrame($1, $2)", subframe_url.spec(), subframe_name));
    DCHECK(result.error.empty());
    navigation_observer.Wait();

    RenderFrameHost* subframe_rfh = FrameMatchingPredicate(
        shell()->web_contents(),
        base::BindRepeating(&FrameMatchesName, subframe_name));
    DCHECK(subframe_rfh);

    return subframe_rfh;
  }

 protected:
  void InitFeatures(bool append_frame_origin_to_network_isolation_key) {
    if (append_frame_origin_to_network_isolation_key) {
      feature_list_.InitWithFeatures(
          {net::features::kSplitCacheByNetworkIsolationKey,
           net::features::kAppendFrameOriginToNetworkIsolationKey},
          {});
    } else {
      feature_list_.InitWithFeatures(
          {net::features::kSplitCacheByNetworkIsolationKey},
          {net::features::kAppendFrameOriginToNetworkIsolationKey});
    }
  }

 private:
  void RegisterWorkerWithUrlParameters(
      RenderFrameHost* subframe_rfh,
      WorkerType worker_type,
      const std::string& main_script_file,
      const std::map<std::string, std::string>& params) {
    std::string main_script_file_with_param(main_script_file);
    for (auto it = params.begin(); it != params.end(); ++it) {
      main_script_file_with_param += base::StrCat(
          {(it == params.begin()) ? "?" : "&", it->first, "=", it->second});
    }

    switch (worker_type) {
      case WorkerType::kServiceWorker:
        DCHECK(subframe_rfh->GetLastCommittedURL().path() ==
               "/workers/service_worker_setup.html");
        EXPECT_EQ("ok",
                  EvalJs(subframe_rfh,
                         JsReplace("setup($1,$2)", main_script_file_with_param,
                                   "{\"updateViaCache\": \"all\"}")));
        break;
      case WorkerType::kSharedWorker:
        EXPECT_EQ(nullptr, EvalJs(subframe_rfh,
                                  JsReplace("let worker = new SharedWorker($1)",
                                            main_script_file_with_param)));
        break;
    }
  }

  std::string GetUniqueSubframeName() {
    subframe_id_ += 1;
    return "subframe_name_" + base::NumberToString(subframe_id_);
  }

  size_t subframe_id_ = 0;
  base::test::ScopedFeatureList feature_list_;
};

class WorkerImportScriptsAndFetchRequestNetworkIsolationKeyBrowserTest
    : public WorkerNetworkIsolationKeyBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<bool /* append_frame_origin_to_network_isolation_key */,
                     bool /* test_same_network_isolation_key */,
                     WorkerType>> {
 public:
  void SetUp() override {
    bool append_frame_origin_to_network_isolation_key;
    std::tie(append_frame_origin_to_network_isolation_key, std::ignore,
             std::ignore) = GetParam();
    InitFeatures(append_frame_origin_to_network_isolation_key);
    ContentBrowserTest::SetUp();
  }
};

// Test that network isolation key is filled in correctly for service/shared
// workers. The test navigates to "a.com" and creates two cross-origin iframes
// that each start a worker. The frames/workers may have the same origin, so
// worker1 is on "b.com" and worker2 is on either "b.com" or "c.com". The test
// checks the cache status of importScripts() and a fetch() request from the
// workers to another origin "d.com". When the workers had the same origin (the
// same network isolation key), we expect the second importScripts() and fetch()
// request to exist in the cache. When the origins are different, we expect the
// second requests to not exist in the cache.
IN_PROC_BROWSER_TEST_P(
    WorkerImportScriptsAndFetchRequestNetworkIsolationKeyBrowserTest,
    ImportScriptsAndFetchRequest) {
  bool test_same_network_isolation_key;
  WorkerType worker_type;
  std::tie(std::ignore, test_same_network_isolation_key, worker_type) =
      GetParam();

  if (worker_type == WorkerType::kSharedWorker && !SupportsSharedWorker())
    return;

  net::EmbeddedTestServer cross_origin_server_1;
  cross_origin_server_1.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(cross_origin_server_1.Start());

  net::EmbeddedTestServer cross_origin_server_tmp;
  cross_origin_server_tmp.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(cross_origin_server_tmp.Start());

  auto& cross_origin_server_2 = test_same_network_isolation_key
                                    ? cross_origin_server_1
                                    : cross_origin_server_tmp;

  net::EmbeddedTestServer resource_request_server;
  resource_request_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(resource_request_server.Start());
  GURL import_script_url = resource_request_server.GetURL("/workers/empty.js");
  GURL fetch_url = resource_request_server.GetURL("/workers/empty.html");

  std::map<GURL, size_t> request_completed_count;

  base::RunLoop cache_status_waiter;
  URLLoaderInterceptor interceptor(
      base::BindLambdaForTesting(
          [&](URLLoaderInterceptor::RequestParams* params) { return false; }),
      base::BindLambdaForTesting(
          [&](const GURL& request_url,
              const network::URLLoaderCompletionStatus& status) {
            if (request_url == import_script_url || request_url == fetch_url) {
              size_t& num_completed = request_completed_count[request_url];
              num_completed += 1;
              if (num_completed == 1) {
                EXPECT_FALSE(status.exists_in_cache);
              } else if (num_completed == 2) {
                EXPECT_EQ(status.exists_in_cache,
                          test_same_network_isolation_key);
              } else {
                NOTREACHED();
              }
            }
            if (request_completed_count[import_script_url] == 2 &&
                request_completed_count[fetch_url] == 2) {
              cache_status_waiter.Quit();
            }
          }),
      {});

  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL("/workers/frame_factory.html"),
      1);
  RenderFrameHost* subframe_rfh_1 = CreateSubframe(
      cross_origin_server_1.GetURL("/workers/service_worker_setup.html"));
  RegisterWorkerThatDoesImportScriptsAndFetch(subframe_rfh_1, worker_type,
                                              "worker_with_import_and_fetch.js",
                                              import_script_url, fetch_url);

  RenderFrameHost* subframe_rfh_2 = CreateSubframe(
      cross_origin_server_2.GetURL("/workers/service_worker_setup.html"));
  RegisterWorkerThatDoesImportScriptsAndFetch(
      subframe_rfh_2, worker_type, "worker_with_import_and_fetch_2.js",
      import_script_url, fetch_url);

  cache_status_waiter.Run();
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    WorkerImportScriptsAndFetchRequestNetworkIsolationKeyBrowserTest,
    ::testing::Combine(testing::Bool(),
                       testing::Bool(),
                       ::testing::Values(WorkerType::kServiceWorker,
                                         WorkerType::kSharedWorker)));

class ServiceWorkerMainScriptRequestNetworkIsolationKeyBrowserTest
    : public WorkerNetworkIsolationKeyBrowserTest,
      public ::testing::WithParamInterface<
          bool /* append_frame_origin_to_network_isolation_key */> {
 public:
  void SetUp() override {
    bool append_frame_origin_to_network_isolation_key = GetParam();
    InitFeatures(append_frame_origin_to_network_isolation_key);
    ContentBrowserTest::SetUp();
  }
};

// Test that network isolation key is filled in correctly for service worker's
// main script request. The test navigates to "a.com" and creates an iframe
// having origin "c.com" that registers |worker1|. The test then navigates to
// "b.com" and creates an iframe also having origin "c.com". We now want to test
// a second register request for |worker1| but just calling register() would be
// a no-op since |worker1| is already the current worker. So we register a new
// |worker2| and then |worker1| again.
//
// Note that the second navigation to "c.com" also triggers an update check for
// |worker1|. We expect both the second register request for |worker1| and this
// update request to exist in the cache.
//
// Note that it's sufficient not to test the cache miss when subframe origins
// are different as in that case the two script urls must be different and it
// also won't trigger an update.
IN_PROC_BROWSER_TEST_P(
    ServiceWorkerMainScriptRequestNetworkIsolationKeyBrowserTest,
    ServiceWorkerMainScriptRequest) {
  net::EmbeddedTestServer subframe_server;
  subframe_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(subframe_server.Start());

  net::EmbeddedTestServer new_tab_server;
  new_tab_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(new_tab_server.Start());

  size_t num_completed = 0;
  std::string main_script_file = "empty.js";
  GURL main_script_request_url =
      subframe_server.GetURL("/workers/" + main_script_file);

  base::RunLoop cache_status_waiter;
  URLLoaderInterceptor interceptor(
      base::BindLambdaForTesting(
          [&](URLLoaderInterceptor::RequestParams* params) { return false; }),
      base::BindLambdaForTesting(
          [&](const GURL& request_url,
              const network::URLLoaderCompletionStatus& status) {
            if (request_url == main_script_request_url) {
              num_completed += 1;
              if (num_completed == 1) {
                EXPECT_FALSE(status.exists_in_cache);
              } else if (num_completed == 2) {
                EXPECT_TRUE(status.exists_in_cache);
              } else if (num_completed == 3) {
                EXPECT_TRUE(status.exists_in_cache);
                cache_status_waiter.Quit();
              } else {
                NOTREACHED();
              }
            }
          }),
      {});

  // Navigate to "a.com" and create the iframe "c.com", which registers
  // |worker1|.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL("/workers/frame_factory.html"),
      1);
  RenderFrameHost* subframe_rfh_1 = CreateSubframe(
      subframe_server.GetURL("/workers/service_worker_setup.html"));
  RegisterWorker(subframe_rfh_1, WorkerType::kServiceWorker, "empty.js");

  // Navigate to "b.com" and create the another iframe on "c.com", which
  // registers |worker2| and then |worker1| again.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), new_tab_server.GetURL("/workers/frame_factory.html"), 1);
  RenderFrameHost* subframe_rfh_2 = CreateSubframe(
      subframe_server.GetURL("/workers/service_worker_setup.html"));
  RegisterWorker(subframe_rfh_2, WorkerType::kServiceWorker, "empty2.js");
  RegisterWorker(subframe_rfh_2, WorkerType::kServiceWorker, "empty.js");

  cache_status_waiter.Run();
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ServiceWorkerMainScriptRequestNetworkIsolationKeyBrowserTest,
    testing::Bool());

using SharedWorkerMainScriptRequestNetworkIsolationKeyBrowserTest =
    ServiceWorkerMainScriptRequestNetworkIsolationKeyBrowserTest;

// Test that network isolation key is filled in correctly for shared worker's
// main script request. The test navigates to "a.com" and creates an iframe
// having origin "c.com" that creates |worker1|. The test then navigates to
// "b.com" and creates an iframe also having origin "c.com" that creates
// |worker1| again.
//
// We expect the second creation request for |worker1| to exist in the cache.
//
// Note that it's sufficient not to test the cache miss when subframe origins
// are different as in that case the two script urls must be different.
IN_PROC_BROWSER_TEST_P(
    SharedWorkerMainScriptRequestNetworkIsolationKeyBrowserTest,
    SharedWorkerMainScriptRequest) {
  if (!SupportsSharedWorker())
    return;

  net::EmbeddedTestServer subframe_server;
  subframe_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(subframe_server.Start());

  net::EmbeddedTestServer new_tab_server;
  new_tab_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(new_tab_server.Start());

  size_t num_completed = 0;
  std::string main_script_file = "empty.js";
  GURL main_script_request_url =
      subframe_server.GetURL("/workers/" + main_script_file);

  base::RunLoop cache_status_waiter;
  URLLoaderInterceptor interceptor(
      base::BindLambdaForTesting(
          [&](URLLoaderInterceptor::RequestParams* params) { return false; }),
      base::BindLambdaForTesting(
          [&](const GURL& request_url,
              const network::URLLoaderCompletionStatus& status) {
            if (request_url == main_script_request_url) {
              num_completed += 1;
              if (num_completed == 1) {
                EXPECT_FALSE(status.exists_in_cache);
              } else if (num_completed == 2) {
                EXPECT_TRUE(status.exists_in_cache);
                cache_status_waiter.Quit();
              } else {
                NOTREACHED();
              }
            }
          }),
      {});

  // Navigate to "a.com" and create the iframe "c.com", which creates |worker1|.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL("/workers/frame_factory.html"),
      1);
  RenderFrameHost* subframe_rfh_1 = CreateSubframe(
      subframe_server.GetURL("/workers/service_worker_setup.html"));
  RegisterWorker(subframe_rfh_1, WorkerType::kSharedWorker, "empty.js");

  // Navigate to "b.com" and create the another iframe on "c.com", which creates
  // |worker1| again.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), new_tab_server.GetURL("/workers/frame_factory.html"), 1);
  RenderFrameHost* subframe_rfh_2 = CreateSubframe(
      subframe_server.GetURL("/workers/service_worker_setup.html"));
  RegisterWorker(subframe_rfh_2, WorkerType::kSharedWorker, "empty.js");

  cache_status_waiter.Run();
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SharedWorkerMainScriptRequestNetworkIsolationKeyBrowserTest,
    testing::Bool());

}  // namespace content

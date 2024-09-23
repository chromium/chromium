// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/features.h"

namespace content {

namespace {

bool SupportsSharedWorker() {
#if BUILDFLAG(IS_ANDROID)
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
  WorkerNetworkIsolationKeyBrowserTest() {
    feature_list_.InitAndEnableFeature(
        net::features::kSplitCacheByNetworkIsolationKey);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->AddDefaultHandlers(GetTestDataFilePath());
    content::SetupCrossSiteRedirector(https_server_.get());
    ASSERT_TRUE(https_server_->Start());
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

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
    DCHECK_EQ(shell()->web_contents()->GetLastCommittedURL().path(),
              "/workers/frame_factory.html");

    content::TestNavigationObserver navigation_observer(
        shell()->web_contents(), /*number_of_navigations*/ 1,
        content::MessageLoopRunner::QuitMode::DEFERRED);

    std::string subframe_name = GetUniqueSubframeName();
    EvalJsResult result = EvalJs(
        shell()->web_contents()->GetPrimaryMainFrame(),
        JsReplace("createFrame($1, $2)", subframe_url.spec(), subframe_name));
    DCHECK(result.error.empty());
    navigation_observer.Wait();

    RenderFrameHost* subframe_rfh = FrameMatchingPredicate(
        shell()->web_contents()->GetPrimaryPage(),
        base::BindRepeating(&FrameMatchesName, subframe_name));
    DCHECK(subframe_rfh);

    return subframe_rfh;
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
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

class WorkerImportScriptsAndFetchRequestNetworkIsolationKeyBrowserTest
    : public WorkerNetworkIsolationKeyBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<bool /* test_same_network_isolation_key */, WorkerType>> {
};

// Test that network isolation key is filled in correctly for service/shared
// workers. The test navigates to "a.test" and creates two cross-origin iframes
// that each start a worker. The frames/workers may have the same origin, so
// worker1 is on "b.test" and worker2 is on either "b.test" or "c.test". The
// test checks the cache status of importScripts() and a fetch() request from
// the workers to another origin "d.test". When the workers had the same origin
// (the same network isolation key), we expect the second importScripts() and
// fetch() request to exist in the cache. When the origins are different, we
// expect the second requests to not exist in the cache.
IN_PROC_BROWSER_TEST_P(
    WorkerImportScriptsAndFetchRequestNetworkIsolationKeyBrowserTest,
    ImportScriptsAndFetchRequest) {
  bool test_same_network_isolation_key;
  WorkerType worker_type;
  std::tie(test_same_network_isolation_key, worker_type) = GetParam();

  if (worker_type == WorkerType::kSharedWorker && !SupportsSharedWorker())
    return;

  GURL import_script_url =
      https_server()->GetURL("d.test", "/workers/empty.js");
  GURL fetch_url = https_server()->GetURL("d.test", "/workers/empty.html");

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
                NOTREACHED_IN_MIGRATION();
              }
            }
            if (request_completed_count[import_script_url] == 2 &&
                request_completed_count[fetch_url] == 2) {
              cache_status_waiter.Quit();
            }
          }),
      {});

  NavigateToURLBlockUntilNavigationsComplete(
      shell(), https_server()->GetURL("a.test", "/workers/frame_factory.html"),
      1);
  RenderFrameHost* subframe_rfh_1 = CreateSubframe(
      https_server()->GetURL("b.test", "/workers/service_worker_setup.html"));
  RegisterWorkerThatDoesImportScriptsAndFetch(subframe_rfh_1, worker_type,
                                              "worker_with_import_and_fetch.js",
                                              import_script_url, fetch_url);

  RenderFrameHost* subframe_rfh_2 = CreateSubframe(https_server()->GetURL(
      test_same_network_isolation_key ? "b.test" : "c.test",
      "/workers/service_worker_setup.html"));
  RegisterWorkerThatDoesImportScriptsAndFetch(
      subframe_rfh_2, worker_type, "worker_with_import_and_fetch_2.js",
      import_script_url, fetch_url);

  cache_status_waiter.Run();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WorkerImportScriptsAndFetchRequestNetworkIsolationKeyBrowserTest,
    ::testing::Combine(testing::Bool(),
                       ::testing::Values(WorkerType::kServiceWorker,
                                         WorkerType::kSharedWorker)));

class ServiceWorkerMainScriptRequestNetworkIsolationKeyBrowserTest
    : public WorkerNetworkIsolationKeyBrowserTest {
 public:
  ServiceWorkerMainScriptRequestNetworkIsolationKeyBrowserTest() {
    // TODO(crbug.com/40053828): Tests under this class fail when
    // kThirdPartyStoragePartitioning is enabled.
    feature_list_.InitAndDisableFeature(
        net::features::kThirdPartyStoragePartitioning);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that network isolation key is filled in correctly for service worker's
// main script request. The test navigates to "a.test" and creates an iframe
// having origin "c.test" that registers |worker1|. The test then navigates to
// "b.test" and creates an iframe also having origin "c.test". We now want to
// test a second register request for |worker1| but just calling register()
// would be a no-op since |worker1| is already the current worker. So we
// register a new |worker2| and then |worker1| again.
//
// Note that the second navigation to "c.test" also triggers an update check for
// |worker1|. We expect both the second register request for |worker1| and this
// update request to exist in the cache.
//
// Note that it's sufficient not to test the cache miss when subframe origins
// are different as in that case the two script urls must be different and it
// also won't trigger an update.
//
// TODO(crbug.com/40053828): Update test to not depend on
// kThirdPartyStoragePartitioning being disabled.
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerMainScriptRequestNetworkIsolationKeyBrowserTest,
    ServiceWorkerMainScriptRequest) {
  size_t num_completed = 0;
  std::string main_script_file = "empty.js";
  GURL main_script_request_url =
      https_server()->GetURL("c.test", "/workers/" + main_script_file);

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
                NOTREACHED_IN_MIGRATION();
              }
            }
          }),
      {});

  // Navigate to "a.test" and create the iframe "c.test", which registers
  // |worker1|.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), https_server()->GetURL("a.test", "/workers/frame_factory.html"),
      1);
  RenderFrameHost* subframe_rfh_1 = CreateSubframe(
      https_server()->GetURL("c.test", "/workers/service_worker_setup.html"));
  RegisterWorker(subframe_rfh_1, WorkerType::kServiceWorker, "empty.js");

  // Navigate to "b.test" and create the another iframe on "c.test", which
  // registers |worker2| and then |worker1| again.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), https_server()->GetURL("b.test", "/workers/frame_factory.html"),
      1);
  RenderFrameHost* subframe_rfh_2 = CreateSubframe(
      https_server()->GetURL("c.test", "/workers/service_worker_setup.html"));
  RegisterWorker(subframe_rfh_2, WorkerType::kServiceWorker, "empty2.js");
  RegisterWorker(subframe_rfh_2, WorkerType::kServiceWorker, "empty.js");

  cache_status_waiter.Run();
}

using SharedWorkerMainScriptRequestNetworkIsolationKeyBrowserTest =
    WorkerNetworkIsolationKeyBrowserTest;

// Test that network isolation key is filled in correctly for shared worker's
// main script request. The test navigates to "a.test" and creates an iframe
// having origin "c.test" that creates |worker1|. The test then navigates to
// "b.test" and creates an iframe also having origin "c.test" that creates
// |worker1| again.
//
// We expect the second creation request for |worker1| to not exist in the
// cache since the workers should be partitioned by top-level site.
//
// Note that it's sufficient not to test the cache miss when subframe origins
// are different as in that case the two script urls must be different.
IN_PROC_BROWSER_TEST_F(
    SharedWorkerMainScriptRequestNetworkIsolationKeyBrowserTest,
    SharedWorkerMainScriptRequest) {
  if (!SupportsSharedWorker())
    return;

  size_t num_completed = 0;
  std::string main_script_file = "empty.js";
  GURL main_script_request_url =
      https_server()->GetURL("c.test", "/workers/" + main_script_file);

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
                EXPECT_FALSE(status.exists_in_cache);
                cache_status_waiter.Quit();
              } else {
                NOTREACHED_IN_MIGRATION();
              }
            }
          }),
      {});

  // Navigate to "a.test" and create the iframe "c.test", which creates
  // |worker1|.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), https_server()->GetURL("a.test", "/workers/frame_factory.html"),
      1);
  RenderFrameHost* subframe_rfh_1 = CreateSubframe(
      https_server()->GetURL("c.test", "/workers/service_worker_setup.html"));
  RegisterWorker(subframe_rfh_1, WorkerType::kSharedWorker, "empty.js");

  // Navigate to "b.test" and create the another iframe on "c.test", which
  // creates |worker1| again.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), https_server()->GetURL("b.test", "/workers/frame_factory.html"),
      1);
  RenderFrameHost* subframe_rfh_2 = CreateSubframe(
      https_server()->GetURL("c.test", "/workers/service_worker_setup.html"));
  RegisterWorker(subframe_rfh_2, WorkerType::kSharedWorker, "empty.js");

  cache_status_waiter.Run();
}

}  // namespace content

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/loader/prefetch_url_loader_service.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_package/mock_signed_exchange_handler.h"
#include "content/browser/web_package/signed_exchange_loader.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"

namespace content {

struct PrefetchBrowserTestParam {
  PrefetchBrowserTestParam(bool signed_exchange_enabled)
      : signed_exchange_enabled(signed_exchange_enabled) {}
  const bool signed_exchange_enabled;
};

struct ScopedSignedExchangeHandlerFactory {
  explicit ScopedSignedExchangeHandlerFactory(
      SignedExchangeHandlerFactory* factory) {
    SignedExchangeLoader::SetSignedExchangeHandlerFactoryForTest(factory);
  }
  ~ScopedSignedExchangeHandlerFactory() {
    SignedExchangeLoader::SetSignedExchangeHandlerFactoryForTest(nullptr);
  }
};

class PrefetchBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<PrefetchBrowserTestParam> {
 public:
  struct ResponseEntry {
    ResponseEntry() = default;
    explicit ResponseEntry(
        const std::string& content,
        const std::string& content_type = "text/html",
        const std::vector<std::pair<std::string, std::string>>& headers = {})
        : content(content), content_type(content_type), headers(headers) {}
    ~ResponseEntry() = default;
    std::string content;
    std::string content_type;
    std::vector<std::pair<std::string, std::string>> headers;
  };

  PrefetchBrowserTest() = default;
  ~PrefetchBrowserTest() = default;

  void SetUp() override {
    std::vector<base::Feature> enable_features;
    if (GetParam().signed_exchange_enabled)
      enable_features.push_back(features::kSignedHTTPExchange);
    feature_list_.InitWithFeatures(enable_features, {});
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
        BrowserContext::GetDefaultStoragePartition(
            shell()->web_contents()->GetBrowserContext()));
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        BindOnce(
            &PrefetchURLLoaderService::RegisterPrefetchLoaderCallbackForTest,
            base::RetainedRef(partition->GetPrefetchURLLoaderService()),
            base::BindRepeating(&PrefetchBrowserTest::OnPrefetchURLLoaderCalled,
                                base::Unretained(this))));
  }

  void RegisterResponse(const std::string& url, const ResponseEntry& entry) {
    response_map_[url] = entry;
  }

  std::unique_ptr<net::test_server::HttpResponse> ServeResponses(
      const net::test_server::HttpRequest& request) {
    auto found = response_map_.find(request.relative_url);
    if (found != response_map_.end()) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content(found->second.content);
      response->set_content_type(found->second.content_type);
      for (const auto& header : found->second.headers)
        response->AddCustomHeader(header.first, header.second);
      return std::move(response);
    }
    return nullptr;
  }

  void WatchURLAndRunClosure(
      const std::string& relative_url,
      int* visit_count,
      base::OnceClosure closure,
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == relative_url) {
      (*visit_count)++;
      if (closure)
        std::move(closure).Run();
    }
  }

  void OnPrefetchURLLoaderCalled() { prefetch_url_loader_called_++; }

  bool CheckPrefetchURLLoaderCountIfSupported(int expected) const {
    if (!base::FeatureList::IsEnabled(features::kSignedHTTPExchange))
      return true;
    return prefetch_url_loader_called_ == expected;
  }

  int prefetch_url_loader_called_ = 0;

 private:
  base::test::ScopedFeatureList feature_list_;
  std::map<std::string, ResponseEntry> response_map_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchBrowserTest);
};

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, Simple) {
  int target_fetch_count = 0;
  const char* prefetch_url = "/prefetch.html";
  const char* target_url = "/target.html";
  RegisterResponse(
      prefetch_url,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_url)));
  RegisterResponse(
      target_url, ResponseEntry("<head><title>Prefetch Target</title></head>"));

  base::RunLoop prefetch_waiter;
  embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
      &PrefetchBrowserTest::WatchURLAndRunClosure, base::Unretained(this),
      target_url, &target_fetch_count, prefetch_waiter.QuitClosure()));
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &PrefetchBrowserTest::ServeResponses, base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, target_fetch_count);
  EXPECT_TRUE(CheckPrefetchURLLoaderCountIfSupported(0));

  // Loading a page that prefetches the target URL would increment the
  // |target_fetch_count|.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_url));
  prefetch_waiter.Run();
  EXPECT_EQ(1, target_fetch_count);
  EXPECT_TRUE(CheckPrefetchURLLoaderCountIfSupported(1));

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL (therefore not increment |target_fetch_count|.
  // The target content should still be read correctly.
  base::string16 title = base::ASCIIToUTF16("Prefetch Target");
  TitleWatcher title_watcher(shell()->web_contents(), title);
  NavigateToURL(shell(), embedded_test_server()->GetURL(target_url));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  EXPECT_EQ(1, target_fetch_count);
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, DoublePrefetch) {
  int target_fetch_count = 0;
  const char* prefetch_url = "/prefetch.html";
  const char* target_url = "/target.html";
  RegisterResponse(prefetch_url, ResponseEntry(base::StringPrintf(
                                     "<body><link rel='prefetch' href='%s'>"
                                     "<link rel='prefetch' href='%s'></body>",
                                     target_url, target_url)));
  RegisterResponse(
      target_url, ResponseEntry("<head><title>Prefetch Target</title></head>"));

  base::RunLoop prefetch_waiter;
  embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
      &PrefetchBrowserTest::WatchURLAndRunClosure, base::Unretained(this),
      target_url, &target_fetch_count, prefetch_waiter.QuitClosure()));
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &PrefetchBrowserTest::ServeResponses, base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, target_fetch_count);
  EXPECT_TRUE(CheckPrefetchURLLoaderCountIfSupported(0));

  // Loading a page that prefetches the target URL would increment the
  // |target_fetch_count|, but it should hit only once.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_url));
  prefetch_waiter.Run();
  EXPECT_EQ(1, target_fetch_count);
  EXPECT_TRUE(CheckPrefetchURLLoaderCountIfSupported(1));

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL (therefore not increment |target_fetch_count|.
  // The target content should still be read correctly.
  base::string16 title = base::ASCIIToUTF16("Prefetch Target");
  TitleWatcher title_watcher(shell()->web_contents(), title);
  NavigateToURL(shell(), embedded_test_server()->GetURL(target_url));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  EXPECT_EQ(1, target_fetch_count);
  EXPECT_TRUE(CheckPrefetchURLLoaderCountIfSupported(1));
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, NoCacheAndNoStore) {
  int nocache_fetch_count = 0;
  int nostore_fetch_count = 0;
  const char* prefetch_url = "/prefetch.html";
  const char* nocache_url = "/target1.html";
  const char* nostore_url = "/target2.html";
  RegisterResponse(prefetch_url, ResponseEntry(base::StringPrintf(
                                     "<body>"
                                     "<link rel='prefetch' href='%s'>"
                                     "<link rel='prefetch' href='%s'></body>",
                                     nocache_url, nostore_url)));
  RegisterResponse(nocache_url,
                   ResponseEntry("<head><title>NoCache Target</title></head>",
                                 "text/html", {{"cache-control", "no-cache"}}));
  RegisterResponse(nostore_url,
                   ResponseEntry("<head><title>NoStore Target</title></head>",
                                 "text/html", {{"cache-control", "no-store"}}));

  base::RunLoop nocache_waiter;
  base::RunLoop nostore_waiter;
  embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
      &PrefetchBrowserTest::WatchURLAndRunClosure, base::Unretained(this),
      nocache_url, &nocache_fetch_count, nocache_waiter.QuitClosure()));
  embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
      &PrefetchBrowserTest::WatchURLAndRunClosure, base::Unretained(this),
      nostore_url, &nostore_fetch_count, nostore_waiter.QuitClosure()));
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &PrefetchBrowserTest::ServeResponses, base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(CheckPrefetchURLLoaderCountIfSupported(0));

  // Loading a page that prefetches the target URL would increment the
  // fetch count for the both targets.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_url));
  nocache_waiter.Run();
  nostore_waiter.Run();
  EXPECT_EQ(1, nocache_fetch_count);
  EXPECT_EQ(1, nostore_fetch_count);
  EXPECT_TRUE(CheckPrefetchURLLoaderCountIfSupported(2));

  {
    // Subsequent navigation to the no-cache URL wouldn't hit the network,
    // because no-cache resource is kept available up to kPrefetchReuseMins.
    base::string16 title = base::ASCIIToUTF16("NoCache Target");
    TitleWatcher title_watcher(shell()->web_contents(), title);
    NavigateToURL(shell(), embedded_test_server()->GetURL(nocache_url));
    EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
    EXPECT_EQ(1, nocache_fetch_count);
  }
  {
    // Subsequent navigation to the no-store URL hit the network again,
    // because no-store resource is not cached even for prefetch.
    base::string16 title = base::ASCIIToUTF16("NoStore Target");
    TitleWatcher title_watcher(shell()->web_contents(), title);
    NavigateToURL(shell(), embedded_test_server()->GetURL(nostore_url));
    EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
    EXPECT_EQ(2, nostore_fetch_count);
  }
  EXPECT_TRUE(CheckPrefetchURLLoaderCountIfSupported(2));
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, WithPreload) {
  int target_fetch_count = 0;
  int preload_fetch_count = 0;
  const char* prefetch_url = "/prefetch.html";
  const char* target_url = "/target.html";
  const char* preload_url = "/preload.js";
  RegisterResponse(
      prefetch_url,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_url)));
  RegisterResponse(
      target_url,
      ResponseEntry(
          "<head><title>Prefetch Target</title></head>", "text/html",
          {{"link", "<./preload.js>;rel=\"preload\";as=\"script\""}}));
  RegisterResponse(preload_url,
                   ResponseEntry("function foo() {}", "text/javascript"));

  base::RunLoop preload_waiter;
  embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
      &PrefetchBrowserTest::WatchURLAndRunClosure, base::Unretained(this),
      target_url, &target_fetch_count, base::RepeatingClosure()));
  embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
      &PrefetchBrowserTest::WatchURLAndRunClosure, base::Unretained(this),
      preload_url, &preload_fetch_count, preload_waiter.QuitClosure()));
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &PrefetchBrowserTest::ServeResponses, base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(CheckPrefetchURLLoaderCountIfSupported(0));

  // Loading a page that prefetches the target URL would increment both
  // |target_fetch_count| and |preload_fetch_count|.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_url));
  preload_waiter.Run();
  EXPECT_EQ(1, target_fetch_count);
  EXPECT_EQ(1, preload_fetch_count);
  EXPECT_TRUE(CheckPrefetchURLLoaderCountIfSupported(1));

  int preload_url_entries = 0;
  while (preload_url_entries == 0) {
    const bool result = ExecuteScriptAndExtractInt(
        shell()->web_contents(),
        base::StringPrintf(
            "window.domAutomationController.send(performance.getEntriesByName('"
            "%s').length)",
            embedded_test_server()->GetURL(preload_url).spec().c_str()),
        &preload_url_entries);
    ASSERT_TRUE(result);
  }
  EXPECT_GE(preload_url_entries, 1);
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, WebPackageWithPreload) {
  int target_fetch_count = 0;
  int preload_fetch_count = 0;
  const char* prefetch_url = "/prefetch.html";
  const char* target_sxg = "/target.sxg";
  const char* target_url = "/target.html";
  const char* preload_url_in_sxg = "/preload.js";

  RegisterResponse(
      prefetch_url,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg)));
  RegisterResponse(
      target_sxg,
      // We mock the SignedExchangeHandler, so just return a HTML content
      // as "application/signed-exchange;v=b2".
      ResponseEntry("<head><title>Prefetch Target (SXG)</title></head>",
                    "application/signed-exchange;v=b2"));
  RegisterResponse(preload_url_in_sxg,
                   ResponseEntry("function foo() {}", "text/javascript"));

  base::RunLoop preload_waiter;
  base::RunLoop prefetch_waiter;
  embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
      &PrefetchBrowserTest::WatchURLAndRunClosure, base::Unretained(this),
      target_sxg, &target_fetch_count, prefetch_waiter.QuitClosure()));
  embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
      &PrefetchBrowserTest::WatchURLAndRunClosure, base::Unretained(this),
      preload_url_in_sxg, &preload_fetch_count, preload_waiter.QuitClosure()));
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &PrefetchBrowserTest::ServeResponses, base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(CheckPrefetchURLLoaderCountIfSupported(0));

  MockSignedExchangeHandlerFactory factory(
      SignedExchangeLoadResult::kSuccess, net::OK,
      GURL(embedded_test_server()->GetURL(target_url)), "text/html",
      {base::StringPrintf(
          "Link: <%s>;rel=\"preload\";as=\"script\"",
          embedded_test_server()->GetURL(preload_url_in_sxg).spec().c_str())});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  // Loading a page that prefetches the target URL would increment both
  // |target_fetch_count| and |preload_fetch_count|.
  NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_url));
  prefetch_waiter.Run();
  EXPECT_EQ(1, target_fetch_count);
  EXPECT_TRUE(CheckPrefetchURLLoaderCountIfSupported(1));

  // Test after this point requires SignedHTTPExchange support
  if (!base::FeatureList::IsEnabled(features::kSignedHTTPExchange))
    return;

  // If the header in the .sxg file is correctly extracted, we should
  // be able to also see the preload.
  preload_waiter.Run();
  EXPECT_EQ(1, preload_fetch_count);
}

INSTANTIATE_TEST_CASE_P(PrefetchBrowserTest,
                        PrefetchBrowserTest,
                        testing::Values(PrefetchBrowserTestParam(true),
                                        PrefetchBrowserTestParam(false)));

}  // namespace content

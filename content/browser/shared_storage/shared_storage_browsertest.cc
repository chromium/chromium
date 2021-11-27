// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/shared_storage/shared_storage_worklet_driver.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/browser/shared_storage/shared_storage_worklet_host_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

const char kSimplePagePath[] = "/simple_page.html";

const char kPageWithBlankIframePath[] = "/page_with_blank_iframe.html";

}  // namespace

class TestSharedStorageWorkletHost : public SharedStorageWorkletHost {
 public:
  using SharedStorageWorkletHost::SharedStorageWorkletHost;

  ~TestSharedStorageWorkletHost() override = default;

  void WaitForWorkletResponsesCount(size_t count) {
    if (worklet_responses_count_ >= count)
      return;

    expected_worklet_responses_count_ = count;
    worklet_responses_count_waiter_.Run();
  }

 private:
  void OnAddModuleOnWorkletFinished(
      blink::mojom::SharedStorageDocumentService::AddModuleOnWorkletCallback
          callback,
      bool success,
      const std::string& error_message) override {
    SharedStorageWorkletHost::OnAddModuleOnWorkletFinished(
        std::move(callback), success, error_message);
    OnWorkletResponseReceived();
  }

  void OnRunOperationOnWorkletFinished(
      bool success,
      const std::string& error_message) override {
    SharedStorageWorkletHost::OnRunOperationOnWorkletFinished(success,
                                                              error_message);
    OnWorkletResponseReceived();
  }

  void OnWorkletResponseReceived() {
    ++worklet_responses_count_;

    if (worklet_responses_count_waiter_.running() &&
        worklet_responses_count_ >= expected_worklet_responses_count_) {
      worklet_responses_count_waiter_.Quit();
    }
  }

  // How many worklet operations have finished. This only include addModule and
  // runOperation.
  size_t worklet_responses_count_ = 0;
  size_t expected_worklet_responses_count_ = 0;
  base::RunLoop worklet_responses_count_waiter_;
};

class TestSharedStorageWorkletHostManager
    : public SharedStorageWorkletHostManager {
 public:
  using SharedStorageWorkletHostManager::SharedStorageWorkletHostManager;

  ~TestSharedStorageWorkletHostManager() override = default;

  std::unique_ptr<SharedStorageWorkletHost> CreateSharedStorageWorkletHost(
      std::unique_ptr<SharedStorageWorkletDriver> driver,
      RenderFrameHost& render_frame_host) override {
    // Use the default handling for unrelated worklet hosts
    if (render_frame_host.GetLastCommittedURL().path() != kSimplePagePath &&
        render_frame_host.GetLastCommittedURL().path() !=
            kPageWithBlankIframePath) {
      return SharedStorageWorkletHostManager::CreateSharedStorageWorkletHost(
          std::move(driver), render_frame_host);
    }

    return std::make_unique<TestSharedStorageWorkletHost>(std::move(driver),
                                                          render_frame_host);
  }

  // Precondition: there's only one eligible worklet host.
  TestSharedStorageWorkletHost* GetWorkletHost() {
    DCHECK_EQ(1u, GetWorkletHostsCount());
    return static_cast<TestSharedStorageWorkletHost*>(
        GetWorkletHostsForTesting().begin()->second.get());
  }

  size_t GetWorkletHostsCount() { return GetWorkletHostsForTesting().size(); }
};

class SharedStorageBrowserTest : public ContentBrowserTest {
 public:
  SharedStorageBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kSharedStorageAPI);
  }

  void SetUpOnMainThread() override {
    auto test_worklet_host_manager =
        std::make_unique<TestSharedStorageWorkletHostManager>();

    test_worklet_host_manager_ = test_worklet_host_manager.get();

    static_cast<StoragePartitionImpl*>(shell()
                                           ->web_contents()
                                           ->GetBrowserContext()
                                           ->GetDefaultStoragePartition())
        ->OverrideSharedStorageWorkletHostManagerForTesting(
            std::move(test_worklet_host_manager));

    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  TestSharedStorageWorkletHostManager& test_worklet_host_manager() {
    DCHECK(test_worklet_host_manager_);
    return *test_worklet_host_manager_;
  }

  ~SharedStorageBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  raw_ptr<TestSharedStorageWorkletHostManager> test_worklet_host_manager_ =
      nullptr;
};

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, AddModule_Success) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_EQ(nullptr, EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetWorkletHostsCount());
  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, AddModule_ScriptNotFound) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  std::string expected_error = base::StrCat(
      {"a JavaScript error:\nError: Failed to load ",
       embedded_test_server()
           ->GetURL("a.com", "/shared_storage/nonexistent_module.js")
           .spec(),
       " HTTP status = 404 Not Found.\n"});

  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/nonexistent_module.js');
    )");

  EXPECT_EQ(expected_error, result.error);

  EXPECT_EQ(1u, test_worklet_host_manager().GetWorkletHostsCount());
  EXPECT_EQ(0u, console_observer.messages().size());
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, AddModule_RedirectNotAllowed) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  std::string expected_error = base::StrCat(
      {"a JavaScript error:\nError: Unexpected redirect on ",
       embedded_test_server()
           ->GetURL("a.com", "/server-redirect?shared_storage/simple_module.js")
           .spec(),
       ".\n"});

  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.worklet.addModule(
          '/server-redirect?shared_storage/simple_module.js');
    )");

  EXPECT_EQ(expected_error, result.error);

  EXPECT_EQ(1u, test_worklet_host_manager().GetWorkletHostsCount());
  EXPECT_EQ(0u, console_observer.messages().size());
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       AddModule_ScriptExecutionFailure) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  std::string expected_error = base::StrCat(
      {"a JavaScript error:\nError: ",
       embedded_test_server()
           ->GetURL("a.com", "/shared_storage/erroneous_module.js")
           .spec(),
       ":6 Uncaught ReferenceError: undefinedVariable is not defined.\n"});

  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/erroneous_module.js');
    )");

  EXPECT_EQ(expected_error, result.error);

  EXPECT_EQ(1u, test_worklet_host_manager().GetWorkletHostsCount());
  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("Start executing erroneous_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       AddModule_MultipleAddModuleFailure) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_EQ(nullptr, EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  std::string expected_error =
      "a JavaScript error:\nError: sharedStorage.worklet.addModule() can only "
      "be invoked once per browsing context.\n";

  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )");
  EXPECT_EQ(expected_error, result.error);

  EXPECT_EQ(1u, test_worklet_host_manager().GetWorkletHostsCount());
  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, RunOperation_Success) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_EQ(nullptr, EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetWorkletHostsCount());
  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));

  EXPECT_EQ(nullptr, EvalJs(shell(), R"(
      sharedStorage.runOperation(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"));

  // There are 2 "worklet operations": addModule and runOperation.
  test_worklet_host_manager().GetWorkletHost()->WaitForWorkletResponsesCount(2);

  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("{\"customKey\":\"customValue\"}",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       RunOperation_Failure_RunOperationBeforeAddModule) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_EQ(nullptr, EvalJs(shell(), R"(
      sharedStorage.runOperation(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"));

  EXPECT_EQ(nullptr, EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetWorkletHostsCount());

  // There are 2 "worklet operations": runOperation and addModule.
  test_worklet_host_manager().GetWorkletHost()->WaitForWorkletResponsesCount(2);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ(
      "sharedStorage.worklet.addModule() has to be called before "
      "sharedStorage.runOperation().",
      base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[0].log_level);
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       RunOperation_Failure_InvalidOptionsArgument) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", kSimplePagePath)));

  EXPECT_EQ(nullptr, EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EvalJsResult result = EvalJs(shell(), R"(
      function testFunction() {}

      sharedStorage.runOperation(
          'test-operation', {data: {'customKey': testFunction}});
    )");

  EXPECT_EQ(
      std::string(
          "a JavaScript error:\nError: function testFunction() {} could not be "
          "cloned.\n    at eval (__const_std::string&_script__:4:21):\n        "
          "         .then((result) => true ? result : Promise.reject(),\n      "
          "                      ^^^^^\n    at eval (<anonymous>)\n    at "
          "EvalJs-runner.js:2:34\n"),
      result.error);
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       RunOperation_Failure_ErrorInRunOperation) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_EQ(nullptr, EvalJs(shell(), R"(
      sharedStorage.worklet.addModule(
          'shared_storage/erroneous_function_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetWorkletHostsCount());
  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("Start executing erroneous_function_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[0].log_level);
  EXPECT_EQ("Finish executing erroneous_function_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[0].log_level);

  EXPECT_EQ(nullptr, EvalJs(shell(), R"(
      sharedStorage.runOperation(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"));

  // There are 2 "worklet operations": addModule and runOperation.
  test_worklet_host_manager().GetWorkletHost()->WaitForWorkletResponsesCount(2);

  EXPECT_EQ(4u, console_observer.messages().size());
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[2].log_level);
  EXPECT_EQ("ReferenceError: undefinedVariable is not defined",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[3].log_level);
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       RunOperation_Failure_UnimplementedSharedStorageMethod) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_EQ(nullptr, EvalJs(shell(), R"(
      sharedStorage.worklet.addModule(
          'shared_storage/shared_storage_keys_function_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetWorkletHostsCount());
  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("Start executing shared_storage_keys_function_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing shared_storage_keys_function_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));

  EXPECT_EQ(nullptr, EvalJs(shell(), R"(
      sharedStorage.runOperation('test-operation');
    )"));

  // There are 2 "worklet operations": addModule and runOperation.
  test_worklet_host_manager().GetWorkletHost()->WaitForWorkletResponsesCount(2);

  EXPECT_EQ(4u, console_observer.messages().size());
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[2].log_level);
  EXPECT_EQ("sharedStorage.keys() is not implemented",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[3].log_level);
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, WorkletDestroyed) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_ASSUMES_NO_CACHING);

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", kSimplePagePath)));

  EXPECT_EQ(nullptr, EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetWorkletHostsCount());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  EXPECT_EQ(0u, test_worklet_host_manager().GetWorkletHostsCount());
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, TwoWorklets) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_ASSUMES_NO_CACHING);

  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "a.com", kPageWithBlankIframePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  RenderFrameHost* iframe =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryFrameTree()
          .root()
          ->child_at(0)
          ->current_frame_host();

  EXPECT_EQ(nullptr, EvalJs(iframe, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module2.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetWorkletHostsCount());

  EXPECT_EQ(nullptr, EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(2u, test_worklet_host_manager().GetWorkletHostsCount());

  NavigateIframeToURL(shell()->web_contents(), "test_iframe",
                      GURL(url::kAboutBlankURL));

  EXPECT_EQ(1u, test_worklet_host_manager().GetWorkletHostsCount());

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("Executing simple_module2.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
}

}  // namespace content

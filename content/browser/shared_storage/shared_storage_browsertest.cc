// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/browser/renderer_host/navigation_request.h"
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
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/fenced_frame_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"

namespace content {

using testing::Pair;
using testing::UnorderedElementsAre;
using SharedStorageReportingMap = base::flat_map<std::string, ::GURL>;

namespace {

const char kSimplePagePath[] = "/simple_page.html";

const char kFencedFramePath[] = "/fenced_frames/title0.html";

const char kPageWithBlankIframePath[] = "/page_with_blank_iframe.html";

const double kBudgetAllowed = 5.0;

const char kSelectFrom8URLsScript[] = R"(
    let urls = [];
    for (let i = 0; i < 8; ++i) {
      urls.push({url: 'fenced_frames/title' + i.toString() + '.html',
                 reportingMetadata: {
                   'click': 'fenced_frames/report' + i.toString() + '.html'
                 }});
    }

    sharedStorage.selectURL(
        'test-url-selection-operation', urls, {data: {'mockResult': 1}});
  )";

}  // namespace

class TestSharedStorageWorkletHost : public SharedStorageWorkletHost {
 public:
  TestSharedStorageWorkletHost(
      std::unique_ptr<SharedStorageWorkletDriver> driver,
      SharedStorageDocumentServiceImpl& document_service,
      bool should_defer_worklet_messages)
      : SharedStorageWorkletHost(std::move(driver), document_service),
        should_defer_worklet_messages_(should_defer_worklet_messages) {}

  ~TestSharedStorageWorkletHost() override = default;

  void WaitForWorkletResponsesCount(size_t count) {
    if (worklet_responses_count_ >= count)
      return;

    expected_worklet_responses_count_ = count;
    worklet_responses_count_waiter_.Run();
  }

  void set_should_defer_worklet_messages(bool should_defer_worklet_messages) {
    should_defer_worklet_messages_ = should_defer_worklet_messages;
  }

  const std::vector<base::OnceClosure>& pending_worklet_messages() {
    return pending_worklet_messages_;
  }

  void ConsoleLog(const std::string& message) override {
    ConsoleLogHelper(message, /*initial_message=*/true);
  }

  void ConsoleLogHelper(const std::string& message, bool initial_message) {
    if (should_defer_worklet_messages_ && initial_message) {
      pending_worklet_messages_.push_back(base::BindOnce(
          &TestSharedStorageWorkletHost::ConsoleLogHelper,
          weak_ptr_factory_.GetWeakPtr(), message, /*initial_message=*/false));
      return;
    }

    SharedStorageWorkletHost::ConsoleLog(message);
  }

  void FireKeepAliveTimerNow() {
    ASSERT_TRUE(GetKeepAliveTimerForTesting().IsRunning());
    GetKeepAliveTimerForTesting().FireNow();
  }

  void ExecutePendingWorkletMessages() {
    for (auto& callback : pending_worklet_messages_) {
      std::move(callback).Run();
    }
  }

 private:
  void OnAddModuleOnWorkletFinished(
      blink::mojom::SharedStorageDocumentService::AddModuleOnWorkletCallback
          callback,
      bool success,
      const std::string& error_message) override {
    OnAddModuleOnWorkletFinishedHelper(std::move(callback), success,
                                       error_message,
                                       /*initial_message=*/true);
  }

  void OnAddModuleOnWorkletFinishedHelper(
      blink::mojom::SharedStorageDocumentService::AddModuleOnWorkletCallback
          callback,
      bool success,
      const std::string& error_message,
      bool initial_message) {
    if (should_defer_worklet_messages_ && initial_message) {
      pending_worklet_messages_.push_back(base::BindOnce(
          &TestSharedStorageWorkletHost::OnAddModuleOnWorkletFinishedHelper,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), success,
          error_message, /*initial_message=*/false));
    } else {
      SharedStorageWorkletHost::OnAddModuleOnWorkletFinished(
          std::move(callback), success, error_message);
    }

    if (initial_message)
      OnWorkletResponseReceived();
  }

  void OnRunOperationOnWorkletFinished(
      bool success,
      const std::string& error_message) override {
    OnRunOperationOnWorkletFinishedHelper(success, error_message,
                                          /*initial_message=*/true);
  }

  void OnRunOperationOnWorkletFinishedHelper(bool success,
                                             const std::string& error_message,
                                             bool initial_message) {
    if (should_defer_worklet_messages_ && initial_message) {
      pending_worklet_messages_.push_back(base::BindOnce(
          &TestSharedStorageWorkletHost::OnRunOperationOnWorkletFinishedHelper,
          weak_ptr_factory_.GetWeakPtr(), success, error_message,
          /*initial_message=*/false));
    } else {
      SharedStorageWorkletHost::OnRunOperationOnWorkletFinished(success,
                                                                error_message);
    }

    if (initial_message)
      OnWorkletResponseReceived();
  }

  void OnRunURLSelectionOperationOnWorkletFinished(
      const GURL& urn_uuid,
      bool script_execution_success,
      const std::string& script_execution_error_message,
      uint32_t index,
      BudgetResult budget_result) override {
    OnRunURLSelectionOperationOnWorkletFinishedHelper(
        urn_uuid, script_execution_success, script_execution_error_message,
        index, std::move(budget_result),
        /*initial_message=*/true);
  }

  void OnRunURLSelectionOperationOnWorkletFinishedHelper(
      const GURL& urn_uuid,
      bool script_execution_success,
      const std::string& script_execution_error_message,
      uint32_t index,
      BudgetResult budget_result,
      bool initial_message) {
    if (should_defer_worklet_messages_ && initial_message) {
      pending_worklet_messages_.push_back(base::BindOnce(
          &TestSharedStorageWorkletHost::
              OnRunURLSelectionOperationOnWorkletFinishedHelper,
          weak_ptr_factory_.GetWeakPtr(), urn_uuid, script_execution_success,
          script_execution_error_message, index, std::move(budget_result),
          /*initial_message=*/false));
    } else {
      SharedStorageWorkletHost::OnRunURLSelectionOperationOnWorkletFinished(
          urn_uuid, script_execution_success, script_execution_error_message,
          index, std::move(budget_result));
    }

    if (initial_message)
      OnWorkletResponseReceived();
  }

  void OnWorkletResponseReceived() {
    ++worklet_responses_count_;

    if (worklet_responses_count_waiter_.running() &&
        worklet_responses_count_ >= expected_worklet_responses_count_) {
      worklet_responses_count_waiter_.Quit();
    }
  }

  base::TimeDelta GetKeepAliveTimeout() const override {
    // Configure a timeout large enough so that the scheduled task won't run
    // automatically. Instead, we will manually call OneShotTimer::FireNow().
    return base::Seconds(30);
  }

  // How many worklet operations have finished. This only include `addModule()`,
  // `selectURL()` and `run()`.
  size_t worklet_responses_count_ = 0;
  size_t expected_worklet_responses_count_ = 0;
  base::RunLoop worklet_responses_count_waiter_;

  // Whether we should defer messages received from the worklet environment to
  // handle them later. This includes request callbacks (e.g. for `addModule()`,
  // `selectURL()` and `run()`), as well as commands initiated from the worklet
  // (e.g. `console.log()`).
  bool should_defer_worklet_messages_;
  std::vector<base::OnceClosure> pending_worklet_messages_;

  base::WeakPtrFactory<TestSharedStorageWorkletHost> weak_ptr_factory_{this};
};

class TestSharedStorageWorkletHostManager
    : public SharedStorageWorkletHostManager {
 public:
  using SharedStorageWorkletHostManager::SharedStorageWorkletHostManager;

  ~TestSharedStorageWorkletHostManager() override = default;

  std::unique_ptr<SharedStorageWorkletHost> CreateSharedStorageWorkletHost(
      std::unique_ptr<SharedStorageWorkletDriver> driver,
      SharedStorageDocumentServiceImpl& document_service) override {
    return std::make_unique<TestSharedStorageWorkletHost>(
        std::move(driver), document_service, should_defer_worklet_messages_);
  }

  // Precondition: there's only one eligible worklet host.
  TestSharedStorageWorkletHost* GetAttachedWorkletHost() {
    DCHECK_EQ(1u, GetAttachedWorkletHostsCount());
    return static_cast<TestSharedStorageWorkletHost*>(
        GetAttachedWorkletHostsForTesting().begin()->second.get());
  }

  // Precondition: there's only one eligible worklet host.
  TestSharedStorageWorkletHost* GetKeepAliveWorkletHost() {
    DCHECK_EQ(1u, GetKeepAliveWorkletHostsCount());
    return static_cast<TestSharedStorageWorkletHost*>(
        GetKeepAliveWorkletHostsForTesting().begin()->second.get());
  }

  // Precondition: there's only one eligible worklet host.
  TestSharedStorageWorkletHost* GetAttachedWorkletHostForOrigin(
      const url::Origin& origin) {
    size_t count = 0;
    TestSharedStorageWorkletHost* result_host = nullptr;
    for (auto& p : GetAttachedWorkletHostsForTesting()) {
      if (p.second->shared_storage_origin_for_testing() == origin) {
        ++count;
        DCHECK(!result_host);
        result_host =
            static_cast<TestSharedStorageWorkletHost*>(p.second.get());
      }
    }

    DCHECK_EQ(count, 1u);
    DCHECK(result_host);
    return result_host;
  }

  void ConfigureShouldDeferWorkletMessagesOnWorkletHostCreation(
      bool should_defer_worklet_messages) {
    should_defer_worklet_messages_ = should_defer_worklet_messages;
  }

  size_t GetAttachedWorkletHostsCount() {
    return GetAttachedWorkletHostsForTesting().size();
  }

  size_t GetKeepAliveWorkletHostsCount() {
    return GetKeepAliveWorkletHostsForTesting().size();
  }

 private:
  bool should_defer_worklet_messages_ = false;
};

class SharedStorageBrowserTest : public ContentBrowserTest {
 public:
  SharedStorageBrowserTest() {
    scoped_feature_list_
        .InitWithFeaturesAndParameters(/*enabled_features=*/
                                       {{blink::features::kSharedStorageAPI,
                                         {{"SharedStorageBitBudget",
                                           base::NumberToString(
                                               kBudgetAllowed)}}},
                                        {features::
                                             kPrivacySandboxAdsAPIsOverride,
                                         {}}},
                                       /*disabled_features=*/{});
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
    FinishSetup();
  }

  // Virtual so that derived classes can delay starting the server, and/or add
  // other set up steps.
  virtual void FinishSetup() {
    https_server()->AddDefaultHandlers(GetTestDataFilePath());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    SetupCrossSiteRedirector(https_server());
    ASSERT_TRUE(https_server()->Start());
  }

  double GetRemainingBudget(const url::Origin& origin) {
    base::test::TestFuture<SharedStorageWorkletHost::BudgetResult> future;
    static_cast<StoragePartitionImpl*>(shell()
                                           ->web_contents()
                                           ->GetBrowserContext()
                                           ->GetDefaultStoragePartition())
        ->GetSharedStorageManager()
        ->GetRemainingBudget(origin, future.GetCallback());
    return future.Take().bits;
  }

  FencedFrameURLMapping::SharedStorageBudgetMetadata*
  GetSharedStorageBudgetMetadata(const GURL& urn_uuid) {
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();

    FencedFrameURLMapping& fenced_frame_url_mapping =
        root->current_frame_host()->GetPage().fenced_frame_urls_map();

    FencedFrameURLMapping::SharedStorageBudgetMetadata* metadata =
        fenced_frame_url_mapping.GetSharedStorageBudgetMetadata(GURL(urn_uuid));

    return metadata;
  }

  SharedStorageReportingMap GetSharedStorageReportingMap(const GURL& urn_uuid) {
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();

    FencedFrameURLMapping& fenced_frame_url_mapping =
        root->current_frame_host()->GetPage().fenced_frame_urls_map();

    SharedStorageReportingMap reporting_map;

    fenced_frame_url_mapping.GetSharedStorageReportingMapForTesting(
        GURL(urn_uuid), &reporting_map);

    return reporting_map;
  }

  void ExecuteScriptInWorklet(const ToRenderFrameHost& execution_target,
                              const std::string& script) {
    base::StringPairs run_function_body_replacement;
    run_function_body_replacement.push_back(
        std::make_pair("{{RUN_FUNCTION_BODY}}", script));

    std::string host =
        execution_target.render_frame_host()->GetLastCommittedOrigin().host();

    GURL module_script_url = https_server()->GetURL(
        host, net::test_server::GetFilePathWithReplacements(
                  "/shared_storage/customizable_module.js",
                  run_function_body_replacement));

    EXPECT_TRUE(ExecJs(
        execution_target,
        JsReplace("sharedStorage.worklet.addModule($1)", module_script_url)));

    EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
    EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

    EXPECT_TRUE(ExecJs(execution_target, R"(
        sharedStorage.run('test-operation');
      )"));

    // There are 2 "worklet operations": `addModule()` and `run()`.
    test_worklet_host_manager()
        .GetAttachedWorkletHost()
        ->WaitForWorkletResponsesCount(2);
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  TestSharedStorageWorkletHostManager& test_worklet_host_manager() {
    DCHECK(test_worklet_host_manager_);
    return *test_worklet_host_manager_;
  }

  ~SharedStorageBrowserTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};

  raw_ptr<TestSharedStorageWorkletHostManager, DanglingUntriaged>
      test_worklet_host_manager_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, AddModule_Success) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());
  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, AddModule_ScriptNotFound) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  std::string expected_error = base::StrCat(
      {"a JavaScript error: \"Error: Failed to load ",
       https_server()
           ->GetURL("a.test", "/shared_storage/nonexistent_module.js")
           .spec(),
       " HTTP status = 404 Not Found.\"\n"});

  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/nonexistent_module.js');
    )");

  EXPECT_EQ(expected_error, result.error);

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());
  EXPECT_EQ(0u, console_observer.messages().size());
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, AddModule_RedirectNotAllowed) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  std::string expected_error = base::StrCat(
      {"a JavaScript error: \"Error: Unexpected redirect on ",
       https_server()
           ->GetURL("a.test",
                    "/server-redirect?shared_storage/simple_module.js")
           .spec(),
       ".\"\n"});

  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.worklet.addModule(
          '/server-redirect?shared_storage/simple_module.js');
    )");

  EXPECT_EQ(expected_error, result.error);

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());
  EXPECT_EQ(0u, console_observer.messages().size());
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       AddModule_ScriptExecutionFailure) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  std::string expected_error = base::StrCat(
      {"a JavaScript error: \"Error: ",
       https_server()
           ->GetURL("a.test", "/shared_storage/erroneous_module.js")
           .spec(),
       ":6 Uncaught ReferenceError: undefinedVariable is not defined.\"\n"});

  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/erroneous_module.js');
    )");

  EXPECT_EQ(expected_error, result.error);

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());
  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("Start executing erroneous_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       AddModule_MultipleAddModuleFailure) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  std::string expected_error =
      "a JavaScript error: \"Error: sharedStorage.worklet.addModule() can only "
      "be invoked once per browsing context.\"\n";

  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )");
  EXPECT_EQ(expected_error, result.error);

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());
  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, RunOperation_Success) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());
  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"));

  // There are 2 "worklet operations": `addModule()` and `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponsesCount(2);

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
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  // There are 2 "worklet operations": `run()` and `addModule()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponsesCount(2);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ(
      "sharedStorage.worklet.addModule() has to be called before "
      "sharedStorage.run().",
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
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EvalJsResult result = EvalJs(shell(), R"(
      function testFunction() {}

      sharedStorage.run(
          'test-operation', {data: {'customKey': testFunction}});
    )");

  EXPECT_EQ(
      std::string("a JavaScript error: \""
                  "Error: function testFunction() {} could not be cloned.\n"
                  "    at __const_std::string&_script__:4:21):\n"
                  "              sharedStorage.run(\n"
                  "                            ^^^^^\n"),
      result.error);
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       RunOperation_Failure_ErrorInRunOperation) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule(
          'shared_storage/erroneous_function_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());
  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("Start executing erroneous_function_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[0].log_level);
  EXPECT_EQ("Finish executing erroneous_function_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[0].log_level);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"));

  // There are 2 "worklet operations": `addModule()` and `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponsesCount(2);

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

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, WorkletDestroyed) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, TwoWorklets) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  EXPECT_TRUE(NavigateToURL(
      shell(), https_server()->GetURL("a.test", kPageWithBlankIframePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  RenderFrameHost* iframe =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryFrameTree()
          .root()
          ->child_at(0)
          ->current_frame_host();

  EXPECT_TRUE(ExecJs(iframe, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module2.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(2u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  NavigateIframeToURL(shell()->web_contents(), "test_iframe",
                      GURL(url::kAboutBlankURL));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("Executing simple_module2.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageBrowserTest,
    KeepAlive_StartBeforeAddModuleComplete_EndAfterAddModuleComplete) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  test_worklet_host_manager()
      .ConfigureShouldDeferWorkletMessagesOnWorkletHostCreation(true);

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )",
                               EXECUTE_SCRIPT_NO_RESOLVE_PROMISES);

  // Navigate to trigger keep-alive
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(1u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  test_worklet_host_manager()
      .GetKeepAliveWorkletHost()
      ->WaitForWorkletResponsesCount(1);

  // Three pending messages are expected: two for console.log and one for
  // `addModule()` response.
  EXPECT_EQ(3u, test_worklet_host_manager()
                    .GetKeepAliveWorkletHost()
                    ->pending_worklet_messages()
                    .size());

  // Execute all the deferred messages. This will terminate the keep-alive.
  test_worklet_host_manager()
      .GetKeepAliveWorkletHost()
      ->ExecutePendingWorkletMessages();

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  // Expect no console logging, as messages logged during keep-alive are
  // dropped.
  EXPECT_EQ(0u, console_observer.messages().size());
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       KeepAlive_StartBeforeAddModuleComplete_EndAfterTimeout) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  test_worklet_host_manager()
      .ConfigureShouldDeferWorkletMessagesOnWorkletHostCreation(true);

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )",
                               EXECUTE_SCRIPT_NO_RESOLVE_PROMISES);

  // Navigate to trigger keep-alive
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(1u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  test_worklet_host_manager()
      .GetKeepAliveWorkletHost()
      ->WaitForWorkletResponsesCount(1);

  // Three pending messages are expected: two for console.log and one for
  // `addModule()` response.
  EXPECT_EQ(3u, test_worklet_host_manager()
                    .GetKeepAliveWorkletHost()
                    ->pending_worklet_messages()
                    .size());

  // Fire the keep-alive timer. This will terminate the keep-alive.
  test_worklet_host_manager()
      .GetKeepAliveWorkletHost()
      ->FireKeepAliveTimerNow();

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageBrowserTest,
    KeepAlive_StartBeforeRunOperationComplete_EndAfterRunOperationComplete) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(2u, console_observer.messages().size());

  // Configure the worklet host to defer processing the subsequent `run()`
  // response.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->set_should_defer_worklet_messages(true);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}})
    )"));

  // Navigate to trigger keep-alive
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(1u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  test_worklet_host_manager()
      .GetKeepAliveWorkletHost()
      ->WaitForWorkletResponsesCount(2);

  // Four pending messages are expected: three for console.log and one for
  // `run()` response.
  EXPECT_EQ(4u, test_worklet_host_manager()
                    .GetKeepAliveWorkletHost()
                    ->pending_worklet_messages()
                    .size());

  // Execute all the deferred messages. This will terminate the keep-alive.
  test_worklet_host_manager()
      .GetKeepAliveWorkletHost()
      ->ExecutePendingWorkletMessages();

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  // Expect no more console logging, as messages logged during keep-alive was
  // dropped.
  EXPECT_EQ(2u, console_observer.messages().size());
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, KeepAlive_SubframeWorklet) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  EXPECT_TRUE(NavigateToURL(
      shell(), https_server()->GetURL("a.test", kPageWithBlankIframePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // Configure the worklet host for the subframe to defer worklet responses.
  test_worklet_host_manager()
      .ConfigureShouldDeferWorkletMessagesOnWorkletHostCreation(true);

  RenderFrameHost* iframe =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryFrameTree()
          .root()
          ->child_at(0)
          ->current_frame_host();

  EvalJsResult result = EvalJs(iframe, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )",
                               EXECUTE_SCRIPT_NO_RESOLVE_PROMISES);

  // Navigate away to let the subframe's worklet enter keep-alive.
  NavigateIframeToURL(shell()->web_contents(), "test_iframe",
                      GURL(url::kAboutBlankURL));

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(1u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  // Ensure that the response is deferred.
  test_worklet_host_manager()
      .GetKeepAliveWorkletHost()
      ->WaitForWorkletResponsesCount(1);

  // Three pending messages are expected: two for console.log and one for
  // `addModule()` response.
  EXPECT_EQ(3u, test_worklet_host_manager()
                    .GetKeepAliveWorkletHost()
                    ->pending_worklet_messages()
                    .size());

  // Configure the worklet host for the main frame to handle worklet responses
  // directly.
  test_worklet_host_manager()
      .ConfigureShouldDeferWorkletMessagesOnWorkletHostCreation(false);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module2.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(1u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  // Execute all the deferred messages. This will terminate the keep-alive.
  test_worklet_host_manager()
      .GetKeepAliveWorkletHost()
      ->ExecutePendingWorkletMessages();

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  // Expect loggings only from executing top document's worklet.
  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("Executing simple_module2.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       RenderProcessHostDestroyedDuringWorkletKeepAlive) {
  // The test assumes pages gets deleted after navigation, letting the worklet
  // enter keep-alive phase. To ensure this, disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  test_worklet_host_manager()
      .ConfigureShouldDeferWorkletMessagesOnWorkletHostCreation(true);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )",
                               EXECUTE_SCRIPT_NO_RESOLVE_PROMISES);

  // Navigate to trigger keep-alive
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(1u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  // The BrowserContext will be destroyed right after this test body, which will
  // cause the RenderProcessHost to be destroyed before the keep-alive
  // SharedStorageWorkletHost. Expect no fatal error.
}

// Test that there's no need to charge budget if the input urls' size is 1.
// This specifically tests the operation success scenario.
IN_PROC_BROWSER_TEST_F(
    SharedStorageBrowserTest,
    SelectURL_BudgetMetadata_OperationSuccess_SingleInputURL) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  std::string urn_uuid = EvalJs(shell(), R"(
      sharedStorage.selectURL(
          'test-url-selection-operation',
          [{url: "fenced_frames/title0.html",
          reportingMetadata: {"click": "fenced_frames/report1.html",
              "mouse interaction": "fenced_frames/report2.html"}}],
          {data: {'mockResult':0}});
    )")
                             .ExtractString();

  EXPECT_TRUE(blink::IsValidUrnUuidURL(GURL(urn_uuid)));

  // There are 2 "worklet operations": `addModule()` and `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponsesCount(2);

  FencedFrameURLMapping::SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(GURL(urn_uuid));
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->origin, https_server()->GetOrigin("a.test"));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, 0.0);

  EXPECT_THAT(GetSharedStorageReportingMap(GURL(urn_uuid)),
              UnorderedElementsAre(
                  Pair("click", https_server()->GetURL(
                                    "a.test", "/fenced_frames/report1.html")),
                  Pair("mouse interaction",
                       https_server()->GetURL("a.test",
                                              "/fenced_frames/report2.html"))));

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));
}

// Test that there's no need to charge budget if the input urls' size is 1.
// This specifically tests the operation failure scenario.
IN_PROC_BROWSER_TEST_F(
    SharedStorageBrowserTest,
    SelectURL_BudgetMetadata_OperationFailure_SingleInputURL) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  std::string urn_uuid = EvalJs(shell(), R"(
      sharedStorage.selectURL(
          'test-url-selection-operation',
          [{url: "fenced_frames/title0.html",
          reportingMetadata: {"click": "fenced_frames/report1.html"}}],
          {data: {'mockResult':-1}});
    )")
                             .ExtractString();

  EXPECT_TRUE(blink::IsValidUrnUuidURL(GURL(urn_uuid)));

  // There are 2 "worklet operations": `addModule()` and `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponsesCount(2);

  FencedFrameURLMapping::SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(GURL(urn_uuid));
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->origin, https_server()->GetOrigin("a.test"));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, 0.0);

  EXPECT_THAT(GetSharedStorageReportingMap(GURL(urn_uuid)),
              UnorderedElementsAre(
                  Pair("click", https_server()->GetURL(
                                    "a.test", "/fenced_frames/report1.html"))));

  EXPECT_EQ(
      "Promise resolved to a number outside the length of the input urls.",
      base::UTF16ToUTF8(console_observer.messages().back().message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       SelectURL_BudgetMetadata_Origin) {
  EXPECT_TRUE(NavigateToURL(
      shell(), https_server()->GetURL("a.test", kPageWithBlankIframePath)));

  GURL iframe_url = https_server()->GetURL("b.test", kSimplePagePath);
  NavigateIframeToURL(shell()->web_contents(), "test_iframe", iframe_url);

  RenderFrameHost* iframe =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryFrameTree()
          .root()
          ->child_at(0)
          ->current_frame_host();

  EXPECT_TRUE(ExecJs(iframe, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  std::string urn_uuid = EvalJs(iframe, R"(
      sharedStorage.selectURL(
          'test-url-selection-operation',
          [{url: "fenced_frames/title0.html"},
          {url: "fenced_frames/title1.html",
          reportingMetadata: {"click": "fenced_frames/report1.html"}},
          {url: "fenced_frames/title2.html"}], {data: {'mockResult': 1}});
    )")
                             .ExtractString();

  EXPECT_TRUE(blink::IsValidUrnUuidURL(GURL(urn_uuid)));

  // There are 2 "worklet operations": `addModule()` and `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponsesCount(2);

  FencedFrameURLMapping::SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(GURL(urn_uuid));
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->origin, https_server()->GetOrigin("b.test"));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, std::log2(3));

  SharedStorageReportingMap reporting_map =
      GetSharedStorageReportingMap(GURL(urn_uuid));
  EXPECT_FALSE(reporting_map.empty());
  EXPECT_EQ(1U, reporting_map.size());
  EXPECT_EQ("click", reporting_map.begin()->first);
  EXPECT_EQ(https_server()->GetURL("b.test", "/fenced_frames/report1.html"),
            reporting_map.begin()->second);
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       SelectURL_ReportingMetadata_EmptyReportEvent) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  std::string urn_uuid = EvalJs(shell(), R"(
      sharedStorage.selectURL(
          'test-url-selection-operation',
          [{url: "fenced_frames/title0.html",
          reportingMetadata: {"": "fenced_frames/report1.html"}}],
          {data: {'mockResult':0}});
    )")
                             .ExtractString();

  EXPECT_TRUE(blink::IsValidUrnUuidURL(GURL(urn_uuid)));

  // There are 2 "worklet operations": `addModule()` and `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponsesCount(2);

  FencedFrameURLMapping::SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(GURL(urn_uuid));
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->origin, https_server()->GetOrigin("a.test"));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, 0.0);

  EXPECT_THAT(GetSharedStorageReportingMap(GURL(urn_uuid)),
              UnorderedElementsAre(
                  Pair("", https_server()->GetURL(
                               "a.test", "/fenced_frames/report1.html"))));

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, SetAppendOperationInDocument) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.set('key0', 'value0');

      sharedStorage.set('key1', 'value1');
      sharedStorage.set('key1', 'value111');

      sharedStorage.set('key2', 'value2');
      sharedStorage.set('key2', 'value222', {ignoreIfPresent: true});

      sharedStorage.set('key3', 'value3');
      sharedStorage.append('key3', 'value333');
    )"));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('key0'));
      console.log(await sharedStorage.get('key1'));
      console.log(await sharedStorage.get('key2'));
      console.log(await sharedStorage.get('key3'));
      console.log(await sharedStorage.length());
    )");

  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("value0",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("value111",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("value2",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("value3value333",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("4", base::UTF16ToUTF8(console_observer.messages()[4].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, DeleteOperationInDocument) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.set('key0', 'value0');
      sharedStorage.delete('key0');
    )"));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.length());
      console.log(await sharedStorage.get('key0'));
    )");

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[0].log_level);
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[1].log_level);
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, ClearOperationInDocument) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.set('key0', 'value0');
      sharedStorage.clear();
    )"));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.length());
    )");

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[0].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, SetAppendOperationInWorklet) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  ExecuteScriptInWorklet(shell(), R"(
      sharedStorage.set('key0', 'value0');

      sharedStorage.set('key1', 'value1');
      sharedStorage.set('key1', 'value111');

      sharedStorage.set('key2', 'value2');
      sharedStorage.set('key2', 'value222', {ignoreIfPresent: true});

      sharedStorage.set('key3', 'value3');
      sharedStorage.append('key3', 'value333');

      console.log(await sharedStorage.get('key0'));
      console.log(await sharedStorage.get('key1'));
      console.log(await sharedStorage.get('key2'));
      console.log(await sharedStorage.get('key3'));
      console.log(await sharedStorage.length());
    )");

  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("value0",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("value111",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("value2",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("value3value333",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("4", base::UTF16ToUTF8(console_observer.messages()[4].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       AppendOperationFailedInWorklet) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  ExecuteScriptInWorklet(shell(), R"(
      await sharedStorage.set('key0', 'a'.repeat(1024));

      // This will fail due to the would-be length being too big.
      await sharedStorage.append('key0', 'a');
    )");

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("sharedStorage.append() failed",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[0].log_level);
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, DeleteOperationInWorklet) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  ExecuteScriptInWorklet(shell(), R"(
      sharedStorage.set('key0', 'value0');
      console.log(await sharedStorage.length());
      console.log(await sharedStorage.get('key0'));

      sharedStorage.delete('key0');

      console.log(await sharedStorage.length());
      console.log(await sharedStorage.get('key0'));
    )");

  EXPECT_EQ(4u, console_observer.messages().size());
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("value0",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[0].log_level);
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[1].log_level);
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[2].log_level);
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[3].log_level);
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, ClearOperationInWorklet) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  ExecuteScriptInWorklet(shell(), R"(
      sharedStorage.set('key0', 'value0');
      console.log(await sharedStorage.length());
      console.log(await sharedStorage.get('key0'));

      sharedStorage.clear();

      console.log(await sharedStorage.length());
    )");

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("value0",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[2].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       AccessStorageInSameOriginDocument) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.set('key0', 'value0');
    )"));

  EXPECT_TRUE(
      NavigateToURL(shell(), https_server()->GetURL("a.test", "/title1.html")));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.length());
    )");

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[0].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       AccessStorageInDifferentOriginDocument) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.set('key0', 'value0');
    )"));

  EXPECT_TRUE(
      NavigateToURL(shell(), https_server()->GetURL("b.test", "/title1.html")));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.length());
    )");

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[0].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, KeysAndEntriesOperation) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.set('key0', 'value0');
      sharedStorage.set('key1', 'value1');
      sharedStorage.set('key2', 'value2');
    )"));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  ExecuteScriptInWorklet(shell(), R"(
      for await (const key of sharedStorage.keys()) {
        console.log(key);
      }
      for await (const [key, value] of sharedStorage.entries()) {
        console.log(key + ';' + value);
      }
    )");

  EXPECT_EQ(6u, console_observer.messages().size());
  EXPECT_EQ("key0", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("key1", base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("key2", base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("key0;value0",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("key1;value1",
            base::UTF16ToUTF8(console_observer.messages()[4].message));
  EXPECT_EQ("key2;value2",
            base::UTF16ToUTF8(console_observer.messages()[5].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       KeysAndEntriesOperation_MultipleBatches) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  EXPECT_TRUE(ExecJs(shell(), R"(
      for (let i = 0; i < 150; ++i) {
        sharedStorage.set('key' + i.toString().padStart(3, '0'),
                          'value' + i.toString().padStart(3, '0'));
      }
    )"));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  ExecuteScriptInWorklet(shell(), R"(
      for await (const key of sharedStorage.keys()) {
        console.log(key);
      }
      for await (const [key, value] of sharedStorage.entries()) {
        console.log(key + ';' + value);
      }
    )");

  EXPECT_EQ(300u, console_observer.messages().size());
  for (int i = 0; i < 150; ++i) {
    std::string zero_padded_i = base::NumberToString(i);
    zero_padded_i.insert(zero_padded_i.begin(), 3 - zero_padded_i.size(), '0');

    EXPECT_EQ(base::StrCat({"key", zero_padded_i}),
              base::UTF16ToUTF8(console_observer.messages()[i].message));
    EXPECT_EQ(base::StrCat({"key", zero_padded_i, ";value", zero_padded_i}),
              base::UTF16ToUTF8(console_observer.messages()[i + 150].message));
  }
}

class SharedStorageFencedFrameInteractionBrowserTest
    : public SharedStorageBrowserTest,
      public ::testing::WithParamInterface<
          blink::features::FencedFramesImplementationType> {
 public:
  // Provides meaningful param names instead of /0 and /1.
  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    switch (info.param) {
      case blink::features::FencedFramesImplementationType::kShadowDOM:
        return "ShadowDOM";
      case blink::features::FencedFramesImplementationType::kMPArch:
        return "MPArch";
    }
  }

  SharedStorageFencedFrameInteractionBrowserTest() {
    scoped_feature_list_
        .InitWithFeaturesAndParameters(/*enabled_features=*/
                                       {{blink::features::kFencedFrames,
                                         {{"implementation_type",
                                           GetParam() ==
                                                   blink::features::
                                                       FencedFramesImplementationType::
                                                           kShadowDOM
                                               ? "shadow_dom"
                                               : "mparch"}}}},
                                       /*disabled_features=*/{});
  }

  FrameTreeNode* CreateFencedFrame(FrameTreeNode* root, const GURL& url) {
    size_t initial_child_count = root->child_count();

    EXPECT_TRUE(ExecJs(root,
                       "var f = document.createElement('fencedframe');"
                       "f.mode = 'opaque-ads';"
                       "document.body.appendChild(f);"));

    EXPECT_EQ(initial_child_count + 1, root->child_count());
    FrameTreeNode* fenced_frame_root_node =
        GetFencedFrameRootNode(root->child_at(initial_child_count));

    std::string navigate_fenced_frame_script =
        JsReplace("f.src = $1;", url.spec());

    TestFrameNavigationObserver observer(
        fenced_frame_root_node->current_frame_host());

    EXPECT_EQ(url.spec(), EvalJs(root, navigate_fenced_frame_script));

    observer.Wait();

    return fenced_frame_root_node;
  }

  FrameTreeNode* CreateFencedFrame(const GURL& url) {
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();

    return CreateFencedFrame(root, url);
  }

  FrameTreeNode* CreateIFrame(FrameTreeNode* root, const GURL& url) {
    size_t initial_child_count = root->child_count();

    EXPECT_TRUE(ExecJs(root,
                       "var f = document.createElement('iframe');"
                       "document.body.appendChild(f);"));

    EXPECT_EQ(initial_child_count + 1, root->child_count());
    FrameTreeNode* child_node = root->child_at(initial_child_count);

    std::string navigate_frame_script = JsReplace("f.src = $1;", url.spec());

    TestFrameNavigationObserver observer(child_node->current_frame_host());

    EXPECT_EQ(url.spec(), EvalJs(root, navigate_frame_script));

    observer.Wait();

    return child_node;
  }

  // Create an iframe and run sharedStorage.selectURL() on 8 urls. This
  // generates an URN associated with `origin` and 3 bits of shared storage
  // budget. This can be called at most once per origin per test, because
  // `GetAttachedWorkletHostForOrigin()` will expect only one worklet host for
  // this origin, and `WaitForWorkletResponsesCount()` is expected to be invoked
  // once per worklet host.
  GURL SelectFrom8URLsInContext(const url::Origin& origin) {
    FrameTreeNode* iframe =
        CreateIFrame(static_cast<WebContentsImpl*>(shell()->web_contents())
                         ->GetPrimaryFrameTree()
                         .root(),
                     origin.GetURL());

    EXPECT_TRUE(ExecJs(iframe, R"(
        sharedStorage.worklet.addModule('shared_storage/simple_module.js');
      )"));

    std::string urn_uuid =
        EvalJs(iframe, kSelectFrom8URLsScript).ExtractString();

    // There are 2 "worklet operations": `addModule()` and `run()`.
    test_worklet_host_manager()
        .GetAttachedWorkletHostForOrigin(origin)
        ->WaitForWorkletResponsesCount(2);

    return GURL(urn_uuid);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURL_FinishBeforeStartingFencedFrameNavigation) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());
  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));

  std::string urn_uuid = EvalJs(shell(), R"(
      sharedStorage.selectURL(
          'test-url-selection-operation',
          [{url: "fenced_frames/title0.html"},
          {url: "fenced_frames/title1.html",
          reportingMetadata: {"click": "fenced_frames/report1.html"}},
          {url: "fenced_frames/title2.html"}], {data: {'mockResult': 1}});
    )")
                             .ExtractString();

  EXPECT_TRUE(blink::IsValidUrnUuidURL(GURL(urn_uuid)));

  // There are 2 "worklet operations": `addModule()` and `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponsesCount(2);

  FencedFrameURLMapping::SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(GURL(urn_uuid));
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->origin, https_server()->GetOrigin("a.test"));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, std::log2(3));

  EXPECT_THAT(GetSharedStorageReportingMap(GURL(urn_uuid)),
              UnorderedElementsAre(
                  Pair("click", https_server()->GetURL(
                                    "a.test", "/fenced_frames/report1.html"))));

  GURL url0 = https_server()->GetURL("a.test", "/fenced_frames/title0.html");
  GURL url1 = https_server()->GetURL("a.test", "/fenced_frames/title1.html");
  GURL url2 = https_server()->GetURL("a.test", "/fenced_frames/title2.html");

  EXPECT_EQ(6u, console_observer.messages().size());
  EXPECT_EQ("Start executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ(base::StrCat({"[\"", url0.spec(), "\",\"", url1.spec(), "\",\"",
                          url2.spec(), "\"]"}),
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("{\"mockResult\":1}",
            base::UTF16ToUTF8(console_observer.messages()[4].message));
  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages()[5].message));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "f.mode = 'opaque-ads';"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  std::string navigate_fenced_frame_to_urn_script =
      JsReplace("f.src = $1;", urn_uuid);

  TestFrameNavigationObserver observer(
      fenced_frame_root_node->current_frame_host());

  EXPECT_EQ(urn_uuid, EvalJs(root, navigate_fenced_frame_to_urn_script));

  observer.Wait();

  EXPECT_EQ(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());
}

// Currently, Shared Storage is not allowed in Fenced Frames as Fenced Frames
// disallow all permissions policies. This may change in the future.
// https://github.com/WICG/fenced-frame/issues/44
IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
                       SharedStorageNotAllowedInFencedFrame) {
  GURL main_frame_url = https_server()->GetURL("a.test", kSimplePagePath);

  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  GURL fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");

  FrameTreeNode* fenced_frame_node = CreateFencedFrame(fenced_frame_url);

  EvalJsResult result = EvalJs(fenced_frame_node, R"(
      sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
    )");

  EXPECT_THAT(
      result.error,
      testing::HasSubstr("The \"shared-storage\" Permissions Policy denied the "
                         "method on window.sharedStorage."));

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());
}

IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURL_FinishAfterStartingFencedFrameNavigation) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  // Configure the worklet host to defer processing the subsequent
  // `selectURL()` response.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->set_should_defer_worklet_messages(true);

  std::string urn_uuid = EvalJs(shell(), R"(
      sharedStorage.selectURL(
          'test-url-selection-operation',
          [{url: "fenced_frames/title0.html"},
          {url: "fenced_frames/title1.html",
          reportingMetadata: {"click": "fenced_frames/report1.html"}},
          {url: "fenced_frames/title2.html"}], {data: {'mockResult': 1}});
    )")
                             .ExtractString();

  EXPECT_TRUE(blink::IsValidUrnUuidURL(GURL(urn_uuid)));

  // There are 2 "worklet operations": `addModule()` and `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponsesCount(2);

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "f.mode = 'opaque-ads';"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  std::string navigate_fenced_frame_to_urn_script =
      JsReplace("f.src = $1;", urn_uuid);

  TestFrameNavigationObserver observer(
      fenced_frame_root_node->current_frame_host());

  EXPECT_EQ(urn_uuid, EvalJs(root, navigate_fenced_frame_to_urn_script));

  // After the previous EvalJs, the NavigationRequest should have been created,
  // but may not have begun. Wait for BeginNavigation() and expect it to be
  // deferred on fenced frame url mapping.
  NavigationRequest* request = fenced_frame_root_node->navigation_request();
  if (!request->is_deferred_on_fenced_frame_url_mapping_for_testing()) {
    base::RunLoop run_loop;
    request->set_begin_navigation_callback_for_testing(
        run_loop.QuitWhenIdleClosure());
    run_loop.Run();

    EXPECT_TRUE(request->is_deferred_on_fenced_frame_url_mapping_for_testing());
  }

  FencedFrameURLMapping& url_mapping =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();

  EXPECT_TRUE(url_mapping.HasObserverForTesting(GURL(urn_uuid), request));

  // Execute the deferred messages. This should finish the url mapping and
  // resume the deferred navigation.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->ExecutePendingWorkletMessages();

  observer.Wait();

  FencedFrameURLMapping::SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(GURL(urn_uuid));
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->origin, https_server()->GetOrigin("a.test"));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, std::log2(3));

  EXPECT_THAT(GetSharedStorageReportingMap(GURL(urn_uuid)),
              UnorderedElementsAre(
                  Pair("click", https_server()->GetURL(
                                    "a.test", "/fenced_frames/report1.html"))));

  EXPECT_EQ(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());
}

// Tests that the URN from SelectURL() is valid in different
// context in the page, but it's not valid in a new page.
IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURL_URNLifetime) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  GURL urn_uuid = SelectFrom8URLsInContext(url::Origin::Create(main_url));
  EXPECT_TRUE(blink::IsValidUrnUuidURL(GURL(urn_uuid)));

  FrameTreeNode* iframe_node = root->child_at(0);

  // Navigate the iframe to about:blank.
  TestFrameNavigationObserver observer(iframe_node->current_frame_host());
  EXPECT_TRUE(ExecJs(iframe_node, JsReplace("window.location.href=$1",
                                            GURL(url::kAboutBlankURL).spec())));
  observer.Wait();

  // Verify that the `urn_uuid` is still valid in the main page.
  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(urn_uuid);
  EXPECT_EQ(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());

  // Navigate to a new page. Verify that the `urn_uuid` is not valid in this
  // new page.
  GURL new_page_main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), new_page_main_url));

  fenced_frame_root_node = CreateFencedFrame(urn_uuid);
  EXPECT_NE(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());
}

// Tests that if the URN mapping is not finished before the keep-alive timeout,
// the mapping will be considered to be failed when the timeout is reached.
IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURL_NotFinishBeforeKeepAliveTimeout) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  EXPECT_TRUE(NavigateToURL(
      shell(), https_server()->GetURL("a.test", kPageWithBlankIframePath)));

  RenderFrameHost* iframe =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryFrameTree()
          .root()
          ->child_at(0)
          ->current_frame_host();

  EXPECT_TRUE(ExecJs(iframe, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  // Configure the worklet host to defer processing the subsequent
  // `selectURL()` response.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->set_should_defer_worklet_messages(true);

  std::string urn_uuid = EvalJs(iframe, kSelectFrom8URLsScript).ExtractString();

  // Navigate away to let the subframe's worklet enter keep-alive.
  NavigateIframeToURL(shell()->web_contents(), "test_iframe",
                      GURL(url::kAboutBlankURL));

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(1u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  // There are 2 "worklet operations": `addModule()` and `selectURL()`.
  test_worklet_host_manager()
      .GetKeepAliveWorkletHost()
      ->WaitForWorkletResponsesCount(2);

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "f.mode = 'opaque-ads';"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(2U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(1));

  std::string navigate_fenced_frame_to_urn_script =
      JsReplace("f.src = $1;", urn_uuid);

  TestFrameNavigationObserver observer(
      fenced_frame_root_node->current_frame_host());

  EXPECT_EQ(urn_uuid, EvalJs(root, navigate_fenced_frame_to_urn_script));

  // After the previous EvalJs, the NavigationRequest should have been created,
  // but may not have begun. Wait for BeginNavigation() and expect it to be
  // deferred on fenced frame url mapping.
  NavigationRequest* request = fenced_frame_root_node->navigation_request();
  if (!request->is_deferred_on_fenced_frame_url_mapping_for_testing()) {
    base::RunLoop run_loop;
    request->set_begin_navigation_callback_for_testing(
        run_loop.QuitWhenIdleClosure());
    run_loop.Run();

    EXPECT_TRUE(request->is_deferred_on_fenced_frame_url_mapping_for_testing());
  }

  // Fire the keep-alive timer. This will terminate the keep-alive, and the
  // deferred navigation will resume to navigate to the default url (at index
  // 0).
  test_worklet_host_manager()
      .GetKeepAliveWorkletHost()
      ->FireKeepAliveTimerNow();

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  observer.Wait();

  FencedFrameURLMapping::SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(GURL(urn_uuid));
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->origin, https_server()->GetOrigin("a.test"));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, 0.0);

  EXPECT_THAT(GetSharedStorageReportingMap(GURL(urn_uuid)),
              UnorderedElementsAre(
                  Pair("click", https_server()->GetURL(
                                    "a.test", "/fenced_frames/report0.html"))));

  EXPECT_EQ(
      https_server()->GetURL("a.test", "/fenced_frames/title0.html"),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURL_WorkletReturnInvalidIndex) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  std::string urn_uuid = EvalJs(shell(), R"(
      sharedStorage.selectURL(
          'test-url-selection-operation',
          [{url: "fenced_frames/title0.html"},
          {url: "fenced_frames/title1.html",
          reportingMetadata: {"click": "fenced_frames/report1.html"}},
          {url: "fenced_frames/title2.html"}], {data: {'mockResult': 3}});
    )")
                             .ExtractString();

  EXPECT_TRUE(blink::IsValidUrnUuidURL(GURL(urn_uuid)));

  // There are 2 "worklet operations": `addModule()` and `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponsesCount(2);

  EXPECT_EQ(
      "Promise resolved to a number outside the length of the input urls.",
      base::UTF16ToUTF8(console_observer.messages().back().message));

  FencedFrameURLMapping::SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(GURL(urn_uuid));
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->origin, https_server()->GetOrigin("a.test"));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, 1.0);

  EXPECT_TRUE(GetSharedStorageReportingMap(GURL(urn_uuid)).empty());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "f.mode = 'opaque-ads';"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  std::string navigate_fenced_frame_to_urn_script =
      JsReplace("f.src = $1;", urn_uuid);

  TestFrameNavigationObserver observer(
      fenced_frame_root_node->current_frame_host());

  EXPECT_EQ(urn_uuid, EvalJs(root, navigate_fenced_frame_to_urn_script));

  observer.Wait();

  EXPECT_EQ(
      https_server()->GetURL("a.test", "/fenced_frames/title0.html"),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURL_DuplicateUrl) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  std::string urn_uuid = EvalJs(shell(), R"(
      sharedStorage.selectURL(
          'test-url-selection-operation',
          [{url: "fenced_frames/title.html"},
          {url: "fenced_frames/title0.html",
          url: "fenced_frames/title1.html",
          reportingMetadata: {"click": "fenced_frames/report1.html"}},
          {url: "fenced_frames/title2.html"}], {data: {'mockResult': 1}});
    )")
                             .ExtractString();

  EXPECT_TRUE(blink::IsValidUrnUuidURL(GURL(urn_uuid)));

  // There are 2 "worklet operations": `addModule()` and `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponsesCount(2);

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  FencedFrameURLMapping::SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(GURL(urn_uuid));
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->origin, https_server()->GetOrigin("a.test"));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, std::log2(3));

  EXPECT_THAT(GetSharedStorageReportingMap(GURL(urn_uuid)),
              UnorderedElementsAre(
                  Pair("click", https_server()->GetURL(
                                    "a.test", "/fenced_frames/report1.html"))));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "f.mode = 'opaque-ads';"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  std::string navigate_fenced_frame_to_urn_script =
      JsReplace("f.src = $1;", urn_uuid);

  TestFrameNavigationObserver observer(
      fenced_frame_root_node->current_frame_host());

  EXPECT_EQ(urn_uuid, EvalJs(root, navigate_fenced_frame_to_urn_script));

  observer.Wait();

  EXPECT_EQ(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
                       FencedFrameNavigateSelf_NoBudgetWithdrawal) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  GURL urn_uuid = SelectFrom8URLsInContext(shared_storage_origin);

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(urn_uuid);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);

  TestFrameNavigationObserver observer(
      fenced_frame_root_node->current_frame_host());
  EXPECT_TRUE(ExecJs(fenced_frame_root_node, "location.reload()"));
  observer.Wait();

  // No budget withdrawal as the fenced frame did not initiate a top navigation.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
}

IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
                       FencedFrameNavigateTop_BudgetWithdrawal) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  GURL urn_uuid = SelectFrom8URLsInContext(shared_storage_origin);

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(urn_uuid);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);

  GURL new_page_url = https_server()->GetURL("c.test", kSimplePagePath);

  TestNavigationObserver top_navigation_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(
      fenced_frame_root_node,
      JsReplace("window.open($1, '_unfencedTop')", new_page_url.spec())));
  top_navigation_observer.Wait();

  // After the top navigation, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageFencedFrameInteractionBrowserTest,
    FencedFrameNavigateFromParentToRegularURLAndThenNavigateTop_NoBudgetWithdrawal) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  GURL urn_uuid = SelectFrom8URLsInContext(shared_storage_origin);

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(urn_uuid);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);

  GURL new_frame_url = https_server()->GetURL("c.test", kFencedFramePath);

  TestFrameNavigationObserver observer(
      fenced_frame_root_node->current_frame_host());
  std::string navigate_fenced_frame_script = JsReplace(
      "var f = document.getElementsByTagName('fencedframe')[0]; f.src = $1;",
      new_frame_url.spec());

  EXPECT_TRUE(ExecJs(shell(), navigate_fenced_frame_script));
  observer.Wait();

  GURL new_page_url = https_server()->GetURL("d.test", kSimplePagePath);

  TestNavigationObserver top_navigation_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(
      fenced_frame_root_node,
      JsReplace("window.open($1, '_unfencedTop')", new_page_url.spec())));
  top_navigation_observer.Wait();

  // No budget withdrawal as the initial fenced frame was navigated away by its
  // parent before it triggers a top navigation.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(GetRemainingBudget(url::Origin::Create(new_frame_url)),
                   kBudgetAllowed);
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageFencedFrameInteractionBrowserTest,
    FencedFrameNavigateSelfAndThenNavigateTop_BudgetWithdrawal) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  GURL urn_uuid = SelectFrom8URLsInContext(shared_storage_origin);

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(urn_uuid);

  {
    GURL new_frame_url = https_server()->GetURL("c.test", kFencedFramePath);

    TestFrameNavigationObserver observer(
        fenced_frame_root_node->current_frame_host());
    EXPECT_TRUE(
        ExecJs(fenced_frame_root_node,
               JsReplace("window.location.href=$1", new_frame_url.spec())));
    observer.Wait();
  }

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);

  {
    GURL new_page_url = https_server()->GetURL("d.test", kSimplePagePath);

    TestNavigationObserver top_navigation_observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(
        fenced_frame_root_node,
        JsReplace("window.open($1, '_unfencedTop')", new_page_url.spec())));
    top_navigation_observer.Wait();
  }

  // After the top navigation, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);
}

IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
                       NestedFencedFrameNavigateTop_BudgetWithdrawal) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  GURL urn_uuid = SelectFrom8URLsInContext(shared_storage_origin);

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(urn_uuid);

  GURL nested_fenced_frame_url =
      https_server()->GetURL("c.test", kFencedFramePath);
  FrameTreeNode* nested_fenced_frame_root_node =
      CreateFencedFrame(fenced_frame_root_node, nested_fenced_frame_url);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);

  GURL new_page_url = https_server()->GetURL("d.test", kSimplePagePath);
  TestNavigationObserver top_navigation_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(
      nested_fenced_frame_root_node,
      JsReplace("window.open($1, '_unfencedTop')", new_page_url.spec())));
  top_navigation_observer.Wait();

  // After the top navigation, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);
}

IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
                       IframeInFencedFrameNavigateTop_BudgetWithdrawal) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  GURL urn_uuid = SelectFrom8URLsInContext(shared_storage_origin);

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(urn_uuid);

  GURL nested_fenced_frame_url =
      https_server()->GetURL("c.test", kFencedFramePath);
  FrameTreeNode* nested_fenced_frame_root_node =
      CreateIFrame(fenced_frame_root_node, nested_fenced_frame_url);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);

  GURL new_page_url = https_server()->GetURL("d.test", kSimplePagePath);
  TestNavigationObserver top_navigation_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(
      nested_fenced_frame_root_node,
      JsReplace("window.open($1, '_unfencedTop')", new_page_url.spec())));
  top_navigation_observer.Wait();

  // After the top navigation, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);
}

IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
                       FencedFrame_PopupTwice_BudgetWithdrawalOnce) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  GURL urn_uuid = SelectFrom8URLsInContext(shared_storage_origin);

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(urn_uuid);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);

  OpenPopup(fenced_frame_root_node,
            https_server()->GetURL("c.test", kSimplePagePath), /*name=*/"");

  // After the popup, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);

  OpenPopup(fenced_frame_root_node,
            https_server()->GetURL("c.test", kSimplePagePath), /*name=*/"");

  // The budget can only be withdrawn once for each urn_uuid.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageFencedFrameInteractionBrowserTest,
    TwoFencedFrames_DifferentURNs_EachPopupOnce_BudgetWithdrawalTwice) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin = url::Origin::Create(main_url);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  GURL urn_uuid1 =
      GURL(EvalJs(shell(), kSelectFrom8URLsScript).ExtractString());
  GURL urn_uuid2 =
      GURL(EvalJs(shell(), kSelectFrom8URLsScript).ExtractString());

  // There are three "worklet operations": one `addModule()` and two
  // `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponsesCount(3);

  FrameTreeNode* fenced_frame_root_node1 = CreateFencedFrame(urn_uuid1);
  FrameTreeNode* fenced_frame_root_node2 = CreateFencedFrame(urn_uuid2);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);

  OpenPopup(fenced_frame_root_node1,
            https_server()->GetURL("b.test", kSimplePagePath), /*name=*/"");

  // After the popup, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);

  OpenPopup(fenced_frame_root_node2,
            https_server()->GetURL("b.test", kSimplePagePath), /*name=*/"");

  // After the popup, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin. The budget for `shared_storage_origin` can
  // be charged once for each distinct URN, and therefore here it gets charged
  // twice.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3 - 3);
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageFencedFrameInteractionBrowserTest,
    TwoFencedFrames_SameURNs_EachPopupOnce_BudgetWithdrawalOnce) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin = url::Origin::Create(main_url);

  GURL urn_uuid = SelectFrom8URLsInContext(shared_storage_origin);

  FrameTreeNode* fenced_frame_root_node1 = CreateFencedFrame(urn_uuid);
  FrameTreeNode* fenced_frame_root_node2 = CreateFencedFrame(urn_uuid);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);

  OpenPopup(fenced_frame_root_node1,
            https_server()->GetURL("b.test", kSimplePagePath), /*name=*/"");

  // After the popup, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);

  OpenPopup(fenced_frame_root_node2,
            https_server()->GetURL("b.test", kSimplePagePath), /*name=*/"");

  // The budget can only be withdrawn once for each urn_uuid.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);
}

IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURL_InsufficientBudget) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin = url::Origin::Create(main_url);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  GURL urn_uuid1 =
      GURL(EvalJs(shell(), kSelectFrom8URLsScript).ExtractString());

  FrameTreeNode* fenced_frame_root_node1 = CreateFencedFrame(urn_uuid1);
  OpenPopup(fenced_frame_root_node1,
            https_server()->GetURL("b.test", kSimplePagePath), /*name=*/"");

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);

  GURL urn_uuid2 =
      GURL(EvalJs(shell(), kSelectFrom8URLsScript).ExtractString());

  // Wait for the `addModule()` and two `selectURL()` to finish.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponsesCount(3);

  EXPECT_EQ("Insufficient budget for selectURL().",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  // The failed mapping due to insufficient budget (i.e. `urn_uuid2`) should not
  // incur any budget withdrawal on subsequent top navigation from inside
  // the fenced frame.
  FrameTreeNode* fenced_frame_root_node2 = CreateFencedFrame(urn_uuid2);
  OpenPopup(fenced_frame_root_node2,
            https_server()->GetURL("c.test", kSimplePagePath), /*name=*/"");

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SharedStorageFencedFrameInteractionBrowserTest,
    ::testing::Values(
        blink::features::FencedFramesImplementationType::kShadowDOM,
        blink::features::FencedFramesImplementationType::kMPArch),
    &SharedStorageFencedFrameInteractionBrowserTest::DescribeParams);

class SharedStorageReportEventBrowserTest
    : public SharedStorageFencedFrameInteractionBrowserTest {
  void FinishSetup() override {
    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  }
};

IN_PROC_BROWSER_TEST_P(SharedStorageReportEventBrowserTest,
                       SelectURL_ReportEvent) {
  net::test_server::ControllableHttpResponse response1(
      https_server(), "/fenced_frames/report1.html");
  net::test_server::ControllableHttpResponse response2(
      https_server(), "/fenced_frames/report2.html");
  ASSERT_TRUE(https_server()->Start());

  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin = url::Origin::Create(main_url);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  GURL urn_uuid = GURL(EvalJs(shell(), R"(
      sharedStorage.selectURL(
          'test-url-selection-operation',
          [{url: "fenced_frames/title0.html"},
          {url: "fenced_frames/title1.html",
          reportingMetadata: {'click': "fenced_frames/report1.html",
              'mouse interaction': "fenced_frames/report2.html"}}],
          {data: {'mockResult':1}});
    )")
                           .ExtractString());

  // There are three "worklet operations": one `addModule()` and two
  // `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponsesCount(2);

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(urn_uuid);

  std::string event_data1 = "this is a click";
  EXPECT_TRUE(
      ExecJs(fenced_frame_root_node,
             JsReplace("window.fence.reportEvent({"
                       "  eventType: 'click',"
                       "  eventData: $1,"
                       "  destination: ['shared-storage-select-url']});",
                       event_data1)));

  response1.WaitForRequest();
  EXPECT_EQ(response1.http_request()->content, event_data1);

  std::string event_data2 = "this is a mouse interaction";
  EXPECT_TRUE(
      ExecJs(fenced_frame_root_node,
             JsReplace("window.fence.reportEvent({"
                       "  eventType: 'mouse interaction',"
                       "  eventData: $1,"
                       "  destination: ['shared-storage-select-url']});",
                       event_data2)));

  response2.WaitForRequest();
  EXPECT_EQ(response2.http_request()->content, event_data2);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SharedStorageReportEventBrowserTest,
    ::testing::Values(
        blink::features::FencedFramesImplementationType::kShadowDOM,
        blink::features::FencedFramesImplementationType::kMPArch),
    &SharedStorageReportEventBrowserTest::DescribeParams);

}  // namespace content

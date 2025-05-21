// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_BROWSERTEST_BASE_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_BROWSERTEST_BASE_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/fenced_frame/fenced_frame_config.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/private_aggregation/private_aggregation_test_utils.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/shared_storage/shared_storage_runtime_manager.h"
#include "content/browser/shared_storage/test_shared_storage_observer.h"
#include "content/browser/shared_storage/test_shared_storage_runtime_manager.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"  // For `ToRenderFrameHost`
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class SharedStorageBrowserTestBase : public ContentBrowserTest {
 public:
  using MockPrivateAggregationShellContentBrowserClient =
      MockPrivateAggregationContentBrowserClientBase<
          ContentBrowserTestContentBrowserClient>;

  SharedStorageBrowserTestBase();

  void SetUpOnMainThread() override;

  MockPrivateAggregationShellContentBrowserClient& browser_client() {
    return *browser_client_;
  }

  virtual bool ResolveSelectURLToConfig();

  StoragePartition* GetStoragePartition();

  void TearDownOnMainThread() override;

  // Virtual so that derived classes can use a different flavor of mock instead
  // of `testing::NiceMock`.
  virtual void MakeMockPrivateAggregationShellContentBrowserClient();

  // Virtual so that derived classes can delay starting the server, and/or add
  // other set up steps.
  virtual void FinishSetup();

  // Virtual so that derived classes can register custom request handlers.
  virtual void RegisterCustomRequestHandlers();

  void ExpectAccessObserved(
      const std::vector<TestSharedStorageObserver::Access>& expected_accesses);

  void ExpectOperationFinishedInfosObserved(
      const std::vector<TestSharedStorageObserver::OperationFinishedInfo>&
          expected_infos);

  std::map<int, base::UnguessableToken>& GetCachedWorkletHostDevToolsTokens();

  // Precondition: At least one worklet host has been created.
  // Returns the DevTools token associated with the first-created worklet host
  // (which would have ordinal worklet ID 0).
  base::UnguessableToken GetFirstWorkletHostDevToolsToken();

  uint16_t port() { return https_server()->port(); }

  double GetRemainingBudget(const url::Origin& origin);

  FrameTreeNode* PrimaryFrameTreeNodeRoot();

  GlobalRenderFrameHostId MainFrameId();

  SharedStorageBudgetMetadata* GetSharedStorageBudgetMetadata(
      const GURL& urn_uuid);

  SharedStorageReportingMap GetSharedStorageReportingMap(const GURL& urn_uuid);

  void ExecuteScriptInWorklet(
      const ToRenderFrameHost& execution_target,
      std::string_view script,
      GURL* out_module_script_url,
      size_t expected_total_host_count = 1u,
      bool keep_alive_after_operation = true,
      std::optional<std::string> context_id = std::nullopt,
      std::optional<std::string> filtering_id_max_bytes = std::nullopt,
      std::optional<std::string> max_contributions = std::nullopt,
      std::string* out_error = nullptr,
      bool wait_for_operation_finish = true,
      bool use_add_module = true);

  void ExecuteScriptInWorkletUsingCreateWorklet(
      const ToRenderFrameHost& execution_target,
      const std::string& script,
      GURL* out_module_script_url,
      size_t expected_total_host_count = 1u,
      bool keep_alive_after_operation = true,
      std::optional<std::string> context_id = std::nullopt,
      std::optional<std::string> filtering_id_max_bytes = std::nullopt,
      std::optional<std::string> max_contributions = std::nullopt,
      std::string* out_error = nullptr,
      bool wait_for_operation_finish = true);

  FrameTreeNode* CreateIFrame(FrameTreeNode* root,
                              const GURL& url,
                              std::string sandbox_flags = "");

  // Create an iframe of origin `origin` inside `parent_node`, and run
  // sharedStorage.selectURL() on 8 urls. If `parent_node` is not specified,
  // the primary frame tree's root node will be chosen. This generates an URN
  // associated with `origin` and 3 bits of shared storage budget.
  std::optional<GURL> SelectFrom8URLsInContext(
      const url::Origin& origin,
      FrameTreeNode* parent_node = nullptr,
      bool keep_alive_after_operation = true);

  // Prerequisite: The worklet for `frame` has registered a
  // "remaining-budget-operation" that logs the remaining budget to the console
  // after `kRemainingBudgetPrefix`. Also, if any previous operations are
  // called, they use the option `keepAlive: true`.
  double RemainingBudgetViaJSForFrame(FrameTreeNode* frame,
                                      bool keep_alive_after_operation = true);

  double RemainingBudgetViaJSForOrigin(const url::Origin& origin);

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  TestSharedStorageRuntimeManager& test_runtime_manager();

  const std::vector<GURL>& urn_uuids_observed() const;

  ~SharedStorageBrowserTestBase() override;

 protected:
  static void WaitForHistogram(const std::string& histogram_name);
  static void WaitForHistograms(
      const std::vector<std::string>& histogram_names);
  static void WaitForHistogramWithCount(std::string_view histogram_name,
                                        int count);
  static void WaitForHistogramsWithCounts(
      const std::vector<std::tuple<std::string_view, int>>&
          histogram_names_and_counts);

  static constexpr double kBudgetAllowed = 5.0;
  static constexpr int kStalenessThresholdDays = 1;

  static constexpr char kRemainingBudgetPrefix[] = "remaining budget: ";

  static constexpr char kSimplePagePath[] = "/simple_page.html";
  static constexpr char kFencedFramePath[] = "/fenced_frames/title0.html";

  static constexpr char kGenerateURLsListScript[] = R"(
  function generateUrls(size) {
    return new Array(size).fill(0).map((e, i) => {
      return {
        url: '/fenced_frames/title' + i.toString() + '.html',
        reportingMetadata: {
          'click': '/fenced_frames/report' + i.toString() + '.html',
          'mouse interaction':
            '/fenced_frames/report' + (i + 1).toString() + '.html'
        }
      }
    });
  }
)";

  static constexpr char kTimingSelectUrlExecutedInWorkletHistogram[] =
      "Storage.SharedStorage.Document.Timing.SelectURL.ExecutedInWorklet";
  static constexpr char kSelectUrlBudgetStatusHistogram[] =
      "Storage.SharedStorage.Worklet.SelectURL.BudgetStatus";

  static constexpr char kPageWithBlankIframePath[] =
      "/page_with_blank_iframe.html";

  test::FencedFrameTestHelper fenced_frame_test_helper_;

  base::test::ScopedFeatureList privacy_sandbox_ads_apis_override_feature_;
  base::test::ScopedFeatureList shared_storage_feature_;
  base::test::ScopedFeatureList fenced_frame_feature_;

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  base::HistogramTester histogram_tester_;

  raw_ptr<TestSharedStorageRuntimeManager> test_runtime_manager_ = nullptr;
  std::unique_ptr<TestSharedStorageObserver> observer_;

  std::unique_ptr<MockPrivateAggregationShellContentBrowserClient>
      browser_client_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_BROWSERTEST_BASE_H_

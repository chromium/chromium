// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_browsertest_base.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/debug/crash_logging.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "content/browser/fenced_frame/fenced_frame_config.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/shared_storage/shared_storage_document_service_impl.h"
#include "content/browser/shared_storage/shared_storage_runtime_manager.h"
#include "content/browser/shared_storage/test_shared_storage_observer.h"
#include "content/browser/shared_storage/test_shared_storage_runtime_manager.h"
#include "content/browser/shared_storage/test_shared_storage_worklet_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_select_url_fenced_frame_config_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"  // For `ConvertToRenderFrameHost()`
#include "content/test/fenced_frame_test_utils.h"
#include "net/base/schemeful_site.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

std::string TimeDeltaToString(base::TimeDelta delta) {
  return base::StrCat({base::NumberToString(delta.InMilliseconds()), "ms"});
}

int GetSampleCountForHistogram(std::string_view histogram_name) {
  auto* histogram = base::StatisticsRecorder::FindHistogram(histogram_name);
  return histogram ? histogram->SnapshotSamples()->TotalCount() : 0;
}

}  // namespace

SharedStorageBrowserTestBase::SharedStorageBrowserTestBase() {
  privacy_sandbox_ads_apis_override_feature_.InitAndEnableFeature(
      ::features::kPrivacySandboxAdsAPIsOverride);

  shared_storage_feature_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{network::features::kSharedStorageAPI,
        {
            {"SharedStorageBitBudget", base::NumberToString(kBudgetAllowed)},
            {"SharedStorageStalenessThreshold",
             TimeDeltaToString(base::Days(kStalenessThresholdDays))},
        }}},
      /*disabled_features=*/{});

  fenced_frame_feature_.InitAndEnableFeature(blink::features::kFencedFrames);
}

void SharedStorageBrowserTestBase::SetUpOnMainThread() {
  auto test_runtime_manager = std::make_unique<TestSharedStorageRuntimeManager>(
      *static_cast<StoragePartitionImpl*>(GetStoragePartition()));
  observer_ = std::make_unique<TestSharedStorageObserver>();

  test_runtime_manager->AddSharedStorageObserver(observer_.get());
  test_runtime_manager_ = test_runtime_manager.get();

  static_cast<StoragePartitionImpl*>(GetStoragePartition())
      ->OverrideSharedStorageRuntimeManagerForTesting(
          std::move(test_runtime_manager));

  host_resolver()->AddRule("*", "127.0.0.1");

  MakeMockPrivateAggregationShellContentBrowserClient();

  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsPrivacySandboxReportingDestinationAttested)
      .WillByDefault(testing::Return(true));

  FinishSetup();
}

bool SharedStorageBrowserTestBase::ResolveSelectURLToConfig() {
  return false;
}

StoragePartition* SharedStorageBrowserTestBase::GetStoragePartition() {
  return shell()
      ->web_contents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition();
}

void SharedStorageBrowserTestBase::TearDownOnMainThread() {
  test_runtime_manager_ = nullptr;
}

// Virtual so that derived classes can use a different flavor of mock instead
// of `testing::NiceMock`.
void SharedStorageBrowserTestBase::
    MakeMockPrivateAggregationShellContentBrowserClient() {
  browser_client_ = std::make_unique<
      testing::NiceMock<MockPrivateAggregationShellContentBrowserClient>>();
}

// Virtual so that derived classes can delay starting the server, and/or add
// other set up steps.
void SharedStorageBrowserTestBase::FinishSetup() {
  https_server()->AddDefaultHandlers(GetTestDataFilePath());
  RegisterCustomRequestHandlers();
  https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  SetupCrossSiteRedirector(https_server());
  ASSERT_TRUE(https_server()->Start());
}

// Virtual so that derived classes can register custom request handlers.
void SharedStorageBrowserTestBase::RegisterCustomRequestHandlers() {}

void SharedStorageBrowserTestBase::ExpectAccessObserved(
    const std::vector<TestSharedStorageObserver::Access>& expected_accesses) {
  observer_->ExpectAccessObserved(expected_accesses);
}

void SharedStorageBrowserTestBase::ExpectOperationFinishedInfosObserved(
    const std::vector<TestSharedStorageObserver::OperationFinishedInfo>&
        expected_infos) {
  observer_->ExpectOperationFinishedInfosObserved(expected_infos);
}

double SharedStorageBrowserTestBase::GetRemainingBudget(
    const url::Origin& origin) {
  base::test::TestFuture<SharedStorageWorkletHost::BudgetResult> future;
  static_cast<StoragePartitionImpl*>(GetStoragePartition())
      ->GetSharedStorageManager()
      ->GetRemainingBudget(net::SchemefulSite(origin), future.GetCallback());
  return future.Take().bits;
}

FrameTreeNode* SharedStorageBrowserTestBase::PrimaryFrameTreeNodeRoot() {
  return static_cast<WebContentsImpl*>(shell()->web_contents())
      ->GetPrimaryFrameTree()
      .root();
}

GlobalRenderFrameHostId SharedStorageBrowserTestBase::MainFrameId() {
  return PrimaryFrameTreeNodeRoot()->current_frame_host()->GetGlobalId();
}

SharedStorageBudgetMetadata*
SharedStorageBrowserTestBase::GetSharedStorageBudgetMetadata(
    const GURL& urn_uuid) {
  FencedFrameURLMapping& fenced_frame_url_mapping =
      PrimaryFrameTreeNodeRoot()
          ->current_frame_host()
          ->GetPage()
          .fenced_frame_urls_map();

  SharedStorageBudgetMetadata* metadata =
      fenced_frame_url_mapping.GetSharedStorageBudgetMetadataForTesting(
          GURL(urn_uuid));

  return metadata;
}

SharedStorageReportingMap
SharedStorageBrowserTestBase::GetSharedStorageReportingMap(
    const GURL& urn_uuid) {
  FencedFrameURLMapping& fenced_frame_url_mapping =
      PrimaryFrameTreeNodeRoot()
          ->current_frame_host()
          ->GetPage()
          .fenced_frame_urls_map();
  FencedFrameURLMappingTestPeer fenced_frame_url_mapping_test_peer(
      &fenced_frame_url_mapping);

  SharedStorageReportingMap reporting_map;

  fenced_frame_url_mapping_test_peer.GetSharedStorageReportingMap(
      GURL(urn_uuid), &reporting_map);

  return reporting_map;
}

void SharedStorageBrowserTestBase::ExecuteScriptInWorklet(
    const ToRenderFrameHost& execution_target,
    std::string_view script,
    GURL* out_module_script_url,
    size_t expected_total_host_count,
    bool keep_alive_after_operation,
    std::optional<std::string> context_id,
    std::optional<std::string> filtering_id_max_bytes,
    std::optional<std::string> max_contributions,
    std::string* out_error,
    bool wait_for_operation_finish,
    bool use_add_module) {
  DCHECK(out_module_script_url);

  base::StringPairs run_function_body_replacement;
  run_function_body_replacement.emplace_back("{{RUN_FUNCTION_BODY}}", script);

  std::string host =
      execution_target.render_frame_host()->GetLastCommittedOrigin().host();

  *out_module_script_url = https_server()->GetURL(
      host, net::test_server::GetFilePathWithReplacements(
                "/shared_storage/customizable_module.js",
                run_function_body_replacement));

  std::string worklet_creation_script =
      use_add_module ? JsReplace("sharedStorage.worklet.addModule($1)",
                                 *out_module_script_url)
                     : JsReplace(R"(
      if (typeof window.testWorklets === 'undefined' ||
          !Array.isArray(window.testWorklets)) {
        window.testWorklets = [];
      }
      new Promise((resolve, reject) => {
        sharedStorage.createWorklet($1)
        .then((worklet) => {
          window.testWorklets.push(worklet);
          resolve();
        });
      })
    )",
                                 *out_module_script_url);

  EXPECT_TRUE(ExecJs(execution_target, worklet_creation_script));

  auto* worklet_host =
      test_runtime_manager().GetLastAttachedWorkletHostForFrameWithScriptSrc(
          execution_target.render_frame_host(), *out_module_script_url);
  ASSERT_TRUE(worklet_host);

  // There may be more than one host in the worklet host manager if we are
  // executing inside a nested fenced frame that was created using
  // `selectURL()`.
  EXPECT_EQ(expected_total_host_count,
            test_runtime_manager().GetAttachedWorkletHostsCount());

  EXPECT_EQ(0u, test_runtime_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_EQ(
      worklet_host->creation_method(),
      use_add_module
          ? blink::mojom::SharedStorageWorkletCreationMethod::kAddModule
          : blink::mojom::SharedStorageWorkletCreationMethod::kCreateWorklet);

  // There is 1 more "worklet operation": `run()`.
  worklet_host->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(execution_target, JsReplace("window.keepWorklet = $1;",
                                                 keep_alive_after_operation)));

  std::string private_aggregation_config_js = "";
  if (context_id.has_value() || filtering_id_max_bytes.has_value() ||
      max_contributions.has_value()) {
    private_aggregation_config_js = base::StrCat(
        {", privateAggregationConfig: {",
         context_id.has_value()
             ? JsReplace("contextId: $1,", context_id.value())
             : "",
         filtering_id_max_bytes.has_value()
             ? base::StrCat({"filteringIdMaxBytes: ",
                             filtering_id_max_bytes.value(), ","})
             : "",
         max_contributions.has_value()
             ? base::StrCat(
                   {"maxContributions: ", max_contributions.value(), ","})
             : "",
         "}"});
  }

  std::string run_operation_script =
      use_add_module
          ? base::StrCat(
                {"sharedStorage.run('test-operation', {keepAlive: keepWorklet",
                 private_aggregation_config_js, "});"})
          : base::StrCat({"window.testWorklets.at(-1).run('test-operation',",
                          " {keepAlive: keepWorklet",
                          private_aggregation_config_js, "});"});
  testing::AssertionResult result =
      ExecJs(execution_target, run_operation_script);
  EXPECT_EQ(!!result, out_error == nullptr);
  if (out_error) {
    *out_error = std::string(result.message());
    return;
  }
  if (wait_for_operation_finish) {
    CHECK(worklet_host);
    worklet_host->WaitForWorkletResponses();
  }
}

void SharedStorageBrowserTestBase::ExecuteScriptInWorkletUsingCreateWorklet(
    const ToRenderFrameHost& execution_target,
    const std::string& script,
    GURL* out_module_script_url,
    size_t expected_total_host_count,
    bool keep_alive_after_operation,
    std::optional<std::string> context_id,
    std::optional<std::string> filtering_id_max_bytes,
    std::optional<std::string> max_contributions,
    std::string* out_error,
    bool wait_for_operation_finish) {
  ExecuteScriptInWorklet(execution_target, script, out_module_script_url,
                         expected_total_host_count, keep_alive_after_operation,
                         context_id, filtering_id_max_bytes, max_contributions,
                         out_error, wait_for_operation_finish,
                         /*use_add_module=*/false);
}

FrameTreeNode* SharedStorageBrowserTestBase::CreateIFrame(
    FrameTreeNode* root,
    const GURL& url,
    std::string sandbox_flags) {
  size_t initial_child_count = root->child_count();

  EXPECT_TRUE(ExecJs(root, JsReplace(R"(
                          var f = document.createElement('iframe');
                          if ($1) {
                            f.sandbox = $1;
                          }
                          document.body.appendChild(f);
                        )",
                                     sandbox_flags)));

  EXPECT_EQ(initial_child_count + 1, root->child_count());
  FrameTreeNode* child_node = root->child_at(initial_child_count);

  TestFrameNavigationObserver observer(child_node);

  EXPECT_EQ(url.spec(), EvalJs(root, JsReplace("f.src = $1;", url)));

  observer.Wait();

  return child_node;
}

// Create an iframe of origin `origin` inside `parent_node`, and run
// sharedStorage.selectURL() on 8 urls. If `parent_node` is not specified,
// the primary frame tree's root node will be chosen. This generates an URN
// associated with `origin` and 3 bits of shared storage budget.
std::optional<GURL> SharedStorageBrowserTestBase::SelectFrom8URLsInContext(
    const url::Origin& origin,
    FrameTreeNode* parent_node,
    bool keep_alive_after_operation) {
  if (!parent_node) {
    parent_node = PrimaryFrameTreeNodeRoot();
  }

  // If this is called inside a fenced frame, creating an iframe will need
  // "Supports-Loading-Mode: fenced-frame" response header. Thus, we simply
  // always set the path to `kFencedFramePath`.
  GURL iframe_url = origin.GetURL().Resolve(kFencedFramePath);

  FrameTreeNode* iframe = CreateIFrame(parent_node, iframe_url);

  EXPECT_TRUE(ExecJs(iframe, JsReplace("window.keepWorklet = $1;",
                                       keep_alive_after_operation)));

  EXPECT_TRUE(ExecJs(iframe, R"(
        sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
      )"));

  // There is 1 more "worklet operation": `selectURL()`.
  test_runtime_manager()
      .GetAttachedWorkletHostForFrame(iframe->current_frame_host())
      ->SetExpectedWorkletResponsesCount(1);

  // Generate 8 candidates urls in to a list variable `urls`.
  EXPECT_TRUE(ExecJs(iframe, kGenerateURLsListScript));
  EXPECT_TRUE(ExecJs(iframe, JsReplace("window.resolveSelectURLToConfig = $1;",
                                       ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());
  EvalJsResult result = EvalJs(iframe, R"(
        (async function() {
          const urls = generateUrls(8);
          window.select_url_result = await sharedStorage.selectURL(
            'test-url-selection-operation',
            urls,
            {
              data: {'mockResult': 1},
              resolveToConfig: resolveSelectURLToConfig,
              keepAlive: keepWorklet
            }
          );
          if (resolveSelectURLToConfig &&
              !(select_url_result instanceof FencedFrameConfig)) {
            throw new Error('selectURL() did not return a FencedFrameConfig.');
          }
          return window.select_url_result;
        })()
      )");

  EXPECT_TRUE(result.is_ok());
  const std::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  if (observed_urn_uuid.has_value()) {
    EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

    if (!ResolveSelectURLToConfig()) {
      EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
    }

    test_runtime_manager()
        .GetAttachedWorkletHostForFrame(iframe->current_frame_host())
        ->WaitForWorkletResponses();
  }
  return observed_urn_uuid;
}

// Prerequisite: The worklet for `frame` has registered a
// "remaining-budget-operation" that logs the remaining budget to the console
// after `kRemainingBudgetPrefix`. Also, if any previous operations are
// called, they use the option `keepAlive: true`.
double SharedStorageBrowserTestBase::RemainingBudgetViaJSForFrame(
    FrameTreeNode* frame,
    bool keep_alive_after_operation) {
  DCHECK(frame);

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  const std::string kRemainingBudgetPrefixStr(kRemainingBudgetPrefix);
  console_observer.SetPattern(base::StrCat({kRemainingBudgetPrefixStr, "*"}));
  EXPECT_TRUE(ExecJs(frame, JsReplace("window.keepWorklet = $1;",
                                      keep_alive_after_operation)));

  // There is 1 "worklet operation": `run()`.
  test_runtime_manager()
      .GetAttachedWorkletHostForFrame(frame->current_frame_host())
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(frame, R"(
      sharedStorage.run('remaining-budget-operation',
                        {
                          data: {},
                          keepAlive: keepWorklet
                        }
      );
    )"));

  bool observed = console_observer.Wait();
  EXPECT_TRUE(observed);
  if (!observed) {
    return nan("");
  }

  EXPECT_LE(1u, console_observer.messages().size());
  std::string console_message =
      base::UTF16ToUTF8(console_observer.messages().back().message);
  EXPECT_TRUE(base::StartsWith(console_message, kRemainingBudgetPrefixStr));

  std::string result_string = console_message.substr(
      kRemainingBudgetPrefixStr.size(),
      console_message.size() - kRemainingBudgetPrefixStr.size());

  double result = 0.0;
  EXPECT_TRUE(base::StringToDouble(result_string, &result));

  test_runtime_manager()
      .GetAttachedWorkletHostForFrame(frame->current_frame_host())
      ->WaitForWorkletResponses();
  return result;
}

double SharedStorageBrowserTestBase::RemainingBudgetViaJSForOrigin(
    const url::Origin& origin) {
  FrameTreeNode* iframe =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), origin.GetURL());

  EXPECT_TRUE(ExecJs(iframe, R"(
        sharedStorage.worklet.addModule('shared_storage/simple_module.js');
      )"));

  return RemainingBudgetViaJSForFrame(iframe);
}

TestSharedStorageRuntimeManager&
SharedStorageBrowserTestBase::test_runtime_manager() {
  DCHECK(test_runtime_manager_);
  return *test_runtime_manager_;
}

const std::vector<GURL>& SharedStorageBrowserTestBase::urn_uuids_observed()
    const {
  DCHECK(observer_);
  return observer_->urn_uuids_observed();
}

SharedStorageBrowserTestBase::~SharedStorageBrowserTestBase() = default;

// static
void SharedStorageBrowserTestBase::WaitForHistogram(
    const std::string& histogram_name) {
  // Continue if histogram was already recorded.
  if (base::StatisticsRecorder::FindHistogram(histogram_name)) {
    return;
  }

  // Else, wait until the histogram is recorded.
  base::RunLoop run_loop;
  auto histogram_observer =
      std::make_unique<base::StatisticsRecorder::ScopedHistogramSampleObserver>(
          histogram_name,
          base::BindLambdaForTesting(
              [&](std::string_view histogram_name, uint64_t name_hash,
                  base::HistogramBase::Sample32 sample) { run_loop.Quit(); }));
  run_loop.Run();
}

// static
void SharedStorageBrowserTestBase::WaitForHistograms(
    const std::vector<std::string>& histogram_names) {
  for (const auto& name : histogram_names) {
    WaitForHistogram(name);
  }
}

// static
void SharedStorageBrowserTestBase::WaitForHistogramWithCount(
    std::string_view histogram_name,
    int count) {
  if (GetSampleCountForHistogram(histogram_name) >= count) {
    return;
  }

  base::RunLoop run_loop;
  auto histogram_observer =
      std::make_unique<base::StatisticsRecorder::ScopedHistogramSampleObserver>(
          histogram_name,
          base::BindLambdaForTesting([&](std::string_view histogram_name,
                                         uint64_t name_hash,
                                         base::HistogramBase::Sample32 sample) {
            if (GetSampleCountForHistogram(histogram_name) >= count) {
              run_loop.Quit();
            }
          }));
  run_loop.Run();
}

// static
void SharedStorageBrowserTestBase::WaitForHistogramsWithCounts(
    const std::vector<std::tuple<std::string_view, int>>&
        histogram_names_and_counts) {
  for (auto [name, count] : histogram_names_and_counts) {
    WaitForHistogramWithCount(name, count);
  }
}

std::map<int, base::UnguessableToken>&
SharedStorageBrowserTestBase::GetCachedWorkletHostDevToolsTokens() {
  return test_runtime_manager().GetCachedWorkletHostDevToolsTokens();
}

base::UnguessableToken
SharedStorageBrowserTestBase::GetFirstWorkletHostDevToolsToken() {
  CHECK(!test_runtime_manager().GetCachedWorkletHostDevToolsTokens().empty());
  return test_runtime_manager().GetCachedWorkletHostDevToolsTokens()[0];
}

}  // namespace content

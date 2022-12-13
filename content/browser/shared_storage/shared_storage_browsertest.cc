// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "content/browser/private_aggregation/private_aggregation_manager_impl.h"
#include "content/browser/private_aggregation/private_aggregation_test_utils.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/shared_storage/shared_storage_document_service_impl.h"
#include "content/browser/shared_storage/shared_storage_event_params.h"
#include "content/browser/shared_storage/shared_storage_worklet_driver.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/browser/shared_storage/shared_storage_worklet_host_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/private_aggregation_features.h"
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
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/fenced_frame_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace content {

using testing::Pair;
using testing::UnorderedElementsAre;
using SharedStorageReportingMap = base::flat_map<std::string, ::GURL>;
using SharedStorageUrlSpecWithMetadata =
    SharedStorageEventParams::SharedStorageUrlSpecWithMetadata;

namespace {

const char kSimplePagePath[] = "/simple_page.html";

const char kFencedFramePath[] = "/fenced_frames/title0.html";

const char kPageWithBlankIframePath[] = "/page_with_blank_iframe.html";

const char kDestroyedStatusHistogram[] =
    "Storage.SharedStorage.Worklet.DestroyedStatus";

const char kTimingKeepAliveDurationHistogram[] =
    "Storage.SharedStorage.Worklet.Timing."
    "KeepAliveEndedDueToOperationsFinished.KeepAliveDuration";

const char kErrorTypeHistogram[] = "Storage.SharedStorage.Worklet.Error.Type";

const char kTimingUsefulResourceHistogram[] =
    "Storage.SharedStorage.Worklet.Timing.UsefulResourceDuration";

const char kTimingRunExecutedInWorkletHistogram[] =
    "Storage.SharedStorage.Document.Timing.Run.ExecutedInWorklet";

const char kTimingSelectUrlExecutedInWorkletHistogram[] =
    "Storage.SharedStorage.Document.Timing.SelectURL.ExecutedInWorklet";

const double kBudgetAllowed = 5.0;

const int kStalenessThresholdDays = 1;

const char kSelectFrom8URLsScript[] = R"(
    let urls = [];
    for (let i = 0; i < 8; ++i) {
      urls.push({url: '/fenced_frames/title' + i.toString() + '.html',
                 reportingMetadata: {
                   'click': '/fenced_frames/report' + i.toString() + '.html'
                 }});
    }

    sharedStorage.selectURL(
        'test-url-selection-operation', urls, {data: {'mockResult': 1}});
  )";

const char kRemainingBudgetPrefix[] = "remaining budget: ";

std::string TimeDeltaToString(base::TimeDelta delta) {
  return base::StrCat({base::NumberToString(delta.InMilliseconds()), "ms"});
}

void WaitForHistogram(const std::string& histogram_name) {
  // Continue if histogram was already recorded.
  if (base::StatisticsRecorder::FindHistogram(histogram_name))
    return;

  // Else, wait until the histogram is recorded.
  base::RunLoop run_loop;
  auto histogram_observer =
      std::make_unique<base::StatisticsRecorder::ScopedHistogramSampleObserver>(
          histogram_name,
          base::BindLambdaForTesting(
              [&](const char* histogram_name, uint64_t name_hash,
                  base::HistogramBase::Sample sample) { run_loop.Quit(); }));
  run_loop.Run();
}

void WaitForHistograms(const std::vector<std::string>& histogram_names) {
  for (const auto& name : histogram_names)
    WaitForHistogram(name);
}

std::string SerializeOptionalString(absl::optional<std::string> str) {
  if (str)
    return *str;

  return "absl::nullopt";
}

std::string SerializeOptionalBool(absl::optional<bool> b) {
  if (b)
    return (*b) ? "true" : "false";

  return "absl::nullopt";
}

std::string SerializeOptionalUrlsWithMetadata(
    absl::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
        urls_with_metadata) {
  if (!urls_with_metadata)
    return "absl::nullopt";

  std::vector<std::string> urls_str_vector = {"{ "};
  for (const auto& url_with_metadata : *urls_with_metadata) {
    urls_str_vector.push_back("{url: ");
    urls_str_vector.push_back(url_with_metadata.url);
    urls_str_vector.push_back(", reporting_metadata: { ");
    for (const auto& metadata_pair : url_with_metadata.reporting_metadata) {
      urls_str_vector.push_back("{");
      urls_str_vector.push_back(metadata_pair.first);
      urls_str_vector.push_back(" : ");
      urls_str_vector.push_back(metadata_pair.second);
      urls_str_vector.push_back("} ");
    }
    urls_str_vector.push_back("}} ");
  }
  urls_str_vector.push_back("}");

  return base::StrCat(urls_str_vector);
}

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
    if (worklet_responses_count_ >= count) {
      ResetResponseCounts();
      return;
    }

    expected_worklet_responses_count_ = count;
    worklet_responses_count_waiter_ = std::make_unique<base::RunLoop>();
    worklet_responses_count_waiter_->Run();
    worklet_responses_count_waiter_.reset();
    ResetResponseCounts();
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
      base::TimeTicks start_time,
      bool success,
      const std::string& error_message) override {
    OnRunOperationOnWorkletFinishedHelper(start_time, success, error_message,
                                          /*initial_message=*/true);
  }

  void OnRunOperationOnWorkletFinishedHelper(base::TimeTicks start_time,
                                             bool success,
                                             const std::string& error_message,
                                             bool initial_message) {
    if (should_defer_worklet_messages_ && initial_message) {
      pending_worklet_messages_.push_back(base::BindOnce(
          &TestSharedStorageWorkletHost::OnRunOperationOnWorkletFinishedHelper,
          weak_ptr_factory_.GetWeakPtr(), start_time, success, error_message,
          /*initial_message=*/false));
    } else {
      SharedStorageWorkletHost::OnRunOperationOnWorkletFinished(
          start_time, success, error_message);
    }

    if (initial_message)
      OnWorkletResponseReceived();
  }

  void OnRunURLSelectionOperationOnWorkletFinished(
      const GURL& urn_uuid,
      base::TimeTicks start_time,
      bool script_execution_success,
      const std::string& script_execution_error_message,
      uint32_t index,
      BudgetResult budget_result) override {
    OnRunURLSelectionOperationOnWorkletFinishedHelper(
        urn_uuid, start_time, script_execution_success,
        script_execution_error_message, index, std::move(budget_result),
        /*initial_message=*/true);
  }

  void OnRunURLSelectionOperationOnWorkletFinishedHelper(
      const GURL& urn_uuid,
      base::TimeTicks start_time,
      bool script_execution_success,
      const std::string& script_execution_error_message,
      uint32_t index,
      BudgetResult budget_result,
      bool initial_message) {
    if (should_defer_worklet_messages_ && initial_message) {
      pending_worklet_messages_.push_back(base::BindOnce(
          &TestSharedStorageWorkletHost::
              OnRunURLSelectionOperationOnWorkletFinishedHelper,
          weak_ptr_factory_.GetWeakPtr(), urn_uuid, start_time,
          script_execution_success, script_execution_error_message, index,
          std::move(budget_result),
          /*initial_message=*/false));
    } else {
      SharedStorageWorkletHost::OnRunURLSelectionOperationOnWorkletFinished(
          urn_uuid, start_time, script_execution_success,
          script_execution_error_message, index, std::move(budget_result));
    }

    if (initial_message)
      OnWorkletResponseReceived();
  }

  void OnWorkletResponseReceived() {
    ++worklet_responses_count_;

    if (worklet_responses_count_waiter_ &&
        worklet_responses_count_waiter_->running() &&
        worklet_responses_count_ >= expected_worklet_responses_count_) {
      worklet_responses_count_waiter_->Quit();
    }
  }

  void ResetResponseCounts() {
    expected_worklet_responses_count_ = 0u;
    worklet_responses_count_ = 0u;
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
  std::unique_ptr<base::RunLoop> worklet_responses_count_waiter_;

  // Whether we should defer messages received from the worklet environment to
  // handle them later. This includes request callbacks (e.g. for `addModule()`,
  // `selectURL()` and `run()`), as well as commands initiated from the worklet
  // (e.g. `console.log()`).
  bool should_defer_worklet_messages_;
  std::vector<base::OnceClosure> pending_worklet_messages_;

  base::WeakPtrFactory<TestSharedStorageWorkletHost> weak_ptr_factory_{this};
};

class TestSharedStorageObserver
    : public SharedStorageWorkletHostManager::SharedStorageObserverInterface {
 public:
  using Access = std::
      tuple<AccessType, std::string, std::string, SharedStorageEventParams>;

  void OnSharedStorageAccessed(
      const base::Time& access_time,
      AccessType type,
      const std::string& main_frame_id,
      const std::string& owner_origin,
      const SharedStorageEventParams& params) override {
    accesses_.emplace_back(type, main_frame_id, owner_origin, params);
  }

  bool EventParamsMatch(const SharedStorageEventParams& expected_params,
                        const SharedStorageEventParams& actual_params) {
    if (expected_params.script_source_url != actual_params.script_source_url) {
      LOG(ERROR) << "expected `script_source_url`: '"
                 << SerializeOptionalString(expected_params.script_source_url)
                 << "'";
      LOG(ERROR) << "actual `sript_source_url`:   '"
                 << SerializeOptionalString(actual_params.script_source_url)
                 << "'";
      return false;
    }
    if (expected_params.operation_name != actual_params.operation_name) {
      LOG(ERROR) << "expected `operation_name`: '"
                 << SerializeOptionalString(expected_params.operation_name)
                 << "'";
      LOG(ERROR) << "actual `operation_name`:   '"
                 << SerializeOptionalString(actual_params.operation_name)
                 << "'";
      return false;
    }
    if (expected_params.urls_with_metadata !=
        actual_params.urls_with_metadata) {
      LOG(ERROR) << "expected `urls_with_metadata`: "
                 << SerializeOptionalUrlsWithMetadata(
                        expected_params.urls_with_metadata);
      LOG(ERROR) << "actual `urls_with_metadata`:   "
                 << SerializeOptionalUrlsWithMetadata(
                        actual_params.urls_with_metadata);
      return false;
    }
    if (expected_params.key != actual_params.key) {
      LOG(ERROR) << "expected `key`: '"
                 << SerializeOptionalString(expected_params.key) << "'";
      LOG(ERROR) << "actual key:   '"
                 << SerializeOptionalString(actual_params.key) << "'";
      return false;
    }
    if (expected_params.value != actual_params.value) {
      LOG(ERROR) << "expected `value`: '"
                 << SerializeOptionalString(expected_params.value) << "'";
      LOG(ERROR) << "actual `value`:   '"
                 << SerializeOptionalString(actual_params.value) << "'";
      return false;
    }
    if (expected_params.ignore_if_present != actual_params.ignore_if_present) {
      LOG(ERROR) << "expected `ignore_if_present`: "
                 << SerializeOptionalBool(expected_params.ignore_if_present);
      LOG(ERROR) << "actual `ignore_if_present`:   "
                 << SerializeOptionalBool(actual_params.ignore_if_present);
      return false;
    }

    if (expected_params.serialized_data && !actual_params.serialized_data) {
      LOG(ERROR) << "`serialized_data` unexpectedly null";
      LOG(ERROR) << "expected `serialized_data`: '"
                 << SerializeOptionalString(expected_params.serialized_data)
                 << "'";
      LOG(ERROR) << "actual `serialized_data`: '"
                 << SerializeOptionalString(actual_params.serialized_data)
                 << "'";
      return false;
    }

    if (!expected_params.serialized_data && actual_params.serialized_data) {
      LOG(ERROR) << "`serialized_data` unexpectedly non-null";
      LOG(ERROR) << "expected `serialized_data`: '"
                 << SerializeOptionalString(expected_params.serialized_data)
                 << "'";
      LOG(ERROR) << "actual `serialized_data`: '"
                 << SerializeOptionalString(actual_params.serialized_data)
                 << "'";
      return false;
    }

    return true;
  }

  bool AccessesMatch(const Access& expected_access,
                     const Access& actual_access) {
    if (std::get<0>(expected_access) != std::get<0>(actual_access)) {
      LOG(ERROR) << "expected `type`: " << std::get<0>(expected_access);
      LOG(ERROR) << "actual `type`:   " << std::get<0>(actual_access);
      return false;
    }

    if (std::get<1>(expected_access) != std::get<1>(actual_access)) {
      LOG(ERROR) << "expected `main_frame_id`: '"
                 << std::get<1>(expected_access) << "'";
      LOG(ERROR) << "actual `main_frame_id`:   '" << std::get<1>(actual_access)
                 << "'";
      return false;
    }

    if (std::get<2>(expected_access) != std::get<2>(actual_access)) {
      LOG(ERROR) << "expected `origin`: '" << std::get<2>(expected_access)
                 << "'";
      LOG(ERROR) << "actual `origin`:   '" << std::get<2>(actual_access) << "'";
      return false;
    }

    return EventParamsMatch(std::get<3>(expected_access),
                            std::get<3>(actual_access));
  }

  void ExpectAccessObserved(const std::vector<Access>& expected_accesses) {
    ASSERT_EQ(expected_accesses.size(), accesses_.size());
    for (size_t i = 0; i < accesses_.size(); ++i) {
      EXPECT_TRUE(AccessesMatch(expected_accesses[i], accesses_[i]));
      if (!AccessesMatch(expected_accesses[i], accesses_[i]))
        LOG(ERROR) << "Event access at index " << i << " differs";
    }
  }

 private:
  std::vector<Access> accesses_;
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

  // Precondition: `frame` is associated with a
  // `SharedStorageDocumentServiceImpl` and an attached
  // `SharedStorageWorkletHost`.
  TestSharedStorageWorkletHost* GetAttachedWorkletHostForFrame(
      RenderFrameHost* frame) {
    SharedStorageDocumentServiceImpl* document_service = DocumentUserData<
        SharedStorageDocumentServiceImpl>::GetForCurrentDocument(frame);
    DCHECK(document_service);
    return static_cast<TestSharedStorageWorkletHost*>(
        GetAttachedWorkletHostsForTesting().at(document_service).get());
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
  using AccessType = TestSharedStorageObserver::AccessType;

  SharedStorageBrowserTest() {
    scoped_feature_list_
        .InitWithFeaturesAndParameters(/*enabled_features=*/
                                       {{blink::features::kSharedStorageAPI,
                                         {{"SharedStorageBitBudget",
                                           base::NumberToString(
                                               kBudgetAllowed)},
                                          {"SharedStorageStalenessThreshold",
                                           TimeDeltaToString(base::Days(
                                               kStalenessThresholdDays))}}},
                                        {features::
                                             kPrivacySandboxAdsAPIsOverride,
                                         {}}},
                                       /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    auto test_worklet_host_manager =
        std::make_unique<TestSharedStorageWorkletHostManager>();
    observer_ = std::make_unique<TestSharedStorageObserver>();

    test_worklet_host_manager->AddSharedStorageObserver(observer_.get());
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

  void TearDownOnMainThread() override {
    test_worklet_host_manager_->RemoveSharedStorageObserver(observer_.get());
  }

  // Virtual so that derived classes can delay starting the server, and/or add
  // other set up steps.
  virtual void FinishSetup() {
    https_server()->AddDefaultHandlers(GetTestDataFilePath());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    SetupCrossSiteRedirector(https_server());
    ASSERT_TRUE(https_server()->Start());
  }

  void ExpectAccessObserved(
      const std::vector<TestSharedStorageObserver::Access>& expected_accesses) {
    observer_->ExpectAccessObserved(expected_accesses);
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

  FrameTreeNode* PrimaryFrameTreeNodeRoot() {
    return static_cast<WebContentsImpl*>(shell()->web_contents())
        ->GetPrimaryFrameTree()
        .root();
  }

  std::string MainFrameId() {
    return PrimaryFrameTreeNodeRoot()
        ->current_frame_host()
        ->devtools_frame_token()
        .ToString();
  }

  SharedStorageBudgetMetadata* GetSharedStorageBudgetMetadata(
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

  SharedStorageReportingMap GetSharedStorageReportingMap(const GURL& urn_uuid) {
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

  void ExecuteScriptInWorklet(const ToRenderFrameHost& execution_target,
                              const std::string& script,
                              GURL* out_module_script_url) {
    DCHECK(out_module_script_url);

    base::StringPairs run_function_body_replacement;
    run_function_body_replacement.push_back(
        std::make_pair("{{RUN_FUNCTION_BODY}}", script));

    std::string host =
        execution_target.render_frame_host()->GetLastCommittedOrigin().host();

    *out_module_script_url = https_server()->GetURL(
        host, net::test_server::GetFilePathWithReplacements(
                  "/shared_storage/customizable_module.js",
                  run_function_body_replacement));

    EXPECT_TRUE(ExecJs(execution_target,
                       JsReplace("sharedStorage.worklet.addModule($1)",
                                 *out_module_script_url)));

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

  // Create an iframe of origin `origin` inside `parent_node`, and run
  // sharedStorage.selectURL() on 8 urls. If `parent_node` is not specified,
  // the primary frame tree's root node will be chosen. This generates an URN
  // associated with `origin` and 3 bits of shared storage budget.
  GURL SelectFrom8URLsInContext(const url::Origin& origin,
                                FrameTreeNode* parent_node = nullptr) {
    if (!parent_node)
      parent_node = PrimaryFrameTreeNodeRoot();

    // If this is called inside a fenced frame, creating an iframe will need
    // "Supports-Loading-Mode: fenced-frame" response header. Thus, we simply
    // always set the path to `kFencedFramePath`.
    GURL iframe_url = origin.GetURL().Resolve(kFencedFramePath);

    FrameTreeNode* iframe = CreateIFrame(parent_node, iframe_url);

    EXPECT_TRUE(ExecJs(iframe, R"(
        sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
      )"));

    std::string urn_uuid =
        EvalJs(iframe, kSelectFrom8URLsScript).ExtractString();

    // There are 2 "worklet operations": `addModule()` and `selectURL()`.
    test_worklet_host_manager()
        .GetAttachedWorkletHostForFrame(iframe->current_frame_host())
        ->WaitForWorkletResponsesCount(2);

    return GURL(urn_uuid);
  }

  // Prerequisite: The worklet for `frame` has registered a
  // "remaining-budget-operation" that logs the remaining budget to the console
  // after `kRemainingBudgetPrefix`.
  double RemainingBudgetViaJSForFrame(FrameTreeNode* frame) {
    DCHECK(frame);

    WebContentsConsoleObserver console_observer(shell()->web_contents());
    const std::string kRemainingBudgetPrefixStr(kRemainingBudgetPrefix);
    console_observer.SetPattern(base::StrCat({kRemainingBudgetPrefixStr, "*"}));

    EXPECT_TRUE(ExecJs(frame, R"(
      sharedStorage.run('remaining-budget-operation', {data: {}});
    )"));

    bool observed = console_observer.Wait();
    EXPECT_TRUE(observed);
    if (!observed) {
      return nan("");
    }

    EXPECT_EQ(1u, console_observer.messages().size());
    std::string console_message =
        base::UTF16ToUTF8(console_observer.messages()[0].message);
    EXPECT_TRUE(base::StartsWith(console_message, kRemainingBudgetPrefixStr));

    std::string result_string = console_message.substr(
        kRemainingBudgetPrefixStr.size(),
        console_message.size() - kRemainingBudgetPrefixStr.size());

    double result = 0.0;
    EXPECT_TRUE(base::StringToDouble(result_string, &result));

    // There is 1 "worklet operation": `run()`.
    test_worklet_host_manager()
        .GetAttachedWorkletHostForFrame(frame->current_frame_host())
        ->WaitForWorkletResponsesCount(1);
    return result;
  }

  double RemainingBudgetViaJSForOrigin(const url::Origin& origin) {
    FrameTreeNode* iframe =
        CreateIFrame(PrimaryFrameTreeNodeRoot(), origin.GetURL());

    EXPECT_TRUE(ExecJs(iframe, R"(
        sharedStorage.worklet.addModule('shared_storage/simple_module.js');
      )"));

    // There is 1 "worklet operation": `addModule()`.
    test_worklet_host_manager()
        .GetAttachedWorkletHostForFrame(iframe->current_frame_host())
        ->WaitForWorkletResponsesCount(1);
    return RemainingBudgetViaJSForFrame(iframe);
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
  base::HistogramTester histogram_tester_;

  raw_ptr<TestSharedStorageWorkletHostManager, DanglingUntriaged>
      test_worklet_host_manager_ = nullptr;
  std::unique_ptr<TestSharedStorageObserver> observer_;
};

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, AddModule_Success) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  // Navigate again to record histograms.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kDestroyedStatusHistogram, kTimingUsefulResourceHistogram});

  histogram_tester_.ExpectUniqueSample(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::kDidNotEnterKeepAlive, 1);
  histogram_tester_.ExpectTotalCount(kTimingUsefulResourceHistogram, 1);

  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(),
        url::Origin::Create(url).Serialize(),
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, AddModule_ScriptNotFound) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(),
        url::Origin::Create(url).Serialize(),
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/nonexistent_module.js"))}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, AddModule_RedirectNotAllowed) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(),
        url::Origin::Create(url).Serialize(),
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/server-redirect?shared_storage/simple_module.js"))}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       AddModule_ScriptExecutionFailure) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(),
        url::Origin::Create(url).Serialize(),
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/erroneous_module.js"))}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       AddModule_MultipleAddModuleFailure) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  // Navigate again to record histograms.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kDestroyedStatusHistogram, kTimingUsefulResourceHistogram});

  histogram_tester_.ExpectUniqueSample(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::kDidNotEnterKeepAlive, 1);
  histogram_tester_.ExpectTotalCount(kTimingUsefulResourceHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))},
       {AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, RunOperation_Success) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               std::vector<uint8_t>())}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       RunOperation_Failure_RunOperationBeforeAddModule) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  // Navigate again to record histograms.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  WaitForHistograms({kDestroyedStatusHistogram, kTimingUsefulResourceHistogram,
                     kErrorTypeHistogram,
                     kTimingRunExecutedInWorkletHistogram});

  histogram_tester_.ExpectUniqueSample(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::kDidNotEnterKeepAlive, 1);
  histogram_tester_.ExpectUniqueSample(
      kErrorTypeHistogram,
      blink::SharedStorageWorkletErrorType::kRunNonWebVisible, 1);
  histogram_tester_.ExpectTotalCount(kTimingUsefulResourceHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               std::vector<uint8_t>())},
       {AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       RunOperation_Failure_InvalidOptionsArgument) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 0);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       RunOperation_Failure_ErrorInRunOperation) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/erroneous_function_module.js"))},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               std::vector<uint8_t>())}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, WorkletDestroyed) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  WaitForHistograms(
      {kDestroyedStatusHistogram, kTimingUsefulResourceHistogram});

  histogram_tester_.ExpectUniqueSample(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::kDidNotEnterKeepAlive, 1);
  histogram_tester_.ExpectTotalCount(kTimingUsefulResourceHistogram, 1);

  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(),
        url::Origin::Create(url).Serialize(),
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, TwoWorklets) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kPageWithBlankIframePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  RenderFrameHost* iframe =
      PrimaryFrameTreeNodeRoot()->child_at(0)->current_frame_host();

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

  // Navigate again to record histograms.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  WaitForHistograms(
      {kDestroyedStatusHistogram, kTimingUsefulResourceHistogram});

  histogram_tester_.ExpectUniqueSample(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::kDidNotEnterKeepAlive, 2);
  histogram_tester_.ExpectTotalCount(kTimingUsefulResourceHistogram, 2);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module2.js"))},
       {AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))}});
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageBrowserTest,
    KeepAlive_StartBeforeAddModuleComplete_EndAfterAddModuleComplete) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  WaitForHistograms({kDestroyedStatusHistogram, kTimingUsefulResourceHistogram,
                     kTimingKeepAliveDurationHistogram});

  histogram_tester_.ExpectUniqueSample(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::
          kKeepAliveEndedDueToOperationsFinished,
      1);
  histogram_tester_.ExpectTotalCount(kTimingKeepAliveDurationHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingUsefulResourceHistogram, 1);

  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(),
        url::Origin::Create(url).Serialize(),
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       KeepAlive_StartBeforeAddModuleComplete_EndAfterTimeout) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  WaitForHistograms(
      {kDestroyedStatusHistogram, kTimingUsefulResourceHistogram});

  histogram_tester_.ExpectUniqueSample(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::kKeepAliveEndedDueToTimeout,
      1);
  histogram_tester_.ExpectTotalCount(kTimingKeepAliveDurationHistogram, 0);
  histogram_tester_.ExpectUniqueSample(kTimingUsefulResourceHistogram, 100, 1);

  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(),
        url::Origin::Create(url).Serialize(),
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))}});
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageBrowserTest,
    KeepAlive_StartBeforeRunOperationComplete_EndAfterRunOperationComplete) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  WaitForHistograms({kDestroyedStatusHistogram, kTimingUsefulResourceHistogram,
                     kTimingKeepAliveDurationHistogram,
                     kTimingRunExecutedInWorkletHistogram});

  histogram_tester_.ExpectUniqueSample(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::
          kKeepAliveEndedDueToOperationsFinished,
      1);
  histogram_tester_.ExpectTotalCount(kTimingKeepAliveDurationHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingUsefulResourceHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               std::vector<uint8_t>())}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, KeepAlive_SubframeWorklet) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kPageWithBlankIframePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // Configure the worklet host for the subframe to defer worklet responses.
  test_worklet_host_manager()
      .ConfigureShouldDeferWorkletMessagesOnWorkletHostCreation(true);

  RenderFrameHost* iframe =
      PrimaryFrameTreeNodeRoot()->child_at(0)->current_frame_host();

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

  // Navigate again to record histograms.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  WaitForHistograms({kDestroyedStatusHistogram, kTimingUsefulResourceHistogram,
                     kTimingKeepAliveDurationHistogram});

  histogram_tester_.ExpectBucketCount(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::
          kKeepAliveEndedDueToOperationsFinished,
      1);
  histogram_tester_.ExpectBucketCount(
      kDestroyedStatusHistogram,
      blink::SharedStorageWorkletDestroyedStatus::kDidNotEnterKeepAlive, 1);
  histogram_tester_.ExpectTotalCount(kTimingKeepAliveDurationHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingUsefulResourceHistogram, 2);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))},
       {AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module2.js"))}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       RenderProcessHostDestroyedDuringWorkletKeepAlive) {
  // The test assumes pages gets deleted after navigation, letting the worklet
  // enter keep-alive phase. To ensure this, disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  SharedStorageBudgetMetadata* metadata =
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

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);

  std::string origin_str = url::Origin::Create(main_url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))},
       {AccessType::kDocumentSelectURL, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSelectURL(
            "test-url-selection-operation", std::vector<uint8_t>(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {{"click",
                    https_server()
                        ->GetURL("a.test", "/fenced_frames/report1.html")
                        .spec()},
                   {"mouse interaction",
                    https_server()
                        ->GetURL("a.test", "/fenced_frames/report2.html")
                        .spec()}}}}))}});
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

  SharedStorageBudgetMetadata* metadata =
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

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);

  std::string origin_str = url::Origin::Create(main_url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))},
       {AccessType::kDocumentSelectURL, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSelectURL(
            "test-url-selection-operation", std::vector<uint8_t>(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {{"click",
                    https_server()
                        ->GetURL("a.test", "/fenced_frames/report1.html")
                        .spec()}}}}))}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       SelectURL_BudgetMetadata_Origin) {
  EXPECT_TRUE(NavigateToURL(
      shell(), https_server()->GetURL("a.test", kPageWithBlankIframePath)));

  GURL iframe_url = https_server()->GetURL("b.test", kSimplePagePath);
  NavigateIframeToURL(shell()->web_contents(), "test_iframe", iframe_url);

  RenderFrameHost* iframe =
      PrimaryFrameTreeNodeRoot()->child_at(0)->current_frame_host();

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

  SharedStorageBudgetMetadata* metadata =
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

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);

  std::string origin_str = url::Origin::Create(iframe_url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "b.test", "/shared_storage/simple_module.js"))},
       {AccessType::kDocumentSelectURL, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSelectURL(
            "test-url-selection-operation", std::vector<uint8_t>(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("b.test",
                                         "/fenced_frames/title0.html"),
                  {}},
                 {https_server()->GetURL("b.test",
                                         "/fenced_frames/title1.html"),
                  {{"click",
                    https_server()
                        ->GetURL("b.test", "/fenced_frames/report1.html")
                        .spec()}}},
                 {https_server()->GetURL("b.test",
                                         "/fenced_frames/title2.html"),
                  {}}}))}});
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

  SharedStorageBudgetMetadata* metadata =
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

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);

  std::string origin_str = url::Origin::Create(main_url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))},
       {AccessType::kDocumentSelectURL, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSelectURL(
            "test-url-selection-operation", std::vector<uint8_t>(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {{"", https_server()
                            ->GetURL("a.test", "/fenced_frames/report1.html")
                            .spec()}}}}))}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, SetAppendOperationInDocument) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

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

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('key0'));
      console.log(await sharedStorage.get('key1'));
      console.log(await sharedStorage.get('key2'));
      console.log(await sharedStorage.get('key3'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

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

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key0", "value0", false)},
       {AccessType::kDocumentSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key1", "value1", false)},
       {AccessType::kDocumentSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key1", "value111", false)},
       {AccessType::kDocumentSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key2", "value2", false)},
       {AccessType::kDocumentSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key2", "value222", true)},
       {AccessType::kDocumentSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key3", "value3", false)},
       {AccessType::kDocumentAppend, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAppend("key3", "value333")},
       {AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(out_script_url)},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               std::vector<uint8_t>())},
       {AccessType::kWorkletGet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForGetOrDelete("key0")},
       {AccessType::kWorkletGet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForGetOrDelete("key1")},
       {AccessType::kWorkletGet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForGetOrDelete("key2")},
       {AccessType::kWorkletGet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForGetOrDelete("key3")},
       {AccessType::kWorkletLength, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, DeleteOperationInDocument) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.set('key0', 'value0');
      sharedStorage.delete('key0');
    )"));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.length());
      console.log(await sharedStorage.get('key0'));
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[0].log_level);
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[1].log_level);

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key0", "value0", false)},
       {AccessType::kDocumentDelete, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForGetOrDelete("key0")},
       {AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(out_script_url)},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               std::vector<uint8_t>())},
       {AccessType::kWorkletLength, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()},
       {AccessType::kWorkletGet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForGetOrDelete("key0")}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, ClearOperationInDocument) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.set('key0', 'value0');
      sharedStorage.clear();
    )"));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[0].message));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key0", "value0", false)},
       {AccessType::kDocumentClear, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()},
       {AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(out_script_url)},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               std::vector<uint8_t>())},
       {AccessType::kWorkletLength, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, SetAppendOperationInWorklet) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
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
    )",
                         &out_script_url);

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

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(out_script_url)},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               std::vector<uint8_t>())},
       {AccessType::kWorkletSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key0", "value0", false)},
       {AccessType::kWorkletSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key1", "value1", false)},
       {AccessType::kWorkletSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key1", "value111", false)},
       {AccessType::kWorkletSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key2", "value2", false)},
       {AccessType::kWorkletSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key2", "value222", true)},
       {AccessType::kWorkletSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key3", "value3", false)},
       {AccessType::kWorkletAppend, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAppend("key3", "value333")},
       {AccessType::kWorkletGet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForGetOrDelete("key0")},
       {AccessType::kWorkletGet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForGetOrDelete("key1")},
       {AccessType::kWorkletGet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForGetOrDelete("key2")},
       {AccessType::kWorkletGet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForGetOrDelete("key3")},
       {AccessType::kWorkletLength, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       AppendOperationFailedInWorklet) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      await sharedStorage.set('key0', 'a'.repeat(1024));

      // This will fail due to the would-be length being too big.
      await sharedStorage.append('key0', 'a');
    )",
                         &out_script_url);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("sharedStorage.append() failed",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[0].log_level);

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(out_script_url)},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               std::vector<uint8_t>())},
       {AccessType::kWorkletSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key0", std::string(1024, 'a'),
                                               false)},
       {AccessType::kWorkletAppend, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAppend("key0", "a")}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, DeleteOperationInWorklet) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      sharedStorage.set('key0', 'value0');
      console.log(await sharedStorage.length());
      console.log(await sharedStorage.get('key0'));

      sharedStorage.delete('key0');

      console.log(await sharedStorage.length());
      console.log(await sharedStorage.get('key0'));
    )",
                         &out_script_url);

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

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(out_script_url)},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               std::vector<uint8_t>())},
       {AccessType::kWorkletSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key0", "value0", false)},
       {AccessType::kWorkletLength, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()},
       {AccessType::kWorkletGet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForGetOrDelete("key0")},
       {AccessType::kWorkletDelete, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForGetOrDelete("key0")},
       {AccessType::kWorkletLength, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()},
       {AccessType::kWorkletGet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForGetOrDelete("key0")}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, ClearOperationInWorklet) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      sharedStorage.set('key0', 'value0');
      console.log(await sharedStorage.length());
      console.log(await sharedStorage.get('key0'));

      sharedStorage.clear();

      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("value0",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[2].message));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(out_script_url)},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               std::vector<uint8_t>())},
       {AccessType::kWorkletSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key0", "value0", false)},
       {AccessType::kWorkletLength, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()},
       {AccessType::kWorkletGet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForGetOrDelete("key0")},
       {AccessType::kWorkletClear, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()},
       {AccessType::kWorkletLength, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, GetOperationInWorklet) {
  base::SimpleTestClock clock;
  base::RunLoop loop;
  static_cast<StoragePartitionImpl*>(shell()
                                         ->web_contents()
                                         ->GetBrowserContext()
                                         ->GetDefaultStoragePartition())
      ->GetSharedStorageManager()
      ->OverrideClockForTesting(&clock, loop.QuitClosure());
  loop.Run();
  clock.SetNow(base::Time::Now());

  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.set('key0', 'value0');
    )"));

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  GURL script_url =
      https_server()->GetURL("a.test", "/shared_storage/getter_module.js");

  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('get-operation', {data: {'key': 'key0'}});
      )"));

  // There are 2 "worklet operations": `addModule()` and `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponsesCount(2);

  // Advance clock so that key will expire.
  clock.Advance(base::Days(kStalenessThresholdDays) + base::Seconds(1));

  EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('get-operation', {data: {'key': 'key0'}});
      )"));

  // There is one "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponsesCount(1);

  EXPECT_EQ(4u, console_observer.messages().size());
  EXPECT_EQ("sharedStorage.length(): 1",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("sharedStorage.get('key0'): value0",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("sharedStorage.length(): 0",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("sharedStorage.get('key0'): undefined",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[0].log_level);
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[1].log_level);
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[2].log_level);
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[3].log_level);

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 2);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key0", "value0", false)},
       {AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(script_url)},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("get-operation",
                                               std::vector<uint8_t>())},
       {AccessType::kWorkletLength, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()},
       {AccessType::kWorkletGet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForGetOrDelete("key0")},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("get-operation",
                                               std::vector<uint8_t>())},
       {AccessType::kWorkletLength, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()},
       {AccessType::kWorkletGet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForGetOrDelete("key0")}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       AccessStorageInSameOriginDocument) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.set('key0', 'value0');
    )"));

  EXPECT_TRUE(
      NavigateToURL(shell(), https_server()->GetURL("a.test", "/title1.html")));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[0].message));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key0", "value0", false)},
       {AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(out_script_url)},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               std::vector<uint8_t>())},
       {AccessType::kWorkletLength, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       AccessStorageInDifferentOriginDocument) {
  GURL url1 = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.set('key0', 'value0');
    )"));

  GURL url2 = https_server()->GetURL("b.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[0].message));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin2_str = url::Origin::Create(url2).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentSet, MainFrameId(),
        url::Origin::Create(url1).Serialize(),
        SharedStorageEventParams::CreateForSet("key0", "value0", false)},
       {AccessType::kDocumentAddModule, MainFrameId(), origin2_str,
        SharedStorageEventParams::CreateForAddModule(out_script_url)},
       {AccessType::kDocumentRun, MainFrameId(), origin2_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               std::vector<uint8_t>())},
       {AccessType::kWorkletLength, MainFrameId(), origin2_str,
        SharedStorageEventParams::CreateDefault()}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest, KeysAndEntriesOperation) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.set('key0', 'value0');
      sharedStorage.set('key1', 'value1');
      sharedStorage.set('key2', 'value2');
    )"));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      for await (const key of sharedStorage.keys()) {
        console.log(key);
      }
      for await (const [key, value] of sharedStorage.entries()) {
        console.log(key + ';' + value);
      }
    )",
                         &out_script_url);

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

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key0", "value0", false)},
       {AccessType::kDocumentSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key1", "value1", false)},
       {AccessType::kDocumentSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key2", "value2", false)},
       {AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(out_script_url)},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               std::vector<uint8_t>())},
       {AccessType::kWorkletKeys, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()},
       {AccessType::kWorkletEntries, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()}});
}

IN_PROC_BROWSER_TEST_F(SharedStorageBrowserTest,
                       KeysAndEntriesOperation_MultipleBatches) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), R"(
      for (let i = 0; i < 150; ++i) {
        sharedStorage.set('key' + i.toString().padStart(3, '0'),
                          'value' + i.toString().padStart(3, '0'));
      }
    )"));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      for await (const key of sharedStorage.keys()) {
        console.log(key);
      }
      for await (const [key, value] of sharedStorage.entries()) {
        console.log(key + ';' + value);
      }
    )",
                         &out_script_url);

  EXPECT_EQ(300u, console_observer.messages().size());
  std::string origin_str = url::Origin::Create(url).Serialize();
  std::vector<TestSharedStorageObserver::Access> expected_accesses;
  for (int i = 0; i < 150; ++i) {
    std::string zero_padded_i = base::NumberToString(i);
    zero_padded_i.insert(zero_padded_i.begin(), 3 - zero_padded_i.size(), '0');

    std::string padded_key = base::StrCat({"key", zero_padded_i});
    std::string padded_value = base::StrCat({"value", zero_padded_i});
    EXPECT_EQ(padded_key,
              base::UTF16ToUTF8(console_observer.messages()[i].message));
    EXPECT_EQ(base::JoinString({padded_key, padded_value}, ";"),
              base::UTF16ToUTF8(console_observer.messages()[i + 150].message));

    expected_accesses.emplace_back(AccessType::kDocumentSet, MainFrameId(),
                                   origin_str,
                                   SharedStorageEventParams::CreateForSet(
                                       padded_key, padded_value, false));
  }

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  expected_accesses.emplace_back(
      AccessType::kDocumentAddModule, MainFrameId(), origin_str,
      SharedStorageEventParams::CreateForAddModule(out_script_url));
  expected_accesses.emplace_back(AccessType::kDocumentRun, MainFrameId(),
                                 origin_str,
                                 SharedStorageEventParams::CreateForRun(
                                     "test-operation", std::vector<uint8_t>()));
  expected_accesses.emplace_back(AccessType::kWorkletKeys, MainFrameId(),
                                 origin_str,
                                 SharedStorageEventParams::CreateDefault());
  expected_accesses.emplace_back(AccessType::kWorkletEntries, MainFrameId(),
                                 origin_str,
                                 SharedStorageEventParams::CreateDefault());
  ExpectAccessObserved(expected_accesses);
}

class SharedStorageAllowURNsInIframesBrowserTest
    : public SharedStorageBrowserTest {
 public:
  SharedStorageAllowURNsInIframesBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kAllowURNsInIframes);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SharedStorageAllowURNsInIframesBrowserTest,
                       RenderSelectURLResultInIframe) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  GURL urn_uuid = SelectFrom8URLsInContext(shared_storage_origin);

  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), urn_uuid);

  EXPECT_EQ(iframe_node->current_url(),
            https_server()->GetURL("b.test", "/fenced_frames/title1.html"));

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);

  GURL new_page_url = https_server()->GetURL("c.test", kSimplePagePath);

  TestNavigationObserver top_navigation_observer(shell()->web_contents());
  EXPECT_TRUE(
      ExecJs(iframe_node, JsReplace("top.location = $1", new_page_url.spec())));
  top_navigation_observer.Wait();

  // After the top navigation, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);
}

class SharedStorageFencedFrameInteractionBrowserTest
    : public SharedStorageBrowserTest {
 public:
  SharedStorageFencedFrameInteractionBrowserTest() {
    scoped_feature_list_
        .InitWithFeaturesAndParameters(/*enabled_features=*/
                                       {{blink::features::kFencedFrames, {}}},
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
    return CreateFencedFrame(PrimaryFrameTreeNodeRoot(), url);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
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

  SharedStorageBudgetMetadata* metadata =
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

  FrameTreeNode* root = PrimaryFrameTreeNodeRoot();

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

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
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

  FrameTreeNode* root = PrimaryFrameTreeNodeRoot();

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
  FencedFrameURLMappingTestPeer url_mapping_test_peer(&url_mapping);

  EXPECT_TRUE(url_mapping_test_peer.HasObserver(GURL(urn_uuid), request));

  // Execute the deferred messages. This should finish the url mapping and
  // resume the deferred navigation.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->ExecutePendingWorkletMessages();

  observer.Wait();

  SharedStorageBudgetMetadata* metadata =
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

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);

  histogram_tester_.ExpectTotalCount(
      "Storage.SharedStorage.Timing.UrlMappingDuringNavigation", 1);
}

// Tests that the URN from SelectURL() is valid in different
// context in the page, but it's not valid in a new page.
IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURL_URNLifetime) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  GURL urn_uuid = SelectFrom8URLsInContext(url::Origin::Create(main_url));
  EXPECT_TRUE(blink::IsValidUrnUuidURL(GURL(urn_uuid)));

  FrameTreeNode* iframe_node = PrimaryFrameTreeNodeRoot()->child_at(0);

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

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

// Tests that if the URN mapping is not finished before the keep-alive timeout,
// the mapping will be considered to be failed when the timeout is reached.
IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURL_NotFinishBeforeKeepAliveTimeout) {
  // The test assumes pages get deleted after navigation. To ensure this,
  // disable back/forward cache.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url = https_server()->GetURL("a.test", kPageWithBlankIframePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHost* iframe =
      PrimaryFrameTreeNodeRoot()->child_at(0)->current_frame_host();

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

  FrameTreeNode* root = PrimaryFrameTreeNodeRoot();

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

  SharedStorageBudgetMetadata* metadata =
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

  // The worklet execution sequence for `selectURL()` doesn't complete, so the
  // `kTimingSelectUrlExecutedInWorkletHistogram` histogram isn't recorded.
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     0);
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
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

  SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(GURL(urn_uuid));
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->origin, https_server()->GetOrigin("a.test"));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, std::log2(3));

  EXPECT_TRUE(GetSharedStorageReportingMap(GURL(urn_uuid)).empty());

  FrameTreeNode* root = PrimaryFrameTreeNodeRoot();

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

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
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

  SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(GURL(urn_uuid));
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->origin, https_server()->GetOrigin("a.test"));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, std::log2(3));

  EXPECT_THAT(GetSharedStorageReportingMap(GURL(urn_uuid)),
              UnorderedElementsAre(
                  Pair("click", https_server()->GetURL(
                                    "a.test", "/fenced_frames/report1.html"))));

  FrameTreeNode* root = PrimaryFrameTreeNodeRoot();

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

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       FencedFrameNavigateSelf_NoBudgetWithdrawal) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  GURL urn_uuid = SelectFrom8URLsInContext(shared_storage_origin);

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(urn_uuid);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

  TestFrameNavigationObserver observer(
      fenced_frame_root_node->current_frame_host());
  EXPECT_TRUE(ExecJs(fenced_frame_root_node, "location.reload()"));
  observer.Wait();

  // No budget withdrawal as the fenced frame did not initiate a top navigation.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       FencedFrameNavigateTop_BudgetWithdrawal) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  GURL urn_uuid = SelectFrom8URLsInContext(shared_storage_origin);

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(urn_uuid);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

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
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForOrigin(shared_storage_origin),
                   kBudgetAllowed - 3);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageFencedFrameInteractionBrowserTest,
    FencedFrameNavigateFromParentToRegularURLAndThenNavigateTop_NoBudgetWithdrawal) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  GURL urn_uuid = SelectFrom8URLsInContext(shared_storage_origin);

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(urn_uuid);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

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
  url::Origin new_frame_origin = url::Origin::Create(new_frame_url);
  EXPECT_DOUBLE_EQ(GetRemainingBudget(new_frame_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForOrigin(shared_storage_origin),
                   kBudgetAllowed);
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForOrigin(new_frame_origin),
                   kBudgetAllowed);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(
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
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

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
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForOrigin(shared_storage_origin),
                   kBudgetAllowed - 3);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
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
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

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
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForOrigin(shared_storage_origin),
                   kBudgetAllowed - 3);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageFencedFrameInteractionBrowserTest,
    NestedFencedFrameNavigateTop_BudgetWithdrawalFromTwoMetadata) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin1 =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  GURL urn_uuid1 = SelectFrom8URLsInContext(shared_storage_origin1);
  FrameTreeNode* fenced_frame_root_node1 = CreateFencedFrame(urn_uuid1);

  url::Origin shared_storage_origin2 =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  GURL urn_uuid2 =
      SelectFrom8URLsInContext(shared_storage_origin2, fenced_frame_root_node1);

  FrameTreeNode* fenced_frame_root_node2 =
      CreateFencedFrame(fenced_frame_root_node1, urn_uuid2);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin1), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin2), kBudgetAllowed);

  GURL new_page_url = https_server()->GetURL("d.test", kSimplePagePath);
  TestNavigationObserver top_navigation_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(
      fenced_frame_root_node2,
      JsReplace("window.open($1, '_unfencedTop')", new_page_url.spec())));
  top_navigation_observer.Wait();

  // After the top navigation, log(8)=3 bits should have been withdrawn from
  // both `shared_storage_origin1` and `shared_storage_origin2`.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin1),
                   kBudgetAllowed - 3);
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin2),
                   kBudgetAllowed - 3);
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURLNotAllowedInNestedFencedFrame) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin1 =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  GURL urn_uuid1 = SelectFrom8URLsInContext(shared_storage_origin1);
  FrameTreeNode* fenced_frame_root_node1 = CreateFencedFrame(urn_uuid1);

  url::Origin shared_storage_origin2 =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  GURL urn_uuid2 =
      SelectFrom8URLsInContext(shared_storage_origin2, fenced_frame_root_node1);

  FrameTreeNode* fenced_frame_root_node2 =
      CreateFencedFrame(fenced_frame_root_node1, urn_uuid2);

  EXPECT_TRUE(ExecJs(fenced_frame_root_node2, R"(
      sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
    )"));

  EvalJsResult result = EvalJs(fenced_frame_root_node2, R"(
      sharedStorage.selectURL(
          'test-url-selection-operation',
          [{url: "/fenced_frames/title0.html"}], {data: {'mockResult': 0}});
    )");

  EXPECT_THAT(result.error,
              testing::HasSubstr(
                  "selectURL() is called in a context with a fenced frame "
                  "depth (2) exceeding the maximum allowed number (1)."));
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
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
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

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
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForOrigin(shared_storage_origin),
                   kBudgetAllowed - 3);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       FencedFrame_PopupTwice_BudgetWithdrawalOnce) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  GURL urn_uuid = SelectFrom8URLsInContext(shared_storage_origin);

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(urn_uuid);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

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
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForOrigin(shared_storage_origin),
                   kBudgetAllowed - 3);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(
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
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()),
                   kBudgetAllowed);

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
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForOrigin(shared_storage_origin),
                   kBudgetAllowed - 3 - 3);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     2);
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageFencedFrameInteractionBrowserTest,
    TwoFencedFrames_SameURNs_EachPopupOnce_BudgetWithdrawalOnce) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin = url::Origin::Create(main_url);

  GURL urn_uuid = SelectFrom8URLsInContext(shared_storage_origin);

  FrameTreeNode* fenced_frame_root_node1 = CreateFencedFrame(urn_uuid);
  FrameTreeNode* fenced_frame_root_node2 = CreateFencedFrame(urn_uuid);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

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
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForOrigin(shared_storage_origin),
                   kBudgetAllowed - 3);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
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
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForOrigin(shared_storage_origin),
                   kBudgetAllowed - 3);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     2);
}

// When number of urn mappings limit has been reached, subsequent `selectURL()`
// calls will fail.
IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURL_Fails_ExceedNumOfUrnMappingsLimit) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // `selectURL()` succeeds when map is not full.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.selectURL(
          'test-url-selection-operation',
          [{url: "fenced_frames/title0.html"}], {data: {'mockResult': 0}});
    )");
  EXPECT_TRUE(result.error.empty());

  // Wait for the `addModule()` and `selectURL()` to finish.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponsesCount(2);

  FencedFrameURLMapping& fenced_frame_url_mapping =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();
  FencedFrameURLMappingTestPeer fenced_frame_url_mapping_test_peer(
      &fenced_frame_url_mapping);

  // Fill the map until its size reaches the limit.
  GURL url("https://a.test");
  fenced_frame_url_mapping_test_peer.FillMap(url);

  EvalJsResult extra_result = EvalJs(shell(), R"(
      sharedStorage.selectURL(
          'test-url-selection-operation',
          [{url: "fenced_frames/title1.html"}], {data: {'mockResult': 0}});
    )");

  // `selectURL()` fails when map is full.
  std::string expected_error = base::StrCat(
      {"a JavaScript error: \"Error: ",
       "sharedStorage.selectURL() failed because number of urn::uuid to url ",
       "mappings has reached the limit.\"\n"});
  EXPECT_EQ(expected_error, extra_result.error);
}

class SharedStorageSelectURLNotAllowedInFencedFrameBrowserTest
    : public SharedStorageFencedFrameInteractionBrowserTest {
 public:
  SharedStorageSelectURLNotAllowedInFencedFrameBrowserTest() {
    scoped_feature_list_
        .InitWithFeaturesAndParameters(/*enabled_features=*/
                                       {{blink::features::kSharedStorageAPI,
                                         {{"SharedStorageBitBudget",
                                           base::NumberToString(
                                               kBudgetAllowed)},
                                          {"SharedStorageMaxAllowedFencedFrameD"
                                           "epthForSelectURL",
                                           "0"}}},
                                        {features::
                                             kPrivacySandboxAdsAPIsOverride,
                                         {}}},
                                       /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SharedStorageSelectURLNotAllowedInFencedFrameBrowserTest,
                       SelectURLNotAllowedInFencedFrame) {
  GURL main_frame_url = https_server()->GetURL("a.test", kSimplePagePath);

  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  GURL fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");

  FrameTreeNode* fenced_frame_node = CreateFencedFrame(fenced_frame_url);

  EXPECT_TRUE(ExecJs(fenced_frame_node, R"(
      sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  EvalJsResult result = EvalJs(fenced_frame_node, R"(
      sharedStorage.selectURL(
          'test-url-selection-operation',
          [{url: "fenced_frames/title0.html"}], {data: {'mockResult': 0}});
    )");

  EXPECT_THAT(result.error,
              testing::HasSubstr(
                  "selectURL() is called in a context with a fenced frame "
                  "depth (1) exceeding the maximum allowed number (0)."));
}

class SharedStorageReportEventBrowserTest
    : public SharedStorageFencedFrameInteractionBrowserTest {
  void FinishSetup() override {
    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  }
};

IN_PROC_BROWSER_TEST_F(SharedStorageReportEventBrowserTest,
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

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
}

class SharedStoragePrivateAggregationDisabledBrowserTest
    : public SharedStorageBrowserTest {
 public:
  SharedStoragePrivateAggregationDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(content::kPrivateAggregationApi);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationDisabledBrowserTest,
                       PrivateAggregationNotDefined) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.sendHistogramReport({bucket: 1n, value: 2});
    )",
                         &out_script_url);

  ASSERT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("ReferenceError: privateAggregation is not defined",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[0].log_level);
}

class SharedStoragePrivateAggregationDisabledForSharedStorageOnlyBrowserTest
    : public SharedStorageBrowserTest {
 public:
  SharedStoragePrivateAggregationDisabledForSharedStorageOnlyBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        content::kPrivateAggregationApi,
        {{"enabled_in_shared_storage", "false"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SharedStoragePrivateAggregationDisabledForSharedStorageOnlyBrowserTest,
    PrivateAggregationNotDefined) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.sendHistogramReport({bucket: 1n, value: 2});
    )",
                         &out_script_url);

  ASSERT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("ReferenceError: privateAggregation is not defined",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[0].log_level);
}

class SharedStoragePrivateAggregationEnabledBrowserTest
    : public SharedStorageBrowserTest {
 public:
  // TODO(alexmt): Consider factoring out along with FLEDGE definition.
  class TestPrivateAggregationManagerImpl
      : public PrivateAggregationManagerImpl {
   public:
    TestPrivateAggregationManagerImpl(
        std::unique_ptr<PrivateAggregationBudgeter> budgeter,
        std::unique_ptr<PrivateAggregationHost> host)
        : PrivateAggregationManagerImpl(std::move(budgeter),
                                        std::move(host),
                                        /*storage_partition=*/nullptr) {}
  };

  SharedStoragePrivateAggregationEnabledBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(content::kPrivateAggregationApi);
  }

  void SetUpOnMainThread() override {
    SharedStorageBrowserTest::SetUpOnMainThread();

    SetBrowserClientForTesting(&browser_client_);

    a_test_origin_ = https_server()->GetOrigin("a.test");

    auto* storage_partition_impl =
        static_cast<StoragePartitionImpl*>(shell()
                                               ->web_contents()
                                               ->GetBrowserContext()
                                               ->GetDefaultStoragePartition());

    private_aggregation_host_ = new PrivateAggregationHost(
        /*on_report_request_received=*/mock_callback_.Get(),
        storage_partition_impl->browser_context());

    storage_partition_impl->OverridePrivateAggregationManagerForTesting(
        std::make_unique<TestPrivateAggregationManagerImpl>(
            std::make_unique<MockPrivateAggregationBudgeter>(),
            base::WrapUnique<PrivateAggregationHost>(
                private_aggregation_host_.get())));

    EXPECT_TRUE(NavigateToURL(
        shell(), https_server()->GetURL("a.test", kSimplePagePath)));
  }

  const base::MockRepeatingCallback<void(AggregatableReportRequest,
                                         PrivateAggregationBudgetKey)>&
  mock_callback() {
    return mock_callback_;
  }

  MockPrivateAggregationContentBrowserClient& browser_client() {
    return browser_client_;
  }

 protected:
  url::Origin a_test_origin_;

 private:
  raw_ptr<PrivateAggregationHost, DanglingUntriaged> private_aggregation_host_;

  base::test::ScopedFeatureList scoped_feature_list_;

  base::MockRepeatingCallback<void(AggregatableReportRequest,
                                   PrivateAggregationBudgetKey)>
      mock_callback_;

  MockPrivateAggregationContentBrowserClient browser_client_;
};

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       BasicTest) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke([&](AggregatableReportRequest request,
                                    PrivateAggregationBudgetKey budget_key) {
        ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
        EXPECT_EQ(request.payload_contents().contributions[0].bucket, 1);
        EXPECT_EQ(request.payload_contents().contributions[0].value, 2);
        EXPECT_EQ(request.shared_info().reporting_origin, a_test_origin_);
        EXPECT_EQ(budget_key.origin(), a_test_origin_);
        EXPECT_EQ(budget_key.api(),
                  PrivateAggregationBudgetKey::Api::kSharedStorage);
        run_loop.Quit();
      }));

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.sendHistogramReport({bucket: 1n, value: 2});
    )",
                         &out_script_url);

  EXPECT_TRUE(console_observer.messages().empty());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       RejectedTest) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.sendHistogramReport({bucket: -1n, value: 2});
    )",
                         &out_script_url);

  ASSERT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("TypeError: BigInt must be non-negative",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[0].log_level);
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       MultipleRequests) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke([&](AggregatableReportRequest request,
                                    PrivateAggregationBudgetKey budget_key) {
        ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
        EXPECT_EQ(request.payload_contents().contributions[0].bucket, 1);
        EXPECT_EQ(request.payload_contents().contributions[0].value, 2);
        EXPECT_EQ(request.shared_info().reporting_origin, a_test_origin_);
        EXPECT_EQ(budget_key.origin(), a_test_origin_);
        EXPECT_EQ(budget_key.api(),
                  PrivateAggregationBudgetKey::Api::kSharedStorage);
      }))
      .WillOnce(testing::Invoke([&](AggregatableReportRequest request,
                                    PrivateAggregationBudgetKey budget_key) {
        ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
        EXPECT_EQ(request.payload_contents().contributions[0].bucket, 3);
        EXPECT_EQ(request.payload_contents().contributions[0].value, 4);
        EXPECT_EQ(request.shared_info().reporting_origin, a_test_origin_);
        EXPECT_EQ(budget_key.origin(), a_test_origin_);
        EXPECT_EQ(budget_key.api(),
                  PrivateAggregationBudgetKey::Api::kSharedStorage);
        run_loop.Quit();
      }));

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.sendHistogramReport({bucket: 1n, value: 2});
      privateAggregation.sendHistogramReport({bucket: 3n, value: 4});
    )",
                         &out_script_url);

  EXPECT_TRUE(console_observer.messages().empty());

  run_loop.Run();
}

}  // namespace content

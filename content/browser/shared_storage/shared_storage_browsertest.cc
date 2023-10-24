// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/barrier_closure.h"
#include "base/functional/overloaded.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "base/time/time.h"
#include "content/browser/private_aggregation/private_aggregation_manager_impl.h"
#include "content/browser/private_aggregation/private_aggregation_test_utils.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/shared_storage/shared_storage_document_service_impl.h"
#include "content/browser/shared_storage/shared_storage_event_params.h"
#include "content/browser/shared_storage/shared_storage_header_observer.h"
#include "content/browser/shared_storage/shared_storage_worklet_driver.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/browser/shared_storage/shared_storage_worklet_host_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/shared_storage_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_select_url_fenced_frame_config_observer.h"
#include "content/public/test/test_shared_storage_header_observer.h"
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
#include "third_party/abseil-cpp/absl/types/variant.h"
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

const auto& SetOperation = SharedStorageWriteOperationAndResult::SetOperation;
const auto& AppendOperation =
    SharedStorageWriteOperationAndResult::AppendOperation;
const auto& DeleteOperation =
    SharedStorageWriteOperationAndResult::DeleteOperation;
const auto& ClearOperation =
    SharedStorageWriteOperationAndResult::ClearOperation;

const char kSimplePagePath[] = "/simple_page.html";

const char kTitle1Path[] = "/title1.html";

const char kTitle2Path[] = "/title2.html";

const char kFencedFramePath[] = "/fenced_frames/title0.html";

const char kPageWithBlankIframePath[] = "/page_with_blank_iframe.html";

const char kPngPath[] = "/shared_storage/pixel.png";

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

const int kSelectURLOverallBitBudget = 12;

const int kSelectURLSiteBitBudget = 6;

const char kGenerateURLsListScript[] = R"(
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

const char kRemainingBudgetPrefix[] = "remaining budget: ";

std::string TimeDeltaToString(base::TimeDelta delta) {
  return base::StrCat({base::NumberToString(delta.InMilliseconds()), "ms"});
}

using MockPrivateAggregationShellContentBrowserClient =
    MockPrivateAggregationContentBrowserClientBase<
        ContentBrowserTestContentBrowserClient>;

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

bool IsErrorMessage(const content::WebContentsConsoleObserver::Message& msg) {
  return msg.log_level == blink::mojom::ConsoleMessageLevel::kError;
}

auto describe_param = [](const auto& info) {
  return base::StrCat({"ResolveSelectURLTo", info.param ? "Config" : "URN"});
};

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

  // Separate from `WaitForWorkletResponses()` so that we can wait for it
  // without having to set an expected response count beforehand. The worklet
  // host won't exist before the first call to either `addModule(), `run()`, or
  // `selectURL()`. In the correct flow, `addModule()` will be called first.
  void WaitForAddModule() {
    if (add_module_called_) {
      ResetAddModuleCalledAndMaybeCloseWorklet();
      return;
    }

    add_module_waiter_ = std::make_unique<base::RunLoop>();
    add_module_waiter_->Run();
    add_module_waiter_.reset();
    ResetAddModuleCalledAndMaybeCloseWorklet();
  }

  // Only applies to `run()` and `selectURL()`. Must be set before calling the
  // operation. Precondition: Either `addModule()`, `run()`, or `selectURL()`
  // has previously been called so that this worklet host exists.
  void SetExpectedWorkletResponsesCount(size_t count) {
    expected_worklet_responses_count_ = count;
    response_expectation_set_ = true;
  }

  // Only applies to `run()` and `selectURL()`.
  // Precondition: `SetExpectedWorkletResponsesCount()` has been called with the
  // desired expected `count`, followed by the operation(s) itself/themselves.
  void WaitForWorkletResponses() {
    if (worklet_responses_count_ >= expected_worklet_responses_count_) {
      ResetResponseCountsAndMaybeCloseWorklet();
      return;
    }

    worklet_responses_count_waiter_ = std::make_unique<base::RunLoop>();
    worklet_responses_count_waiter_->Run();
    worklet_responses_count_waiter_.reset();
    ResetResponseCountsAndMaybeCloseWorklet();
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
    bool in_keep_alive = IsInKeepAlivePhase();
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
      OnAddModuleResponseReceived();
    if (!in_keep_alive) {
      ProcessAddModuleExpirationIfWorkletExpired();
    }
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
    bool in_keep_alive = IsInKeepAlivePhase();
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
    if (!in_keep_alive) {
      ProcessRunOrSelectURLExpirationIfWorkletExpired();
    }
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
    bool in_keep_alive = IsInKeepAlivePhase();
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
    if (!in_keep_alive) {
      ProcessRunOrSelectURLExpirationIfWorkletExpired();
    }
  }

  void ExpireWorklet() override {
    // We must defer the destruction of the expired worklet until the rest of
    // the test worklet code has run, in order to avoid segmentation faults. In
    // particular, if either the `add_module_waiter_` or the
    // `worklet_responses_count_waiter_` is running, we must quit it first
    // before we actually destroy the worklet (regardless of how many worklet
    // responses have been received). Hence we save a callback to destroy the
    // worklet after we quit the waiter. The `Quit()` will occur in
    // `Process*ExpirationIfWorkletExpired()` after returning to
    // `On*OnWorkletFinishedHelper()`. If no waiter is running, we still have to
    // finish running whichever `On*OnWorkletFinishedHelper()` triggered the
    // call to `ExpireWorklet()`.
    DCHECK(!pending_expire_worklet_callback_);
    pending_expire_worklet_callback_ =
        base::BindOnce(&TestSharedStorageWorkletHost::ExpireWorkletOnBaseClass,
                       weak_ptr_factory_.GetWeakPtr());
  }

  void ExpireWorkletOnBaseClass() { SharedStorageWorkletHost::ExpireWorklet(); }

  void ProcessAddModuleExpirationIfWorkletExpired() {
    if (!pending_expire_worklet_callback_) {
      return;
    }

    // We can't have both waiters running at the same time.
    DCHECK(!worklet_responses_count_waiter_);

    if (add_module_waiter_ && add_module_waiter_->running()) {
      // The worklet is expired and needs to be destroyed. Since
      // `add_module_waiter_` is running, quitting it will return us to
      // `WaitForAddModule()`, where the waiter will be reset, and then, in
      // `ResetAddModuleCalledAndMaybeCloseWorklet()`, the
      // `pending_expire_worklet_callback_` callback will be run.
      add_module_waiter_->Quit();
    }

    std::move(pending_expire_worklet_callback_).Run();

    // Do not add code after this. The worklet has been destroyed.
  }

  void ProcessRunOrSelectURLExpirationIfWorkletExpired() {
    if (!pending_expire_worklet_callback_) {
      return;
    }

    // We can't have both waiters running at the same time.
    DCHECK(!add_module_waiter_);

    if (worklet_responses_count_waiter_ &&
        worklet_responses_count_waiter_->running()) {
      // The worklet is expired and needs to be destroyed. Since
      // `worklet_responses_count_waiter_` is running, quitting it will return
      // us to `WaitForWorkletResponses()`, where the waiter will be reset, and
      // then, in `ResetResponseCountsAndMaybeCloseWorklet()`, the
      // `pending_expire_worklet_callback_` callback will be run.
      worklet_responses_count_waiter_->Quit();
      return;
    }

    if (response_expectation_set_) {
      // We expect a call to `WaitForWorkletResponses()`, which will run the
      // callback.
      return;
    }

    // No response expectation has been set, so we do expect a call to
    // `WaitForWorkletResponses()`.
    std::move(pending_expire_worklet_callback_).Run();

    // Do not add code after this. The worklet has been destroyed.
  }

  void OnAddModuleResponseReceived() {
    add_module_called_ = true;

    if (add_module_waiter_ && add_module_waiter_->running()) {
      add_module_waiter_->Quit();
    }
  }

  void OnWorkletResponseReceived() {
    ++worklet_responses_count_;

    if (worklet_responses_count_waiter_ &&
        worklet_responses_count_waiter_->running() &&
        worklet_responses_count_ >= expected_worklet_responses_count_) {
      worklet_responses_count_waiter_->Quit();
    }
  }

  void ResetAddModuleCalledAndMaybeCloseWorklet() {
    add_module_called_ = false;

    if (pending_expire_worklet_callback_) {
      std::move(pending_expire_worklet_callback_).Run();

      // Do not add code after this. The worklet has been destroyed.
    }
  }

  void ResetResponseCountsAndMaybeCloseWorklet() {
    expected_worklet_responses_count_ = 0u;
    worklet_responses_count_ = 0u;
    response_expectation_set_ = false;

    if (pending_expire_worklet_callback_) {
      std::move(pending_expire_worklet_callback_).Run();

      // Do not add code after this. The worklet has been destroyed.
    }
  }

  base::TimeDelta GetKeepAliveTimeout() const override {
    // Configure a timeout large enough so that the scheduled task won't run
    // automatically. Instead, we will manually call OneShotTimer::FireNow().
    return base::Seconds(30);
  }

  // Whether or not `addModule()` has been called since the last time (if any)
  // that `add_module_waiter_` was reset.
  bool add_module_called_ = false;
  std::unique_ptr<base::RunLoop> add_module_waiter_;

  // How many worklet operations have finished. This only includes `selectURL()`
  // and `run()`.
  size_t worklet_responses_count_ = 0;
  size_t expected_worklet_responses_count_ = 0;
  bool response_expectation_set_ = false;
  std::unique_ptr<base::RunLoop> worklet_responses_count_waiter_;

  // Whether we should defer messages received from the worklet environment to
  // handle them later. This includes request callbacks (e.g. for `addModule()`,
  // `selectURL()` and `run()`), as well as commands initiated from the worklet
  // (e.g. `console.log()`).
  bool should_defer_worklet_messages_;
  std::vector<base::OnceClosure> pending_worklet_messages_;

  // This callback will be non-null if the worklet is pending expiration due to
  // the option `keepAlive: false` (which is the default value) being received
  // in the most recent call to `run()` or `selectURL()`.
  base::OnceClosure pending_expire_worklet_callback_;

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

  void OnUrnUuidGenerated(const GURL& urn_uuid) override {}

  void OnConfigPopulated(
      const absl::optional<FencedFrameConfig>& config) override {}

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

class SharedStorageBrowserTestBase : public ContentBrowserTest {
 public:
  using AccessType = TestSharedStorageObserver::AccessType;

  SharedStorageBrowserTestBase() {
    privacy_sandbox_ads_apis_override_feature_.InitAndEnableFeature(
        features::kPrivacySandboxAdsAPIsOverride);

    shared_storage_feature_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{blink::features::kSharedStorageAPI,
          {
              {"SharedStorageBitBudget", base::NumberToString(kBudgetAllowed)},
              {"SharedStorageStalenessThreshold",
               TimeDeltaToString(base::Days(kStalenessThresholdDays))},
          }}},
        /*disabled_features=*/{});

    fenced_frame_feature_.InitAndEnableFeature(blink::features::kFencedFrames);
  }

  void SetUpOnMainThread() override {
    auto test_worklet_host_manager =
        std::make_unique<TestSharedStorageWorkletHostManager>();
    observer_ = std::make_unique<TestSharedStorageObserver>();

    test_worklet_host_manager->AddSharedStorageObserver(observer_.get());
    test_worklet_host_manager_ = test_worklet_host_manager.get();

    static_cast<StoragePartitionImpl*>(GetStoragePartition())
        ->OverrideSharedStorageWorkletHostManagerForTesting(
            std::move(test_worklet_host_manager));

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

  MockPrivateAggregationShellContentBrowserClient& browser_client() {
    return *browser_client_;
  }

  virtual bool ResolveSelectURLToConfig() { return false; }

  StoragePartition* GetStoragePartition() {
    return shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition();
  }

  void TearDownOnMainThread() override {
    test_worklet_host_manager_->RemoveSharedStorageObserver(observer_.get());
  }

  // Virtual so that derived classes can use a different flavor of mock instead
  // of `testing::NiceMock`.
  virtual void MakeMockPrivateAggregationShellContentBrowserClient() {
    browser_client_ = std::make_unique<
        testing::NiceMock<MockPrivateAggregationShellContentBrowserClient>>();
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
    static_cast<StoragePartitionImpl*>(GetStoragePartition())
        ->GetSharedStorageManager()
        ->GetRemainingBudget(net::SchemefulSite(origin), future.GetCallback());
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

  void ExecuteScriptInWorklet(
      const ToRenderFrameHost& execution_target,
      const std::string& script,
      GURL* out_module_script_url,
      size_t expected_total_host_count = 1u,
      bool keep_alive_after_operation = true,
      absl::optional<std::string> context_id = absl::nullopt,
      std::string* out_error = nullptr,
      bool wait_for_operation_finish = true) {
    DCHECK(out_module_script_url);

    base::StringPairs run_function_body_replacement;
    run_function_body_replacement.emplace_back("{{RUN_FUNCTION_BODY}}", script);

    std::string host =
        execution_target.render_frame_host()->GetLastCommittedOrigin().host();

    *out_module_script_url = https_server()->GetURL(
        host, net::test_server::GetFilePathWithReplacements(
                  "/shared_storage/customizable_module.js",
                  run_function_body_replacement));

    EXPECT_TRUE(ExecJs(execution_target,
                       JsReplace("sharedStorage.worklet.addModule($1)",
                                 *out_module_script_url)));

    // There may be more than one host in the worklet host manager if we are
    // executing inside a nested fenced frame that was created using
    // `selectURL()`.
    EXPECT_EQ(expected_total_host_count,
              test_worklet_host_manager().GetAttachedWorkletHostsCount());

    EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

    // There is 1 more "worklet operation": `run()`.
    test_worklet_host_manager()
        .GetAttachedWorkletHostForFrame(execution_target.render_frame_host())
        ->SetExpectedWorkletResponsesCount(1);

    EXPECT_TRUE(ExecJs(
        execution_target,
        JsReplace("window.keepWorklet = $1;", keep_alive_after_operation)));

    testing::AssertionResult result = ExecJs(
        execution_target,
        base::StrCat(
            {"sharedStorage.run('test-operation', {keepAlive: keepWorklet",
             context_id.has_value()
                 ? JsReplace(", privateAggregationConfig: {contextId: $1}});",
                             context_id.value())
                 : "});"}));
    EXPECT_EQ(!!result, out_error == nullptr);
    if (out_error) {
      *out_error = std::string(result.message());
      return;
    }

    if (wait_for_operation_finish) {
      test_worklet_host_manager()
          .GetAttachedWorkletHostForFrame(execution_target.render_frame_host())
          ->WaitForWorkletResponses();
    }
  }

  FrameTreeNode* CreateIFrame(FrameTreeNode* root, const GURL& url) {
    size_t initial_child_count = root->child_count();

    EXPECT_TRUE(ExecJs(root,
                       "var f = document.createElement('iframe');"
                       "document.body.appendChild(f);"));

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
  absl::optional<GURL> SelectFrom8URLsInContext(
      const url::Origin& origin,
      FrameTreeNode* parent_node = nullptr,
      bool keep_alive_after_operation = true) {
    if (!parent_node)
      parent_node = PrimaryFrameTreeNodeRoot();

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
    test_worklet_host_manager()
        .GetAttachedWorkletHostForFrame(iframe->current_frame_host())
        ->SetExpectedWorkletResponsesCount(1);

    // Generate 8 candidates urls in to a list variable `urls`.
    EXPECT_TRUE(ExecJs(iframe, kGenerateURLsListScript));
    EXPECT_TRUE(
        ExecJs(iframe, JsReplace("window.resolveSelectURLToConfig = $1;",
                                 ResolveSelectURLToConfig())));

    TestSelectURLFencedFrameConfigObserver config_observer(
        GetStoragePartition());
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

    EXPECT_TRUE(result.error.empty());
    const absl::optional<GURL>& observed_urn_uuid =
        config_observer.GetUrnUuid();
    if (observed_urn_uuid.has_value()) {
      EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

      if (!ResolveSelectURLToConfig()) {
        EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
      }

      test_worklet_host_manager()
          .GetAttachedWorkletHostForFrame(iframe->current_frame_host())
          ->WaitForWorkletResponses();
    }
    return observed_urn_uuid;
  }

  // Prerequisite: The worklet for `frame` has registered a
  // "remaining-budget-operation" that logs the remaining budget to the console
  // after `kRemainingBudgetPrefix`. Also, if any previous operations are
  // called, they use the option `keepAlive: true`.
  double RemainingBudgetViaJSForFrame(FrameTreeNode* frame,
                                      bool keep_alive_after_operation = true) {
    DCHECK(frame);

    WebContentsConsoleObserver console_observer(shell()->web_contents());
    const std::string kRemainingBudgetPrefixStr(kRemainingBudgetPrefix);
    console_observer.SetPattern(base::StrCat({kRemainingBudgetPrefixStr, "*"}));
    EXPECT_TRUE(ExecJs(frame, JsReplace("window.keepWorklet = $1;",
                                        keep_alive_after_operation)));

    // There is 1 "worklet operation": `run()`.
    test_worklet_host_manager()
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

    test_worklet_host_manager()
        .GetAttachedWorkletHostForFrame(frame->current_frame_host())
        ->WaitForWorkletResponses();
    return result;
  }

  double RemainingBudgetViaJSForOrigin(const url::Origin& origin) {
    FrameTreeNode* iframe =
        CreateIFrame(PrimaryFrameTreeNodeRoot(), origin.GetURL());

    EXPECT_TRUE(ExecJs(iframe, R"(
        sharedStorage.worklet.addModule('shared_storage/simple_module.js');
      )"));

    return RemainingBudgetViaJSForFrame(iframe);
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  TestSharedStorageWorkletHostManager& test_worklet_host_manager() {
    DCHECK(test_worklet_host_manager_);
    return *test_worklet_host_manager_;
  }

  ~SharedStorageBrowserTestBase() override = default;

 protected:
  test::FencedFrameTestHelper fenced_frame_test_helper_;

  base::test::ScopedFeatureList privacy_sandbox_ads_apis_override_feature_;
  base::test::ScopedFeatureList shared_storage_feature_;
  base::test::ScopedFeatureList fenced_frame_feature_;

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  base::HistogramTester histogram_tester_;

  raw_ptr<TestSharedStorageWorkletHostManager, DanglingUntriaged>
      test_worklet_host_manager_ = nullptr;
  std::unique_ptr<TestSharedStorageObserver> observer_;

  std::unique_ptr<MockPrivateAggregationShellContentBrowserClient>
      browser_client_;
};

class SharedStorageBrowserTest : public SharedStorageBrowserTestBase,
                                 public testing::WithParamInterface<bool> {
 public:
  SharedStorageBrowserTest() {
    fenced_frame_api_change_feature_.InitWithFeatureState(
        blink::features::kFencedFramesAPIChanges, ResolveSelectURLToConfig());
  }

  bool ResolveSelectURLToConfig() override { return GetParam(); }

  ~SharedStorageBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList fenced_frame_api_change_feature_;
};

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, AddModule_Success) {
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

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, AddModule_ScriptNotFound) {
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

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, AddModule_RedirectNotAllowed) {
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

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       AddModule_ScriptExecutionFailure) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/erroneous_module.js');
    )");

  EXPECT_THAT(
      result.error,
      testing::HasSubstr("ReferenceError: undefinedVariable is not defined"));

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

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
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

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       AddModule_SecondAddModuleAfterKeepAliveTrueRun_Failure) {
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

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'},
                             keepAlive: true});
    )"));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  std::string expected_error =
      "a JavaScript error: \"Error: sharedStorage.worklet.addModule() can only "
      "be invoked once per browsing context.\"\n";

  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )");
  EXPECT_EQ(expected_error, result.error);

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());
  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("{\"customKey\":\"customValue\"}",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));

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
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               blink::CloneableMessage())},
       {AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    AddModule_SecondAddModuleAfterKeepAliveFalseRun_Failure) {
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

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'},
                             keepAlive: false});
    )"));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  std::string expected_error = base::StrCat(
      {"a JavaScript error: \"Error: ", kSharedStorageWorkletExpiredMessage,
       "\"\n"});

  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )");
  EXPECT_EQ(expected_error, result.error);

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());
  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("{\"customKey\":\"customValue\"}",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));

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
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               blink::CloneableMessage())}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    AddModule_SecondAddModuleAfterKeepAliveDefaultRun_Failure) {
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

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  std::string expected_error = base::StrCat(
      {"a JavaScript error: \"Error: ", kSharedStorageWorkletExpiredMessage,
       "\"\n"});

  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )");
  EXPECT_EQ(expected_error, result.error);

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());
  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("{\"customKey\":\"customValue\"}",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));

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
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               blink::CloneableMessage())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, RunOperation_Success) {
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

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

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
                                               blink::CloneableMessage())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
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
          'test-operation', {data: {'customKey': 'customValue'},
                             keepAlive: true});
    )"));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  // We cannot set the expected number of responses for `run()/selectURL()`
  // until the worklet host is created. Normally we set these expectations after
  // the call to `addModule()` and before making any calls to `run()` or
  // `selectURL()`. Yet, here `run()` and `addModule()` are intentionally called
  // in the wrong order.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

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
                                               blink::CloneableMessage())},
       {AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
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

  EXPECT_THAT(
      result.error,
      testing::HasSubstr("function testFunction() {} could not be cloned"));

  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 0);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       RunOperation_VerifyUndefinedData) {
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

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run('test-operation', /*options=*/{});
    )"));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       RunOperation_Failure_BlobDataTypeNotSupportedInWorklet) {
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

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      const blob = new Blob(["abc"], {type: 'text/plain'});
      sharedStorage.run('test-operation', /*options=*/{data: blob});
    )"));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("Cannot deserialize data.",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       RunOperation_VerifyCryptoKeyData) {
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

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      const myPromise = new Promise((resolve, reject) => {
        crypto.subtle.generateKey(
          {
            name: "AES-GCM",
            length: 256,
          },
          true,
          ["encrypt", "decrypt"]
        ).then((key) => {
          sharedStorage.run('test-operation', /*options=*/{data: key})
                       .then(() => { resolve(); });
        });
      });
    )"));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ(
      "CryptoKey, algorithm: {\"length\":256,\"name\":\"AES-GCM\"} usages: "
      "[\"encrypt\",\"decrypt\"] extractable: true",
      base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
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

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  EXPECT_EQ(4u, console_observer.messages().size());
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kInfo,
            console_observer.messages()[2].log_level);
  EXPECT_THAT(
      base::UTF16ToUTF8(console_observer.messages()[3].message),
      testing::HasSubstr("ReferenceError: undefinedVariable is not defined"));
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
                                               blink::CloneableMessage())}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    RunOperation_SecondRunOperationAfterKeepAliveTrueRun_Success) {
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

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'},
                             keepAlive: true});
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("{\"customKey\":\"customValue\"}",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  EXPECT_EQ(8u, console_observer.messages().size());
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[5].message));
  EXPECT_EQ("{\"customKey\":\"customValue\"}",
            base::UTF16ToUTF8(console_observer.messages()[6].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[7].message));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 2);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               blink::CloneableMessage())},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               blink::CloneableMessage())}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    RunOperation_SecondRunOperationAfterKeepAliveFalseRun_Failure) {
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

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'},
                             keepAlive: false});
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("{\"customKey\":\"customValue\"}",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));

  std::string expected_error = base::StrCat(
      {"a JavaScript error: \"Error: ", kSharedStorageWorkletExpiredMessage,
       "\"\n"});

  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )");
  EXPECT_EQ(expected_error, result.error);

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               blink::CloneableMessage())}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    RunOperation_SecondRunOperationAfterKeepAliveDefaultRun_Failure) {
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

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("{\"customKey\":\"customValue\"}",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));

  std::string expected_error = base::StrCat(
      {"a JavaScript error: \"Error: ", kSharedStorageWorkletExpiredMessage,
       "\"\n"});

  EvalJsResult result = EvalJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )");
  EXPECT_EQ(expected_error, result.error);

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               blink::CloneableMessage())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, WorkletDestroyed) {
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

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, TwoWorklets) {
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

IN_PROC_BROWSER_TEST_P(
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

  test_worklet_host_manager().GetKeepAliveWorkletHost()->WaitForAddModule();

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

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
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

  test_worklet_host_manager().GetKeepAliveWorkletHost()->WaitForAddModule();

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

IN_PROC_BROWSER_TEST_P(
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

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

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
      ->WaitForWorkletResponses();

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
                                               blink::CloneableMessage())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, KeepAlive_SubframeWorklet) {
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
  test_worklet_host_manager().GetKeepAliveWorkletHost()->WaitForAddModule();

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

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
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
IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    SelectURL_BudgetMetadata_OperationSuccess_SingleInputURL) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());
  // There is 1 more "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html",
              reportingMetadata: {
                "click": "fenced_frames/report1.html",
                "mouse interaction": "fenced_frames/report2.html"
              }
            }
          ],
          {
            data: {'mockResult': 0},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result.error.empty());
  const absl::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
  }

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid_, observed_urn_uuid.value());

  SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(observed_urn_uuid.value());
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->site,
            net::SchemefulSite(https_server()->GetOrigin("a.test")));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, 0.0);

  EXPECT_THAT(GetSharedStorageReportingMap(observed_urn_uuid.value()),
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
            "test-url-selection-operation", blink::CloneableMessage(),
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
IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    SelectURL_BudgetMetadata_OperationFailure_SingleInputURL) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());
  // There is 1 more "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html",
              reportingMetadata: {
                "click": "fenced_frames/report1.html"
              }
            }
          ],
          {
            data: {'mockResult': -1},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result.error.empty());
  const absl::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
  }

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid_, observed_urn_uuid.value());

  SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(observed_urn_uuid.value());
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->site,
            net::SchemefulSite(https_server()->GetOrigin("a.test")));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, 0.0);

  EXPECT_THAT(GetSharedStorageReportingMap(observed_urn_uuid.value()),
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
            "test-url-selection-operation", blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {{"click",
                    https_server()
                        ->GetURL("a.test", "/fenced_frames/report1.html")
                        .spec()}}}}))}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
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

  EXPECT_TRUE(ExecJs(iframe, JsReplace("window.resolveSelectURLToConfig = $1;",
                                       ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());
  // There is 1 more "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(iframe, R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            },
            {
              url: "fenced_frames/title1.html",
              reportingMetadata: {
                "click": "fenced_frames/report1.html"
              }
            },
            {
              url: "fenced_frames/title2.html"
            }
          ],
          {
            data: {'mockResult': 1},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result.error.empty());
  const absl::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
  }

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid_, observed_urn_uuid.value());

  SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(observed_urn_uuid.value());
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->site,
            net::SchemefulSite(https_server()->GetOrigin("b.test")));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, std::log2(3));

  SharedStorageReportingMap reporting_map =
      GetSharedStorageReportingMap(observed_urn_uuid.value());
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
            "test-url-selection-operation", blink::CloneableMessage(),
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

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    SelectURL_SecondSelectURLAfterKeepAliveTrueSelectURL_Success) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer1(
      GetStoragePartition());
  // There is 1 more "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result1 = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            }
          ],
          {
            data: {'mockResult': 0},
            resolveToConfig: resolveSelectURLToConfig,
            keepAlive: true
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result1.error.empty());
  const absl::optional<GURL>& observed_urn_uuid1 =
      config_observer1.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid1.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid1.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result1.ExtractString(), observed_urn_uuid1->spec());
  }

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer1.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config1 =
      config_observer1.GetConfig();
  EXPECT_TRUE(fenced_frame_config1.has_value());
  EXPECT_EQ(fenced_frame_config1->urn_uuid_, observed_urn_uuid1.value());

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  TestSelectURLFencedFrameConfigObserver config_observer2(
      GetStoragePartition());

  // There is 1 more "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result2 = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            }
          ],
          {
            data: {'mockResult': 0},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result2.error.empty());
  const absl::optional<GURL>& observed_urn_uuid2 =
      config_observer2.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid2.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid2.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result2.ExtractString(), observed_urn_uuid2->spec());
  }

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer2.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config2 =
      config_observer2.GetConfig();
  EXPECT_TRUE(fenced_frame_config2.has_value());
  EXPECT_EQ(fenced_frame_config2->urn_uuid_, observed_urn_uuid2.value());

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     2);

  std::string origin_str = url::Origin::Create(main_url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))},
       {AccessType::kDocumentSelectURL, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSelectURL(
            "test-url-selection-operation", blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {}}}))},
       {AccessType::kDocumentSelectURL, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSelectURL(
            "test-url-selection-operation", blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {}}}))}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    SelectURL_SecondSelectURLAfterKeepAliveFalseSelectURL_Failure) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer1(
      GetStoragePartition());

  // There is 1 more "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result1 = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            }
          ],
          {
            data: {'mockResult': 0},
            resolveToConfig: resolveSelectURLToConfig,
            keepAlive: false
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result1.error.empty());
  const absl::optional<GURL>& observed_urn_uuid1 =
      config_observer1.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid1.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid1.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result1.ExtractString(), observed_urn_uuid1->spec());
  }

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer1.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config1 =
      config_observer1.GetConfig();
  EXPECT_TRUE(fenced_frame_config1.has_value());
  EXPECT_EQ(fenced_frame_config1->urn_uuid_, observed_urn_uuid1.value());

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  TestSelectURLFencedFrameConfigObserver config_observer2(
      GetStoragePartition());

  std::string expected_error = base::StrCat(
      {"a JavaScript error: \"Error: ", kSharedStorageWorkletExpiredMessage,
       "\"\n"});

  EvalJsResult result2 = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            }
          ],
          {
            data: {'mockResult': 0},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_EQ(expected_error, result2.error);

  EXPECT_FALSE(config_observer2.ConfigObserved());

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
            "test-url-selection-operation", blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {}}}))}});
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    SelectURL_SecondSelectURLAfterKeepAliveDefaultSelectURL_Failure) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer1(
      GetStoragePartition());

  // There is 1 more "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  const char select_url_script[] = R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            }
          ],
          {
            data: {'mockResult': 0},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )";

  EvalJsResult result1 = EvalJs(shell(), select_url_script);

  EXPECT_TRUE(result1.error.empty());
  const absl::optional<GURL>& observed_urn_uuid1 =
      config_observer1.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid1.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid1.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result1.ExtractString(), observed_urn_uuid1->spec());
  }

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer1.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config1 =
      config_observer1.GetConfig();
  EXPECT_TRUE(fenced_frame_config1.has_value());
  EXPECT_EQ(fenced_frame_config1->urn_uuid_, observed_urn_uuid1.value());

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  TestSelectURLFencedFrameConfigObserver config_observer2(
      GetStoragePartition());

  std::string expected_error = base::StrCat(
      {"a JavaScript error: \"Error: ", kSharedStorageWorkletExpiredMessage,
       "\"\n"});

  EvalJsResult result2 = EvalJs(shell(), select_url_script);

  EXPECT_EQ(expected_error, result2.error);

  EXPECT_FALSE(config_observer2.ConfigObserved());

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
            "test-url-selection-operation", blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {}}}))}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       SelectURL_SelectURLAfterKeepAliveFalseRun_Failure) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'},
                             keepAlive: false});
    )"));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());

  std::string expected_error = base::StrCat(
      {"a JavaScript error: \"Error: ", kSharedStorageWorkletExpiredMessage,
       "\"\n"});

  EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            }
          ],
          {
            data: {'mockResult': 0},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_EQ(expected_error, result.error);

  EXPECT_FALSE(config_observer.ConfigObserved());

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());
  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("{\"customKey\":\"customValue\"}",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(main_url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               blink::CloneableMessage())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       SelectURL_SelectURLAfterKeepAliveTrueRun_Success) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'},
                             keepAlive: true});
    )"));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("{\"customKey\":\"customValue\"}",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  // There is 1 more "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            }
          ],
          {
            data: {'mockResult': 0},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result.error.empty());
  const absl::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
  }

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid_, observed_urn_uuid.value());

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram,
                     kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);

  std::string origin_str = url::Origin::Create(main_url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               blink::CloneableMessage())},
       {AccessType::kDocumentSelectURL, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSelectURL(
            "test-url-selection-operation", blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {}}}))}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       SelectURL_SelectURLAfterKeepAliveDefaultRun_Failure) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());

  std::string expected_error = base::StrCat(
      {"a JavaScript error: \"Error: ", kSharedStorageWorkletExpiredMessage,
       "\"\n"});

  EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            }
          ],
          {
            data: {'mockResult': 0},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_EQ(expected_error, result.error);

  EXPECT_FALSE(config_observer.ConfigObserved());

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());
  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("Start executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("{\"customKey\":\"customValue\"}",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages()[4].message));

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);

  std::string origin_str = url::Origin::Create(main_url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               blink::CloneableMessage())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       RunOperation_RunAfterKeepAliveTrueSelectURL_Success) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  // There is 1 more "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            }
          ],
          {
            data: {'mockResult': 0},
            resolveToConfig: resolveSelectURLToConfig,
            keepAlive: true
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result.error.empty());
  const absl::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
  }

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid_, observed_urn_uuid.value());

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )"));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  EXPECT_EQ("Finish executing 'test-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  WaitForHistograms({kTimingRunExecutedInWorkletHistogram,
                     kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingRunExecutedInWorkletHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);

  std::string origin_str = url::Origin::Create(main_url).Serialize();
  ExpectAccessObserved(
      {{AccessType::kDocumentAddModule, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAddModule(https_server()->GetURL(
            "a.test", "/shared_storage/simple_module.js"))},
       {AccessType::kDocumentSelectURL, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSelectURL(
            "test-url-selection-operation", blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {}}}))},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               blink::CloneableMessage())}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       RunOperation_RunAfterKeepAliveFalseSelectURL_Failure) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  // There is 1 more "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result1 = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            }
          ],
          {
            data: {'mockResult': 0},
            resolveToConfig: resolveSelectURLToConfig,
            keepAlive: false
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result1.error.empty());
  const absl::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result1.ExtractString(), observed_urn_uuid->spec());
  }

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid_, observed_urn_uuid.value());

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  std::string expected_error = base::StrCat(
      {"a JavaScript error: \"Error: ", kSharedStorageWorkletExpiredMessage,
       "\"\n"});

  EvalJsResult result2 = EvalJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )");
  EXPECT_EQ(expected_error, result2.error);

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

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
            "test-url-selection-operation", blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {}}}))}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       RunOperation_RunAfterKeepAliveDefaultSelectURL_Failure) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("Start executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("Finish executing simple_module.js",
            base::UTF16ToUTF8(console_observer.messages()[1].message));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  // There is 1 more "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result1 = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            }
          ],
          {
            data: {'mockResult': 0},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result1.error.empty());
  const absl::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result1.ExtractString(), observed_urn_uuid->spec());
  }

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid_, observed_urn_uuid.value());

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  std::string expected_error = base::StrCat(
      {"a JavaScript error: \"Error: ", kSharedStorageWorkletExpiredMessage,
       "\"\n"});

  EvalJsResult result2 = EvalJs(shell(), R"(
      sharedStorage.run(
          'test-operation', {data: {'customKey': 'customValue'}});
    )");
  EXPECT_EQ(expected_error, result2.error);

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

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
            "test-url-selection-operation", blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {}}}))}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
                       SelectURL_ReportingMetadata_EmptyReportEvent) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());
  // There is 1 more "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html",
              reportingMetadata: {
                "": "fenced_frames/report1.html"
              }
            }
          ],
          {
            data: {'mockResult': 0},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result.error.empty());
  const absl::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
  }

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid_, observed_urn_uuid.value());

  SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(observed_urn_uuid.value());
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->site,
            net::SchemefulSite(https_server()->GetOrigin("a.test")));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, 0.0);

  EXPECT_THAT(GetSharedStorageReportingMap(observed_urn_uuid.value()),
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
            "test-url-selection-operation", blink::CloneableMessage(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {{"", https_server()
                            ->GetURL("a.test", "/fenced_frames/report1.html")
                            .spec()}}}}))}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, SetAppendOperationInDocument) {
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
                                               blink::CloneableMessage())},
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

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, DeleteOperationInDocument) {
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
                                               blink::CloneableMessage())},
       {AccessType::kWorkletLength, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()},
       {AccessType::kWorkletGet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForGetOrDelete("key0")}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, ClearOperationInDocument) {
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
                                               blink::CloneableMessage())},
       {AccessType::kWorkletLength, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, SetAppendOperationInWorklet) {
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
                                               blink::CloneableMessage())},
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

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
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
  EXPECT_THAT(base::UTF16ToUTF8(console_observer.messages()[0].message),
              testing::HasSubstr("sharedStorage.append() failed"));
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
                                               blink::CloneableMessage())},
       {AccessType::kWorkletSet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSet("key0", std::string(1024, 'a'),
                                               false)},
       {AccessType::kWorkletAppend, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForAppend("key0", "a")}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, DeleteOperationInWorklet) {
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
                                               blink::CloneableMessage())},
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

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, ClearOperationInWorklet) {
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
                                               blink::CloneableMessage())},
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

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, GetOperationInWorklet) {
  base::SimpleTestClock clock;
  base::RunLoop loop;
  static_cast<StoragePartitionImpl*>(GetStoragePartition())
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

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('get-operation', {data: {'key': 'key0'},
                                            keepAlive: true});
      )"));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  // Advance clock so that key will expire.
  clock.Advance(base::Days(kStalenessThresholdDays) + base::Seconds(1));

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('get-operation', {data: {'key': 'key0'}});
      )"));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

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
                                               blink::CloneableMessage())},
       {AccessType::kWorkletLength, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()},
       {AccessType::kWorkletGet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForGetOrDelete("key0")},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("get-operation",
                                               blink::CloneableMessage())},
       {AccessType::kWorkletLength, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()},
       {AccessType::kWorkletGet, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForGetOrDelete("key0")}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
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
                                               blink::CloneableMessage())},
       {AccessType::kWorkletLength, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
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
                                               blink::CloneableMessage())},
       {AccessType::kWorkletLength, MainFrameId(), origin2_str,
        SharedStorageEventParams::CreateDefault()}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest, KeysAndEntriesOperation) {
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
                                               blink::CloneableMessage())},
       {AccessType::kWorkletKeys, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()},
       {AccessType::kWorkletEntries, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateDefault()}});
}

IN_PROC_BROWSER_TEST_P(SharedStorageBrowserTest,
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
  expected_accesses.emplace_back(
      AccessType::kDocumentRun, MainFrameId(), origin_str,
      SharedStorageEventParams::CreateForRun("test-operation",
                                             blink::CloneableMessage()));
  expected_accesses.emplace_back(AccessType::kWorkletKeys, MainFrameId(),
                                 origin_str,
                                 SharedStorageEventParams::CreateDefault());
  expected_accesses.emplace_back(AccessType::kWorkletEntries, MainFrameId(),
                                 origin_str,
                                 SharedStorageEventParams::CreateDefault());
  ExpectAccessObserved(expected_accesses);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SharedStorageBrowserTest,
                         testing::Bool(),
                         describe_param);

class SharedStorageAllowURNsInIframesBrowserTest
    : public SharedStorageBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  SharedStorageAllowURNsInIframesBrowserTest() {
    allow_urns_in_frames_feature_.InitAndEnableFeature(
        blink::features::kAllowURNsInIframes);
    fenced_frame_api_change_feature_.InitWithFeatureState(
        blink::features::kFencedFramesAPIChanges, ResolveSelectURLToConfig());
  }

  bool ResolveSelectURLToConfig() override { return GetParam(); }

 private:
  base::test::ScopedFeatureList allow_urns_in_frames_feature_;
  base::test::ScopedFeatureList fenced_frame_api_change_feature_;
};

IN_PROC_BROWSER_TEST_P(SharedStorageAllowURNsInIframesBrowserTest,
                       RenderSelectURLResultInIframe) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  absl::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(shared_storage_origin);
  ASSERT_TRUE(urn_uuid.has_value());

  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), *urn_uuid);

  EXPECT_EQ(iframe_node->current_url(),
            https_server()->GetURL("b.test", "/fenced_frames/title1.html"));

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);

  GURL new_page_url = https_server()->GetURL("c.test", kSimplePagePath);

  TestNavigationObserver top_navigation_observer(shell()->web_contents());
  EXPECT_TRUE(
      ExecJs(iframe_node, JsReplace("top.location = $1", new_page_url)));
  top_navigation_observer.Wait();

  // After the top navigation, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SharedStorageAllowURNsInIframesBrowserTest,
                         testing::Bool(),
                         describe_param);

class SharedStorageFencedFrameInteractionBrowserTestBase
    : public SharedStorageBrowserTestBase {
 public:
  using FencedFrameNavigationTarget = absl::variant<GURL, std::string>;

  // TODO(crbug.com/1414429): This function should be removed. Use
  // `CreateFencedFrame` in fenced_frame_test_util.h instead.
  FrameTreeNode* CreateFencedFrame(FrameTreeNode* root,
                                   const FencedFrameNavigationTarget& target) {
    size_t initial_child_count = root->child_count();

    EXPECT_TRUE(ExecJs(root,
                       "var f = document.createElement('fencedframe');"
                       "document.body.appendChild(f);"));

    EXPECT_EQ(initial_child_count + 1, root->child_count());
    FrameTreeNode* fenced_frame_root_node =
        GetFencedFrameRootNode(root->child_at(initial_child_count));

    TestFrameNavigationObserver observer(fenced_frame_root_node);

    EvalJsResult result = NavigateFencedFrame(root, target);

    observer.Wait();

    return fenced_frame_root_node;
  }

  FrameTreeNode* CreateFencedFrame(const FencedFrameNavigationTarget& target) {
    return CreateFencedFrame(PrimaryFrameTreeNodeRoot(), target);
  }

  EvalJsResult NavigateFencedFrame(FrameTreeNode* root,
                                   const FencedFrameNavigationTarget& target) {
    return EvalJs(
        root,
        absl::visit(base::Overloaded{
                        [](const GURL& url) {
                          return JsReplace(
                              "f.config = new FencedFrameConfig($1);", url);
                        },
                        [](const std::string& config) {
                          return JsReplace("f.config = window[$1]", config);
                        },
                    },
                    target));
  }

  // Precondition: There is exactly one existing fenced frame.
  void NavigateExistingFencedFrame(FrameTreeNode* existing_fenced_frame,
                                   const FencedFrameNavigationTarget& target) {
    FrameTreeNode* root = PrimaryFrameTreeNodeRoot();
    TestFrameNavigationObserver observer(existing_fenced_frame);

    EXPECT_TRUE(ExecJs(root, "var f = document.querySelector('fencedframe');"));
    EvalJsResult result = NavigateFencedFrame(root, target);

    observer.Wait();
  }
};

class SharedStorageFencedFrameInteractionBrowserTest
    : public SharedStorageFencedFrameInteractionBrowserTestBase {
 public:
  bool ResolveSelectURLToConfig() override { return true; }
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

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());
  // There is 1 more "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            },
            {
              url: "fenced_frames/title1.html",
              reportingMetadata: {
                "click": "fenced_frames/report1.html"
              }
            },
            {
              url: "fenced_frames/title2.html"
            }
          ],
          {
            data: {'mockResult': 1},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result.error.empty());
  const absl::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid_, observed_urn_uuid.value());

  SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(observed_urn_uuid.value());
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->site,
            net::SchemefulSite(https_server()->GetOrigin("a.test")));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, std::log2(3));

  EXPECT_THAT(GetSharedStorageReportingMap(observed_urn_uuid.value()),
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
                     "document.body.appendChild(f);"));

  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  TestFrameNavigationObserver observer(fenced_frame_root_node);

  EvalJsResult navigation_result = NavigateFencedFrame(
      root, ResolveSelectURLToConfig()
                ? FencedFrameNavigationTarget("select_url_result")
                : FencedFrameNavigationTarget(observed_urn_uuid.value()));

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

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());
  // There is 1 more "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            },
            {
              url: "fenced_frames/title1.html",
              reportingMetadata: {
                "click": "fenced_frames/report1.html"
              }
            },
            {
              url: "fenced_frames/title2.html"
            }
          ],
          {
            data: {'mockResult': 1},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result.error.empty());
  const absl::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  FrameTreeNode* root = PrimaryFrameTreeNodeRoot();

  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  TestFrameNavigationObserver observer(fenced_frame_root_node);

  EvalJsResult navigation_result = NavigateFencedFrame(
      root, ResolveSelectURLToConfig()
                ? FencedFrameNavigationTarget("select_url_result")
                : FencedFrameNavigationTarget(observed_urn_uuid.value()));

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

  EXPECT_TRUE(
      url_mapping_test_peer.HasObserver(observed_urn_uuid.value(), request));

  // Execute the deferred messages. This should finish the url mapping and
  // resume the deferred navigation.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->ExecutePendingWorkletMessages();

  observer.Wait();

  ASSERT_TRUE(config_observer.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid_, observed_urn_uuid.value());

  SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(observed_urn_uuid.value());
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->site,
            net::SchemefulSite(https_server()->GetOrigin("a.test")));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, std::log2(3));

  EXPECT_THAT(GetSharedStorageReportingMap(observed_urn_uuid.value()),
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

  absl::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(url::Origin::Create(main_url));
  ASSERT_TRUE(urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(*urn_uuid));

  FrameTreeNode* iframe_node = PrimaryFrameTreeNodeRoot()->child_at(0);

  // Navigate the iframe to about:blank.
  TestFrameNavigationObserver observer(iframe_node);
  EXPECT_TRUE(ExecJs(iframe_node, JsReplace("window.location.href=$1",
                                            GURL(url::kAboutBlankURL))));
  observer.Wait();

  // Verify that the `urn_uuid` is still valid in the main page.
  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(*urn_uuid);
  EXPECT_EQ(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());

  // Navigate to a new page. Verify that the `urn_uuid` is not valid in this
  // new page.
  GURL new_page_main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), new_page_main_url));

  fenced_frame_root_node = CreateFencedFrame(*urn_uuid);
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

  EXPECT_TRUE(ExecJs(iframe, kGenerateURLsListScript));
  EXPECT_TRUE(ExecJs(iframe, JsReplace("window.resolveSelectURLToConfig = $1;",
                                       ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());
  // There is 1 more "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(iframe, R"(
      (async function() {
        const urls = generateUrls(8);
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          urls,
          {
            data: {'mockResult': 1},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result.error.empty());
  const absl::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  FrameTreeNode* root = PrimaryFrameTreeNodeRoot();
  if (ResolveSelectURLToConfig()) {
    // Preserve the config in a variable. It is then installed to the new fenced
    // frame. Without this step, the config will be gone after navigating the
    // iframe to about::blank.
    EXPECT_TRUE(ExecJs(root, R"(var fenced_frame_config = document
                                        .getElementById('test_iframe')
                                        .contentWindow
                                        .select_url_result;)"));
  }

  // Navigate away to let the subframe's worklet enter keep-alive.
  NavigateIframeToURL(shell()->web_contents(), "test_iframe",
                      GURL(url::kAboutBlankURL));

  EXPECT_EQ(0u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(1u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  test_worklet_host_manager()
      .GetKeepAliveWorkletHost()
      ->WaitForWorkletResponses();

  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(2U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(1));

  TestFrameNavigationObserver observer(fenced_frame_root_node);

  EvalJsResult navigation_result = NavigateFencedFrame(
      root, ResolveSelectURLToConfig()
                ? FencedFrameNavigationTarget("fenced_frame_config")
                : FencedFrameNavigationTarget(observed_urn_uuid.value()));

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

  ASSERT_FALSE(config_observer.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_FALSE(fenced_frame_config.has_value());

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
      GetSharedStorageBudgetMetadata(observed_urn_uuid.value());
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->site,
            net::SchemefulSite(https_server()->GetOrigin("a.test")));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, 0.0);

  EXPECT_THAT(GetSharedStorageReportingMap(observed_urn_uuid.value()),
              UnorderedElementsAre(
                  Pair("click", https_server()->GetURL(
                                    "a.test", "/fenced_frames/report0.html")),
                  Pair("mouse interaction",
                       https_server()->GetURL("a.test",
                                              "/fenced_frames/report1.html"))));

  EXPECT_EQ(
      https_server()->GetURL("a.test", "/fenced_frames/title0.html"),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());

  // The worklet execution sequence for `selectURL()` doesn't complete, so the
  // `kTimingSelectUrlExecutedInWorkletHistogram` histogram isn't recorded.
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     0);

  // The worklet is destructed. The config corresponds to the unresolved URN is
  // populated in the destructor of `SharedStorageWorkletHost`.
  ASSERT_TRUE(config_observer.ConfigObserved());
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid_, observed_urn_uuid.value());
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURL_WorkletReturnInvalidIndex) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetFilter(base::BindRepeating(IsErrorMessage));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());
  // There is 1 more "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            },
            {
              url: "fenced_frames/title1.html",
              reportingMetadata:
              {
                "click": "fenced_frames/report1.html"
              }
            },
            {
              url: "fenced_frames/title2.html"
            }
          ],
          {
            data: {'mockResult': 3},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result.error.empty());
  const absl::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid_, observed_urn_uuid->spec());

  EXPECT_EQ(
      "Promise resolved to a number outside the length of the input urls.",
      base::UTF16ToUTF8(console_observer.messages().back().message));

  SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(observed_urn_uuid.value());
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->site,
            net::SchemefulSite(https_server()->GetOrigin("a.test")));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, std::log2(3));

  EXPECT_TRUE(GetSharedStorageReportingMap(observed_urn_uuid.value()).empty());

  FrameTreeNode* root = PrimaryFrameTreeNodeRoot();

  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  TestFrameNavigationObserver observer(fenced_frame_root_node);

  EvalJsResult navigation_result = NavigateFencedFrame(
      root, ResolveSelectURLToConfig()
                ? FencedFrameNavigationTarget("select_url_result")
                : FencedFrameNavigationTarget(observed_urn_uuid.value()));

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

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());
  // There is 1 more "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            },
            {
              url: "fenced_frames/title1.html",
              reportingMetadata:
              {
                "click": "fenced_frames/report1.html"
              }
            },
            {
              url: "fenced_frames/title2.html"
            }
          ],
          {
            data: {'mockResult': 1},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result.error.empty());
  const absl::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config =
      config_observer.GetConfig();
  EXPECT_TRUE(fenced_frame_config.has_value());
  EXPECT_EQ(fenced_frame_config->urn_uuid_, observed_urn_uuid.value());

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(observed_urn_uuid.value());
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->site,
            net::SchemefulSite(https_server()->GetOrigin("a.test")));
  EXPECT_DOUBLE_EQ(metadata->budget_to_charge, std::log2(3));

  EXPECT_THAT(GetSharedStorageReportingMap(observed_urn_uuid.value()),
              UnorderedElementsAre(
                  Pair("click", https_server()->GetURL(
                                    "a.test", "/fenced_frames/report1.html"))));

  FrameTreeNode* root = PrimaryFrameTreeNodeRoot();

  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  TestFrameNavigationObserver observer(fenced_frame_root_node);

  EvalJsResult navigation_result = NavigateFencedFrame(
      root, ResolveSelectURLToConfig()
                ? FencedFrameNavigationTarget("select_url_result")
                : FencedFrameNavigationTarget(observed_urn_uuid.value()));

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

  absl::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(shared_storage_origin);
  ASSERT_TRUE(urn_uuid.has_value());

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(*urn_uuid);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

  TestFrameNavigationObserver observer(fenced_frame_root_node);
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

  absl::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(shared_storage_origin);
  ASSERT_TRUE(urn_uuid.has_value());

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(*urn_uuid);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

  GURL new_page_url = https_server()->GetURL("c.test", kSimplePagePath);

  TestNavigationObserver top_navigation_observer(shell()->web_contents());
  EXPECT_TRUE(
      ExecJs(fenced_frame_root_node,
             JsReplace("window.open($1, '_unfencedTop')", new_page_url)));
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
    FencedFrameNavigateFromParentToRegularURLAndThenOpenPopup_NoBudgetWithdrawal) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  absl::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(shared_storage_origin);
  ASSERT_TRUE(urn_uuid.has_value());

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(*urn_uuid);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

  GURL new_frame_url = https_server()->GetURL("c.test", kFencedFramePath);

  TestFrameNavigationObserver observer(fenced_frame_root_node);
  std::string navigate_fenced_frame_script = JsReplace(
      "var f = document.getElementsByTagName('fencedframe')[0]; f.config = new "
      "FencedFrameConfig($1);",
      new_frame_url);

  EXPECT_TRUE(ExecJs(shell(), navigate_fenced_frame_script));
  observer.Wait();

  GURL new_page_url = https_server()->GetURL("d.test", kSimplePagePath);

  OpenPopup(fenced_frame_root_node, new_page_url, /*name=*/"");

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

  absl::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(shared_storage_origin);
  ASSERT_TRUE(urn_uuid.has_value());

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(*urn_uuid);

  {
    GURL new_frame_url = https_server()->GetURL("c.test", kFencedFramePath);

    TestFrameNavigationObserver observer(fenced_frame_root_node);
    EXPECT_TRUE(ExecJs(fenced_frame_root_node,
                       JsReplace("window.location.href=$1", new_frame_url)));
    observer.Wait();
  }

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

  {
    GURL new_page_url = https_server()->GetURL("d.test", kSimplePagePath);

    TestNavigationObserver top_navigation_observer(shell()->web_contents());
    EXPECT_TRUE(
        ExecJs(fenced_frame_root_node,
               JsReplace("window.open($1, '_unfencedTop')", new_page_url)));
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

// TODO(crbug.com/1347953): Reenable this test when it is possible to create a
// nested fenced frame with no reporting metadata, that can call _unfencedTop.
IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       DISABLED_NestedFencedFrameNavigateTop_BudgetWithdrawal) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  absl::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(shared_storage_origin);
  ASSERT_TRUE(urn_uuid.has_value());

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(*urn_uuid);

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
  EXPECT_TRUE(
      ExecJs(nested_fenced_frame_root_node,
             JsReplace("window.open($1, '_unfencedTop')", new_page_url)));
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

  url::Origin shared_storage_origin_1 =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  absl::optional<GURL> urn_uuid_1 =
      SelectFrom8URLsInContext(shared_storage_origin_1);
  ASSERT_TRUE(urn_uuid_1.has_value());
  FrameTreeNode* fenced_frame_root_node_1 = CreateFencedFrame(*urn_uuid_1);

  url::Origin shared_storage_origin_2 =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  absl::optional<GURL> urn_uuid_2 = SelectFrom8URLsInContext(
      shared_storage_origin_2, fenced_frame_root_node_1);
  ASSERT_TRUE(urn_uuid_2.has_value());

  FrameTreeNode* fenced_frame_root_node_2 =
      CreateFencedFrame(fenced_frame_root_node_1, *urn_uuid_2);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin_1), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin_2), kBudgetAllowed);

  GURL new_page_url = https_server()->GetURL("d.test", kSimplePagePath);
  TestNavigationObserver top_navigation_observer(shell()->web_contents());
  EXPECT_TRUE(
      ExecJs(fenced_frame_root_node_2,
             JsReplace("window.open($1, '_unfencedTop')", new_page_url)));
  top_navigation_observer.Wait();

  // After the top navigation, log(8)=3 bits should have been withdrawn from
  // both `shared_storage_origin_1` and `shared_storage_origin_2`.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin_1),
                   kBudgetAllowed - 3);
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin_2),
                   kBudgetAllowed - 3);
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageFencedFrameInteractionBrowserTest,
    SelectURLNotAllowedInFencedFrameNotOriginatedFromSharedStorage) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  GURL fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");

  FrameTreeNode* fenced_frame_root_node_1 =
      static_cast<RenderFrameHostImpl*>(
          fenced_frame_test_helper_.CreateFencedFrame(
              shell()->web_contents()->GetPrimaryMainFrame(), fenced_frame_url,
              net::OK, blink::FencedFrame::DeprecatedFencedFrameMode::kDefault))
          ->frame_tree_node();

  EXPECT_TRUE(ExecJs(fenced_frame_root_node_1,
                     JsReplace("window.resolveSelectURLToConfig = $1;",
                               ResolveSelectURLToConfig())));

  EvalJsResult result = EvalJs(fenced_frame_root_node_1, R"(
      sharedStorage.selectURL(
        'test-url-selection-operation',
        [
          {
            url: "fenced_frames/title0.html"
          }
        ],
        {
          data: {'mockResult': 0},
          resolveToConfig: resolveSelectURLToConfig
        }
      );
    )");

  EXPECT_THAT(
      result.error,
      testing::HasSubstr(
          "The \"shared-storage\" Permissions Policy denied the method on "
          "window.sharedStorage."));
}

IN_PROC_BROWSER_TEST_F(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURLNotAllowedInNestedFencedFrame) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin_1 =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  absl::optional<GURL> urn_uuid_1 =
      SelectFrom8URLsInContext(shared_storage_origin_1);
  ASSERT_TRUE(urn_uuid_1.has_value());
  FrameTreeNode* fenced_frame_root_node_1 = CreateFencedFrame(*urn_uuid_1);

  url::Origin shared_storage_origin_2 =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  absl::optional<GURL> urn_uuid_2 = SelectFrom8URLsInContext(
      shared_storage_origin_2, fenced_frame_root_node_1);
  ASSERT_TRUE(urn_uuid_2.has_value());

  FrameTreeNode* fenced_frame_root_node_2 =
      CreateFencedFrame(fenced_frame_root_node_1, *urn_uuid_2);

  EXPECT_TRUE(ExecJs(fenced_frame_root_node_2, R"(
      sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
    )"));
  EXPECT_TRUE(ExecJs(fenced_frame_root_node_2,
                     JsReplace("window.resolveSelectURLToConfig = $1;",
                               ResolveSelectURLToConfig())));

  EvalJsResult result = EvalJs(fenced_frame_root_node_2, R"(
      sharedStorage.selectURL(
        'test-url-selection-operation',
        [
          {
            url: "fenced_frames/title0.html"
          }
        ],
        {
          data: {'mockResult': 0},
          resolveToConfig: resolveSelectURLToConfig
        }
      );
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

  absl::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(shared_storage_origin);
  ASSERT_TRUE(urn_uuid.has_value());

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(*urn_uuid);

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
  EXPECT_TRUE(
      ExecJs(nested_fenced_frame_root_node,
             JsReplace("window.open($1, '_unfencedTop')", new_page_url)));
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

  absl::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(shared_storage_origin);
  ASSERT_TRUE(urn_uuid.has_value());

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(*urn_uuid);

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

  EXPECT_TRUE(ExecJs(shell(), kGenerateURLsListScript));
  EXPECT_TRUE(ExecJs(shell(), "window.urls = generateUrls(8);"));
  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  // There are 2 more "worklet operations": both `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(2);

  TestSelectURLFencedFrameConfigObserver config_observer_1(
      GetStoragePartition());
  EvalJsResult result_1 = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result_1 = await sharedStorage.selectURL(
          'test-url-selection-operation',
          urls,
          {
            data: {'mockResult': 1},
            resolveToConfig: resolveSelectURLToConfig,
            keepAlive: true
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result_1 instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result_1;
      })()
    )");

  EXPECT_TRUE(result_1.error.empty());
  const absl::optional<GURL>& observed_urn_uuid_1 =
      config_observer_1.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid_1.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid_1.value()));

  TestSelectURLFencedFrameConfigObserver config_observer_2(
      GetStoragePartition());
  EvalJsResult result_2 = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result_2 = await sharedStorage.selectURL(
          'test-url-selection-operation',
          urls,
          {
            data: {'mockResult': 1},
            resolveToConfig: resolveSelectURLToConfig,
            keepAlive: true
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result_2 instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result_2;
      })()
    )");

  EXPECT_TRUE(result_2.error.empty());
  const absl::optional<GURL>& observed_urn_uuid_2 =
      config_observer_2.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid_2.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid_2.value()));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer_1.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config_1 =
      config_observer_1.GetConfig();
  EXPECT_TRUE(fenced_frame_config_1.has_value());
  EXPECT_EQ(fenced_frame_config_1->urn_uuid_, observed_urn_uuid_1.value());

  ASSERT_TRUE(config_observer_2.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config_2 =
      config_observer_2.GetConfig();
  EXPECT_TRUE(fenced_frame_config_2.has_value());
  EXPECT_EQ(fenced_frame_config_2->urn_uuid_, observed_urn_uuid_2.value());

  FrameTreeNode* fenced_frame_root_node_1 = CreateFencedFrame(
      ResolveSelectURLToConfig()
          ? FencedFrameNavigationTarget("select_url_result_1")
          : FencedFrameNavigationTarget(observed_urn_uuid_1.value()));
  FrameTreeNode* fenced_frame_root_node_2 = CreateFencedFrame(
      ResolveSelectURLToConfig()
          ? FencedFrameNavigationTarget("select_url_result_2")
          : FencedFrameNavigationTarget(observed_urn_uuid_2.value()));

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()),
                   kBudgetAllowed);

  OpenPopup(fenced_frame_root_node_1,
            https_server()->GetURL("b.test", kSimplePagePath), /*name=*/"");

  // After the popup, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);

  OpenPopup(fenced_frame_root_node_2,
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

  absl::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(shared_storage_origin);
  ASSERT_TRUE(urn_uuid.has_value());

  FrameTreeNode* fenced_frame_root_node_1 = CreateFencedFrame(*urn_uuid);
  FrameTreeNode* fenced_frame_root_node_2 = CreateFencedFrame(*urn_uuid);

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(
      RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()->child_at(0)),
      kBudgetAllowed);

  OpenPopup(fenced_frame_root_node_1,
            https_server()->GetURL("b.test", kSimplePagePath), /*name=*/"");

  // After the popup, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);

  OpenPopup(fenced_frame_root_node_2,
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
  console_observer.SetFilter(base::BindRepeating(IsErrorMessage));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_TRUE(ExecJs(shell(), kGenerateURLsListScript));
  EXPECT_TRUE(ExecJs(shell(), "window.urls = generateUrls(8);"));
  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer_1(
      GetStoragePartition());

  // There are 2 more "worklet operations": both `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(2);

  EvalJsResult result_1 = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result_1 = await sharedStorage.selectURL(
          'test-url-selection-operation',
          urls,
          {
            data: {'mockResult': 1},
            resolveToConfig: resolveSelectURLToConfig,
            keepAlive: true
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result_1 instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result_1;
      })()
    )");

  EXPECT_TRUE(result_1.error.empty());
  const absl::optional<GURL>& observed_urn_uuid_1 =
      config_observer_1.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid_1.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid_1.value()));

  FrameTreeNode* fenced_frame_root_node_1 = CreateFencedFrame(
      ResolveSelectURLToConfig()
          ? FencedFrameNavigationTarget("select_url_result_1")
          : FencedFrameNavigationTarget(observed_urn_uuid_1.value()));
  OpenPopup(fenced_frame_root_node_1,
            https_server()->GetURL("b.test", kSimplePagePath), /*name=*/"");

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);

  TestSelectURLFencedFrameConfigObserver config_observer_2(
      GetStoragePartition());
  EvalJsResult result_2 = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result_2 = await sharedStorage.selectURL(
          'test-url-selection-operation',
          urls,
          {
            data: {'mockResult': 1},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result_2 instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result_2;
      })()
    )");

  EXPECT_TRUE(result_2.error.empty());
  const absl::optional<GURL>& observed_urn_uuid_2 =
      config_observer_2.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid_2.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid_2.value()));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  ASSERT_TRUE(config_observer_1.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config_1 =
      config_observer_1.GetConfig();
  EXPECT_TRUE(fenced_frame_config_1.has_value());
  EXPECT_EQ(fenced_frame_config_1->urn_uuid_, observed_urn_uuid_1.value());

  ASSERT_TRUE(config_observer_2.ConfigObserved());
  const absl::optional<FencedFrameConfig>& fenced_frame_config_2 =
      config_observer_2.GetConfig();
  EXPECT_TRUE(fenced_frame_config_2.has_value());
  EXPECT_EQ(fenced_frame_config_2->urn_uuid_, observed_urn_uuid_2.value());

  EXPECT_EQ("Insufficient budget for selectURL().",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  // The failed mapping due to insufficient budget (i.e. `urn_uuid_2`) should
  // not incur any budget withdrawal on subsequent top navigation from inside
  // the fenced frame.
  FrameTreeNode* fenced_frame_root_node_2 = CreateFencedFrame(
      ResolveSelectURLToConfig()
          ? FencedFrameNavigationTarget("select_url_result_2")
          : FencedFrameNavigationTarget(observed_urn_uuid_2.value()));
  OpenPopup(fenced_frame_root_node_2,
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

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));
  EXPECT_TRUE(ExecJs(shell(), "window.keepWorklet = true;"));

  // There is 1 more "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  std::string select_url_script = R"(
      sharedStorage.selectURL(
        'test-url-selection-operation',
        [
          {
            url: "fenced_frames/title0.html"
          }
        ],
        {
          data: {'mockResult': 0},
          resolveToConfig: resolveSelectURLToConfig,
          keepAlive: keepWorklet
        }
      );
    )";
  EXPECT_TRUE(ExecJs(shell(), select_url_script));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  FencedFrameURLMapping& fenced_frame_url_mapping =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();
  FencedFrameURLMappingTestPeer fenced_frame_url_mapping_test_peer(
      &fenced_frame_url_mapping);

  // Fill the map until its size reaches the limit.
  GURL url("https://a.test");
  fenced_frame_url_mapping_test_peer.FillMap(url);

  // No need to keep the worklet after the next select operation.
  EXPECT_TRUE(ExecJs(shell(), "window.keepWorklet = false;"));

  EvalJsResult extra_result = EvalJs(shell(), select_url_script);

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
    shared_storage_feature_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{blink::features::kSharedStorageAPI,
          {{"SharedStorageBitBudget", base::NumberToString(kBudgetAllowed)},
           {"SharedStorageMaxAllowedFencedFrameDepthForSelectURL", "0"}}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList shared_storage_feature_;
};

IN_PROC_BROWSER_TEST_F(SharedStorageSelectURLNotAllowedInFencedFrameBrowserTest,
                       SelectURLNotAllowedInFencedFrame) {
  GURL main_frame_url = https_server()->GetURL("a.test", kSimplePagePath);

  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));
  absl::optional<GURL> urn_uuid =
      SelectFrom8URLsInContext(shared_storage_origin);
  ASSERT_TRUE(urn_uuid.has_value());

  FrameTreeNode* fenced_frame_node = CreateFencedFrame(*urn_uuid);

  EXPECT_TRUE(ExecJs(fenced_frame_node, R"(
      sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
    )"));

  EXPECT_EQ(2u, test_worklet_host_manager().GetAttachedWorkletHostsCount());
  EXPECT_EQ(0u, test_worklet_host_manager().GetKeepAliveWorkletHostsCount());

  EXPECT_TRUE(ExecJs(fenced_frame_node,
                     JsReplace("window.resolveSelectURLToConfig = $1;",
                               ResolveSelectURLToConfig())));
  EvalJsResult result = EvalJs(fenced_frame_node, R"(
      sharedStorage.selectURL(
        'test-url-selection-operation',
        [
          {
            url: "fenced_frames/title0.html"
          }
        ],
        {
          data: {'mockResult': 0},
          resolveToConfig: resolveSelectURLToConfig
        }
      );
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

  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());
  // There is 1 more "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: "fenced_frames/title0.html"
            },
            {
              url: "fenced_frames/title1.html",
              reportingMetadata: {
                "click": "fenced_frames/report1.html",
                "mouse interaction": "fenced_frames/report2.html"
              }
            }
          ],
          {
            data: {'mockResult': 1},
            resolveToConfig: resolveSelectURLToConfig
          }
        );
        if (resolveSelectURLToConfig &&
            !(select_url_result instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.select_url_result;
      })()
    )");

  EXPECT_TRUE(result.error.empty());
  const absl::optional<GURL>& observed_urn_uuid = config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
  }

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(
      ResolveSelectURLToConfig()
          ? FencedFrameNavigationTarget("select_url_result")
          : FencedFrameNavigationTarget(observed_urn_uuid.value()));

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
    : public SharedStorageBrowserTestBase {
 public:
  SharedStoragePrivateAggregationDisabledBrowserTest() {
    private_aggregation_feature_.InitAndDisableFeature(
        blink::features::kPrivateAggregationApi);
  }

 private:
  base::test::ScopedFeatureList private_aggregation_feature_;
};

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationDisabledBrowserTest,
                       PrivateAggregationNotDefined) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
    )",
                         &out_script_url);

  ASSERT_EQ(1u, console_observer.messages().size());
  EXPECT_THAT(base::UTF16ToUTF8(console_observer.messages()[0].message),
              testing::HasSubstr("privateAggregation is not defined"));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[0].log_level);
}

class SharedStoragePrivateAggregationDisabledForSharedStorageOnlyBrowserTest
    : public SharedStorageBrowserTestBase {
 public:
  SharedStoragePrivateAggregationDisabledForSharedStorageOnlyBrowserTest() {
    private_aggregation_feature_.InitAndEnableFeatureWithParameters(
        blink::features::kPrivateAggregationApi,
        {{"enabled_in_shared_storage", "false"}});
  }

 private:
  base::test::ScopedFeatureList private_aggregation_feature_;
};

IN_PROC_BROWSER_TEST_F(
    SharedStoragePrivateAggregationDisabledForSharedStorageOnlyBrowserTest,
    PrivateAggregationNotDefined) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
    )",
                         &out_script_url);

  ASSERT_EQ(1u, console_observer.messages().size());
  EXPECT_THAT(base::UTF16ToUTF8(console_observer.messages()[0].message),
              testing::HasSubstr("privateAggregation is not defined"));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[0].log_level);
}

class SharedStoragePrivateAggregationEnabledBrowserTest
    : public SharedStorageBrowserTestBase {
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
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kPrivateAggregationApi,
                              blink::features::kSharedStorageAPIM118},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    SharedStorageBrowserTestBase::SetUpOnMainThread();

    a_test_origin_ = https_server()->GetOrigin("a.test");

    auto* storage_partition_impl =
        static_cast<StoragePartitionImpl*>(GetStoragePartition());

    private_aggregation_host_ = new PrivateAggregationHost(
        /*on_report_request_details_received=*/mock_callback_.Get(),
        storage_partition_impl->browser_context());

    storage_partition_impl->OverridePrivateAggregationManagerForTesting(
        std::make_unique<TestPrivateAggregationManagerImpl>(
            std::make_unique<MockPrivateAggregationBudgeter>(),
            base::WrapUnique<PrivateAggregationHost>(
                private_aggregation_host_.get())));

    EXPECT_TRUE(NavigateToURL(
        shell(), https_server()->GetURL("a.test", kSimplePagePath)));
  }

  void MakeMockPrivateAggregationShellContentBrowserClient() override {
    browser_client_ =
        std::make_unique<MockPrivateAggregationShellContentBrowserClient>();
  }

  const base::MockRepeatingCallback<
      void(PrivateAggregationHost::ReportRequestGenerator,
           std::vector<blink::mojom::AggregatableReportHistogramContribution>,
           PrivateAggregationBudgetKey,
           PrivateAggregationBudgeter::BudgetDeniedBehavior)>&
  mock_callback() {
    return mock_callback_;
  }

 protected:
  url::Origin a_test_origin_;

 private:
  raw_ptr<PrivateAggregationHost, DanglingUntriaged> private_aggregation_host_;

  base::test::ScopedFeatureList scoped_feature_list_;

  base::MockRepeatingCallback<void(
      PrivateAggregationHost::ReportRequestGenerator,
      std::vector<blink::mojom::AggregatableReportHistogramContribution>,
      PrivateAggregationBudgetKey,
      PrivateAggregationBudgeter::BudgetDeniedBehavior)>
      mock_callback_;
};

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       BasicTest) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              std::vector<blink::mojom::AggregatableReportHistogramContribution>
                  contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationBudgeter::BudgetDeniedBehavior
                  budget_denied_behavior) {
            AggregatableReportRequest request =
                std::move(generator).Run(contributions);
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].bucket, 1);
            EXPECT_EQ(request.payload_contents().contributions[0].value, 2);
            EXPECT_EQ(request.shared_info().reporting_origin, a_test_origin_);
            EXPECT_EQ(budget_key.origin(), a_test_origin_);
            EXPECT_EQ(budget_key.api(),
                      PrivateAggregationBudgetKey::Api::kSharedStorage);
            EXPECT_TRUE(request.additional_fields().empty());
            EXPECT_EQ(budget_denied_behavior,
                      PrivateAggregationBudgeter::BudgetDeniedBehavior::
                          kDontSendReport);
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
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
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
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.contributeToHistogram({bucket: -1n, value: 2});
    )",
                         &out_script_url);

  ASSERT_EQ(1u, console_observer.messages().size());
  EXPECT_THAT(
      base::UTF16ToUTF8(console_observer.messages()[0].message),
      testing::HasSubstr(
          "contribution['bucket'] is negative or does not fit in 128 bits"));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[0].log_level);
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       MultipleRequests) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              std::vector<blink::mojom::AggregatableReportHistogramContribution>
                  contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationBudgeter::BudgetDeniedBehavior
                  budget_denied_behavior) {
            AggregatableReportRequest request =
                std::move(generator).Run(contributions);
            ASSERT_EQ(request.payload_contents().contributions.size(), 2u);
            EXPECT_EQ(request.payload_contents().contributions[0].bucket, 1);
            EXPECT_EQ(request.payload_contents().contributions[0].value, 2);
            EXPECT_EQ(request.payload_contents().contributions[1].bucket, 3);
            EXPECT_EQ(request.payload_contents().contributions[1].value, 4);
            EXPECT_EQ(request.shared_info().reporting_origin, a_test_origin_);
            EXPECT_EQ(budget_key.origin(), a_test_origin_);
            EXPECT_EQ(budget_key.api(),
                      PrivateAggregationBudgetKey::Api::kSharedStorage);
            EXPECT_EQ(budget_denied_behavior,
                      PrivateAggregationBudgeter::BudgetDeniedBehavior::
                          kDontSendReport);
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
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
      privateAggregation.contributeToHistogram({bucket: 3n, value: 4});
    )",
                         &out_script_url);

  EXPECT_TRUE(console_observer.messages().empty());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       TimeoutBeforeOperationFinish) {
  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              std::vector<blink::mojom::AggregatableReportHistogramContribution>
                  contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationBudgeter::BudgetDeniedBehavior
                  budget_denied_behavior) {
            AggregatableReportRequest request =
                std::move(generator).Run(contributions);
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].bucket, 1);
            EXPECT_EQ(request.payload_contents().contributions[0].value, 2);
            EXPECT_EQ(request.shared_info().reporting_origin, a_test_origin_);
            EXPECT_EQ(budget_key.origin(), a_test_origin_);
            EXPECT_EQ(budget_key.api(),
                      PrivateAggregationBudgetKey::Api::kSharedStorage);
            EXPECT_EQ(budget_denied_behavior,
                      PrivateAggregationBudgeter::BudgetDeniedBehavior::
                          kSendNullReport);
            EXPECT_THAT(request.additional_fields(),
                        testing::ElementsAre(
                            testing::Pair("context_id", "example_context_id")));
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
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;

  // Run an operation that returns a promise that never resolves.
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
      return new Promise(() => {});
    )",
                         &out_script_url, /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/"example_context_id",
                         /*out_error=*/nullptr,
                         /*wait_for_operation_finish=*/false);

  // Wait for 5 seconds for the timeout to be reached.
  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Seconds(5));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       ContextId) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              std::vector<blink::mojom::AggregatableReportHistogramContribution>
                  contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationBudgeter::BudgetDeniedBehavior
                  budget_denied_behavior) {
            AggregatableReportRequest request =
                std::move(generator).Run(contributions);
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].bucket, 1);
            EXPECT_EQ(request.payload_contents().contributions[0].value, 2);
            EXPECT_EQ(request.shared_info().reporting_origin, a_test_origin_);
            EXPECT_EQ(budget_key.origin(), a_test_origin_);
            EXPECT_EQ(budget_key.api(),
                      PrivateAggregationBudgetKey::Api::kSharedStorage);
            EXPECT_THAT(request.additional_fields(),
                        testing::ElementsAre(
                            testing::Pair("context_id", "example_context_id")));
            EXPECT_EQ(budget_denied_behavior,
                      PrivateAggregationBudgeter::BudgetDeniedBehavior::
                          kSendNullReport);
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
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
    )",
                         &out_script_url, /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/"example_context_id");

  EXPECT_TRUE(console_observer.messages().empty());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       ContextIdEmptyString) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              std::vector<blink::mojom::AggregatableReportHistogramContribution>
                  contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationBudgeter::BudgetDeniedBehavior
                  budget_denied_behavior) {
            AggregatableReportRequest request =
                std::move(generator).Run(contributions);
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].bucket, 1);
            EXPECT_EQ(request.payload_contents().contributions[0].value, 2);
            EXPECT_EQ(request.shared_info().reporting_origin, a_test_origin_);
            EXPECT_EQ(budget_key.origin(), a_test_origin_);
            EXPECT_EQ(budget_key.api(),
                      PrivateAggregationBudgetKey::Api::kSharedStorage);
            EXPECT_THAT(request.additional_fields(),
                        testing::ElementsAre(testing::Pair("context_id", "")));
            EXPECT_EQ(budget_denied_behavior,
                      PrivateAggregationBudgeter::BudgetDeniedBehavior::
                          kSendNullReport);
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
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
    )",
                         &out_script_url, /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/
                         "");

  EXPECT_TRUE(console_observer.messages().empty());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       ContextIdMaxAllowedLength) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              std::vector<blink::mojom::AggregatableReportHistogramContribution>
                  contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationBudgeter::BudgetDeniedBehavior
                  budget_denied_behavior) {
            AggregatableReportRequest request =
                std::move(generator).Run(contributions);
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].bucket, 1);
            EXPECT_EQ(request.payload_contents().contributions[0].value, 2);
            EXPECT_EQ(request.shared_info().reporting_origin, a_test_origin_);
            EXPECT_EQ(budget_key.origin(), a_test_origin_);
            EXPECT_EQ(budget_key.api(),
                      PrivateAggregationBudgetKey::Api::kSharedStorage);
            EXPECT_THAT(request.additional_fields(),
                        testing::ElementsAre(
                            testing::Pair("context_id",
                                          "an_example_of_a_context_id_with_the_"
                                          "exact_maximum_allowed_length")));
            EXPECT_EQ(budget_denied_behavior,
                      PrivateAggregationBudgeter::BudgetDeniedBehavior::
                          kSendNullReport);
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
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(
      shell(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
    )",
      &out_script_url, /*expected_total_host_count=*/1u,
      /*keep_alive_after_operation=*/true,
      /*context_id=*/
      "an_example_of_a_context_id_with_the_exact_maximum_allowed_length");

  EXPECT_TRUE(console_observer.messages().empty());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       ContextIdTooLong) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_CALL(mock_callback(), Run).Times(0);
  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll))
      .Times(0);
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage))
      .Times(0);
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  std::string out_error;
  ExecuteScriptInWorklet(
      shell(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
    )",
      &out_script_url, /*expected_total_host_count=*/1u,
      /*keep_alive_after_operation=*/true,
      /*context_id=*/
      "this_is_an_example_of_a_context_id_that_is_too_long_to_be_allowed",
      &out_error);

  EXPECT_THAT(
      out_error,
      testing::HasSubstr("Error: contextId length cannot be larger than 64"));

  EXPECT_TRUE(console_observer.messages().empty());
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       PrivateAggregationPermissionsPolicyNone) {
  GURL url = https_server()->GetURL(
      "a.test",
      "/shared_storage/private_aggregation_permissions_policy_none.html");

  EXPECT_TRUE(NavigateToURL(shell(), url));

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
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
    )",
                         &out_script_url);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_THAT(
      base::UTF16ToUTF8(console_observer.messages()[0].message),
      testing::HasSubstr("The \"private-aggregation\" Permissions Policy "
                         "denied the method on privateAggregation"));
}

// This is a regression test for crbug.com/1428110.
IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       SimultaneousOperationsReportsArentBatchedTogether) {
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
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/slow_and_fast_module.js');
    )"));

  EXPECT_EQ(1u, test_worklet_host_manager().GetAttachedWorkletHostsCount());

  base::RunLoop run_loop;
  base::RepeatingClosure barrier =
      base::BarrierClosure(/*num_closures=*/3, run_loop.QuitClosure());

  int num_one_contribution_reports = 0;

  EXPECT_CALL(mock_callback(), Run)
      .Times(3)
      .WillRepeatedly(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              std::vector<blink::mojom::AggregatableReportHistogramContribution>
                  contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationBudgeter::BudgetDeniedBehavior
                  budget_denied_behavior) {
            AggregatableReportRequest request =
                std::move(generator).Run(contributions);
            if (request.payload_contents().contributions.size() == 1u) {
              EXPECT_EQ(request.payload_contents().contributions[0].bucket, 3);
              EXPECT_EQ(request.payload_contents().contributions[0].value, 1);
              ++num_one_contribution_reports;
            } else {
              ASSERT_EQ(request.payload_contents().contributions.size(), 2u);
              EXPECT_EQ(request.payload_contents().contributions[0].bucket, 1);
              EXPECT_EQ(request.payload_contents().contributions[0].value, 1);
              EXPECT_EQ(request.payload_contents().contributions[1].bucket, 2);
              EXPECT_EQ(request.payload_contents().contributions[1].value, 1);
            }
            EXPECT_EQ(request.shared_info().reporting_origin, a_test_origin_);
            EXPECT_EQ(budget_key.origin(), a_test_origin_);
            EXPECT_EQ(budget_key.api(),
                      PrivateAggregationBudgetKey::Api::kSharedStorage);
            EXPECT_EQ(budget_denied_behavior,
                      PrivateAggregationBudgeter::BudgetDeniedBehavior::
                          kDontSendReport);
            barrier.Run();
          }));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run('slow-operation', {keepAlive: true});
      sharedStorage.run('slow-operation', {keepAlive: true});
      sharedStorage.run('fast-operation');
    )"));

  run_loop.Run();

  // Ensures we saw exactly one report for the fast operation (and therefore two
  // for the slow operations).
  EXPECT_EQ(num_one_contribution_reports, 1);
}

class SharedStorageSelectURLLimitBrowserTest
    : public SharedStorageBrowserTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  SharedStorageSelectURLLimitBrowserTest() {
    if (LimitSelectURLCalls()) {
      select_url_limit_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{blink::features::kSharedStorageSelectURLLimit,
            {{"SharedStorageSelectURLBitBudgetPerPageLoad",
              base::NumberToString(kSelectURLOverallBitBudget)},
             {"SharedStorageSelectURLBitBudgetPerSitePerPageLoad",
              base::NumberToString(kSelectURLSiteBitBudget)}}}},
          /*disabled_features=*/{});
    } else {
      select_url_limit_feature_list_.InitAndDisableFeature(
          blink::features::kSharedStorageSelectURLLimit);
    }

    fenced_frame_api_change_feature_.InitWithFeatureState(
        blink::features::kFencedFramesAPIChanges, ResolveSelectURLToConfig());
  }

  bool LimitSelectURLCalls() const { return std::get<0>(GetParam()); }

  bool ResolveSelectURLToConfig() override { return std::get<1>(GetParam()); }

  // Precondition: `addModule('shared_storage/simple_module.js')` has been
  // called in the main frame.
  void RunSuccessfulSelectURLInMainFrame(
      std::string host_str,
      int num_urls,
      WebContentsConsoleObserver* console_observer) {
    absl::optional<std::pair<GURL, double>> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(shell(), host_str,
                                                         num_urls);
    ASSERT_TRUE(result_pair.has_value());

    GURL expected_mapped_url = https_server()->GetURL(
        host_str, base::StrCat({"/fenced_frames/title",
                                base::NumberToString(num_urls - 1), ".html"}));
    EXPECT_EQ(result_pair->first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair->second, std::log2(num_urls));

    EXPECT_EQ("Finish executing 'test-url-selection-operation'",
              base::UTF16ToUTF8(console_observer->messages().back().message));
  }

  // Precondition: `addModule('shared_storage/simple_module.js')` has NOT been
  // called in `iframe_node`.
  void RunSuccessfulSelectURLInIframe(
      FrameTreeNode* iframe_node,
      int num_urls,
      WebContentsConsoleObserver* console_observer) {
    std::string host_str =
        iframe_node->current_frame_host()->GetLastCommittedURL().host();
    EXPECT_TRUE(ExecJs(iframe_node, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

    absl::optional<std::pair<GURL, double>> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(iframe_node, host_str,
                                                         num_urls);
    ASSERT_TRUE(result_pair.has_value());

    GURL expected_mapped_url = https_server()->GetURL(
        host_str, base::StrCat({"/fenced_frames/title",
                                base::NumberToString(num_urls - 1), ".html"}));
    EXPECT_EQ(result_pair->first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair->second, std::log2(num_urls));

    EXPECT_EQ("Finish executing 'test-url-selection-operation'",
              base::UTF16ToUTF8(console_observer->messages().back().message));
  }

  // Precondition: `addModule('shared_storage/simple_module.js')` has been
  // called in the `execution_target`.
  absl::optional<std::pair<GURL, double>>
  RunSelectURLExtractingMappedURLAndBudgetToCharge(
      const ToRenderFrameHost& execution_target,
      std::string host_str,
      int num_urls) {
    TestSelectURLFencedFrameConfigObserver config_observer(
        GetStoragePartition());

    // There is 1 "worklet operation": `selectURL()`.
    test_worklet_host_manager()
        .GetAttachedWorkletHostForFrame(execution_target.render_frame_host())
        ->SetExpectedWorkletResponsesCount(1);

    EvalJsResult result = RunSelectURLScript(execution_target, num_urls);

    EXPECT_TRUE(result.error.empty()) << result.error;
    const absl::optional<GURL>& observed_urn_uuid =
        config_observer.GetUrnUuid();
    if (!observed_urn_uuid.has_value()) {
      return absl::nullopt;
    }
    EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

    if (!ResolveSelectURLToConfig()) {
      EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
    }

    test_worklet_host_manager()
        .GetAttachedWorkletHostForFrame(execution_target.render_frame_host())
        ->WaitForWorkletResponses();

    const absl::optional<FencedFrameConfig>& config =
        config_observer.GetConfig();
    if (!config.has_value()) {
      return absl::nullopt;
    }
    EXPECT_TRUE(config->mapped_url_.has_value());

    SharedStorageBudgetMetadata* metadata =
        GetSharedStorageBudgetMetadata(observed_urn_uuid.value());
    if (!metadata) {
      return absl::nullopt;
    }
    EXPECT_EQ(metadata->site,
              net::SchemefulSite(https_server()->GetOrigin(host_str)));

    return std::make_pair(config->mapped_url_->GetValueIgnoringVisibility(),
                          metadata->budget_to_charge);
  }

 private:
  EvalJsResult RunSelectURLScript(const ToRenderFrameHost& execution_target,
                                  int num_urls,
                                  bool keep_alive_after_operation = true) {
    EXPECT_TRUE(ExecJs(execution_target, kGenerateURLsListScript));
    EXPECT_TRUE(
        ExecJs(execution_target, JsReplace("window.numUrls = $1;", num_urls)));
    EXPECT_TRUE(ExecJs(execution_target,
                       JsReplace("window.resolveSelectURLToConfig = $1;",
                                 ResolveSelectURLToConfig())));
    EXPECT_TRUE(ExecJs(
        execution_target,
        JsReplace("window.keepWorklet = $1;", keep_alive_after_operation)));

    EvalJsResult result = EvalJs(execution_target, R"(
      (async function() {
        const urls = generateUrls(numUrls);
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          urls,
          {
            data: {'mockResult': numUrls - 1},
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
    return result;
  }

  base::test::ScopedFeatureList select_url_limit_feature_list_;
  base::test::ScopedFeatureList fenced_frame_api_change_feature_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SharedStorageSelectURLLimitBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()),
                         [](const auto& info) {
                           return base::StrCat(
                               {"LimitSelectURLCalls",
                                std::get<0>(info.param) ? "Enabled"
                                                        : "Disabled",
                                "_ResolveSelectURLTo",
                                std::get<1>(info.param) ? "Config" : "URN"});
                         });

IN_PROC_BROWSER_TEST_P(SharedStorageSelectURLLimitBrowserTest,
                       SelectURL_MainFrame_SameEntropy_SiteLimitReached) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  // This test relies on the assumption that `kSelectURLOverallBitBudget` is set
  // to be greater than or equal to `kSelectURLSiteBitBudget`.
  EXPECT_GE(kSelectURLOverallBitBudget, kSelectURLSiteBitBudget);

  // Here each call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy.
  int call_limit = kSelectURLSiteBitBudget / 3;

  for (int i = 0; i < call_limit; i++) {
    RunSuccessfulSelectURLInMainFrame("a.test", /*num_urls=*/8,
                                      &console_observer);
  }

  if (LimitSelectURLCalls()) {
    // The limit for `selectURL()` has now been reached for "a.test". Make one
    // more call, which will return the default URL due to insufficient site
    // pageload budget.
    absl::optional<std::pair<GURL, double>> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(shell(), "a.test",
                                                         /*num_urls=*/8);
    ASSERT_TRUE(result_pair.has_value());

    GURL expected_mapped_url =
        https_server()->GetURL("a.test", "/fenced_frames/title0.html");
    EXPECT_EQ(result_pair->first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair->second, 0.0);

    EXPECT_EQ("Insufficient budget for selectURL().",
              base::UTF16ToUTF8(console_observer.messages().back().message));
  } else {
    // The `selectURL()` limit is disabled. The call will run successfully.
    RunSuccessfulSelectURLInMainFrame("a.test", /*num_urls=*/8,
                                      &console_observer);
  }

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});

  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     call_limit + 1);
}

IN_PROC_BROWSER_TEST_P(SharedStorageSelectURLLimitBrowserTest,
                       SelectURL_MainFrame_DifferentEntropy_SiteLimitReached) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  // This test relies on the assumptions that `kSelectURLOverallBitBudget` is
  // set to be greater than or equal to `kSelectURLSiteBitBudget` and that the
  // latter is at least 3.
  EXPECT_GE(kSelectURLOverallBitBudget, kSelectURLSiteBitBudget);
  EXPECT_GE(kSelectURLSiteBitBudget, 3);

  // Here the first call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy, and the subsequent calls will each have 4
  // input URLs, and hence 2 = log2(4) bits of entropy.
  int input4_call_limit = (kSelectURLSiteBitBudget - 3) / 2;

  RunSuccessfulSelectURLInMainFrame("a.test", /*num_urls=*/8,
                                    &console_observer);

  for (int i = 0; i < input4_call_limit; i++) {
    RunSuccessfulSelectURLInMainFrame("a.test", /*num_urls=*/4,
                                      &console_observer);
  }

  if (LimitSelectURLCalls()) {
    // The limit for `selectURL()` has now been reached for "a.test". Make one
    // more call, which will return the default URL due to insufficient site
    // pageload budget.
    absl::optional<std::pair<GURL, double>> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(shell(), "a.test",
                                                         /*num_urls=*/4);
    ASSERT_TRUE(result_pair.has_value());

    GURL expected_mapped_url =
        https_server()->GetURL("a.test", "/fenced_frames/title0.html");
    EXPECT_EQ(result_pair->first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair->second, 0.0);

    EXPECT_EQ("Insufficient budget for selectURL().",
              base::UTF16ToUTF8(console_observer.messages().back().message));
  } else {
    // The `selectURL()` limit is disabled. The call will run successfully.
    RunSuccessfulSelectURLInMainFrame("a.test", /*num_urls=*/4,
                                      &console_observer);
  }

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});

  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     input4_call_limit + 2);
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageSelectURLLimitBrowserTest,
    SelectURL_IframesSharingCommonSite_SameEntropy_SiteLimitReached) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // This test relies on the assumption that `kSelectURLOverallBitBudget` is set
  // to be greater than or equal to `kSelectURLSiteBitBudget`.
  EXPECT_GE(kSelectURLOverallBitBudget, kSelectURLSiteBitBudget);

  // Here each call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy.
  int call_limit = kSelectURLSiteBitBudget / 3;

  GURL iframe_url = https_server()->GetURL("b.test", kSimplePagePath);

  for (int i = 0; i < call_limit; i++) {
    // Create a new iframe.
    FrameTreeNode* iframe_node =
        CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

    RunSuccessfulSelectURLInIframe(iframe_node, /*num_urls=*/8,
                                   &console_observer);
  }

  // Create a new iframe.
  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

  if (LimitSelectURLCalls()) {
    EXPECT_TRUE(ExecJs(iframe_node, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

    // The limit for `selectURL()` has now been reached for "b.test". Make one
    // more call, which will return the default URL due to insufficient site
    // pageload budget.
    absl::optional<std::pair<GURL, double>> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(iframe_node, "b.test",
                                                         /*num_urls=*/8);
    ASSERT_TRUE(result_pair.has_value());

    GURL expected_mapped_url =
        https_server()->GetURL("b.test", "/fenced_frames/title0.html");
    EXPECT_EQ(result_pair->first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair->second, 0.0);

    EXPECT_EQ("Insufficient budget for selectURL().",
              base::UTF16ToUTF8(console_observer.messages().back().message));
  } else {
    // The `selectURL()` limit is disabled. The call will run successfully.
    RunSuccessfulSelectURLInIframe(iframe_node, /*num_urls=*/8,
                                   &console_observer);
  }

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     call_limit + 1);
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageSelectURLLimitBrowserTest,
    SelectURL_IframesSharingCommonSite_DifferentEntropy_SiteLimitReached) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL iframe_url = https_server()->GetURL("b.test", kSimplePagePath);

  // Create a new iframe.
  FrameTreeNode* first_iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

  RunSuccessfulSelectURLInIframe(first_iframe_node, /*num_urls=*/8,
                                 &console_observer);

  // This test relies on the assumptions that `kSelectURLOverallBitBudget` is
  // set to be greater than or equal to `kSelectURLSiteBitBudget` and that the
  // latter is at least 3.
  EXPECT_GE(kSelectURLOverallBitBudget, kSelectURLSiteBitBudget);
  EXPECT_GE(kSelectURLSiteBitBudget, 3);

  // Here the first call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy, and the subsequent calls will each have 4
  // input URLs, and hence 2 = log2(4) bits of entropy.
  int input4_call_limit = (kSelectURLSiteBitBudget - 3) / 2;

  for (int i = 0; i < input4_call_limit; i++) {
    // Create a new iframe.
    FrameTreeNode* iframe_node =
        CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

    RunSuccessfulSelectURLInIframe(iframe_node, /*num_urls=*/4,
                                   &console_observer);
  }

  // Create a new iframe.
  FrameTreeNode* last_iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

  if (LimitSelectURLCalls()) {
    EXPECT_TRUE(ExecJs(last_iframe_node, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

    // The limit for `selectURL()` has now been reached for "b.test". Make one
    // more call, which will return the default URL due to insufficient site
    // pageload budget.
    absl::optional<std::pair<GURL, double>> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(last_iframe_node,
                                                         "b.test",
                                                         /*num_urls=*/4);
    ASSERT_TRUE(result_pair.has_value());

    GURL expected_mapped_url =
        https_server()->GetURL("b.test", "/fenced_frames/title0.html");
    EXPECT_EQ(result_pair->first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair->second, 0.0);

    EXPECT_EQ("Insufficient budget for selectURL().",
              base::UTF16ToUTF8(console_observer.messages().back().message));
  } else {
    // The `selectURL()` limit is disabled. The call will run successfully.
    RunSuccessfulSelectURLInIframe(last_iframe_node, /*num_urls=*/4,
                                   &console_observer);
  }

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     input4_call_limit + 2);
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageSelectURLLimitBrowserTest,
    SelectURL_IframesDifferentSite_SameEntropy_OverallLimitNotReached) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // This test relies on the assumption that `kSelectURLOverallBitBudget` is set
  // to be strictly greater than `kSelectURLSiteBitBudget`, enough for at
  // least one 8-URL call to `selectURL()` beyond the per-site limit.
  EXPECT_GE(kSelectURLOverallBitBudget, kSelectURLSiteBitBudget + 3);

  // Here each call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy.
  int per_site_call_limit = kSelectURLSiteBitBudget / 3;

  GURL iframe_url1 = https_server()->GetURL("b.test", kSimplePagePath);

  for (int i = 0; i < per_site_call_limit; i++) {
    // Create a new iframe.
    FrameTreeNode* iframe_node =
        CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url1);

    RunSuccessfulSelectURLInIframe(iframe_node, /*num_urls=*/8,
                                   &console_observer);
  }

  // Create a new iframe.
  FrameTreeNode* penultimate_iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url1);

  if (LimitSelectURLCalls()) {
    EXPECT_TRUE(ExecJs(penultimate_iframe_node, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

    // The limit for `selectURL()` has now been reached for "b.test". Make one
    // more call, which will return the default URL due to insufficient site
    // pageload budget.
    absl::optional<std::pair<GURL, double>> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(
            penultimate_iframe_node, "b.test",
            /*num_urls=*/4);
    ASSERT_TRUE(result_pair.has_value());

    GURL expected_mapped_url =
        https_server()->GetURL("b.test", "/fenced_frames/title0.html");
    EXPECT_EQ(result_pair->first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair->second, 0.0);

    EXPECT_EQ("Insufficient budget for selectURL().",
              base::UTF16ToUTF8(console_observer.messages().back().message));
  } else {
    // The `selectURL()` limit is disabled. The call will run successfully.
    RunSuccessfulSelectURLInIframe(penultimate_iframe_node,
                                   /*num_urls=*/4, &console_observer);
  }

  // Create a new iframe with a different site.
  GURL iframe_url2 = https_server()->GetURL("c.test", kSimplePagePath);
  FrameTreeNode* last_iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url2);

  // If enabled, the limit for `selectURL()` has now been reached for "b.test",
  // but not for "c.test". Make one more call, which will be successful
  // regardless of whether the limit is enabled.
  RunSuccessfulSelectURLInIframe(last_iframe_node, /*num_urls=*/8,
                                 &console_observer);

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     per_site_call_limit + 2);
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageSelectURLLimitBrowserTest,
    SelectURL_IframesDifferentSite_DifferentEntropy_OverallLimitReached) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // This test relies on the assumptions that `kSelectURLOverallBitBudget` is
  // set to be strictly greater than `kSelectURLSiteBitBudget` and that the
  // latter is at least 3.
  EXPECT_GT(kSelectURLOverallBitBudget, kSelectURLSiteBitBudget);
  EXPECT_GE(kSelectURLSiteBitBudget, 3);

  int num_site_limit = kSelectURLOverallBitBudget / kSelectURLSiteBitBudget;

  // We will run out of chars if we have too many sites.
  EXPECT_LT(num_site_limit, 25);

  // For each site, the first call to `selectURL()` will have 8 input URLs,
  // and hence 3 = log2(8) bits of entropy, whereas the subsequent calls for
  // that site will have 2 input URLs, and hence 1 = log2(2) bit of entropy.
  int per_site_input2_call_limit = kSelectURLSiteBitBudget - 3;

  for (int i = 0; i < num_site_limit; i++) {
    std::string iframe_host = base::StrCat({std::string(1, 'b' + i), ".test"});
    GURL iframe_url = https_server()->GetURL(iframe_host, kSimplePagePath);

    // Create a new iframe.
    FrameTreeNode* first_loop_iframe_node =
        CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

    RunSuccessfulSelectURLInIframe(first_loop_iframe_node,
                                   /*num_urls=*/8, &console_observer);

    for (int j = 0; j < per_site_input2_call_limit; j++) {
      // Create a new iframe.
      FrameTreeNode* loop_iframe_node =
          CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

      RunSuccessfulSelectURLInIframe(loop_iframe_node,
                                     /*num_urls=*/2, &console_observer);
    }

    // Create a new iframe.
    FrameTreeNode* last_loop_iframe_node =
        CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

    if (LimitSelectURLCalls()) {
      EXPECT_TRUE(ExecJs(last_loop_iframe_node, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

      // The limit for `selectURL()` has now been reached for `iframe_host`.
      // Make one more call, which will return the default URL due to
      // insufficient site pageload budget.
      absl::optional<std::pair<GURL, double>> result_pair =
          RunSelectURLExtractingMappedURLAndBudgetToCharge(
              last_loop_iframe_node, iframe_host,
              /*num_urls=*/2);
      ASSERT_TRUE(result_pair.has_value());

      GURL expected_mapped_url =
          https_server()->GetURL(iframe_host, "/fenced_frames/title0.html");
      EXPECT_EQ(result_pair->first, expected_mapped_url);
      EXPECT_DOUBLE_EQ(result_pair->second, 0.0);

      EXPECT_EQ("Insufficient budget for selectURL().",
                base::UTF16ToUTF8(console_observer.messages().back().message));

    } else {
      // The `selectURL()` limit is disabled. The call will run successfully.
      RunSuccessfulSelectURLInIframe(last_loop_iframe_node,
                                     /*num_urls=*/2, &console_observer);
    }
  }

  std::string iframe_host =
      base::StrCat({std::string(1, 'b' + num_site_limit), ".test"});
  GURL iframe_url = https_server()->GetURL(iframe_host, kSimplePagePath);

  int overall_budget_remaining =
      kSelectURLOverallBitBudget % kSelectURLSiteBitBudget;

  for (int j = 0; j < overall_budget_remaining; j++) {
    // Create a new iframe.
    FrameTreeNode* iframe_node =
        CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

    RunSuccessfulSelectURLInIframe(iframe_node,
                                   /*num_urls=*/2, &console_observer);
  }

  // Create a new iframe.
  FrameTreeNode* final_iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

  if (LimitSelectURLCalls()) {
    EXPECT_TRUE(ExecJs(final_iframe_node, R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

    // The overall pageload limit for `selectURL()` has now been reached. Make
    // one more call, which will return the default URL due to insufficient
    // overall pageload budget.
    absl::optional<std::pair<GURL, double>> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(final_iframe_node,
                                                         iframe_host,
                                                         /*num_urls=*/2);
    ASSERT_TRUE(result_pair.has_value());

    GURL expected_mapped_url =
        https_server()->GetURL(iframe_host, "/fenced_frames/title0.html");
    EXPECT_EQ(result_pair->first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair->second, 0.0);

    EXPECT_EQ("Insufficient budget for selectURL().",
              base::UTF16ToUTF8(console_observer.messages().back().message));

  } else {
    // The `selectURL()` limit is disabled. The call will run successfully.
    RunSuccessfulSelectURLInIframe(final_iframe_node,
                                   /*num_urls=*/2, &console_observer);
  }

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(
      kTimingSelectUrlExecutedInWorkletHistogram,
      num_site_limit * (2 + per_site_input2_call_limit) +
          overall_budget_remaining + 1);
}

class SharedStorageContextBrowserTest
    : public SharedStorageFencedFrameInteractionBrowserTestBase {
 public:
  SharedStorageContextBrowserTest() {
    fenced_frame_api_change_feature_.InitAndEnableFeature(
        blink::features::kFencedFramesAPIChanges);
  }

  ~SharedStorageContextBrowserTest() override = default;

  void GenerateFencedFrameConfig(std::string hostname,
                                 bool keep_alive_after_operation = true) {
    EXPECT_TRUE(ExecJs(shell(), JsReplace("window.keepWorklet = $1;",
                                          keep_alive_after_operation)));
    EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

    TestSelectURLFencedFrameConfigObserver config_observer(
        GetStoragePartition());
    GURL fenced_frame_url = https_server()->GetURL(hostname, kFencedFramePath);

    // There is 1 more "worklet operation": `selectURL()`.
    test_worklet_host_manager()
        .GetAttachedWorkletHost()
        ->SetExpectedWorkletResponsesCount(1);

    EvalJsResult result = EvalJs(shell(), JsReplace(R"(
      (async function() {
        window.fencedFrameConfig = await sharedStorage.selectURL(
          'test-url-selection-operation',
          [
            {
              url: $1,
            }
          ],
          {
            data: {'mockResult': 0},
            resolveToConfig: true,
            keepAlive: keepWorklet
          }
        );
        if (!(fencedFrameConfig instanceof FencedFrameConfig)) {
          throw new Error('selectURL() did not return a FencedFrameConfig.');
        }
        return window.fencedFrameConfig;
      })()
    )",
                                                    fenced_frame_url.spec()));

    EXPECT_TRUE(result.error.empty());
    const absl::optional<GURL>& observed_urn_uuid =
        config_observer.GetUrnUuid();
    ASSERT_TRUE(observed_urn_uuid.has_value());
    EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

    test_worklet_host_manager()
        .GetAttachedWorkletHost()
        ->WaitForWorkletResponses();

    ASSERT_TRUE(config_observer.ConfigObserved());
    const absl::optional<FencedFrameConfig>& fenced_frame_config =
        config_observer.GetConfig();
    EXPECT_TRUE(fenced_frame_config.has_value());
    EXPECT_EQ(fenced_frame_config->urn_uuid_, observed_urn_uuid.value());
  }

 private:
  base::test::ScopedFeatureList fenced_frame_api_change_feature_;
};

// Tests that `blink::FencedFrameConfig::context` can be set and then accessed
// via `sharedStorage.context`. The context must be set prior to fenced frame
// navigation to the config.
IN_PROC_BROWSER_TEST_F(SharedStorageContextBrowserTest,
                       EmbedderContextSetBeforeNavigation_Defined) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // Generate a config using `selectURL()`.
  GenerateFencedFrameConfig("b.test");

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  // Set the context in the config.
  const std::string kEmbedderContext = "some context";
  EXPECT_TRUE(
      ExecJs(shell(),
             JsReplace("window.fencedFrameConfig.setSharedStorageContext($1);",
                       kEmbedderContext)));

  // Create and navigate a fenced frame to the config.
  FrameTreeNode* fenced_frame_root_node =
      CreateFencedFrame(FencedFrameNavigationTarget("fencedFrameConfig"));
  ASSERT_TRUE(fenced_frame_root_node);

  // Try to retrieve the context from the root fenced frame's worklet.
  GURL script_url;
  ExecuteScriptInWorklet(fenced_frame_root_node, R"(
    console.log(sharedStorage.context);
  )",
                         &script_url, /*expected_total_host_count=*/2u);

  // The root fenced frame will have access to the context.
  EXPECT_EQ(kEmbedderContext,
            base::UTF16ToUTF8(console_observer.messages().back().message));

  // Create and navigate to a nested iframe that is same-origin to the root
  // fenced frame.
  GURL same_origin_url = https_server()->GetURL("b.test", kFencedFramePath);
  FrameTreeNode* same_origin_nested_iframe =
      CreateIFrame(fenced_frame_root_node, same_origin_url);
  ASSERT_TRUE(same_origin_nested_iframe);

  // Try to retrieve the context from the nested same-origin iframe's worklet.
  ExecuteScriptInWorklet(same_origin_nested_iframe, R"(
    console.log(sharedStorage.context);
  )",
                         &script_url, /*expected_total_host_count=*/3u);

  // A same-origin child of the fenced frame will have access to the context.
  EXPECT_EQ(kEmbedderContext,
            base::UTF16ToUTF8(console_observer.messages().back().message));

  // Create and navigate to a nested iframe that is cross-origin to the root
  // fenced frame.
  GURL cross_origin_url = https_server()->GetURL("c.test", kFencedFramePath);
  FrameTreeNode* cross_origin_nested_iframe =
      CreateIFrame(fenced_frame_root_node, cross_origin_url);
  ASSERT_TRUE(cross_origin_nested_iframe);

  // Try to retrieve the context from the nested cross-origin iframe's worklet.
  ExecuteScriptInWorklet(cross_origin_nested_iframe, R"(
    console.log(sharedStorage.context);
  )",
                         &script_url, /*expected_total_host_count=*/4u);

  // A cross-origin child of the fenced frame will not have access to the
  // context.
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages().back().message));
}

// Tests that `blink::FencedFrameConfig::context`, when not set and then
// accessed via `sharedStorage.context`, will be undefined.
IN_PROC_BROWSER_TEST_F(SharedStorageContextBrowserTest,
                       EmbedderContextSetAfterNavigation_Undefined) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // Generate a config using `selectURL()`.
  GenerateFencedFrameConfig("b.test");

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  // Create and navigate a fenced frame to the config.
  FrameTreeNode* fenced_frame_root_node =
      CreateFencedFrame(FencedFrameNavigationTarget("fencedFrameConfig"));
  ASSERT_TRUE(fenced_frame_root_node);

  // Set the context in the config. Since the fenced frame has already been
  // navigated to the config and we are not going to navigate to it again, this
  // context will not be propagated to the browser process and so won't be
  // accessible via `sharedStorage.context`.
  const std::string kEmbedderContext = "some context";
  EXPECT_TRUE(
      ExecJs(shell(),
             JsReplace("window.fencedFrameConfig.setSharedStorageContext($1);",
                       kEmbedderContext)));

  // Try to retrieve the context from the root fenced frame's worklet.
  GURL script_url;
  ExecuteScriptInWorklet(fenced_frame_root_node, R"(
    console.log(sharedStorage.context);
  )",
                         &script_url, /*expected_total_host_count=*/2u);

  // The root fenced frame will see that the context is undefined because it was
  // not set before fenced frame navigation.
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages().back().message));
}

// Tests that `blink::FencedFrameConfig::context`, when set after a first
// navigation to the config and before a second fenced frame navigation to the
// same config, is updated, as seen via `sharedStorage.context`.
IN_PROC_BROWSER_TEST_F(SharedStorageContextBrowserTest,
                       EmbedderContextNavigateTwice_ContextUpdated) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // Generate a config using `selectURL()`.
  GenerateFencedFrameConfig("b.test");

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  // Set the context in the config.
  const std::string kEmbedderContext = "some context";
  EXPECT_TRUE(
      ExecJs(shell(),
             JsReplace("window.fencedFrameConfig.setSharedStorageContext($1);",
                       kEmbedderContext)));

  // Create and navigate a fenced frame to the config.
  FrameTreeNode* fenced_frame_root_node =
      CreateFencedFrame(FencedFrameNavigationTarget("fencedFrameConfig"));
  ASSERT_TRUE(fenced_frame_root_node);

  // Try to retrieve the context from the root fenced frame's worklet.
  GURL script_url;
  ExecuteScriptInWorklet(fenced_frame_root_node, R"(
    console.log(sharedStorage.context);
  )",
                         &script_url, /*expected_total_host_count=*/2u);

  // The root fenced frame will have access to the context.
  EXPECT_EQ(kEmbedderContext,
            base::UTF16ToUTF8(console_observer.messages().back().message));

  // Set the context in the config.
  const std::string kNewEmbedderContext = "some different context";
  EXPECT_TRUE(
      ExecJs(shell(),
             JsReplace("window.fencedFrameConfig.setSharedStorageContext($1);",
                       kNewEmbedderContext)));

  // Navigate the fenced frame again to the updated config.
  NavigateExistingFencedFrame(fenced_frame_root_node,
                              FencedFrameNavigationTarget("fencedFrameConfig"));

  // Try to retrieve the context from the root fenced frame's worklet.
  ExecuteScriptInWorklet(fenced_frame_root_node, R"(
    console.log(sharedStorage.context);
  )",
                         &script_url, /*expected_total_host_count=*/2u);

  // The root fenced frame will have access to the updated context.
  EXPECT_EQ(kNewEmbedderContext,
            base::UTF16ToUTF8(console_observer.messages().back().message));
}

// Tests that `blink::FencedFrameConfig::context` can be set and then accessed
// via `sharedStorage.context`, but that any context string exceeding the length
// limit is truncated.
IN_PROC_BROWSER_TEST_F(SharedStorageContextBrowserTest,
                       EmbedderContextExceedsLengthLimit_Truncated) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // Generate a config using `selectURL()`.
  GenerateFencedFrameConfig("b.test");

  EXPECT_EQ("Finish executing 'test-url-selection-operation'",
            base::UTF16ToUTF8(console_observer.messages().back().message));

  // Set the context in the config.
  const std::string kLongEmbedderContext(
      blink::kFencedFrameConfigSharedStorageContextMaxLength, 'x');
  EXPECT_TRUE(
      ExecJs(shell(),
             JsReplace("window.fencedFrameConfig.setSharedStorageContext($1);",
                       kLongEmbedderContext + 'X')));

  // Create and navigate a fenced frame to the config.
  FrameTreeNode* fenced_frame_root_node =
      CreateFencedFrame(FencedFrameNavigationTarget("fencedFrameConfig"));
  ASSERT_TRUE(fenced_frame_root_node);

  // Try to retrieve the context from the root fenced frame's worklet.
  GURL script_url;
  ExecuteScriptInWorklet(fenced_frame_root_node, R"(
    console.log(sharedStorage.context);
  )",
                         &script_url, /*expected_total_host_count=*/2u);

  // The root fenced frame will have access to the context, which will be
  // truncated.
  EXPECT_EQ(kLongEmbedderContext,
            base::UTF16ToUTF8(console_observer.messages().back().message));
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    StringRoundTrip_SetThenGet_UnpairedSurrogatesArePreserved) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  GURL script_url =
      https_server()->GetURL("a.test", "/shared_storage/round_trip.js");

  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  std::vector<std::tuple<std::vector<uint16_t>, bool>> string_test_cases = {
      {std::vector<uint16_t>({0x0068}), true},          // letter 'h'
      {std::vector<uint16_t>({0xd800}), false},         // lone high surrogate
      {std::vector<uint16_t>({0xdc00}), false},         // lone low surrogate
      {std::vector<uint16_t>({0xd800, 0xdc00}), true},  // surrogate pair
  };

  for (size_t i = 0; i < string_test_cases.size(); ++i) {
    const std::vector<uint16_t>& char_code_array =
        std::get<0>(string_test_cases[i]);
    std::u16string test_string =
        std::u16string(char_code_array.begin(), char_code_array.end());
    base::Value::List char_code_values;
    for (uint16_t char_code : char_code_array) {
      char_code_values.Append(static_cast<int>(char_code));
    }

    const bool expected_valid = std::get<1>(string_test_cases[i]);

    // Check validity of UTF-16.
    std::string base_output;
    EXPECT_EQ(expected_valid,
              base::UTF16ToUTF8(test_string.c_str(), test_string.size(),
                                &base_output));

    // The dummy assignment is necessary, because otherwise under the hood,
    // `ExecJs` makes a call to that tries to evaluate the most recent script
    // result as a `base::Value`, and `char_code_values` causes that to fail.
    EXPECT_TRUE(ExecJs(shell(), JsReplace(R"(window.charCodeArray = $1;
                                             window.dummyAssignment = 0;)",
                                          std::move(char_code_values))));

    // We will wait for 1 "worklet operation": `run()`.
    test_worklet_host_manager()
        .GetAttachedWorkletHost()
        ->SetExpectedWorkletResponsesCount(1);

    EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('set-get-operation',
                          {data: {'key': 'asValue',
                                  'valueCharCodeArray' : charCodeArray},
                           keepAlive: true});
      )"));

    test_worklet_host_manager()
        .GetAttachedWorkletHost()
        ->WaitForWorkletResponses();

    EXPECT_EQ(4u * (i + 1), console_observer.messages().size());
    EXPECT_EQ(u"key: 'asValue'", console_observer.messages()[4 * i].message);
    EXPECT_THAT(
        base::UTF16ToUTF8(console_observer.messages()[4 * i + 1].message),
        testing::HasSubstr("value: '"));
    EXPECT_THAT(
        base::UTF16ToUTF8(console_observer.messages()[4 * i + 2].message),
        testing::HasSubstr("retrieved sharedStorage.get('asValue'): '"));
    EXPECT_THAT(
        base::UTF16ToUTF8(console_observer.messages()[4 * i + 3].message),
        testing::HasSubstr("' was retrieved: true"));
  }
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    StringRoundTrip_SetThenKeys_UnpairedSurrogatesArePreserved) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  GURL script_url =
      https_server()->GetURL("a.test", "/shared_storage/round_trip.js");

  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  std::vector<std::tuple<std::vector<uint16_t>, bool>> string_test_cases = {
      {std::vector<uint16_t>({0x0068}), true},          // letter 'h'
      {std::vector<uint16_t>({0xd800}), false},         // lone high surrogate
      {std::vector<uint16_t>({0xdc00}), false},         // lone low surrogate
      {std::vector<uint16_t>({0xd800, 0xdc00}), true},  // surrogate pair
  };

  for (size_t i = 0; i < string_test_cases.size(); ++i) {
    const std::vector<uint16_t>& char_code_array =
        std::get<0>(string_test_cases[i]);
    std::u16string test_string =
        std::u16string(char_code_array.begin(), char_code_array.end());
    base::Value::List char_code_values;
    for (uint16_t char_code : char_code_array) {
      char_code_values.Append(static_cast<int>(char_code));
    }

    const bool expected_valid = std::get<1>(string_test_cases[i]);

    // Check validity of UTF-16.
    std::string base_output;
    EXPECT_EQ(expected_valid,
              base::UTF16ToUTF8(test_string.c_str(), test_string.size(),
                                &base_output));

    // The dummy assignment is necessary, because otherwise under the hood,
    // `ExecJs` makes a call to that tries to evaluate the most recent script
    // result as a `base::Value`, and `char_code_values` causes that to fail.
    EXPECT_TRUE(ExecJs(shell(), JsReplace(R"(window.charCodeArray = $1;
                                             window.dummyAssignment = 0;)",
                                          std::move(char_code_values))));

    // We will wait for 1 "worklet operation": `run()`.
    test_worklet_host_manager()
        .GetAttachedWorkletHost()
        ->SetExpectedWorkletResponsesCount(1);

    EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('set-keys-operation',
                          {data: {'keyCharCodeArray' : charCodeArray,
                                  'value': 'asKey'},
                           keepAlive: true});
      )"));

    test_worklet_host_manager()
        .GetAttachedWorkletHost()
        ->WaitForWorkletResponses();

    EXPECT_EQ(4u * (i + 1), console_observer.messages().size());
    EXPECT_THAT(base::UTF16ToUTF8(console_observer.messages()[4 * i].message),
                testing::HasSubstr("key: '"));
    EXPECT_EQ(u"value: 'asKey'",
              console_observer.messages()[4 * i + 1].message);
    EXPECT_THAT(
        base::UTF16ToUTF8(console_observer.messages()[4 * i + 2].message),
        testing::HasSubstr("retrieved key: '"));
    EXPECT_THAT(
        base::UTF16ToUTF8(console_observer.messages()[4 * i + 3].message),
        testing::HasSubstr("' was retrieved: true"));
  }
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    StringRoundTrip_AppendThenDelete_UnpairedSurrogatesArePreserved) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  GURL script_url =
      https_server()->GetURL("a.test", "/shared_storage/round_trip.js");

  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  std::vector<std::tuple<std::vector<uint16_t>, bool>> string_test_cases = {
      {std::vector<uint16_t>({0x0068}), true},          // letter 'h'
      {std::vector<uint16_t>({0xd800}), false},         // lone high surrogate
      {std::vector<uint16_t>({0xdc00}), false},         // lone low surrogate
      {std::vector<uint16_t>({0xd800, 0xdc00}), true},  // surrogate pair
  };

  for (size_t i = 0; i < string_test_cases.size(); ++i) {
    const std::vector<uint16_t>& char_code_array =
        std::get<0>(string_test_cases[i]);
    std::u16string test_string =
        std::u16string(char_code_array.begin(), char_code_array.end());
    base::Value::List char_code_values;
    for (uint16_t char_code : char_code_array) {
      char_code_values.Append(static_cast<int>(char_code));
    }

    const bool expected_valid = std::get<1>(string_test_cases[i]);

    // Check validity of UTF-16.
    std::string base_output;
    EXPECT_EQ(expected_valid,
              base::UTF16ToUTF8(test_string.c_str(), test_string.size(),
                                &base_output));

    // The dummy assignment is necessary, because otherwise under the hood,
    // `ExecJs` makes a call to that tries to evaluate the most recent script
    // result as a `base::Value`, and `char_code_values` causes that to fail.
    EXPECT_TRUE(ExecJs(shell(), JsReplace(R"(window.charCodeArray = $1;
                                             window.dummyAssignment = 0;)",
                                          std::move(char_code_values))));

    // We will wait for 1 "worklet operation": `run()`.
    test_worklet_host_manager()
        .GetAttachedWorkletHost()
        ->SetExpectedWorkletResponsesCount(1);

    EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('append-delete-operation',
                          {data: {'keyCharCodeArray' : charCodeArray,
                                  'value': 'asKey'},
                           keepAlive: true});
      )"));

    test_worklet_host_manager()
        .GetAttachedWorkletHost()
        ->WaitForWorkletResponses();

    EXPECT_EQ(3u * (i + 1), console_observer.messages().size());
    EXPECT_THAT(base::UTF16ToUTF8(console_observer.messages()[3 * i].message),
                testing::HasSubstr("key: '"));
    EXPECT_EQ(u"value: 'asKey'",
              console_observer.messages()[3 * i + 1].message);
    EXPECT_EQ(u"delete success: true",
              console_observer.messages()[3 * i + 2].message);
  }
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageBrowserTest,
    StringRoundTrip_AppendThenEntries_UnpairedSurrogatesArePreserved) {
  GURL url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  GURL script_url =
      https_server()->GetURL("a.test", "/shared_storage/round_trip.js");

  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  std::vector<std::tuple<std::vector<uint16_t>, bool>> string_test_cases = {
      {std::vector<uint16_t>({0x0068}), true},          // letter 'h'
      {std::vector<uint16_t>({0xd800}), false},         // lone high surrogate
      {std::vector<uint16_t>({0xdc00}), false},         // lone low surrogate
      {std::vector<uint16_t>({0xd800, 0xdc00}), true},  // surrogate pair
  };

  for (size_t i = 0; i < string_test_cases.size(); ++i) {
    const std::vector<uint16_t>& char_code_array =
        std::get<0>(string_test_cases[i]);
    std::u16string test_string =
        std::u16string(char_code_array.begin(), char_code_array.end());
    base::Value::List char_code_values;
    for (uint16_t char_code : char_code_array) {
      char_code_values.Append(static_cast<int>(char_code));
    }

    const bool expected_valid = std::get<1>(string_test_cases[i]);

    // Check validity of UTF-16.
    std::string base_output;
    EXPECT_EQ(expected_valid,
              base::UTF16ToUTF8(test_string.c_str(), test_string.size(),
                                &base_output));

    // The dummy assignment is necessary, because otherwise under the hood,
    // `ExecJs` makes a call to that tries to evaluate the most recent script
    // result as a `base::Value`, and `char_code_values` causes that to fail.
    EXPECT_TRUE(ExecJs(shell(), JsReplace(R"(window.charCodeArray = $1;
                                             window.dummyAssignment = 0;)",
                                          std::move(char_code_values))));

    // We will wait for 1 "worklet operation": `run()`.
    test_worklet_host_manager()
        .GetAttachedWorkletHost()
        ->SetExpectedWorkletResponsesCount(1);

    EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('append-entries-operation',
                          {data: {'key': 'asValue',
                                  'valueCharCodeArray': charCodeArray},
                           keepAlive: true});
      )"));

    test_worklet_host_manager()
        .GetAttachedWorkletHost()
        ->WaitForWorkletResponses();

    EXPECT_EQ(4u * (i + 1), console_observer.messages().size());
    EXPECT_EQ(u"key: 'asValue'", console_observer.messages()[4 * i].message);
    EXPECT_THAT(
        base::UTF16ToUTF8(console_observer.messages()[4 * i + 1].message),
        testing::HasSubstr("value: '"));
    EXPECT_THAT(
        base::UTF16ToUTF8(console_observer.messages()[4 * i + 2].message),
        testing::HasSubstr("retrieved key: '"));
    EXPECT_THAT(
        base::UTF16ToUTF8(console_observer.messages()[4 * i + 2].message),
        testing::HasSubstr("; retrieved value: '"));
    EXPECT_THAT(
        base::UTF16ToUTF8(console_observer.messages()[4 * i + 3].message),
        testing::HasSubstr("' was retrieved: true"));
  }
}

class SharedStorageHeaderObserverBrowserTest
    : public SharedStorageBrowserTestBase {
 public:
  using Operation = network::mojom::SharedStorageOperation;
  using OperationResult = storage::SharedStorageManager::OperationResult;

  SharedStorageHeaderObserverBrowserTest() {
    shared_storage_m118_feature_.InitAndEnableFeature(
        blink::features::kSharedStorageAPIM118);
  }

  void FinishSetup() override {
    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);

    auto observer = std::make_unique<TestSharedStorageHeaderObserver>(
        GetStoragePartition());
    observer_ = observer->GetMutableWeakPtr();
    static_cast<StoragePartitionImpl*>(GetStoragePartition())
        ->OverrideSharedStorageHeaderObserverForTesting(std::move(observer));
  }

  uint16_t port() { return https_server()->port(); }

  bool NavigateToURLWithResponse(
      Shell* window,
      const GURL& url,
      net::test_server::ControllableHttpResponse& response,
      net::HttpStatusCode http_status,
      const std::string& content_type = std::string("text/html"),
      const std::string& content = std::string(),
      const std::vector<std::string>& cookies = {},
      const std::vector<std::string>& extra_headers = {}) {
    auto* web_contents = window->web_contents();
    DCHECK(web_contents);

    // Prepare for the navigation.
    WaitForLoadStop(web_contents);
    TestNavigationObserver same_tab_observer(
        web_contents,
        /*expected_number_of_navigations=*/1,
        MessageLoopRunner::QuitMode::IMMEDIATE,
        /*ignore_uncommitted_navigations=*/false);
    if (!blink::IsRendererDebugURL(url)) {
      same_tab_observer.set_expected_initial_url(url);
    }

    // This mimics behavior of Shell::LoadURL...
    NavigationController::LoadURLParams params(url);
    params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    web_contents->GetController().LoadURLWithParams(params);
    web_contents->GetOutermostWebContents()->Focus();

    response.WaitForRequest();

    response.Send(http_status, content_type, content, cookies, extra_headers);
    response.Done();

    // Wait until the expected number of navigations finish.
    same_tab_observer.Wait();

    if (!IsLastCommittedEntryOfPageType(web_contents, PAGE_TYPE_NORMAL)) {
      return false;
    }

    bool is_same_url = web_contents->GetLastCommittedURL() == url;
    if (!is_same_url) {
      DLOG(WARNING) << "Expected URL " << url << " but observed "
                    << web_contents->GetLastCommittedURL();
    }
    return is_same_url;
  }

  std::string ReplacePortInString(std::string str) {
    const std::string kToReplace("{{port}}");
    size_t index = str.find(kToReplace);
    while (index != std::string::npos) {
      str = str.replace(index, kToReplace.size(), base::NumberToString(port()));
      index = str.find(kToReplace);
    }
    return str;
  }

  void SetUpResponsesAndNavigateMainPage(
      std::string main_hostname,
      std::string subresource_or_subframe_hostname,
      absl::optional<std::string> shared_storage_permissions = absl::nullopt,
      bool is_image = false,
      absl::optional<std::string> redirect_hostname = absl::nullopt) {
    subresource_or_subframe_content_type_ =
        is_image ? "image/png" : "text/plain;charset=UTF-8";
    const char* subresource_or_subframe_path =
        is_image ? kPngPath : kTitle1Path;
    subresource_or_subframe_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            https_server(), subresource_or_subframe_path);

    std::unique_ptr<net::test_server::ControllableHttpResponse> main_response;

    if (shared_storage_permissions.has_value()) {
      main_response =
          std::make_unique<net::test_server::ControllableHttpResponse>(
              https_server(), kSimplePagePath);
    }
    if (redirect_hostname.has_value()) {
      redirected_response_ =
          std::make_unique<net::test_server::ControllableHttpResponse>(
              https_server(), kTitle2Path);
    }

    ASSERT_TRUE(https_server()->Start());

    main_url_ =
        https_server()->GetURL(std::move(main_hostname), kSimplePagePath);
    subresource_or_subframe_url_ =
        https_server()->GetURL(std::move(subresource_or_subframe_hostname),
                               subresource_or_subframe_path);
    subresource_or_subframe_origin_ =
        url::Origin::Create(subresource_or_subframe_url_);
    if (redirect_hostname.has_value()) {
      redirect_url_ =
          https_server()->GetURL(redirect_hostname.value(), kTitle2Path);
      redirect_origin_ = url::Origin::Create(redirect_url_);
    }

    if (shared_storage_permissions.has_value()) {
      EXPECT_TRUE(NavigateToURLWithResponse(
          shell(), main_url_, *main_response,
          /*http_status=*/net::HTTP_OK,
          /*content_type=*/"text/plain;charset=UTF-8",
          /*content=*/{}, /*cookies=*/{}, /*extra_headers=*/
          {"Permissions-Policy: shared-storage=" +
           ReplacePortInString(
               std::move(shared_storage_permissions.value()))}));
    } else {
      EXPECT_TRUE(NavigateToURL(shell(), main_url_));
    }
  }

  void WaitForRequestAndSendResponse(
      net::test_server::ControllableHttpResponse& response,
      bool expect_writable_header,
      net::HttpStatusCode http_status,
      const std::string& content_type,
      const std::vector<std::string>& extra_headers) {
    response.WaitForRequest();
    if (expect_writable_header) {
      ASSERT_TRUE(base::Contains(response.http_request()->headers,
                                 "Sec-Shared-Storage-Writable"));
    } else {
      EXPECT_FALSE(base::Contains(response.http_request()->headers,
                                  "Sec-Shared-Storage-Writable"));
    }
    EXPECT_EQ(response.http_request()->content, "");
    response.Send(http_status, content_type,
                  /*content=*/{}, /*cookies=*/{}, extra_headers);
    response.Done();
  }

  void WaitForSubresourceOrSubframeRequestAndSendResponse(
      bool expect_writable_header,
      net::HttpStatusCode http_status,
      const std::vector<std::string>& extra_headers) {
    WaitForRequestAndSendResponse(
        *subresource_or_subframe_response_, expect_writable_header, http_status,
        subresource_or_subframe_content_type_, extra_headers);
  }

  void WaitForRedirectRequestAndSendResponse(
      bool expect_writable_header,
      net::HttpStatusCode http_status,
      const std::vector<std::string>& extra_headers) {
    WaitForRequestAndSendResponse(
        *redirected_response_, expect_writable_header, http_status,
        subresource_or_subframe_content_type_, extra_headers);
  }

  void FetchWithSharedStorageWritable(const ToRenderFrameHost& execution_target,
                                      const GURL& url) {
    EXPECT_TRUE(ExecJs(execution_target,
                       JsReplace(R"(
      fetch($1, {sharedStorageWritable: true});
    )",
                                 url.spec()),
                       EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  }

  void StartServerAndLoadMainURLWithSameOriginSubresourceOrSubframe(
      std::string main_hostname,
      std::string main_path) {
    ASSERT_TRUE(https_server()->Start());
    main_url_ =
        https_server()->GetURL(std::move(main_hostname), std::move(main_path));
    subresource_or_subframe_origin_ = url::Origin::Create(main_url_);
    EXPECT_TRUE(NavigateToURL(shell(), main_url_));
  }

  void CreateSharedStorageWritableImage(
      const ToRenderFrameHost& execution_target,
      const GURL& url) {
    EXPECT_TRUE(ExecJs(execution_target, JsReplace(R"(
      let img = document.createElement('img');
      img.src = $1;
      img.sharedStorageWritable = true;
      document.body.appendChild(img);
    )",
                                                   url.spec())));
  }

  void CreateSharedStorageWritableIframe(
      const ToRenderFrameHost& execution_target,
      const GURL& url) {
    EXPECT_TRUE(ExecJs(execution_target, JsReplace(R"(
      let frame = document.createElement('iframe');
      frame.sharedStorageWritable = true;
      frame.src = $1;
      document.body.appendChild(frame);
    )",
                                                   url.spec())));
  }

 protected:
  base::WeakPtr<TestSharedStorageHeaderObserver> observer_;
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      subresource_or_subframe_response_;
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      redirected_response_;
  GURL main_url_;
  GURL subresource_or_subframe_url_;
  GURL redirect_url_;
  url::Origin subresource_or_subframe_origin_;
  url::Origin redirect_origin_;
  std::string subresource_or_subframe_content_type_;

 private:
  base::test::ScopedFeatureList shared_storage_m118_feature_;
};

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_SameOrigin_PermissionsDefault_ClearSetAppend) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(3);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true, true, true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  ClearOperation(subresource_or_subframe_origin_,
                                 OperationResult::kSuccess),
                  SetOperation(subresource_or_subframe_origin_, "hello",
                               "world", true, OperationResult::kSet),
                  AppendOperation(subresource_or_subframe_origin_, "hello",
                                  "there", OperationResult::kSet)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_SameOrigin_PermissionsNone) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/"()");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=wont;key=set"});

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_SameOrigin_PermissionsAll_ClearSetAppend) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/"*");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(3);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true, true, true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  ClearOperation(subresource_or_subframe_origin_,
                                 OperationResult::kSuccess),
                  SetOperation(subresource_or_subframe_origin_, "hello",
                               "world", true, OperationResult::kSet),
                  AppendOperation(subresource_or_subframe_origin_, "hello",
                                  "there", OperationResult::kSet)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_SameOrigin_PermissionsSelf_ClearSetAppend) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/"self");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(3);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true, true, true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  ClearOperation(subresource_or_subframe_origin_,
                                 OperationResult::kSuccess),
                  SetOperation(subresource_or_subframe_origin_, "hello",
                               "world", true, OperationResult::kSet),
                  AppendOperation(subresource_or_subframe_origin_, "hello",
                                  "there", OperationResult::kSet)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_CrossOrigin_PermissionsDefault_ClearSetAppend) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  // Create an iframe that's same-origin to the fetch URL.
  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(),
                   https_server()->GetURL("b.test", kTitle2Path));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(3);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true, true, true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  ClearOperation(subresource_or_subframe_origin_,
                                 OperationResult::kSuccess),
                  SetOperation(subresource_or_subframe_origin_, "hello",
                               "world", true, OperationResult::kSet),
                  AppendOperation(subresource_or_subframe_origin_, "hello",
                                  "there", OperationResult::kSet)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_CrossOrigin_PermissionsNone) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/"()");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=wont;key=set"});

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_CrossOrigin_PermissionsAll_ClearSetAppend) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/"*");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(3);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true, true, true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  ClearOperation(subresource_or_subframe_origin_,
                                 OperationResult::kSuccess),
                  SetOperation(subresource_or_subframe_origin_, "hello",
                               "world", true, OperationResult::kSet),
                  AppendOperation(subresource_or_subframe_origin_, "hello",
                                  "there", OperationResult::kSet)));

  // Create an iframe that's same-origin to the fetch URL.
  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(),
                   https_server()->GetURL("b.test", kTitle2Path));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_CrossOrigin_PermissionsSelf) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/"self");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=wont;key=set"});

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Fetch_CrossOrigin_Redirect_InititalAllowed_FinalAllowed_WriteInitial) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://c.test:{{port}}\")",
      /*is_image=*/false,
      /*redirect_hostname=*/"c.test");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_url_.spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(3);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true, true, true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  ClearOperation(subresource_or_subframe_origin_,
                                 OperationResult::kSuccess),
                  SetOperation(subresource_or_subframe_origin_, "hello",
                               "world", true, OperationResult::kSet),
                  AppendOperation(subresource_or_subframe_origin_, "hello",
                                  "there", OperationResult::kSet)));

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *"});

  // There won't be additional operations invoked.
  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true, true, true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  ClearOperation(subresource_or_subframe_origin_,
                                 OperationResult::kSuccess),
                  SetOperation(subresource_or_subframe_origin_, "hello",
                               "world", true, OperationResult::kSet),
                  AppendOperation(subresource_or_subframe_origin_, "hello",
                                  "there", OperationResult::kSet)));

  // Create an iframe that's same-origin to the original fetch URL.
  FrameTreeNode* iframe_node1 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node1, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Fetch_CrossOrigin_Redirect_InititalAllowed_FinalAllowed_WriteFinal) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://c.test:{{port}}\")",
      /*is_image=*/false,
      /*redirect_hostname=*/"c.test");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_url_.spec()}),
       "Access-Control-Allow-Origin: *"});

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=will;key=set"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(2);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().back().first, redirect_origin_);
  EXPECT_THAT(observer_->header_results().back().second,
              testing::ElementsAre(true, true));
  EXPECT_THAT(
      observer_->operations(),
      testing::ElementsAre(
          DeleteOperation(redirect_origin_, "a", OperationResult::kSuccess),
          SetOperation(redirect_origin_, "set", "will", absl::nullopt,
                       OperationResult::kSet)));

  // Create an iframe that's same-origin to the original fetch URL.
  FrameTreeNode* iframe_node1 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node1, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());

  // Nothing was set in b.test's shared storage.
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[1].message));

  // Create an iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_url_);

  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url, /*expected_total_host_count=*/2u);

  // The entry was set in c.test's shared storage.
  EXPECT_EQ(4u, console_observer.messages().size());
  EXPECT_EQ("will", base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[3].message));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Fetch_CrossOrigin_Redirect_InititalAllowed_FinalAllowed_WriteBoth) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://c.test:{{port}}\")",
      /*is_image=*/false,
      /*redirect_hostname=*/"c.test");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_url_.spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(3);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true, true, true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  ClearOperation(subresource_or_subframe_origin_,
                                 OperationResult::kSuccess),
                  SetOperation(subresource_or_subframe_origin_, "hello",
                               "world", true, OperationResult::kSet),
                  AppendOperation(subresource_or_subframe_origin_, "hello",
                                  "there", OperationResult::kSet)));

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=will;key=set"});

  // There will now have been a total of 5 operations (3 previous, 2 current).
  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(5);

  EXPECT_EQ(observer_->header_results().size(), 2u);
  EXPECT_EQ(observer_->header_results().back().first, redirect_origin_);
  EXPECT_THAT(observer_->header_results().back().second,
              testing::ElementsAre(true, true));
  EXPECT_THAT(
      observer_->operations(),
      testing::ElementsAre(
          ClearOperation(subresource_or_subframe_origin_,
                         OperationResult::kSuccess),
          SetOperation(subresource_or_subframe_origin_, "hello", "world", true,
                       OperationResult::kSet),
          AppendOperation(subresource_or_subframe_origin_, "hello", "there",
                          OperationResult::kSet),
          DeleteOperation(redirect_origin_, "a", OperationResult::kSuccess),
          SetOperation(redirect_origin_, "set", "will", absl::nullopt,
                       OperationResult::kSet)));

  // Create an iframe that's same-origin to the original fetch URL.
  FrameTreeNode* iframe_node1 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node1, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  // Only one entry was set in b.test's shared storage.
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[2].message));

  // Create an iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_url_);

  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url, /*expected_total_host_count=*/2u);

  // One entry was set in c.test's shared storage.
  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("will", base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[4].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_CrossOrigin_Redirect_InititalAllowed_FinalDenied) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\")",
      /*is_image=*/false,
      /*redirect_hostname=*/"c.test");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_url_.spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(3);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true, true, true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  ClearOperation(subresource_or_subframe_origin_,
                                 OperationResult::kSuccess),
                  SetOperation(subresource_or_subframe_origin_, "hello",
                               "world", true, OperationResult::kSet),
                  AppendOperation(subresource_or_subframe_origin_, "hello",
                                  "there", OperationResult::kSet)));

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=wont;key=set"});

  // No new operations are invoked.
  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true, true, true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  ClearOperation(subresource_or_subframe_origin_,
                                 OperationResult::kSuccess),
                  SetOperation(subresource_or_subframe_origin_, "hello",
                               "world", true, OperationResult::kSet),
                  AppendOperation(subresource_or_subframe_origin_, "hello",
                                  "there", OperationResult::kSet)));

  // Create an iframe that's same-origin to the original fetch URL.
  FrameTreeNode* iframe_node1 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node1, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  // Create an iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_url_);

  EvalJsResult result = EvalJs(iframe_node2, R"(
        sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
      )");

  EXPECT_THAT(result.error,
              testing::HasSubstr("The \"shared-storage\" Permissions Policy "
                                 "denied the method on window.sharedStorage."));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_SameOrigin_PermissionsDefault_VerifyDelete) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test");

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(shell(), R"(sharedStorage.set('hello', 'world');)"));

  GURL script_url =
      https_server()->GetURL("a.test", "/shared_storage/getter_module.js");

  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('get-operation', {data: {'key': 'hello'},
                                            keepAlive: true});
      )"));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("sharedStorage.length(): 1",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("sharedStorage.get('hello'): world",
            base::UTF16ToUTF8(console_observer.messages()[1].message));

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: \"delete\";key=hello"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  DeleteOperation(subresource_or_subframe_origin_, "hello",
                                  OperationResult::kSuccess)));

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('get-operation', {data: {'key': 'hello'}});
      )"));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  EXPECT_EQ(4u, console_observer.messages().size());
  EXPECT_EQ("sharedStorage.length(): 0",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("sharedStorage.get('hello'): undefined",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_SameOrigin_PermissionsDefault_VerifyClear) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test");

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(shell(), R"(sharedStorage.set('hello', 'world');)"));

  GURL script_url =
      https_server()->GetURL("a.test", "/shared_storage/getter_module.js");

  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("sharedStorage.worklet.addModule($1)", script_url)));

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('get-operation', {data: {'key': 'hello'},
                                            keepAlive: true});
      )"));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("sharedStorage.length(): 1",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("sharedStorage.get('hello'): world",
            base::UTF16ToUTF8(console_observer.messages()[1].message));

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: \"clear\""});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(ClearOperation(
                  subresource_or_subframe_origin_, OperationResult::kSuccess)));

  // There is 1 more "worklet operation": `run()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EXPECT_TRUE(ExecJs(shell(), R"(
        sharedStorage.run('get-operation', {data: {'key': 'hello'}});
      )"));

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  EXPECT_EQ(4u, console_observer.messages().size());
  EXPECT_EQ("sharedStorage.length(): 0",
            base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("sharedStorage.get('hello'): undefined",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Fetch_SameOrigin_PermissionsDefault_MultipleSet_Bytes) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: set;key=:aGVsbG8=:;value=:d29ybGQ=:, "
       "set;value=:ZnJpZW5k:;key=:aGVsbG8=:;ignore_if_present=?0, "
       "set;ignore_if_present;key=hello;value=there"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(3);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true, true, true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  SetOperation(subresource_or_subframe_origin_, "hello",
                               "world", absl::nullopt, OperationResult::kSet),
                  SetOperation(subresource_or_subframe_origin_, "hello",
                               "friend", false, OperationResult::kSet),
                  SetOperation(subresource_or_subframe_origin_, "hello",
                               "there", true, OperationResult::kIgnored)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("friend",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       NetworkServiceRestarts_HeaderObserverContinuesWorking) {
  subresource_or_subframe_response_ =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), kTitle1Path);
  ASSERT_TRUE(https_server()->Start());

  if (IsInProcessNetworkService()) {
    return;
  }

  main_url_ = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url_));
  ASSERT_TRUE(observer_);

  SimulateNetworkServiceCrash();
  static_cast<StoragePartitionImpl*>(GetStoragePartition())
      ->FlushNetworkInterfaceForTesting();

  // We should still have an `observer_`.
  ASSERT_TRUE(observer_);

  // We need to reinitialize `subresource_or_subframe_url_` after network
  // service restart. Fetching with `sharedStorageWritable` works as expected.
  subresource_or_subframe_url_ = https_server()->GetURL("a.test", kTitle1Path);
  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(3);

  subresource_or_subframe_origin_ =
      url::Origin::Create(subresource_or_subframe_url_);
  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true, true, true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  ClearOperation(subresource_or_subframe_origin_,
                                 OperationResult::kSuccess),
                  SetOperation(subresource_or_subframe_origin_, "hello",
                               "world", true, OperationResult::kSet),
                  AppendOperation(subresource_or_subframe_origin_, "hello",
                                  "there", OperationResult::kSet)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       InvalidHeader_NoOperationsInvoked) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: clear, invalid?item"});

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[0].message));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    ParsableUnrecognizedItemSkipped_RecognizedOperationsInvoked) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: clear, unrecognized;unknown_param=1,"
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(3);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true, true, true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  ClearOperation(subresource_or_subframe_origin_,
                                 OperationResult::kSuccess),
                  SetOperation(subresource_or_subframe_origin_, "hello",
                               "world", true, OperationResult::kSet),
                  AppendOperation(subresource_or_subframe_origin_, "hello",
                                  "there", OperationResult::kSet)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       ExtraParametersIgnored) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: clear;unknown_param=1,"
       "set;another_unknown=willIgnore;key=\"hello\";value=\"world\", "
       "append;key=extra;key=hello;value=there;ignore_if_present;pi=3.14,"
       "delete;value=ignored;key=toDelete;ignore_if_present=?0"});

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(4);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true, true, true, true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  ClearOperation(subresource_or_subframe_origin_,
                                 OperationResult::kSuccess),
                  SetOperation(subresource_or_subframe_origin_, "hello",
                               "world", absl::nullopt, OperationResult::kSet),
                  AppendOperation(subresource_or_subframe_origin_, "hello",
                                  "there", OperationResult::kSet),
                  DeleteOperation(subresource_or_subframe_origin_, "toDelete",
                                  OperationResult::kSuccess)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       KeyOrValueLengthInvalid_ItemSkipped) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test");

  FetchWithSharedStorageWritable(shell(), subresource_or_subframe_url_);

  std::string long_str(1025, 'x');
  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {base::StrCat(
          {"Shared-Storage-Write: clear, set;key=", long_str,
           ";value=v,set;key=k;value=", long_str, ",append;key=k;value=",
           long_str, ",append;key=", long_str, ";value=v,delete;key=", long_str,
           ",set;key=\"\";value=v,append;key=\"\";value=v,delete;key=\"\","
           "clear"})});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(2);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true, false, false, false, false, false,
                                   false, false, false, true));
  EXPECT_THAT(
      observer_->operations(),
      testing::ElementsAre(ClearOperation(subresource_or_subframe_origin_,
                                          OperationResult::kSuccess),
                           ClearOperation(subresource_or_subframe_origin_,
                                          OperationResult::kSuccess)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[0].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Image_SameOrigin_PermissionsDefault) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/absl::nullopt,
      /*is_image=*/true);

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: set;key=a;value=b"});

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(SetOperation(subresource_or_subframe_origin_,
                                                "a", "b", absl::nullopt,
                                                OperationResult::kSet)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Image_SameOrigin_PermissionsNone) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/"()",
      /*is_image=*/true);

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Image_SameOrigin_PermissionsAll) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/"*",
      /*is_image=*/true);

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(SetOperation(subresource_or_subframe_origin_,
                                                "a", "b", absl::nullopt,
                                                OperationResult::kSet)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Image_SameOrigin_PermissionsSelf) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/"self",
      /*is_image=*/true);

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(SetOperation(subresource_or_subframe_origin_,
                                                "a", "b", absl::nullopt,
                                                OperationResult::kSet)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Image_CrossOrigin_PermissionsDefault) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/absl::nullopt,
      /*is_image=*/true);

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: set;key=a;value=b"});

  // Create iframe that's same-origin to the image URL.
  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(),
                   https_server()->GetURL("b.test", kTitle2Path));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node, R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(SetOperation(subresource_or_subframe_origin_,
                                                "a", "b", absl::nullopt,
                                                OperationResult::kSet)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Image_CrossOrigin_PermissionsNone) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/"()",
      /*is_image=*/true);

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Image_CrossOrigin_PermissionsAll) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/"*",
      /*is_image=*/true);

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  // Create iframe that's same-origin to the image URL.
  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(),
                   https_server()->GetURL("b.test", kTitle2Path));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node, R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(SetOperation(subresource_or_subframe_origin_,
                                                "a", "b", absl::nullopt,
                                                OperationResult::kSet)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Image_CrossOrigin_PermissionsSelf) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/"self",
      /*is_image=*/true);

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Image_CrossOrigin_Redirect_InititalAllowed_FinalAllowed_WriteInitial) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://c.test:{{port}}\")",
      /*is_image=*/true,
      /*redirect_hostname=*/"c.test");

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_url_.spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *"});

  // Create an iframe that's same-origin to the original image URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  // There won't be additional operations invoked from the redirect, just the
  // original 3.
  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(3);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true, true, true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  ClearOperation(subresource_or_subframe_origin_,
                                 OperationResult::kSuccess),
                  SetOperation(subresource_or_subframe_origin_, "hello",
                               "world", true, OperationResult::kSet),
                  AppendOperation(subresource_or_subframe_origin_, "hello",
                                  "there", OperationResult::kSet)));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Image_CrossOrigin_Redirect_InititalAllowed_FinalAllowed_WriteFinal) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://c.test:{{port}}\")",
      /*is_image=*/true,
      /*redirect_hostname=*/"c.test");

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_url_.spec()}),
       "Access-Control-Allow-Origin: *"});

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=will;key=set"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(2);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().back().first, redirect_origin_);
  EXPECT_THAT(observer_->header_results().back().second,
              testing::ElementsAre(true, true));
  EXPECT_THAT(
      observer_->operations(),
      testing::ElementsAre(
          DeleteOperation(redirect_origin_, "a", OperationResult::kSuccess),
          SetOperation(redirect_origin_, "set", "will", absl::nullopt,
                       OperationResult::kSet)));

  // Create an iframe that's same-origin to the original image URL.
  FrameTreeNode* iframe_node1 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node1, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());

  // Nothing was set in b.test's shared storage.
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[1].message));

  // Create another iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_url_);

  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url, /*expected_total_host_count=*/2u);

  // The entry was set in c.test's shared storage.
  EXPECT_EQ(4u, console_observer.messages().size());
  EXPECT_EQ("will", base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[3].message));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Image_CrossOrigin_Redirect_InititalAllowed_FinalAllowed_WriteBoth) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://c.test:{{port}}\")",
      /*is_image=*/true,
      /*redirect_hostname=*/"c.test");

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_url_.spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=will;key=set"});

  // There will now have been a total of 5 operations (3 previous, 2 current).
  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(5);

  EXPECT_EQ(observer_->header_results().size(), 2u);
  EXPECT_EQ(observer_->header_results().back().first, redirect_origin_);
  EXPECT_THAT(observer_->header_results().back().second,
              testing::ElementsAre(true, true));
  EXPECT_THAT(
      observer_->operations(),
      testing::ElementsAre(
          ClearOperation(subresource_or_subframe_origin_,
                         OperationResult::kSuccess),
          SetOperation(subresource_or_subframe_origin_, "hello", "world", true,
                       OperationResult::kSet),
          AppendOperation(subresource_or_subframe_origin_, "hello", "there",
                          OperationResult::kSet),
          DeleteOperation(redirect_origin_, "a", OperationResult::kSuccess),
          SetOperation(redirect_origin_, "set", "will", absl::nullopt,
                       OperationResult::kSet)));

  // Create an iframe that's same-origin to the original image URL.
  FrameTreeNode* iframe_node1 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node1, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  // Only one entry was set in b.test's shared storage.
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[2].message));

  // Create another iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_url_);

  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url, /*expected_total_host_count=*/2u);

  // One entry was set in c.test's shared storage.
  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("will", base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[4].message));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Image_CrossOrigin_Redirect_InititalAllowed_FinalDenied) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\")",
      /*is_image=*/true,
      /*redirect_hostname=*/"c.test");

  CreateSharedStorageWritableImage(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_url_.spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=wont;key=set"});

  // There won't be additional operations invoked from the redirect, just the
  // original 3.
  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(3);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true, true, true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  ClearOperation(subresource_or_subframe_origin_,
                                 OperationResult::kSuccess),
                  SetOperation(subresource_or_subframe_origin_, "hello",
                               "world", true, OperationResult::kSet),
                  AppendOperation(subresource_or_subframe_origin_, "hello",
                                  "there", OperationResult::kSet)));

  // Create an iframe that's same-origin to the original image URL.
  FrameTreeNode* iframe_node1 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node1, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  // Create another iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_url_);

  EvalJsResult result = EvalJs(iframe_node2, R"(
        sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
      )");

  EXPECT_THAT(result.error,
              testing::HasSubstr("The \"shared-storage\" Permissions Policy "
                                 "denied the method on window.sharedStorage."));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Image_ContentAttributeIncluded_Set_2ndImageCached_NotSet) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  StartServerAndLoadMainURLWithSameOriginSubresourceOrSubframe(
      /*main_hostname=*/"a.test",
      /*main_path=*/
      "/shared_storage/page-with-shared-storage-writable-image.html");

  // Wait for the image onload to fire.
  EXPECT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("Image Loaded",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[2].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true));
  EXPECT_THAT(observer_->operations(), testing::ElementsAre(AppendOperation(
                                           subresource_or_subframe_origin_, "a",
                                           "b", OperationResult::kSet)));

  EXPECT_EQ(
      true,
      EvalJs(
          shell(),
          JsReplace(
              R"(
      new Promise((resolve, reject) => {
        let img = document.createElement('img');
        img.src = $1;
        img.onload = () => resolve(true);
        img.sharedStorageWritable = true;
        document.body.appendChild(img);
      })
    )",
              https_server()
                  ->GetURL("a.test",
                           "/shared_storage/shared-storage-writable-pixel.png")
                  .spec())));

  // Create an iframe that's same-origin in order to run a second worklet.
  FrameTreeNode* iframe_node =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), main_url_);

  ExecuteScriptInWorklet(iframe_node, R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url, /*expected_total_host_count=*/2u);

  // The value 'b' for the key 'a' is unchanged (nothing is appended to it).
  EXPECT_EQ(6u, console_observer.messages().size());
  EXPECT_EQ("Image Loaded",
            base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[4].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[5].message));

  // No new operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true));
  EXPECT_THAT(observer_->operations(), testing::ElementsAre(AppendOperation(
                                           subresource_or_subframe_origin_, "a",
                                           "b", OperationResult::kSet)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Image_ContentAttributeNotIncluded_NotSet) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  StartServerAndLoadMainURLWithSameOriginSubresourceOrSubframe(
      /*main_hostname=*/"a.test",
      /*main_path=*/
      "/shared_storage/page-with-non-shared-storage-writable-image.html");

  // Wait for the image onload to fire.
  EXPECT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("Image Loaded",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[2].message));

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Iframe_SameOrigin_PermissionsDefault) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/absl::nullopt,
      /*is_image=*/false);

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: set;key=a;value=b"});

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(SetOperation(subresource_or_subframe_origin_,
                                                "a", "b", absl::nullopt,
                                                OperationResult::kSet)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Iframe_SameOrigin_PermissionsNone) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/"()",
      /*is_image=*/false);

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Iframe_SameOrigin_PermissionsAll) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/"*",
      /*is_image=*/false);

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(SetOperation(subresource_or_subframe_origin_,
                                                "a", "b", absl::nullopt,
                                                OperationResult::kSet)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Iframe_SameOrigin_PermissionsSelf) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"a.test",
      /*shared_storage_permissions=*/"self",
      /*is_image=*/false);

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(SetOperation(subresource_or_subframe_origin_,
                                                "a", "b", absl::nullopt,
                                                OperationResult::kSet)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Iframe_CrossOrigin_PermissionsDefault) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/absl::nullopt,
      /*is_image=*/false);

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Shared-Storage-Write: set;key=a;value=b"});

  // Create another iframe that's same-origin to the first iframe URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(),
                   https_server()->GetURL("b.test", kTitle2Path));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(SetOperation(subresource_or_subframe_origin_,
                                                "a", "b", absl::nullopt,
                                                OperationResult::kSet)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Iframe_CrossOrigin_PermissionsNone) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/"()",
      /*is_image=*/false);

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Iframe_CrossOrigin_PermissionsAll) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/"*",
      /*is_image=*/false);

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  // Create another iframe that's same-origin to the first iframe URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(),
                   https_server()->GetURL("b.test", kTitle2Path));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(SetOperation(subresource_or_subframe_origin_,
                                                "a", "b", absl::nullopt,
                                                OperationResult::kSet)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Iframe_CrossOrigin_PermissionsSelf) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/"self",
      /*is_image=*/false);

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: set;key=a;value=b"});

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Iframe_CrossOrigin_Redirect_InititalAllowed_FinalAllowed_WriteInitial) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://c.test:{{port}}\")",
      /*is_image=*/false,
      /*redirect_hostname=*/"c.test");

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_url_.spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *"});

  // Create another iframe that's same-origin to the original iframe URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  // There won't be additional operations invoked from the redirect, just the
  // original 3.
  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(3);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true, true, true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  ClearOperation(subresource_or_subframe_origin_,
                                 OperationResult::kSuccess),
                  SetOperation(subresource_or_subframe_origin_, "hello",
                               "world", true, OperationResult::kSet),
                  AppendOperation(subresource_or_subframe_origin_, "hello",
                                  "there", OperationResult::kSet)));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Iframe_CrossOrigin_Redirect_InititalAllowed_FinalAllowed_WriteFinal) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://c.test:{{port}}\")",
      /*is_image=*/false,
      /*redirect_hostname=*/"c.test");

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_url_.spec()}),
       "Access-Control-Allow-Origin: *"});

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=will;key=set"});

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(2);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().back().first, redirect_origin_);
  EXPECT_THAT(observer_->header_results().back().second,
              testing::ElementsAre(true, true));
  EXPECT_THAT(
      observer_->operations(),
      testing::ElementsAre(
          DeleteOperation(redirect_origin_, "a", OperationResult::kSuccess),
          SetOperation(redirect_origin_, "set", "will", absl::nullopt,
                       OperationResult::kSet)));

  // Create another iframe that's same-origin to the original iframe URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());

  // Nothing was set in b.test's shared storage.
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[1].message));

  // Create another iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node3 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_url_);

  ExecuteScriptInWorklet(iframe_node3, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url, /*expected_total_host_count=*/2u);

  // The entry was set in c.test's shared storage.
  EXPECT_EQ(4u, console_observer.messages().size());
  EXPECT_EQ("will", base::UTF16ToUTF8(console_observer.messages()[2].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[3].message));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Iframe_CrossOrigin_Redirect_InititalAllowed_FinalAllowed_WriteBoth) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\" \"https://c.test:{{port}}\")",
      /*is_image=*/false,
      /*redirect_hostname=*/"c.test");

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_url_.spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=will;key=set"});

  // There will now have been a total of 5 operations (3 previous, 2 current).
  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(5);

  EXPECT_EQ(observer_->header_results().size(), 2u);
  EXPECT_EQ(observer_->header_results().back().first, redirect_origin_);
  EXPECT_THAT(observer_->header_results().back().second,
              testing::ElementsAre(true, true));
  EXPECT_THAT(
      observer_->operations(),
      testing::ElementsAre(
          ClearOperation(subresource_or_subframe_origin_,
                         OperationResult::kSuccess),
          SetOperation(subresource_or_subframe_origin_, "hello", "world", true,
                       OperationResult::kSet),
          AppendOperation(subresource_or_subframe_origin_, "hello", "there",
                          OperationResult::kSet),
          DeleteOperation(redirect_origin_, "a", OperationResult::kSuccess),
          SetOperation(redirect_origin_, "set", "will", absl::nullopt,
                       OperationResult::kSet)));

  // Create another iframe that's same-origin to the original iframe URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node2, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  // Only one entry was set in b.test's shared storage.
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[2].message));

  // Create another iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node3 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_url_);

  ExecuteScriptInWorklet(iframe_node3, R"(
      console.log(await sharedStorage.get('set'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url, /*expected_total_host_count=*/2u);

  // One entry was set in c.test's shared storage.
  EXPECT_EQ(5u, console_observer.messages().size());
  EXPECT_EQ("will", base::UTF16ToUTF8(console_observer.messages()[3].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[4].message));
}

IN_PROC_BROWSER_TEST_F(
    SharedStorageHeaderObserverBrowserTest,
    Iframe_CrossOrigin_Redirect_InititalAllowed_FinalDenied) {
  SetUpResponsesAndNavigateMainPage(
      /*main_hostname=*/"a.test",
      /*subresource_or_subframe_hostname=*/"b.test",
      /*shared_storage_permissions=*/
      "(self \"https://b.test:{{port}}\")",
      /*is_image=*/false,
      /*redirect_hostname=*/"c.test");

  CreateSharedStorageWritableIframe(shell(), subresource_or_subframe_url_);

  WaitForSubresourceOrSubframeRequestAndSendResponse(
      /*expect_writable_header=*/true,
      /*http_status=*/net::HTTP_FOUND,
      /*extra_headers=*/
      {base::StrCat({"Location: ", redirect_url_.spec()}),
       "Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: clear, "
       "set;key=\"hello\";value=\"world\";ignore_if_present, "
       "append;key=hello;value=there"});

  WaitForRedirectRequestAndSendResponse(
      /*expect_writable_header=*/false,
      /*http_status=*/net::HTTP_OK,
      /*extra_headers=*/
      {"Access-Control-Allow-Origin: *",
       "Shared-Storage-Write: delete;key=a, set;value=wont;key=set"});

  // There won't be additional operations invoked from the redirect, just the
  // original 3.
  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(3);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true, true, true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  ClearOperation(subresource_or_subframe_origin_,
                                 OperationResult::kSuccess),
                  SetOperation(subresource_or_subframe_origin_, "hello",
                               "world", true, OperationResult::kSet),
                  AppendOperation(subresource_or_subframe_origin_, "hello",
                                  "there", OperationResult::kSet)));

  // Create an iframe that's same-origin to the original iframe URL.
  FrameTreeNode* iframe_node1 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), subresource_or_subframe_url_);

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(iframe_node1, R"(
      console.log(await sharedStorage.get('hello'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(2u, console_observer.messages().size());
  EXPECT_EQ("worldthere",
            base::UTF16ToUTF8(console_observer.messages()[0].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[1].message));

  // Create another iframe that's same-origin to the redirect URL.
  FrameTreeNode* iframe_node2 =
      CreateIFrame(PrimaryFrameTreeNodeRoot(), redirect_url_);

  EvalJsResult result = EvalJs(iframe_node2, R"(
        sharedStorage.worklet.addModule('/shared_storage/simple_module.js');
      )");

  EXPECT_THAT(result.error,
              testing::HasSubstr("The \"shared-storage\" Permissions Policy "
                                 "denied the method on window.sharedStorage."));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Iframe_ContentAttributeIncluded_Set) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  StartServerAndLoadMainURLWithSameOriginSubresourceOrSubframe(
      /*main_hostname=*/"a.test",
      /*main_path=*/
      "/shared_storage/page-with-shared-storage-writable-iframe.html");

  EXPECT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("Iframe Loaded",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("b", base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("1", base::UTF16ToUTF8(console_observer.messages()[2].message));

  ASSERT_TRUE(observer_);
  observer_->WaitForOperations(1);

  EXPECT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().front().first,
            subresource_or_subframe_origin_);
  EXPECT_THAT(observer_->header_results().front().second,
              testing::ElementsAre(true));
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(SetOperation(subresource_or_subframe_origin_,
                                                "a", "b", absl::nullopt,
                                                OperationResult::kSet)));
}

IN_PROC_BROWSER_TEST_F(SharedStorageHeaderObserverBrowserTest,
                       Iframe_ContentAttributeNotIncluded_NotSet) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  StartServerAndLoadMainURLWithSameOriginSubresourceOrSubframe(
      /*main_hostname=*/"a.test",
      /*main_path=*/
      "/shared_storage/page-with-non-shared-storage-writable-iframe.html");

  EXPECT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ("Iframe Loaded",
            base::UTF16ToUTF8(console_observer.messages()[0].message));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      console.log(await sharedStorage.get('a'));
      console.log(await sharedStorage.length());
    )",
                         &out_script_url);

  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ("undefined",
            base::UTF16ToUTF8(console_observer.messages()[1].message));
  EXPECT_EQ("0", base::UTF16ToUTF8(console_observer.messages()[2].message));

  // No operations are invoked.
  ASSERT_TRUE(observer_);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
}

}  // namespace content

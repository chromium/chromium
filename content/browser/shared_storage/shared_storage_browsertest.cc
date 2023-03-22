// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "base/functional/overloaded.h"
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
#include "base/test/with_feature_override.h"
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
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_select_url_fenced_frame_config_observer.h"
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

const int kSelectURLOverallBitBudget = 12;

const int kSelectURLOriginBitBudget = 6;

const int kReportEventBitBudget = 6;

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

// With `WebContentsConsoleObserver`, we can only wait for the last message in a
// group.
base::RepeatingCallback<
    bool(const content::WebContentsConsoleObserver::Message& message)>
MakeFilter(std::vector<std::string> possible_last_messages) {
  return base::BindRepeating(
      [](std::vector<std::string> possible_last_messages,
         const content::WebContentsConsoleObserver::Message& message) {
        return base::Contains(possible_last_messages,
                              base::UTF16ToUTF8(message.message));
      },
      std::move(possible_last_messages));
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

bool IsErrorMessage(const content::WebContentsConsoleObserver::Message& msg) {
  return msg.log_level == blink::mojom::ConsoleMessageLevel::kError;
}

auto describe_param = [](const auto& info) {
  if (info.param) {
    return "ResolveSelectURLToConfig";
  } else {
    return "ResolveSelectURLToURN";
  }
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
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{blink::features::kSharedStorageAPI,
          {
              {"SharedStorageBitBudget", base::NumberToString(kBudgetAllowed)},
              {"SharedStorageStalenessThreshold",
               TimeDeltaToString(base::Days(kStalenessThresholdDays))},
          }},
         {features::kPrivacySandboxAdsAPIsOverride, {}}},
        /*disabled_features=*/{});
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
    FinishSetup();
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
                              GURL* out_module_script_url,
                              size_t expected_total_host_count = 1u,
                              bool keep_alive_after_operation = true) {
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
    EXPECT_TRUE(ExecJs(execution_target, R"(
        sharedStorage.run('test-operation', {keepAlive: keepWorklet});
      )"));

    test_worklet_host_manager()
        .GetAttachedWorkletHostForFrame(execution_target.render_frame_host())
        ->WaitForWorkletResponses();
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
  GURL SelectFrom8URLsInContext(const url::Origin& origin,
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
    EXPECT_TRUE(observed_urn_uuid.has_value());
    EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

    if (!ResolveSelectURLToConfig()) {
      EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
    }

    test_worklet_host_manager()
        .GetAttachedWorkletHostForFrame(iframe->current_frame_host())
        ->WaitForWorkletResponses();

    return observed_urn_uuid.value();
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
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  base::HistogramTester histogram_tester_;

  raw_ptr<TestSharedStorageWorkletHostManager, DanglingUntriaged>
      test_worklet_host_manager_ = nullptr;
  std::unique_ptr<TestSharedStorageObserver> observer_;
};

class SharedStorageBrowserTest : public base::test::WithFeatureOverride,
                                 public SharedStorageBrowserTestBase {
 public:
  SharedStorageBrowserTest()
      : base::test::WithFeatureOverride(
            blink::features::kFencedFramesAPIChanges) {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kFencedFrames);
  }

  bool ResolveSelectURLToConfig() override { return IsParamFeatureEnabled(); }

  ~SharedStorageBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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
                                               std::vector<uint8_t>())},
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
                                               std::vector<uint8_t>())}});
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
                                               std::vector<uint8_t>())}});
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
                                               std::vector<uint8_t>())}});
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
                                               std::vector<uint8_t>())},
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
                                               std::vector<uint8_t>())},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               std::vector<uint8_t>())}});
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
                                               std::vector<uint8_t>())}});
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
                                               std::vector<uint8_t>())}});
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
                                               std::vector<uint8_t>())}});
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
  EXPECT_EQ(metadata->origin, https_server()->GetOrigin("a.test"));
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
  EXPECT_EQ(metadata->origin, https_server()->GetOrigin("a.test"));
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
            "test-url-selection-operation", std::vector<uint8_t>(),
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
  EXPECT_EQ(metadata->origin, https_server()->GetOrigin("b.test"));
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
            "test-url-selection-operation", std::vector<uint8_t>(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {}}}))},
       {AccessType::kDocumentSelectURL, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSelectURL(
            "test-url-selection-operation", std::vector<uint8_t>(),
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
            "test-url-selection-operation", std::vector<uint8_t>(),
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
            "test-url-selection-operation", std::vector<uint8_t>(),
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
                                               std::vector<uint8_t>())}});
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
                                               std::vector<uint8_t>())},
       {AccessType::kDocumentSelectURL, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForSelectURL(
            "test-url-selection-operation", std::vector<uint8_t>(),
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
                                               std::vector<uint8_t>())}});
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
            "test-url-selection-operation", std::vector<uint8_t>(),
            std::vector<SharedStorageUrlSpecWithMetadata>(
                {{https_server()->GetURL("a.test",
                                         "/fenced_frames/title0.html"),
                  {}}}))},
       {AccessType::kDocumentRun, MainFrameId(), origin_str,
        SharedStorageEventParams::CreateForRun("test-operation",
                                               std::vector<uint8_t>())}});
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
            "test-url-selection-operation", std::vector<uint8_t>(),
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
            "test-url-selection-operation", std::vector<uint8_t>(),
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
  EXPECT_EQ(metadata->origin, https_server()->GetOrigin("a.test"));
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
            "test-url-selection-operation", std::vector<uint8_t>(),
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
                                               std::vector<uint8_t>())},
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
                                               std::vector<uint8_t>())},
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
                                               std::vector<uint8_t>())},
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
                                               std::vector<uint8_t>())},
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
                                               std::vector<uint8_t>())},
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

INSTANTIATE_TEST_SUITE_P(All,
                         SharedStorageBrowserTest,
                         testing::Bool(),
                         describe_param);

// TODO(yaoxia): when the majority of the blink-style worklet migration is done,
// we should remove this test suite and just parameterize the existing tests.
class BlinkStyleSharedStorageBrowserTest : public SharedStorageBrowserTestBase {
 public:
  BlinkStyleSharedStorageBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{blink::features::kSharedStorageAPI,
          {{"SharedStorageWorkletImplementationType", "blink_style"}}}},
        /*disabled_features=*/{});
  }

  ~BlinkStyleSharedStorageBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BlinkStyleSharedStorageBrowserTest, AddModule_Success) {
  GURL main_frame_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  base::StringPairs run_function_body_replacement;
  run_function_body_replacement.emplace_back("{{SCRIPT_BODY}}", "let a = 1;");

  GURL module_script_url = https_server()->GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/customizable_script.js",
                    run_function_body_replacement));

  EXPECT_TRUE(ExecJs(
      shell()->web_contents(),
      JsReplace("sharedStorage.worklet.addModule($1)", module_script_url)));
}

IN_PROC_BROWSER_TEST_F(BlinkStyleSharedStorageBrowserTest, AddModule_Failure) {
  GURL main_frame_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  base::StringPairs run_function_body_replacement;
  run_function_body_replacement.emplace_back("{{SCRIPT_BODY}}", "a;");

  GURL module_script_url = https_server()->GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/shared_storage/customizable_script.js",
                    run_function_body_replacement));

  EvalJsResult result = EvalJs(
      shell()->web_contents(),
      JsReplace("sharedStorage.worklet.addModule($1)", module_script_url));

  EXPECT_THAT(result.error,
              testing::HasSubstr("ReferenceError: a is not defined"));
}

class SharedStorageAllowURNsInIframesBrowserTest
    : public base::test::WithFeatureOverride,
      public SharedStorageBrowserTestBase {
 public:
  SharedStorageAllowURNsInIframesBrowserTest()
      : base::test::WithFeatureOverride(
            blink::features::kFencedFramesAPIChanges) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {blink::features::kFencedFrames, blink::features::kAllowURNsInIframes},
        /*disabled_features=*/{});
  }

  bool ResolveSelectURLToConfig() override { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(SharedStorageAllowURNsInIframesBrowserTest,
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
  SharedStorageFencedFrameInteractionBrowserTestBase() {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kFencedFrames);
  }

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

    EXPECT_TRUE(result.error.empty());
    if (absl::holds_alternative<GURL>(target)) {
      EXPECT_EQ(result, absl::get<GURL>(target).spec());
    }

    return fenced_frame_root_node;
  }

  FrameTreeNode* CreateFencedFrame(const FencedFrameNavigationTarget& target) {
    return CreateFencedFrame(PrimaryFrameTreeNodeRoot(), target);
  }

  EvalJsResult NavigateFencedFrame(FrameTreeNode* root,
                                   const FencedFrameNavigationTarget& target) {
    return EvalJs(root, absl::visit(base::Overloaded{
                                        [](const GURL& url) {
                                          return JsReplace("f.src = $1;", url);
                                        },
                                        [](const std::string& config) {
                                          return JsReplace(
                                              "f.config = window[$1]", config);
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

    EXPECT_TRUE(result.error.empty());
    if (absl::holds_alternative<GURL>(target)) {
      EXPECT_EQ(result, absl::get<GURL>(target).spec());
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class SharedStorageFencedFrameInteractionBrowserTest
    : public base::test::WithFeatureOverride,
      public SharedStorageFencedFrameInteractionBrowserTestBase {
 public:
  SharedStorageFencedFrameInteractionBrowserTest()
      : base::test::WithFeatureOverride(
            blink::features::kFencedFramesAPIChanges) {}

  bool ResolveSelectURLToConfig() override { return IsParamFeatureEnabled(); }

 protected:
  test::FencedFrameTestHelper fenced_frame_test_helper_;
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
  EXPECT_EQ(metadata->origin, https_server()->GetOrigin("a.test"));
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
  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(navigation_result, observed_urn_uuid.value());
  }

  observer.Wait();

  EXPECT_EQ(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
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

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
  }

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
  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(navigation_result, observed_urn_uuid.value());
  }

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
  EXPECT_EQ(metadata->origin, https_server()->GetOrigin("a.test"));
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
IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURL_URNLifetime) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  GURL urn_uuid = SelectFrom8URLsInContext(url::Origin::Create(main_url));
  EXPECT_TRUE(blink::IsValidUrnUuidURL(GURL(urn_uuid)));

  FrameTreeNode* iframe_node = PrimaryFrameTreeNodeRoot()->child_at(0);

  // Navigate the iframe to about:blank.
  TestFrameNavigationObserver observer(iframe_node);
  EXPECT_TRUE(ExecJs(iframe_node, JsReplace("window.location.href=$1",
                                            GURL(url::kAboutBlankURL))));
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
IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
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

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
  }

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
  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(navigation_result, observed_urn_uuid.value());
  }

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
  EXPECT_EQ(metadata->origin, https_server()->GetOrigin("a.test"));
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

IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
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
  EXPECT_EQ(fenced_frame_config->urn_uuid_, observed_urn_uuid->spec());

  EXPECT_EQ(
      "Promise resolved to a number outside the length of the input urls.",
      base::UTF16ToUTF8(console_observer.messages().back().message));

  SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(observed_urn_uuid.value());
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->origin, https_server()->GetOrigin("a.test"));
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
  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(navigation_result, observed_urn_uuid.value());
  }

  observer.Wait();

  EXPECT_EQ(
      https_server()->GetURL("a.test", "/fenced_frames/title0.html"),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
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

  SharedStorageBudgetMetadata* metadata =
      GetSharedStorageBudgetMetadata(observed_urn_uuid.value());
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->origin, https_server()->GetOrigin("a.test"));
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
  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(navigation_result, observed_urn_uuid.value());
  }

  observer.Wait();

  EXPECT_EQ(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());

  WaitForHistograms({kTimingSelectUrlExecutedInWorkletHistogram});
  histogram_tester_.ExpectTotalCount(kTimingSelectUrlExecutedInWorkletHistogram,
                                     1);
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

IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
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

IN_PROC_BROWSER_TEST_P(
    SharedStorageFencedFrameInteractionBrowserTest,
    FencedFrameNavigateFromParentToRegularURLAndThenOpenPopup_NoBudgetWithdrawal) {
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

  TestFrameNavigationObserver observer(fenced_frame_root_node);
  std::string navigate_fenced_frame_script = JsReplace(
      "var f = document.getElementsByTagName('fencedframe')[0]; f.src = $1;",
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
IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
                       DISABLED_NestedFencedFrameNavigateTop_BudgetWithdrawal) {
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

IN_PROC_BROWSER_TEST_P(
    SharedStorageFencedFrameInteractionBrowserTest,
    NestedFencedFrameNavigateTop_BudgetWithdrawalFromTwoMetadata) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin_1 =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  GURL urn_uuid_1 = SelectFrom8URLsInContext(shared_storage_origin_1);
  FrameTreeNode* fenced_frame_root_node_1 = CreateFencedFrame(urn_uuid_1);

  url::Origin shared_storage_origin_2 =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  GURL urn_uuid_2 = SelectFrom8URLsInContext(shared_storage_origin_2,
                                             fenced_frame_root_node_1);

  FrameTreeNode* fenced_frame_root_node_2 =
      CreateFencedFrame(fenced_frame_root_node_1, urn_uuid_2);

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

IN_PROC_BROWSER_TEST_P(
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
              net::OK,
              blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds))
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
          "selectURL() is not allowed in a fenced frame that did not originate "
          "from shared storage."));
}

IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
                       SelectURLNotAllowedInNestedFencedFrame) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin_1 =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  GURL urn_uuid_1 = SelectFrom8URLsInContext(shared_storage_origin_1);
  FrameTreeNode* fenced_frame_root_node_1 = CreateFencedFrame(urn_uuid_1);

  url::Origin shared_storage_origin_2 =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  GURL urn_uuid_2 = SelectFrom8URLsInContext(shared_storage_origin_2,
                                             fenced_frame_root_node_1);

  FrameTreeNode* fenced_frame_root_node_2 =
      CreateFencedFrame(fenced_frame_root_node_1, urn_uuid_2);

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

IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
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

IN_PROC_BROWSER_TEST_P(
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

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result_1.ExtractString(), observed_urn_uuid_1->spec());
  }

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

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result_2.ExtractString(), observed_urn_uuid_2->spec());
  }

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

IN_PROC_BROWSER_TEST_P(
    SharedStorageFencedFrameInteractionBrowserTest,
    TwoFencedFrames_SameURNs_EachPopupOnce_BudgetWithdrawalOnce) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin = url::Origin::Create(main_url);

  GURL urn_uuid = SelectFrom8URLsInContext(shared_storage_origin);

  FrameTreeNode* fenced_frame_root_node_1 = CreateFencedFrame(urn_uuid);
  FrameTreeNode* fenced_frame_root_node_2 = CreateFencedFrame(urn_uuid);

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

IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
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

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result_1.ExtractString(), observed_urn_uuid_1->spec());
  }

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

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(result_2.ExtractString(), observed_urn_uuid_2->spec());
  }

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
IN_PROC_BROWSER_TEST_P(SharedStorageFencedFrameInteractionBrowserTest,
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

INSTANTIATE_TEST_SUITE_P(All,
                         SharedStorageFencedFrameInteractionBrowserTest,
                         testing::Bool(),
                         describe_param);

class SharedStorageSelectURLNotAllowedInFencedFrameBrowserTest
    : public SharedStorageFencedFrameInteractionBrowserTest {
 public:
  SharedStorageSelectURLNotAllowedInFencedFrameBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{blink::features::kSharedStorageAPI,
          {{"SharedStorageBitBudget", base::NumberToString(kBudgetAllowed)},
           {"SharedStorageMaxAllowedFencedFrameDepthForSelectURL", "0"}}},
         {features::kPrivacySandboxAdsAPIsOverride, {}}},
        /*disabled_features=*/{});

    fenced_frame_api_change_feature_.InitWithFeatureState(
        blink::features::kFencedFramesAPIChanges, ResolveSelectURLToConfig());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedFeatureList fenced_frame_api_change_feature_;
};

IN_PROC_BROWSER_TEST_P(SharedStorageSelectURLNotAllowedInFencedFrameBrowserTest,
                       SelectURLNotAllowedInFencedFrame) {
  GURL main_frame_url = https_server()->GetURL("a.test", kSimplePagePath);

  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  url::Origin shared_storage_origin =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));
  GURL urn_uuid = SelectFrom8URLsInContext(shared_storage_origin);

  FrameTreeNode* fenced_frame_node = CreateFencedFrame(urn_uuid);

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

INSTANTIATE_TEST_SUITE_P(
    All,
    SharedStorageSelectURLNotAllowedInFencedFrameBrowserTest,
    testing::Bool(),
    describe_param);

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

INSTANTIATE_TEST_SUITE_P(All,
                         SharedStorageReportEventBrowserTest,
                         testing::Bool(),
                         describe_param);

class SharedStoragePrivateAggregationDisabledBrowserTest
    : public SharedStorageBrowserTestBase {
 public:
  SharedStoragePrivateAggregationDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kPrivateAggregationApi);
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
    : public SharedStorageBrowserTestBase {
 public:
  SharedStoragePrivateAggregationDisabledForSharedStorageOnlyBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kPrivateAggregationApi,
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
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kPrivateAggregationApi);
  }

  void SetUpOnMainThread() override {
    SharedStorageBrowserTestBase::SetUpOnMainThread();

    browser_client_ =
        std::make_unique<MockPrivateAggregationShellContentBrowserClient>();

    a_test_origin_ = https_server()->GetOrigin("a.test");

    auto* storage_partition_impl =
        static_cast<StoragePartitionImpl*>(GetStoragePartition());

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

  MockPrivateAggregationShellContentBrowserClient& browser_client() {
    return *browser_client_;
  }

 protected:
  url::Origin a_test_origin_;

 private:
  raw_ptr<PrivateAggregationHost, DanglingUntriaged> private_aggregation_host_;

  base::test::ScopedFeatureList scoped_feature_list_;

  base::MockRepeatingCallback<void(AggregatableReportRequest,
                                   PrivateAggregationBudgetKey)>
      mock_callback_;

  std::unique_ptr<MockPrivateAggregationShellContentBrowserClient>
      browser_client_;
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
  ON_CALL(browser_client(), IsSharedStorageAllowed)
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
  ON_CALL(browser_client(), IsSharedStorageAllowed)
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
        ASSERT_EQ(request.payload_contents().contributions.size(), 2u);
        EXPECT_EQ(request.payload_contents().contributions[0].bucket, 1);
        EXPECT_EQ(request.payload_contents().contributions[0].value, 2);
        EXPECT_EQ(request.payload_contents().contributions[1].bucket, 3);
        EXPECT_EQ(request.payload_contents().contributions[1].value, 4);
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
  ON_CALL(browser_client(), IsSharedStorageAllowed)
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
      privateAggregation.sendHistogramReport({bucket: 1n, value: 2});
    )",
                         &out_script_url);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ(
      "TypeError: The \"private-aggregation\" Permissions Policy denied the "
      "method on privateAggregation",
      base::UTF16ToUTF8(console_observer.messages()[0].message));
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
             {"SharedStorageSelectURLBitBudgetPerOriginPerPageLoad",
              base::NumberToString(kSelectURLOriginBitBudget)}}}},
          /*disabled_features=*/{});
    } else {
      select_url_limit_feature_list_.InitAndDisableFeature(
          blink::features::kSharedStorageSelectURLLimit);
    }

    fenced_frame_api_change_feature_.InitWithFeatureState(
        blink::features::kFencedFramesAPIChanges, ResolveSelectURLToConfig());
    fenced_frame_feature_.InitAndEnableFeature(blink::features::kFencedFrames);
  }

  bool LimitSelectURLCalls() const { return std::get<0>(GetParam()); }

  bool ResolveSelectURLToConfig() override { return std::get<1>(GetParam()); }

  // Precondition: `addModule('shared_storage/simple_module.js')` has been
  // called in the main frame.
  void RunSuccessfulSelectURLInMainFrame(
      std::string host_str,
      int num_urls,
      WebContentsConsoleObserver* console_observer) {
    std::pair<GURL, double> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(shell(), host_str,
                                                         num_urls);

    GURL expected_mapped_url = https_server()->GetURL(
        host_str, base::StrCat({"/fenced_frames/title",
                                base::NumberToString(num_urls - 1), ".html"}));
    EXPECT_EQ(result_pair.first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair.second, std::log2(num_urls));

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

    std::pair<GURL, double> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(iframe_node, host_str,
                                                         num_urls);

    GURL expected_mapped_url = https_server()->GetURL(
        host_str, base::StrCat({"/fenced_frames/title",
                                base::NumberToString(num_urls - 1), ".html"}));
    EXPECT_EQ(result_pair.first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair.second, std::log2(num_urls));

    EXPECT_EQ("Finish executing 'test-url-selection-operation'",
              base::UTF16ToUTF8(console_observer->messages().back().message));
  }

  // Precondition: `addModule('shared_storage/simple_module.js')` has been
  // called in the `execution_target`.
  std::pair<GURL, double> RunSelectURLExtractingMappedURLAndBudgetToCharge(
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
    EXPECT_TRUE(observed_urn_uuid.has_value());
    EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

    if (!ResolveSelectURLToConfig()) {
      EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
    }

    test_worklet_host_manager()
        .GetAttachedWorkletHostForFrame(execution_target.render_frame_host())
        ->WaitForWorkletResponses();

    const absl::optional<FencedFrameConfig>& config =
        config_observer.GetConfig();
    EXPECT_TRUE(config.has_value());
    EXPECT_TRUE(config->mapped_url_.has_value());

    SharedStorageBudgetMetadata* metadata =
        GetSharedStorageBudgetMetadata(observed_urn_uuid.value());
    EXPECT_TRUE(metadata);
    EXPECT_EQ(metadata->origin, https_server()->GetOrigin(host_str));

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
  base::test::ScopedFeatureList fenced_frame_feature_;
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
                       SelectURL_MainFrame_SameEntropy_OriginLimitReached) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  // This test relies on the assumption that `kSelectURLOverallBitBudget` is set
  // to be greater than or equal to `kSelectURLOriginBitBudget`.
  EXPECT_GE(kSelectURLOverallBitBudget, kSelectURLOriginBitBudget);

  // Here each call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy.
  int call_limit = kSelectURLOriginBitBudget / 3;

  for (int i = 0; i < call_limit; i++) {
    RunSuccessfulSelectURLInMainFrame("a.test", /*num_urls=*/8,
                                      &console_observer);
  }

  if (LimitSelectURLCalls()) {
    // The limit for `selectURL()` has now been reached for "a.test". Make one
    // more call, which will return the default URL due to insufficient origin
    // pageload budget.
    std::pair<GURL, double> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(shell(), "a.test",
                                                         /*num_urls=*/8);

    GURL expected_mapped_url =
        https_server()->GetURL("a.test", "/fenced_frames/title0.html");
    EXPECT_EQ(result_pair.first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair.second, 0.0);

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

IN_PROC_BROWSER_TEST_P(
    SharedStorageSelectURLLimitBrowserTest,
    SelectURL_MainFrame_DifferentEntropy_OriginLimitReached) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  // This test relies on the assumptions that `kSelectURLOverallBitBudget` is
  // set to be greater than or equal to `kSelectURLOriginBitBudget` and that the
  // latter is at least 3.
  EXPECT_GE(kSelectURLOverallBitBudget, kSelectURLOriginBitBudget);
  EXPECT_GE(kSelectURLOriginBitBudget, 3);

  // Here the first call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy, and the subsequent calls will each have 4
  // input URLs, and hence 2 = log2(4) bits of entropy.
  int input4_call_limit = (kSelectURLOriginBitBudget - 3) / 2;

  RunSuccessfulSelectURLInMainFrame("a.test", /*num_urls=*/8,
                                    &console_observer);

  for (int i = 0; i < input4_call_limit; i++) {
    RunSuccessfulSelectURLInMainFrame("a.test", /*num_urls=*/4,
                                      &console_observer);
  }

  if (LimitSelectURLCalls()) {
    // The limit for `selectURL()` has now been reached for "a.test". Make one
    // more call, which will return the default URL due to insufficient origin
    // pageload budget.
    std::pair<GURL, double> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(shell(), "a.test",
                                                         /*num_urls=*/4);

    GURL expected_mapped_url =
        https_server()->GetURL("a.test", "/fenced_frames/title0.html");
    EXPECT_EQ(result_pair.first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair.second, 0.0);

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
    SelectURL_IframesSharingCommonOrigin_SameEntropy_OriginLimitReached) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // This test relies on the assumption that `kSelectURLOverallBitBudget` is set
  // to be greater than or equal to `kSelectURLOriginBitBudget`.
  EXPECT_GE(kSelectURLOverallBitBudget, kSelectURLOriginBitBudget);

  // Here each call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy.
  int call_limit = kSelectURLOriginBitBudget / 3;

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
    // more call, which will return the default URL due to insufficient origin
    // pageload budget.
    std::pair<GURL, double> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(iframe_node, "b.test",
                                                         /*num_urls=*/8);

    GURL expected_mapped_url =
        https_server()->GetURL("b.test", "/fenced_frames/title0.html");
    EXPECT_EQ(result_pair.first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair.second, 0.0);

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
    SelectURL_IframesSharingCommonOrigin_DifferentEntropy_OriginLimitReached) {
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
  // set to be greater than or equal to `kSelectURLOriginBitBudget` and that the
  // latter is at least 3.
  EXPECT_GE(kSelectURLOverallBitBudget, kSelectURLOriginBitBudget);
  EXPECT_GE(kSelectURLOriginBitBudget, 3);

  // Here the first call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy, and the subsequent calls will each have 4
  // input URLs, and hence 2 = log2(4) bits of entropy.
  int input4_call_limit = (kSelectURLOriginBitBudget - 3) / 2;

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
    // more call, which will return the default URL due to insufficient origin
    // pageload budget.
    std::pair<GURL, double> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(last_iframe_node,
                                                         "b.test",
                                                         /*num_urls=*/4);

    GURL expected_mapped_url =
        https_server()->GetURL("b.test", "/fenced_frames/title0.html");
    EXPECT_EQ(result_pair.first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair.second, 0.0);

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
    SelectURL_IframesDifferentOrigin_SameEntropy_OverallLimitNotReached) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // This test relies on the assumption that `kSelectURLOverallBitBudget` is set
  // to be strictly greater than `kSelectURLOriginBitBudget`, enough for at
  // least one 8-URL call to `selectURL()` beyond the per-origin limit.
  EXPECT_GE(kSelectURLOverallBitBudget, kSelectURLOriginBitBudget + 3);

  // Here each call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy.
  int per_origin_call_limit = kSelectURLOriginBitBudget / 3;

  GURL iframe_url1 = https_server()->GetURL("b.test", kSimplePagePath);

  for (int i = 0; i < per_origin_call_limit; i++) {
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
    // more call, which will return the default URL due to insufficient origin
    // pageload budget.
    std::pair<GURL, double> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(
            penultimate_iframe_node, "b.test",
            /*num_urls=*/4);

    GURL expected_mapped_url =
        https_server()->GetURL("b.test", "/fenced_frames/title0.html");
    EXPECT_EQ(result_pair.first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair.second, 0.0);

    EXPECT_EQ("Insufficient budget for selectURL().",
              base::UTF16ToUTF8(console_observer.messages().back().message));
  } else {
    // The `selectURL()` limit is disabled. The call will run successfully.
    RunSuccessfulSelectURLInIframe(penultimate_iframe_node,
                                   /*num_urls=*/4, &console_observer);
  }

  // Create a new iframe with a different origin.
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
                                     per_origin_call_limit + 2);
}

IN_PROC_BROWSER_TEST_P(
    SharedStorageSelectURLLimitBrowserTest,
    SelectURL_IframesDifferentOrigin_DifferentEntropy_OverallLimitReached) {
  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  // This test relies on the assumptions that `kSelectURLOverallBitBudget` is
  // set to be strictly greater than `kSelectURLOriginBitBudget` and that the
  // latter is at least 3.
  EXPECT_GT(kSelectURLOverallBitBudget, kSelectURLOriginBitBudget);
  EXPECT_GE(kSelectURLOriginBitBudget, 3);

  int num_origin_limit = kSelectURLOverallBitBudget / kSelectURLOriginBitBudget;

  // We will run out of chars if we have too many origins.
  EXPECT_LT(num_origin_limit, 25);

  // For each origin, the first call to `selectURL()` will have 8 input URLs,
  // and hence 3 = log2(8) bits of entropy, whereas the subsequent calls for
  // that origin will have 2 input URLs, and hence 1 = log2(2) bit of entropy.
  int per_origin_input2_call_limit = kSelectURLOriginBitBudget - 3;

  for (int i = 0; i < num_origin_limit; i++) {
    std::string iframe_host = base::StrCat({std::string(1, 'b' + i), ".test"});
    GURL iframe_url = https_server()->GetURL(iframe_host, kSimplePagePath);

    // Create a new iframe.
    FrameTreeNode* first_loop_iframe_node =
        CreateIFrame(PrimaryFrameTreeNodeRoot(), iframe_url);

    RunSuccessfulSelectURLInIframe(first_loop_iframe_node,
                                   /*num_urls=*/8, &console_observer);

    for (int j = 0; j < per_origin_input2_call_limit; j++) {
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
      // insufficient origin pageload budget.
      std::pair<GURL, double> result_pair =
          RunSelectURLExtractingMappedURLAndBudgetToCharge(
              last_loop_iframe_node, iframe_host,
              /*num_urls=*/2);

      GURL expected_mapped_url =
          https_server()->GetURL(iframe_host, "/fenced_frames/title0.html");
      EXPECT_EQ(result_pair.first, expected_mapped_url);
      EXPECT_DOUBLE_EQ(result_pair.second, 0.0);

      EXPECT_EQ("Insufficient budget for selectURL().",
                base::UTF16ToUTF8(console_observer.messages().back().message));

    } else {
      // The `selectURL()` limit is disabled. The call will run successfully.
      RunSuccessfulSelectURLInIframe(last_loop_iframe_node,
                                     /*num_urls=*/2, &console_observer);
    }
  }

  std::string iframe_host =
      base::StrCat({std::string(1, 'b' + num_origin_limit), ".test"});
  GURL iframe_url = https_server()->GetURL(iframe_host, kSimplePagePath);

  int overall_budget_remaining =
      kSelectURLOverallBitBudget % kSelectURLOriginBitBudget;

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
    std::pair<GURL, double> result_pair =
        RunSelectURLExtractingMappedURLAndBudgetToCharge(final_iframe_node,
                                                         iframe_host,
                                                         /*num_urls=*/2);

    GURL expected_mapped_url =
        https_server()->GetURL(iframe_host, "/fenced_frames/title0.html");
    EXPECT_EQ(result_pair.first, expected_mapped_url);
    EXPECT_DOUBLE_EQ(result_pair.second, 0.0);

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
      num_origin_limit * (2 + per_origin_input2_call_limit) +
          overall_budget_remaining + 1);
}

class SharedStorageReportEventLimitBrowserTest
    : public SharedStorageFencedFrameInteractionBrowserTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  SharedStorageReportEventLimitBrowserTest() {
    if (LimitSharedStorageReportEventCalls()) {
      report_event_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{blink::features::kSharedStorageReportEventLimit,
            {{"SharedStorageReportEventBitBudgetPerPageLoad",
              base::NumberToString(kReportEventBitBudget)}}}},
          /*disabled_features=*/{});
    } else {
      report_event_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/{},
          /*disabled_features=*/
          {blink::features::kSharedStorageReportEventLimit});
    }

    fenced_frame_feature_list_.InitWithFeatureState(
        blink::features::kFencedFramesAPIChanges, ResolveSelectURLToConfig());
  }

  // Defer the server to start after `ControllableHttpResponse` is
  // constructed.
  void FinishSetup() override {
    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  }

  bool LimitSharedStorageReportEventCalls() const {
    return std::get<0>(GetParam());
  }

  bool ResolveSelectURLToConfig() override { return std::get<1>(GetParam()); }

  // Precondition: `addModule('shared_storage/simple_module.js')` and
  // `selectURL()` have been called in the main frame.
  void RunSuccessfulReportEvents(
      FrameTreeNode* fenced_frame_root_node,
      net::test_server::ControllableHttpResponse* response1,
      net::test_server::ControllableHttpResponse* response2) {
    std::string click_event_data = "this is a click";
    EXPECT_TRUE(
        ExecJs(fenced_frame_root_node,
               JsReplace("window.fence.reportEvent({"
                         "  eventType: 'click',"
                         "  eventData: $1,"
                         "  destination: ['shared-storage-select-url']});",
                         click_event_data)));

    response1->WaitForRequest();
    EXPECT_EQ(response1->http_request()->content, click_event_data);

    std::string mouse_event_data = "this is a mouse interaction";
    EXPECT_TRUE(
        ExecJs(fenced_frame_root_node,
               JsReplace("window.fence.reportEvent({"
                         "  eventType: 'mouse interaction',"
                         "  eventData: $1,"
                         "  destination: ['shared-storage-select-url']});",
                         mouse_event_data)));

    response2->WaitForRequest();
    EXPECT_EQ(response2->http_request()->content, mouse_event_data);
  }

 private:
  base::test::ScopedFeatureList report_event_feature_list_;
  base::test::ScopedFeatureList fenced_frame_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SharedStorageReportEventLimitBrowserTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const auto& info) {
      return base::StrCat(
          {"ReportEventLimit", std::get<0>(info.param) ? "Enabled" : "Disabled",
           "_ResolveSelectURLTo", std::get<1>(info.param) ? "Config" : "URN"});
    });

IN_PROC_BROWSER_TEST_P(SharedStorageReportEventLimitBrowserTest,
                       ReportEvent_SameEntropyCalls_LimitReached) {
  // Here each call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy.
  size_t call_limit = kReportEventBitBudget / 3;

  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      responses;
  for (size_t i = 0; i <= call_limit; ++i) {
    responses.emplace_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            https_server(), "/fenced_frames/report1.html"));
    responses.emplace_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            https_server(), "/fenced_frames/report2.html"));
  }
  ASSERT_TRUE(https_server()->Start());

  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetFilter(
      MakeFilter({"The call to fence.reportEvent was blocked due to "
                  "insufficient budget."}));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_TRUE(ExecJs(shell(), kGenerateURLsListScript));
  EXPECT_TRUE(ExecJs(shell(), "window.urls = generateUrls(8);"));
  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  // There are `call_limit + 1` "worklet operations": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(call_limit + 1);

  std::vector<GURL> urns;
  for (size_t i = 0; i <= call_limit; ++i) {
    TestSelectURLFencedFrameConfigObserver config_observer(
        GetStoragePartition());
    EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          urls,
          {
            data: {'mockResult': 1},
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
    const absl::optional<GURL>& observed_urn_uuid =
        config_observer.GetUrnUuid();
    EXPECT_TRUE(observed_urn_uuid.has_value());
    EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid.value()));

    if (!ResolveSelectURLToConfig()) {
      EXPECT_EQ(result.ExtractString(), observed_urn_uuid->spec());
    }

    urns.push_back(observed_urn_uuid.value());
  }

  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  for (size_t i = 0; i < call_limit; ++i) {
    FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(urns[i]);

    RunSuccessfulReportEvents(fenced_frame_root_node, responses[2 * i].get(),
                              responses[2 * i + 1].get());
  }

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(urns[call_limit]);

  if (LimitSharedStorageReportEventCalls()) {
    // The limit for `reportEvent()` has now been reached for this page.
    // Make one more call, which will be blocked.
    std::string click_event_data = "this is a click";
    EXPECT_TRUE(
        ExecJs(fenced_frame_root_node,
               JsReplace("window.fence.reportEvent({"
                         "  eventType: 'click',"
                         "  eventData: $1,"
                         "  destination: ['shared-storage-select-url']});",
                         click_event_data)));

    EXPECT_TRUE(console_observer.Wait());
    ASSERT_LE(1u, console_observer.messages().size());
    EXPECT_EQ(
        "The call to fence.reportEvent was blocked due to insufficient "
        "budget.",
        base::UTF16ToUTF8(console_observer.messages().back().message));
  } else {
    // The `reportEvent()` limit is disabled. The calls will run
    // successfully.
    RunSuccessfulReportEvents(fenced_frame_root_node,
                              responses[2 * call_limit].get(),
                              responses[2 * call_limit + 1].get());
  }
}

IN_PROC_BROWSER_TEST_P(SharedStorageReportEventLimitBrowserTest,
                       ReportEvent_DifferentEntropyCalls_LimitReached) {
  // This test relies on the assumption that `kReportEventBitBudget` is at
  // least 3.
  EXPECT_GE(kReportEventBitBudget, 3);

  // Here the first call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy, and the subsequent calls will each have 4
  // input URLs, and hence 2 = log2(4) bits of entropy.
  size_t input4_call_limit = (kReportEventBitBudget - 3) / 2;

  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      responses;
  for (size_t i = 0; i < input4_call_limit + 2; ++i) {
    responses.emplace_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            https_server(), "/fenced_frames/report1.html"));
    responses.emplace_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            https_server(), "/fenced_frames/report2.html"));
  }
  ASSERT_TRUE(https_server()->Start());

  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetFilter(
      MakeFilter({"The call to fence.reportEvent was blocked due to "
                  "insufficient budget."}));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  std::vector<GURL> urns;
  EXPECT_TRUE(ExecJs(shell(), kGenerateURLsListScript));
  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver select_from_8urls_config_observer(
      GetStoragePartition());
  EvalJsResult select_from_8urls_result = EvalJs(shell(), R"(
      (async function() {
        const urls_8 = generateUrls(8);
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          urls_8,
          {
            data: {'mockResult': 1},
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

  EXPECT_TRUE(select_from_8urls_result.error.empty());
  const absl::optional<GURL>& observed_urn_uuid_from_8urls =
      select_from_8urls_config_observer.GetUrnUuid();
  EXPECT_TRUE(observed_urn_uuid_from_8urls.has_value());
  EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid_from_8urls.value()));

  if (!ResolveSelectURLToConfig()) {
    EXPECT_EQ(select_from_8urls_result.ExtractString(),
              observed_urn_uuid_from_8urls->spec());
  }

  urns.push_back(observed_urn_uuid_from_8urls.value());
  EXPECT_TRUE(ExecJs(shell(), "window.urls_4 = generateUrls(4);"));

  // There are `input4_call_limit + 2` "worklet operations": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(input4_call_limit + 2);

  for (size_t i = 0; i <= input4_call_limit; ++i) {
    TestSelectURLFencedFrameConfigObserver select_from_4urls_config_observer(
        GetStoragePartition());
    EvalJsResult select_from_4urls_result = EvalJs(shell(), R"(
      (async function() {
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          urls_4,
          {
            data: {'mockResult': 1},
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

    EXPECT_TRUE(select_from_4urls_result.error.empty());
    const absl::optional<GURL>& observed_urn_uuid_from_4urls =
        select_from_4urls_config_observer.GetUrnUuid();
    EXPECT_TRUE(observed_urn_uuid_from_4urls.has_value());
    EXPECT_TRUE(blink::IsValidUrnUuidURL(observed_urn_uuid_from_4urls.value()));

    if (!ResolveSelectURLToConfig()) {
      EXPECT_EQ(select_from_4urls_result.ExtractString(),
                observed_urn_uuid_from_4urls->spec());
    }

    urns.push_back(observed_urn_uuid_from_4urls.value());
  }

  // There are `input4_call_limit + 2` "worklet operations": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->WaitForWorkletResponses();

  // The first pair of `reportEvent()` calls will deduct 3 bits from the
  // budget.
  FrameTreeNode* fenced_frame_root_node_0 = CreateFencedFrame(urns[0]);

  RunSuccessfulReportEvents(fenced_frame_root_node_0, responses[0].get(),
                            responses[1].get());

  for (size_t i = 1; i <= input4_call_limit; ++i) {
    // Subsequent pairs of calls to `reportEvent()` will deduct 2 bits from
    // the budget.
    FrameTreeNode* fenced_frame_root_node_1 = CreateFencedFrame(urns[i]);

    RunSuccessfulReportEvents(fenced_frame_root_node_1, responses[2 * i].get(),
                              responses[2 * i + 1].get());
  }

  FrameTreeNode* fenced_frame_root_node_2 =
      CreateFencedFrame(urns[input4_call_limit + 1]);

  size_t current_response_index = 2 * (input4_call_limit + 1);

  if (LimitSharedStorageReportEventCalls()) {
    // The limit for `reportEvent()` has now been reached for this page.
    // Make one more call, which will be blocked.
    std::string click_event_data = "this is a click";
    EXPECT_TRUE(
        ExecJs(fenced_frame_root_node_2,
               JsReplace("window.fence.reportEvent({"
                         "  eventType: 'click',"
                         "  eventData: $1,"
                         "  destination: ['shared-storage-select-url']});",
                         click_event_data)));

    EXPECT_TRUE(console_observer.Wait());
    ASSERT_LE(1u, console_observer.messages().size());
    EXPECT_EQ(
        "The call to fence.reportEvent was blocked due to insufficient "
        "budget.",
        base::UTF16ToUTF8(console_observer.messages().back().message));

    // Running the first pair of calls again will not cause any errors.
    RunSuccessfulReportEvents(fenced_frame_root_node_0,
                              responses[current_response_index].get(),
                              responses[current_response_index + 1].get());
  } else {
    // The `reportEvent()` limit is disabled. The calls will run
    // successfully.
    RunSuccessfulReportEvents(fenced_frame_root_node_2,
                              responses[current_response_index].get(),
                              responses[current_response_index + 1].get());
  }
}

IN_PROC_BROWSER_TEST_P(SharedStorageReportEventLimitBrowserTest,
                       ReportEventThenPopup) {
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      responses;
  responses.emplace_back(
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/fenced_frames/report1.html"));
  responses.emplace_back(
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/fenced_frames/report2.html"));
  ASSERT_TRUE(https_server()->Start());

  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin = url::Origin::Create(main_url);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_TRUE(ExecJs(shell(), kGenerateURLsListScript));
  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());
  // There is one "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        const urls = generateUrls(8);
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          urls,
          {
            data: {'mockResult': 1},
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

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(
      ResolveSelectURLToConfig()
          ? FencedFrameNavigationTarget("select_url_result")
          : FencedFrameNavigationTarget(observed_urn_uuid.value()));

  RunSuccessfulReportEvents(fenced_frame_root_node, responses[0].get(),
                            responses[1].get());

  // The origin's entropy budget is untouched.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()),
                   kBudgetAllowed);

  OpenPopup(fenced_frame_root_node,
            https_server()->GetURL("b.test", kSimplePagePath), /*name=*/"");

  // After the popup, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin without any error.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);
}

IN_PROC_BROWSER_TEST_P(SharedStorageReportEventLimitBrowserTest,
                       PopupThenReportEvent) {
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      responses;
  responses.emplace_back(
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/fenced_frames/report1.html"));
  responses.emplace_back(
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/fenced_frames/report2.html"));
  ASSERT_TRUE(https_server()->Start());

  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin shared_storage_origin = url::Origin::Create(main_url);

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/simple_module.js');
    )"));

  EXPECT_TRUE(ExecJs(shell(), kGenerateURLsListScript));
  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.resolveSelectURLToConfig = $1;",
                                        ResolveSelectURLToConfig())));

  TestSelectURLFencedFrameConfigObserver config_observer(GetStoragePartition());
  // There is one "worklet operation": `selectURL()`.
  test_worklet_host_manager()
      .GetAttachedWorkletHost()
      ->SetExpectedWorkletResponsesCount(1);

  EvalJsResult result = EvalJs(shell(), R"(
      (async function() {
        const urls = generateUrls(8);
        window.select_url_result = await sharedStorage.selectURL(
          'test-url-selection-operation',
          urls,
          {
            data: {'mockResult': 1},
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

  FrameTreeNode* fenced_frame_root_node = CreateFencedFrame(
      ResolveSelectURLToConfig()
          ? FencedFrameNavigationTarget("select_url_result")
          : FencedFrameNavigationTarget(observed_urn_uuid.value()));

  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin), kBudgetAllowed);
  EXPECT_DOUBLE_EQ(RemainingBudgetViaJSForFrame(PrimaryFrameTreeNodeRoot()),
                   kBudgetAllowed);

  OpenPopup(fenced_frame_root_node,
            https_server()->GetURL("b.test", kSimplePagePath),
            /*name=*/"");

  // After the popup, log(8)=3 bits should have been withdrawn from the
  // original shared storage origin.
  EXPECT_DOUBLE_EQ(GetRemainingBudget(shared_storage_origin),
                   kBudgetAllowed - 3);

  // the calls to `reportEvent()` should still succeed after the popup.
  RunSuccessfulReportEvents(fenced_frame_root_node, responses[0].get(),
                            responses[1].get());
}

IN_PROC_BROWSER_TEST_P(SharedStorageReportEventLimitBrowserTest,
                       ReportEvent_NestedFencedFrames_LimitReached) {
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      responses;
  for (size_t i = 0; i < 2; ++i) {
    responses.emplace_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            https_server(), "/fenced_frames/report1.html"));
    responses.emplace_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            https_server(), "/fenced_frames/report2.html"));
  }
  ASSERT_TRUE(https_server()->Start());

  GURL main_url = https_server()->GetURL("a.test", kSimplePagePath);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetFilter(
      MakeFilter({"The call to fence.reportEvent was blocked due to "
                  "insufficient budget."}));

  url::Origin shared_storage_origin_1 =
      url::Origin::Create(https_server()->GetURL("b.test", kSimplePagePath));

  // This call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy.
  GURL urn_uuid_1 = SelectFrom8URLsInContext(shared_storage_origin_1);
  FrameTreeNode* outer_fenced_frame_root_node = CreateFencedFrame(urn_uuid_1);

  url::Origin shared_storage_origin_2 =
      url::Origin::Create(https_server()->GetURL("c.test", kSimplePagePath));

  // This call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy.
  GURL urn_uuid_2 = SelectFrom8URLsInContext(shared_storage_origin_2,
                                             outer_fenced_frame_root_node);

  FrameTreeNode* inner_fenced_frame_root_node =
      CreateFencedFrame(outer_fenced_frame_root_node, urn_uuid_2);

  RunSuccessfulReportEvents(inner_fenced_frame_root_node, responses[0].get(),
                            responses[1].get());

  // This call to `selectURL()` will have 8 input URLs, and hence
  // 3 = log2(8) bits of entropy.
  GURL extra_urn = SelectFrom8URLsInContext(shared_storage_origin_1);

  FrameTreeNode* extra_fenced_frame_root_node = CreateFencedFrame(extra_urn);

  if (LimitSharedStorageReportEventCalls()) {
    // The limit for `reportEvent()` has now been reached for this page.
    // Make one more call, which will be blocked.
    std::string click_event_data = "this is a click";
    EXPECT_TRUE(
        ExecJs(extra_fenced_frame_root_node,
               JsReplace("window.fence.reportEvent({"
                         "  eventType: 'click',"
                         "  eventData: $1,"
                         "  destination: ['shared-storage-select-url']});",
                         click_event_data)));

    EXPECT_TRUE(console_observer.Wait());
    ASSERT_LE(1u, console_observer.messages().size());
    EXPECT_EQ(
        "The call to fence.reportEvent was blocked due to insufficient "
        "budget.",
        base::UTF16ToUTF8(console_observer.messages().back().message));
  } else {
    // The `reportEvent()` limit is disabled. The calls will run
    // successfully.
    RunSuccessfulReportEvents(extra_fenced_frame_root_node, responses[2].get(),
                              responses[3].get());
  }
}

class SharedStorageContextBrowserTest
    : public SharedStorageFencedFrameInteractionBrowserTestBase {
 public:
  SharedStorageContextBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
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
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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

}  // namespace content

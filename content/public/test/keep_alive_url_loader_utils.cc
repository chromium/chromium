// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/public/test/keep_alive_url_loader_utils.h"

#include <memory>
#include <set>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "content/browser/loader/keep_alive_url_loader.h"
#include "content/browser/loader/keep_alive_url_loader_service.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/color/color_id.h"

namespace content {

namespace {

using testing::IsNull;
using testing::NotNull;
using testing::Pointee;

// NOTE: The following are sorted in the topological order of the corresponding
// request stage types.
const std::set<std::string>& GetTimeDeltaMetricNames() {
  static base::NoDestructor<std::set<std::string>> names({
      "TimeDelta.RequestStarted",
      "TimeDelta.FirstRedirectReceived",
      "TimeDelta.SecondRedirectReceived",
      "TimeDelta.ThirdOrLaterRedirectReceived",
      "TimeDelta.ResponseReceived",
      "TimeDelta.RequestFailed",
      "TimeDelta.RequestCancelledAfterTimeLimit",
      "TimeDelta.RequestCancelledByRenderer",
      "TimeDelta.LoaderDisconnectedFromRenderer",
      "TimeDelta.BrowserShutdown",
      "TimeDelta.LoaderCompleted",
      "TimeDelta.RequestRetried",
      "TimeDelta.EventLogged",
  });
  return *names;
}

// Counts the total triggering of `Increment()` asynchronously.
//
// Use `WaitUntil()` to wait until this counter reaching specific value.
class AtomicCounter {
 public:
  AtomicCounter() = default;
  // Not Copyable.
  AtomicCounter(const AtomicCounter&) = delete;
  AtomicCounter& operator=(const AtomicCounter&) = delete;

  // Increments the internal counter, and stops `waiting_run_loop_` if exists.
  void Increment() {
    base::AutoLock auto_lock(lock_);
    count_++;
    if (waiting_run_loop_ && last_waited_value_ <= count_) {
      waiting_run_loop_->Quit();
    }
  }

  // If `count_` does not yet reach `value`, a RunLoop will be created and runs
  // until it is stopped by `Increment()`.
  void WaitUntil(size_t value) {
    {
      base::AutoLock auto_lock(lock_);
      if (count_ >= value) {
        return;
      }
      last_waited_value_ = value;
    }

    {
      base::AutoLock auto_lock(lock_);
      waiting_run_loop_ = std::make_unique<base::RunLoop>(
          base::RunLoop::Type::kNestableTasksAllowed);
    }
    waiting_run_loop_->Run();

    {
      base::AutoLock auto_lock(lock_);
      waiting_run_loop_.reset();
    }
  }

 private:
  base::Lock lock_;
  size_t count_ GUARDED_BY(lock_) = 0;
  size_t last_waited_value_ = 0;
  std::unique_ptr<base::RunLoop> waiting_run_loop_ = nullptr;
};

// `arg` is a 2-tuple (URLLoaderCompletionStatus, int).
MATCHER(ErrorCodeEq, "match the same error code") {
  const auto& expected = std::get<1>(arg);
  const auto& got = std::get<0>(arg).error_code;
  if (got != expected) {
    *result_listener << "expected error code [" << expected << "] got [" << got
                     << "]";
    return false;
  }
  return true;
}

}  // namespace

class TestObserverImpl : public KeepAliveURLLoader::TestObserver {
 public:
  TestObserverImpl() = default;

  // Not Copyable.
  TestObserverImpl(const TestObserverImpl&) = delete;
  TestObserverImpl& operator=(const TestObserverImpl&) = delete;

  // Waits for `OnReceiveRedirectForwarded()` to be called `total` times.
  void WaitForTotalOnReceiveRedirectForwarded(size_t total) {
    on_receive_redirect_forwarded_count_.WaitUntil(total);
  }
  // Waits for `OnReceiveRedirectProcessed()` to be called `total` times.
  void WaitForTotalOnReceiveRedirectProcessed(size_t total) {
    on_receive_redirect_processed_count_.WaitUntil(total);
  }
  // Waits for `OnReceiveResponse()` to be called `total` times.
  void WaitForTotalOnReceiveResponse(size_t total) {
    on_receive_response_count_.WaitUntil(total);
  }
  // Waits for `OnReceiveResponseForwarded()` to be called `total` times.
  void WaitForTotalOnReceiveResponseForwarded(size_t total) {
    on_receive_response_forwarded_count_.WaitUntil(total);
  }
  // Waits for `OnReceiveResponseProcessed()` to be called `total` times.
  void WaitForTotalOnReceiveResponseProcessed(size_t total) {
    on_receive_response_processed_count_.WaitUntil(total);
  }
  // Waits for `OnComplete()` to be called `error_codes.size()` times,
  // and the error codes from `on_complete_forwarded_status_` should match
  // `error_codes`.
  void WaitForTotalOnComplete(const std::vector<int>& error_codes) {
    on_complete_count_.WaitUntil(error_codes.size());
    CHECK_EQ(error_codes.size(), on_complete_status_.size());
    EXPECT_THAT(on_complete_status_,
                testing::Pointwise(ErrorCodeEq(), error_codes));
  }
  // Waits for `OnCompleteForwarded()` to be called `error_codes.size()` times,
  // and the error codes from `on_complete_forwarded_status_` should match
  // `error_codes`.
  void WaitForTotalOnCompleteForwarded(const std::vector<int>& error_codes) {
    on_complete_forwarded_count_.WaitUntil(error_codes.size());
    EXPECT_THAT(on_complete_forwarded_status_,
                testing::Pointwise(ErrorCodeEq(), error_codes));
  }
  // Waits for `OnCompleteProcessed()` to be called `error_codes.size()` times,
  // and the error codes from `on_complete_processed_status_` should match
  // `error_codes`.
  void WaitForTotalOnCompleteProcessed(const std::vector<int>& error_codes) {
    on_complete_processed_count_.WaitUntil(error_codes.size());
    EXPECT_THAT(on_complete_processed_status_,
                testing::Pointwise(ErrorCodeEq(), error_codes));
  }

 private:
  ~TestObserverImpl() override = default;

  // KeepAliveURLLoader::TestObserver overrides:
  void OnReceiveRedirectForwarded(KeepAliveURLLoader* loader) override {
    on_receive_redirect_forwarded_count_.Increment();
  }
  void OnReceiveRedirectProcessed(KeepAliveURLLoader* loader) override {
    on_receive_redirect_processed_count_.Increment();
  }
  void OnReceiveResponse(KeepAliveURLLoader* loader) override {
    on_receive_response_count_.Increment();
  }
  void OnReceiveResponseForwarded(KeepAliveURLLoader* loader) override {
    on_receive_response_forwarded_count_.Increment();
  }
  void OnReceiveResponseProcessed(KeepAliveURLLoader* loader) override {
    on_receive_response_processed_count_.Increment();
  }
  void OnComplete(
      KeepAliveURLLoader* loader,
      const network::URLLoaderCompletionStatus& completion_status) override {
    on_complete_status_.push_back(completion_status);
    on_complete_count_.Increment();
  }
  void OnCompleteForwarded(
      KeepAliveURLLoader* loader,
      const network::URLLoaderCompletionStatus& completion_status) override {
    on_complete_forwarded_status_.push_back(completion_status);
    on_complete_forwarded_count_.Increment();
  }
  void OnCompleteProcessed(
      KeepAliveURLLoader* loader,
      const network::URLLoaderCompletionStatus& completion_status) override {
    on_complete_processed_status_.push_back(completion_status);
    on_complete_processed_count_.Increment();
  }

  // OnReceiveRedirect*:
  AtomicCounter on_receive_redirect_forwarded_count_;
  AtomicCounter on_receive_redirect_processed_count_;
  // OnReceiveResponse*:
  AtomicCounter on_receive_response_count_;
  AtomicCounter on_receive_response_forwarded_count_;
  AtomicCounter on_receive_response_processed_count_;
  // OnComplete*:
  AtomicCounter on_complete_count_;
  AtomicCounter on_complete_forwarded_count_;
  AtomicCounter on_complete_processed_count_;

  std::vector<network::URLLoaderCompletionStatus> on_complete_status_;
  std::vector<network::URLLoaderCompletionStatus> on_complete_forwarded_status_;
  std::vector<network::URLLoaderCompletionStatus> on_complete_processed_status_;
};

class KeepAliveURLLoadersTestObserverImpl {
 public:
  KeepAliveURLLoadersTestObserverImpl()
      : refptr_(base::MakeRefCounted<TestObserverImpl>()) {}
  ~KeepAliveURLLoadersTestObserverImpl() = default;

  scoped_refptr<TestObserverImpl> refptr() const { return refptr_; }
  TestObserverImpl* get() const { return refptr_.get(); }

 private:
  scoped_refptr<TestObserverImpl> refptr_;
};

KeepAliveURLLoadersTestObserver::KeepAliveURLLoadersTestObserver(
    BrowserContext* browser_context)
    : impl_(std::make_unique<KeepAliveURLLoadersTestObserverImpl>()) {
  CHECK(browser_context);

  static_cast<StoragePartitionImpl*>(
      browser_context->GetDefaultStoragePartition())
      ->GetKeepAliveURLLoaderService()
      ->SetLoaderObserverForTesting(impl_->refptr());
}

KeepAliveURLLoadersTestObserver::~KeepAliveURLLoadersTestObserver() = default;

void KeepAliveURLLoadersTestObserver::WaitForTotalOnReceiveRedirectForwarded(
    size_t total) {
  impl_->get()->WaitForTotalOnReceiveRedirectForwarded(total);
}

void KeepAliveURLLoadersTestObserver::WaitForTotalOnReceiveRedirectProcessed(
    size_t total) {
  impl_->get()->WaitForTotalOnReceiveRedirectProcessed(total);
}

void KeepAliveURLLoadersTestObserver::WaitForTotalOnReceiveResponse(
    size_t total) {
  impl_->get()->WaitForTotalOnReceiveResponse(total);
}

void KeepAliveURLLoadersTestObserver::WaitForTotalOnReceiveResponseForwarded(
    size_t total) {
  impl_->get()->WaitForTotalOnReceiveResponseForwarded(total);
}

void KeepAliveURLLoadersTestObserver::WaitForTotalOnReceiveResponseProcessed(
    size_t total) {
  impl_->get()->WaitForTotalOnReceiveResponseProcessed(total);
}

void KeepAliveURLLoadersTestObserver::WaitForTotalOnComplete(
    const std::vector<int>& error_codes) {
  impl_->get()->WaitForTotalOnComplete(error_codes);
}

void KeepAliveURLLoadersTestObserver::WaitForTotalOnCompleteForwarded(
    const std::vector<int>& error_codes) {
  impl_->get()->WaitForTotalOnCompleteForwarded(error_codes);
}

void KeepAliveURLLoadersTestObserver::WaitForTotalOnCompleteProcessed(
    const std::vector<int>& error_codes) {
  impl_->get()->WaitForTotalOnCompleteProcessed(error_codes);
}

KeepAliveRequestUkmMatcher::CommonUkm::CommonUkm(
    KeepAliveRequestTracker::RequestType request_type,
    size_t category_id,
    size_t num_redirects,
    size_t num_retries,
    bool is_context_detached,
    KeepAliveRequestTracker::RequestStageType end_stage,
    std::optional<KeepAliveRequestTracker::RequestStageType> previous_stage,
    const std::optional<base::UnguessableToken>& keepalive_token,
    std::optional<int64_t> failed_error_code,
    std::optional<int64_t> failed_extended_error_code,
    std::optional<int64_t> completed_error_code,
    std::optional<int64_t> completed_extended_error_code,
    std::optional<int64_t> retried_error_code,
    std::optional<int64_t> retried_extended_error_code)
    : request_type(request_type),
      category_id(category_id),
      num_redirects(num_redirects),
      num_retries(num_retries),
      is_context_detached(is_context_detached),
      end_stage(end_stage),
      previous_stage(previous_stage),
      keepalive_token(keepalive_token),
      failed_error_code(failed_error_code),
      failed_extended_error_code(failed_extended_error_code),
      completed_error_code(completed_error_code),
      completed_extended_error_code(completed_extended_error_code),
      retried_error_code(retried_error_code),
      retried_extended_error_code(retried_extended_error_code) {}

KeepAliveRequestUkmMatcher::CommonUkm::CommonUkm(const CommonUkm& other) =
    default;

const ukm::mojom::UkmEntry* KeepAliveRequestUkmMatcher::GetUkmEntry() {
  auto entries = ukm_recorder().GetEntriesByName(UkmEvent::kEntryName);
  CHECK_EQ(entries.size(), 1u)
      << "The number of recorded UKM event [" << kUkmName << "] must be 1.";
  return entries[0];
}

void KeepAliveRequestUkmMatcher::ExpectNoUkm() {
  auto entries = ukm_recorder().GetEntriesByName(UkmEvent::kEntryName);
  EXPECT_THAT(entries, testing::IsEmpty())
      << "Unexpected UKM event [" << kUkmName << "] was recorded";
}

void KeepAliveRequestUkmMatcher::ExpectCommonUkm(
    const ukm::mojom::UkmEntry* entry,
    KeepAliveRequestTracker::RequestType request_type,
    size_t category_id,
    size_t num_redirects,
    size_t num_retries,
    bool is_context_detached,
    KeepAliveRequestTracker::RequestStageType end_stage,
    std::optional<KeepAliveRequestTracker::RequestStageType> previous_stage,
    const std::optional<base::UnguessableToken>& keepalive_token,
    std::optional<int64_t> failed_error_code,
    std::optional<int64_t> failed_extended_error_code,
    std::optional<int64_t> completed_error_code,
    std::optional<int64_t> completed_extended_error_code,
    std::optional<int64_t> retried_error_code,
    std::optional<int64_t> retried_extended_error_code) {
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(entry, "Id.Low"));
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(entry, "Id.High"));
  if (keepalive_token.has_value()) {
    ukm_recorder().ExpectEntryMetric(entry, "Id.Low",
                                     keepalive_token->GetLowForSerialization());
    ukm_recorder().ExpectEntryMetric(
        entry, "Id.High", keepalive_token->GetHighForSerialization());
  }

  ukm_recorder().ExpectEntryMetric(entry, "RequestType",
                                   static_cast<int64_t>(request_type));
  ukm_recorder().ExpectEntryMetric(entry, "Category", category_id);
  ukm_recorder().ExpectEntryMetric(entry, "NumRedirects",
                                   static_cast<int64_t>(num_redirects));
  ukm_recorder().ExpectEntryMetric(entry, "NumRetries",
                                   static_cast<int64_t>(num_retries));
  ukm_recorder().ExpectEntryMetric(entry, "IsContextDetached",
                                   static_cast<int64_t>(is_context_detached));
  ukm_recorder().ExpectEntryMetric(entry, "EndStage",
                                   static_cast<int64_t>(end_stage));

  if (previous_stage.has_value()) {
    ukm_recorder().ExpectEntryMetric(entry, "PreviousStage",
                                     static_cast<int64_t>(*previous_stage));
  } else {
    EXPECT_FALSE(ukm_recorder().EntryHasMetric(entry, "PreviousStage"));
  }

  if (failed_error_code.has_value()) {
    ukm_recorder().ExpectEntryMetric(entry, "RequestFailed.ErrorCode",
                                     static_cast<int64_t>(*failed_error_code));
  } else {
    EXPECT_FALSE(
        ukm_recorder().EntryHasMetric(entry, "RequestFailed.ErrorCode"));
  }

  if (failed_extended_error_code.has_value()) {
    ukm_recorder().ExpectEntryMetric(
        entry, "RequestFailed.ExtendedErrorCode",
        static_cast<int64_t>(*failed_extended_error_code));
  } else {
    EXPECT_FALSE(ukm_recorder().EntryHasMetric(
        entry, "RequestFailed.ExtendedErrorCode"));
  }

  if (completed_error_code.has_value()) {
    ukm_recorder().ExpectEntryMetric(
        entry, "LoaderCompleted.ErrorCode",
        static_cast<int64_t>(*completed_error_code));
  } else {
    EXPECT_FALSE(
        ukm_recorder().EntryHasMetric(entry, "LoaderCompleted.ErrorCode"));
  }

  if (completed_extended_error_code.has_value()) {
    ukm_recorder().ExpectEntryMetric(
        entry, "LoaderCompleted.ExtendedErrorCode",
        static_cast<int64_t>(*completed_extended_error_code));
  } else {
    EXPECT_FALSE(ukm_recorder().EntryHasMetric(
        entry, "LoaderCompleted.ExtendedErrorCode"));
  }

  if (retried_error_code.has_value()) {
    ukm_recorder().ExpectEntryMetric(entry, "RequestRetried.ErrorCode",
                                     static_cast<int64_t>(*retried_error_code));
  } else {
    EXPECT_FALSE(
        ukm_recorder().EntryHasMetric(entry, "RequestRetried.ErrorCode"));
  }

  if (retried_extended_error_code.has_value()) {
    ukm_recorder().ExpectEntryMetric(
        entry, "RequestRetried.ExtendedErrorCode",
        static_cast<int64_t>(*retried_extended_error_code));
  } else {
    EXPECT_FALSE(ukm_recorder().EntryHasMetric(
        entry, "RequestRetried.ExtendedErrorCode"));
  }
}

void KeepAliveRequestUkmMatcher::ExpectCommonUkm(
    KeepAliveRequestTracker::RequestType request_type,
    size_t category_id,
    size_t num_redirects,
    size_t num_retries,
    bool is_context_detached,
    KeepAliveRequestTracker::RequestStageType end_stage,
    std::optional<KeepAliveRequestTracker::RequestStageType> previous_stage,
    const std::optional<base::UnguessableToken>& keepalive_token,
    std::optional<int64_t> failed_error_code,
    std::optional<int64_t> failed_extended_error_code,
    std::optional<int64_t> completed_error_code,
    std::optional<int64_t> completed_extended_error_code,
    std::optional<int64_t> retried_error_code,
    std::optional<int64_t> retried_extended_error_code) {
  const ukm::mojom::UkmEntry* entry = GetUkmEntry();
  ExpectCommonUkm(entry, request_type, category_id, num_redirects, num_retries,
                  is_context_detached, end_stage, previous_stage,
                  keepalive_token, failed_error_code,
                  failed_extended_error_code, completed_error_code,
                  completed_extended_error_code, retried_error_code,
                  retried_extended_error_code);
}

void KeepAliveRequestUkmMatcher::ExpectCommonUkms(
    const std::vector<CommonUkm>& ukms) {
  auto entries = ukm_recorder().GetEntriesByName(UkmEvent::kEntryName);
  EXPECT_EQ(entries.size(), ukms.size())
      << "The number of recorded UKM event [" << kUkmName << "] "
      << entries.size() << " must equal to the number of expected UKMs "
      << ukms.size();

  for (size_t i = 0; i < entries.size(); ++i) {
    ExpectCommonUkm(
        entries[i], ukms[i].request_type, ukms[i].category_id,
        ukms[i].num_redirects, ukms[i].num_retries, ukms[i].is_context_detached,
        ukms[i].end_stage, ukms[i].previous_stage, ukms[i].keepalive_token,
        ukms[i].failed_error_code, ukms[i].failed_extended_error_code,
        ukms[i].completed_error_code, ukms[i].completed_extended_error_code,
        ukms[i].retried_error_code, ukms[i].retried_extended_error_code);
  }
}

void KeepAliveRequestUkmMatcher::ExpectTimeSortedTimeDeltaUkm(
    const std::vector<std::string>& time_sorted_metric_names) {
  const ukm::mojom::UkmEntry* entry = GetUkmEntry();
  const auto& all_time_delta_metric_names = GetTimeDeltaMetricNames();

  for (const auto& time_sorted_metric_name : time_sorted_metric_names) {
    CHECK(all_time_delta_metric_names.find(time_sorted_metric_name) !=
          all_time_delta_metric_names.end())
        << "TimeDelta UKM metric [" << time_sorted_metric_name
        << "] is not defined.";
  }

  std::set<std::string> time_sorted_metric_names_set(
      time_sorted_metric_names.begin(), time_sorted_metric_names.end());
  for (const auto& metric_name : GetTimeDeltaMetricNames()) {
    if (time_sorted_metric_names_set.find(metric_name) !=
        time_sorted_metric_names_set.end()) {
      EXPECT_TRUE(ukm_recorder().EntryHasMetric(entry, metric_name))
          << "TimeDelta UKM metric [" << metric_name << "] must exist.";
    } else {
      EXPECT_FALSE(ukm_recorder().EntryHasMetric(entry, metric_name))
          << "TimeDelta UKM metric [" << metric_name
          << "] must not be recorded, but got unexpected value.";
    }
  }

  if (time_sorted_metric_names.size() <= 1) {
    return;
  }
  int64_t previous_metric =
      *ukm_recorder().GetEntryMetric(entry, time_sorted_metric_names[0]);
  for (size_t i = 1; i < time_sorted_metric_names.size(); i++) {
    auto current_metric =
        *ukm_recorder().GetEntryMetric(entry, time_sorted_metric_names[i]);
    EXPECT_GE(current_metric, previous_metric)
        << "TimeDelta UKM metric [" << time_sorted_metric_names[i]
        << "] is unexpectedly smaller than the TimeDelta UKM metric ["
        << time_sorted_metric_names[i - 1] << "] from its previous stage.";
    previous_metric = current_metric;
  }
}

const ukm::mojom::UkmEntry*
NavigationKeepAliveRequestUkmMatcher::GetUkmEntry() {
  auto entries = ukm_recorder().GetEntriesByName(UkmEvent::kEntryName);
  CHECK_EQ(entries.size(), 1u)
      << "The number of recorded UKM event [" << kUkmName << "] must be 1.";
  return entries[0];
}

void NavigationKeepAliveRequestUkmMatcher::ExpectNoUkm() {
  auto entries = ukm_recorder().GetEntriesByName(UkmEvent::kEntryName);
  EXPECT_THAT(entries, testing::IsEmpty())
      << "Unexpected UKM event [" << kUkmName << "] was recorded";
}

void NavigationKeepAliveRequestUkmMatcher::ExpectNavigationUkm(
    const ukm::mojom::UkmEntry* entry,
    size_t category_id,
    std::optional<int64_t> navigation_id,
    const std::optional<base::UnguessableToken>& keepalive_token) {
  ukm_recorder().ExpectEntryMetric(entry, "Category", category_id);

  EXPECT_TRUE(ukm_recorder().EntryHasMetric(entry, "NavigationId"));
  if (navigation_id.has_value()) {
    ukm_recorder().ExpectEntryMetric(entry, "NavigationId", *navigation_id);
  }

  EXPECT_TRUE(ukm_recorder().EntryHasMetric(entry, "Id.Low"));
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(entry, "Id.High"));
  if (keepalive_token.has_value()) {
    ukm_recorder().ExpectEntryMetric(entry, "Id.Low",
                                     keepalive_token->GetLowForSerialization());
    ukm_recorder().ExpectEntryMetric(
        entry, "Id.High", keepalive_token->GetHighForSerialization());
  }
}

void NavigationKeepAliveRequestUkmMatcher::ExpectNavigationUkm(
    size_t category_id,
    std::optional<int64_t> navigation_id,
    const std::optional<base::UnguessableToken>& keepalive_token) {
  const ukm::mojom::UkmEntry* entry = GetUkmEntry();
  ExpectNavigationUkm(entry, category_id, navigation_id, keepalive_token);
}

void NavigationKeepAliveRequestUkmMatcher::ExpectNavigationUkms(
    const std::vector<NavigationUkm>& ukms) {
  auto entries = ukm_recorder().GetEntriesByName(UkmEvent::kEntryName);
  EXPECT_EQ(entries.size(), ukms.size())
      << "The number of recorded UKM event [" << kUkmName << "] "
      << entries.size() << " must equal to the number of expected UKMs "
      << ukms.size();

  for (size_t i = 0; i < entries.size(); ++i) {
    ExpectNavigationUkm(entries[i], ukms[i].category_id, ukms[i].navigation_id,
                        ukms[i].keepalive_token);
  }
}

}  // namespace content

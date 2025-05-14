// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/test_shared_storage_worklet_host.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/blink/public/mojom/origin_trials/origin_trial_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom.h"

namespace content {

TestSharedStorageWorkletHost::TestSharedStorageWorkletHost(
    SharedStorageDocumentServiceImpl& document_service,
    const url::Origin& frame_origin,
    const url::Origin& data_origin,
    blink::mojom::SharedStorageDataOriginType data_origin_type,
    const GURL& script_source_url,
    network::mojom::CredentialsMode credentials_mode,
    blink::mojom::SharedStorageWorkletCreationMethod creation_method,
    int worklet_ordinal_id,
    const std::vector<blink::mojom::OriginTrialFeature>& origin_trial_features,
    mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageWorkletHost>
        worklet_host,
    blink::mojom::SharedStorageDocumentService::CreateWorkletCallback callback,
    bool should_defer_worklet_messages)
    : SharedStorageWorkletHost(document_service,
                               frame_origin,
                               data_origin,
                               data_origin_type,
                               script_source_url,
                               credentials_mode,
                               creation_method,
                               worklet_ordinal_id,
                               origin_trial_features,
                               std::move(worklet_host),
                               std::move(callback)),
      should_defer_worklet_messages_(should_defer_worklet_messages) {}

TestSharedStorageWorkletHost::~TestSharedStorageWorkletHost() = default;

// Separate from `WaitForWorkletResponses()` so that we can wait for it without
// having to set an expected response count beforehand. The worklet host won't
// exist before the first call to either `addModule(), `run()`, or
// `selectURL()`. In the correct flow, `addModule()` will be called first.
void TestSharedStorageWorkletHost::WaitForAddModule() {
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
// operation. Precondition: Either `addModule()`, `run()`, or `selectURL()` has
// previously been called so that this worklet host exists.
void TestSharedStorageWorkletHost::SetExpectedWorkletResponsesCount(
    size_t count) {
  expected_worklet_responses_count_ = count;
  response_expectation_set_ = true;
}

// Only applies to `run()` and `selectURL()`.
// Precondition: `SetExpectedWorkletResponsesCount()` has been called with the
// desired expected `count`, followed by the operation(s) itself/themselves.
void TestSharedStorageWorkletHost::WaitForWorkletResponses() {
  if (worklet_responses_count_ >= expected_worklet_responses_count_) {
    ResetResponseCountsAndMaybeCloseWorklet();
    return;
  }

  worklet_responses_count_waiter_ = std::make_unique<base::RunLoop>();
  worklet_responses_count_waiter_->Run();
  worklet_responses_count_waiter_.reset();
  ResetResponseCountsAndMaybeCloseWorklet();
}

void TestSharedStorageWorkletHost::DidAddMessageToConsole(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message) {
  DidAddMessageToConsoleHelper(level, message, /*initial_message=*/true);
}

void TestSharedStorageWorkletHost::DidAddMessageToConsoleHelper(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message,
    bool initial_message) {
  if (should_defer_worklet_messages_ && initial_message) {
    pending_worklet_messages_.push_back(base::BindOnce(
        &TestSharedStorageWorkletHost::DidAddMessageToConsoleHelper,
        weak_ptr_factory_.GetWeakPtr(), level, message,
        /*initial_message=*/false));
    return;
  }

  SharedStorageWorkletHost::DidAddMessageToConsole(level, message);
}

void TestSharedStorageWorkletHost::FireKeepAliveTimerNow() {
  ASSERT_TRUE(GetKeepAliveTimerForTesting().IsRunning());
  GetKeepAliveTimerForTesting().FireNow();
}

void TestSharedStorageWorkletHost::ExecutePendingWorkletMessages() {
  for (auto& callback : pending_worklet_messages_) {
    std::move(callback).Run();
  }
}

void TestSharedStorageWorkletHost::OnCreateWorkletScriptLoadingFinished(
    bool success,
    const std::string& error_message) {
  OnCreateWorkletScriptLoadingFinishedHelper(success, error_message,
                                             /*initial_message=*/true);
}

void TestSharedStorageWorkletHost::OnCreateWorkletScriptLoadingFinishedHelper(
    bool success,
    const std::string& error_message,
    bool initial_message) {
  bool in_keep_alive = IsInKeepAlivePhase();
  if (should_defer_worklet_messages_ && initial_message) {
    pending_worklet_messages_.push_back(
        base::BindOnce(&TestSharedStorageWorkletHost::
                           OnCreateWorkletScriptLoadingFinishedHelper,
                       weak_ptr_factory_.GetWeakPtr(), success, error_message,
                       /*initial_message=*/false));
  } else {
    SharedStorageWorkletHost::OnCreateWorkletScriptLoadingFinished(
        success, error_message);
  }

  if (initial_message) {
    OnAddModuleResponseReceived();
  }
  if (!in_keep_alive) {
    ProcessAddModuleExpirationIfWorkletExpired();
  }
}

void TestSharedStorageWorkletHost::OnRunOperationOnWorkletFinished(
    base::TimeTicks run_start_time,
    base::TimeTicks execution_start_time,
    int operation_id,
    bool success,
    const std::string& error_message) {
  OnRunOperationOnWorkletFinishedHelper(run_start_time, execution_start_time,
                                        operation_id, success, error_message,
                                        /*initial_message=*/true);
}

void TestSharedStorageWorkletHost::OnRunOperationOnWorkletFinishedHelper(
    base::TimeTicks run_start_time,
    base::TimeTicks execution_start_time,
    int operation_id,
    bool success,
    const std::string& error_message,
    bool initial_message) {
  bool in_keep_alive = IsInKeepAlivePhase();
  if (should_defer_worklet_messages_ && initial_message) {
    pending_worklet_messages_.push_back(base::BindOnce(
        &TestSharedStorageWorkletHost::OnRunOperationOnWorkletFinishedHelper,
        weak_ptr_factory_.GetWeakPtr(), run_start_time, execution_start_time,
        operation_id, success, error_message,
        /*initial_message=*/false));
  } else {
    SharedStorageWorkletHost::OnRunOperationOnWorkletFinished(
        run_start_time, execution_start_time, operation_id, success,
        error_message);
  }

  if (initial_message) {
    OnWorkletResponseReceived();
  }
  if (!in_keep_alive) {
    ProcessRunOrSelectURLExpirationIfWorkletExpired();
  }
}

void TestSharedStorageWorkletHost::OnRunURLSelectionOperationOnWorkletFinished(
    const GURL& urn_uuid,
    base::TimeTicks select_url_start_time,
    base::TimeTicks execution_start_time,
    int operation_id,
    const std::string& operation_name,
    const std::u16string& saved_query_name_to_cache,
    bool script_execution_success,
    const std::string& script_execution_error_message,
    uint32_t index,
    bool use_page_budgets,
    BudgetResult budget_result) {
  OnRunURLSelectionOperationOnWorkletFinishedHelper(
      urn_uuid, select_url_start_time, execution_start_time, operation_id,
      operation_name, saved_query_name_to_cache, script_execution_success,
      script_execution_error_message, index, use_page_budgets,
      std::move(budget_result), /*initial_message=*/true);
}

void TestSharedStorageWorkletHost::
    OnRunURLSelectionOperationOnWorkletFinishedHelper(
        const GURL& urn_uuid,
        base::TimeTicks select_url_start_time,
        base::TimeTicks execution_start_time,
        int operation_id,
        const std::string& operation_name,
        const std::u16string& saved_query_name_to_cache,
        bool script_execution_success,
        const std::string& script_execution_error_message,
        uint32_t index,
        bool use_page_budgets,
        BudgetResult budget_result,
        bool initial_message) {
  bool in_keep_alive = IsInKeepAlivePhase();
  if (should_defer_worklet_messages_ && initial_message) {
    pending_worklet_messages_.push_back(base::BindOnce(
        &TestSharedStorageWorkletHost::
            OnRunURLSelectionOperationOnWorkletFinishedHelper,
        weak_ptr_factory_.GetWeakPtr(), urn_uuid, select_url_start_time,
        execution_start_time, operation_id, operation_name,
        saved_query_name_to_cache, script_execution_success,
        script_execution_error_message, index, use_page_budgets,
        std::move(budget_result), /*initial_message=*/false));
  } else {
    SharedStorageWorkletHost::OnRunURLSelectionOperationOnWorkletFinished(
        urn_uuid, select_url_start_time, execution_start_time, operation_id,
        operation_name, saved_query_name_to_cache, script_execution_success,
        script_execution_error_message, index, use_page_budgets,
        std::move(budget_result));
  }

  if (initial_message) {
    OnWorkletResponseReceived();
  }
  if (!in_keep_alive) {
    ProcessRunOrSelectURLExpirationIfWorkletExpired();
  }
}

void TestSharedStorageWorkletHost::ExpireWorklet() {
  // We must defer the destruction of the expired worklet until the rest of the
  // test worklet code has run, in order to avoid segmentation faults. In
  // particular, if either the `add_module_waiter_` or the
  // `worklet_responses_count_waiter_` is running, we must quit it first before
  // we actually destroy the worklet (regardless of how many worklet responses
  // have been received). Hence we save a callback to destroy the worklet after
  // we quit the waiter. The `Quit()` will occur in
  // `Process*ExpirationIfWorkletExpired()` after returning to
  // `On*OnWorkletFinishedHelper()`. If no waiter is running, we still have to
  // finish running whichever `On*OnWorkletFinishedHelper()` triggered the call
  // to `ExpireWorklet()`.
  DCHECK(!pending_expire_worklet_callback_);
  pending_expire_worklet_callback_ =
      base::BindOnce(&TestSharedStorageWorkletHost::ExpireWorkletOnBaseClass,
                     weak_ptr_factory_.GetWeakPtr());
}

void TestSharedStorageWorkletHost::
    ProcessAddModuleExpirationIfWorkletExpired() {
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

void TestSharedStorageWorkletHost::
    ProcessRunOrSelectURLExpirationIfWorkletExpired() {
  if (!pending_expire_worklet_callback_) {
    return;
  }

  // We can't have both waiters running at the same time.
  DCHECK(!add_module_waiter_);

  if (worklet_responses_count_waiter_ &&
      worklet_responses_count_waiter_->running()) {
    // The worklet is expired and needs to be destroyed. Since
    // `worklet_responses_count_waiter_` is running, quitting it will return us
    // to `WaitForWorkletResponses()`, where the waiter will be reset, and then,
    // in `ResetResponseCountsAndMaybeCloseWorklet()`, the
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

void TestSharedStorageWorkletHost::OnAddModuleResponseReceived() {
  add_module_called_ = true;

  if (add_module_waiter_ && add_module_waiter_->running()) {
    add_module_waiter_->Quit();
  }
}

void TestSharedStorageWorkletHost::OnWorkletResponseReceived() {
  ++worklet_responses_count_;

  if (worklet_responses_count_waiter_ &&
      worklet_responses_count_waiter_->running() &&
      worklet_responses_count_ >= expected_worklet_responses_count_) {
    worklet_responses_count_waiter_->Quit();
  }
}

void TestSharedStorageWorkletHost::ResetAddModuleCalledAndMaybeCloseWorklet() {
  add_module_called_ = false;

  if (pending_expire_worklet_callback_) {
    std::move(pending_expire_worklet_callback_).Run();

    // Do not add code after this. The worklet has been destroyed.
  }
}

void TestSharedStorageWorkletHost::ResetResponseCountsAndMaybeCloseWorklet() {
  expected_worklet_responses_count_ = 0u;
  worklet_responses_count_ = 0u;
  response_expectation_set_ = false;

  if (pending_expire_worklet_callback_) {
    std::move(pending_expire_worklet_callback_).Run();

    // Do not add code after this. The worklet has been destroyed.
  }
}

base::TimeDelta TestSharedStorageWorkletHost::GetKeepAliveTimeout() const {
  // Configure a timeout large enough so that the scheduled task won't run
  // automatically. Instead, we will manually call OneShotTimer::FireNow().
  return base::Seconds(30);
}

}  // namespace content

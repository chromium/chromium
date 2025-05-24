// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_TEST_SHARED_STORAGE_WORKLET_HOST_H_
#define CONTENT_BROWSER_SHARED_STORAGE_TEST_SHARED_STORAGE_WORKLET_HOST_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/blink/public/mojom/origin_trials/origin_trial_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom.h"

class GURL;
namespace url {
class Origin;
}

namespace content {

class SharedStorageDocumentServiceImpl;

class TestSharedStorageWorkletHost : public SharedStorageWorkletHost {
 public:
  TestSharedStorageWorkletHost(
      SharedStorageDocumentServiceImpl& document_service,
      const url::Origin& frame_origin,
      const url::Origin& data_origin,
      blink::mojom::SharedStorageDataOriginType data_origin_type,
      const GURL& script_source_url,
      network::mojom::CredentialsMode credentials_mode,
      blink::mojom::SharedStorageWorkletCreationMethod creation_method,
      int worklet_ordinal_id,
      const std::vector<blink::mojom::OriginTrialFeature>&
          origin_trial_features,
      mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageWorkletHost>
          worklet_host,
      blink::mojom::SharedStorageDocumentService::CreateWorkletCallback
          callback,
      bool should_defer_worklet_messages);

  ~TestSharedStorageWorkletHost() override;

  // Separate from `WaitForWorkletResponses()` so that we can wait for it
  // without having to set an expected response count beforehand. The worklet
  // host won't exist before the first call to either `addModule(), `run()`, or
  // `selectURL()`. In the correct flow, `addModule()` will be called first.
  void WaitForAddModule();

  // Only applies to `run()` and `selectURL()`. Must be set before calling the
  // operation. Precondition: Either `addModule()`, `run()`, or `selectURL()`
  // has previously been called so that this worklet host exists.
  void SetExpectedWorkletResponsesCount(size_t count);

  // Only applies to `run()` and `selectURL()`.
  // Precondition: `SetExpectedWorkletResponsesCount()` has been called with the
  // desired expected `count`, followed by the operation(s) itself/themselves.
  void WaitForWorkletResponses();

  void set_should_defer_worklet_messages(bool should_defer_worklet_messages) {
    should_defer_worklet_messages_ = should_defer_worklet_messages;
  }

  const std::vector<base::OnceClosure>& pending_worklet_messages() {
    return pending_worklet_messages_;
  }

  void DidAddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                              const std::string& message) override;

  void FireKeepAliveTimerNow();

  void ExecutePendingWorkletMessages();

 private:
  void DidAddMessageToConsoleHelper(blink::mojom::ConsoleMessageLevel level,
                                    const std::string& message,
                                    bool initial_message);

  void OnCreateWorkletScriptLoadingFinished(
      bool success,
      const std::string& error_message) override;

  void OnCreateWorkletScriptLoadingFinishedHelper(
      bool success,
      const std::string& error_message,
      bool initial_message);

  void OnRunOperationOnWorkletFinished(
      base::TimeTicks run_start_time,
      base::TimeTicks execution_start_time,
      int operation_id,
      bool success,
      const std::string& error_message) override;

  void OnRunOperationOnWorkletFinishedHelper(
      base::TimeTicks run_start_time,
      base::TimeTicks execution_start_time,
      int operation_id,
      bool success,
      const std::string& error_message,
      bool initial_message);

  void OnRunURLSelectionOperationOnWorkletFinished(
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
      BudgetResult budget_result) override;

  void OnRunURLSelectionOperationOnWorkletFinishedHelper(
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
      bool initial_message);

  void ExpireWorklet() override;

  void ExpireWorkletOnBaseClass() { SharedStorageWorkletHost::ExpireWorklet(); }

  void ProcessAddModuleExpirationIfWorkletExpired();

  void ProcessRunOrSelectURLExpirationIfWorkletExpired();

  void OnAddModuleResponseReceived();

  void OnWorkletResponseReceived();

  void ResetAddModuleCalledAndMaybeCloseWorklet();

  void ResetResponseCountsAndMaybeCloseWorklet();

  base::TimeDelta GetKeepAliveTimeout() const override;

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

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_TEST_SHARED_STORAGE_WORKLET_HOST_H_

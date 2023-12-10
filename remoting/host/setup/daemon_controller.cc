// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/daemon_controller.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"

namespace remoting {

// Name of the Daemon Controller's worker thread.
const char kDaemonControllerThreadName[] = "Daemon Controller thread";

DaemonController::DaemonController(std::unique_ptr<Delegate> delegate)
    : caller_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      delegate_(std::move(delegate)) {
  // Launch the delegate thread.
  delegate_thread_ = std::make_unique<AutoThread>(kDaemonControllerThreadName);
#if BUILDFLAG(IS_WIN)
  delegate_thread_->SetComInitType(AutoThread::COM_INIT_STA);
  delegate_task_runner_ =
      delegate_thread_->StartWithType(base::MessagePumpType::UI);
#else
  delegate_task_runner_ =
      delegate_thread_->StartWithType(base::MessagePumpType::DEFAULT);
#endif
}

DaemonController::State DaemonController::GetState() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  return delegate_->GetState();
}

void DaemonController::GetConfig(GetConfigCallback done) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  GetConfigCallback wrapped_done =
      base::BindOnce(&DaemonController::InvokeConfigCallbackAndScheduleNext,
                     this, std::move(done));
  base::OnceClosure request = base::BindOnce(&DaemonController::DoGetConfig,
                                             this, std::move(wrapped_done));
  ServiceOrQueueRequest(std::move(request));
}

void DaemonController::CheckPermission(bool it2me, BoolCallback callback) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  return delegate_->CheckPermission(it2me, std::move(callback));
}

void DaemonController::SetConfigAndStart(base::Value::Dict config,
                                         bool consent,
                                         CompletionCallback done) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  CompletionCallback wrapped_done =
      base::BindOnce(&DaemonController::InvokeCompletionCallbackAndScheduleNext,
                     this, std::move(done));
  base::OnceClosure request =
      base::BindOnce(&DaemonController::DoSetConfigAndStart, this,
                     std::move(config), consent, std::move(wrapped_done));
  ServiceOrQueueRequest(std::move(request));
}

void DaemonController::UpdateConfig(base::Value::Dict config,
                                    CompletionCallback done) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  CompletionCallback wrapped_done =
      base::BindOnce(&DaemonController::InvokeCompletionCallbackAndScheduleNext,
                     this, std::move(done));
  base::OnceClosure request =
      base::BindOnce(&DaemonController::DoUpdateConfig, this, std::move(config),
                     std::move(wrapped_done));
  ServiceOrQueueRequest(std::move(request));
}

void DaemonController::Stop(CompletionCallback done) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  CompletionCallback wrapped_done =
      base::BindOnce(&DaemonController::InvokeCompletionCallbackAndScheduleNext,
                     this, std::move(done));
  base::OnceClosure request =
      base::BindOnce(&DaemonController::DoStop, this, std::move(wrapped_done));
  ServiceOrQueueRequest(std::move(request));
}

void DaemonController::GetUsageStatsConsent(GetUsageStatsConsentCallback done) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  GetUsageStatsConsentCallback wrapped_done =
      base::BindOnce(&DaemonController::InvokeConsentCallbackAndScheduleNext,
                     this, std::move(done));
  base::OnceClosure request = base::BindOnce(
      &DaemonController::DoGetUsageStatsConsent, this, std::move(wrapped_done));
  ServiceOrQueueRequest(std::move(request));
}

DaemonController::~DaemonController() {
  // Make sure |delegate_| is deleted on the background thread.
  delegate_task_runner_->DeleteSoon(FROM_HERE, delegate_.release());

  // Stop the thread.
  delegate_task_runner_ = nullptr;
  caller_task_runner_->DeleteSoon(FROM_HERE, delegate_thread_.release());
}

void DaemonController::DoGetConfig(GetConfigCallback done) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  std::optional<base::Value::Dict> config = delegate_->GetConfig();
  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(done), std::move(config)));
}

void DaemonController::DoSetConfigAndStart(base::Value::Dict config,
                                           bool consent,
                                           CompletionCallback done) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  delegate_->SetConfigAndStart(std::move(config), consent, std::move(done));
}

void DaemonController::DoUpdateConfig(base::Value::Dict config,
                                      CompletionCallback done) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  delegate_->UpdateConfig(std::move(config), std::move(done));
}

void DaemonController::DoStop(CompletionCallback done) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  delegate_->Stop(std::move(done));
}

void DaemonController::DoGetUsageStatsConsent(
    GetUsageStatsConsentCallback done) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  UsageStatsConsent consent = delegate_->GetUsageStatsConsent();
  caller_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(std::move(done), consent));
}

void DaemonController::InvokeCompletionCallbackAndScheduleNext(
    CompletionCallback done,
    AsyncResult result) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &DaemonController::InvokeCompletionCallbackAndScheduleNext, this,
            std::move(done), result));
    return;
  }

  std::move(done).Run(result);
  OnServicingDone();
}

void DaemonController::InvokeConfigCallbackAndScheduleNext(
    GetConfigCallback done,
    std::optional<base::Value::Dict> config) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  std::move(done).Run(std::move(config));
  OnServicingDone();
}

void DaemonController::InvokeConsentCallbackAndScheduleNext(
    GetUsageStatsConsentCallback done,
    const UsageStatsConsent& consent) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  std::move(done).Run(consent);
  OnServicingDone();
}

void DaemonController::OnServicingDone() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  servicing_request_ = false;
  ServiceNextRequest();
}

void DaemonController::ServiceOrQueueRequest(base::OnceClosure request) {
  pending_requests_.push(std::move(request));
  if (!servicing_request_) {
    ServiceNextRequest();
  }
}

void DaemonController::ServiceNextRequest() {
  if (!pending_requests_.empty()) {
    base::OnceClosure request = std::move(pending_requests_.front());
    pending_requests_.pop();
    delegate_task_runner_->PostTask(FROM_HERE, std::move(request));
    servicing_request_ = true;
  }
}

}  // namespace remoting

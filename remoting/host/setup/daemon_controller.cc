// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/daemon_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_pump_type.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"

namespace remoting {

// Name of the Daemon Controller's worker thread.
const char kDaemonControllerThreadName[] = "Daemon Controller thread";

DaemonController::DaemonController(std::unique_ptr<Delegate> delegate)
    : caller_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      delegate_(std::move(delegate)) {
  // Launch the delegate thread.
  delegate_thread_.reset(new AutoThread(kDaemonControllerThreadName));
#if defined(OS_WIN)
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

void DaemonController::GetConfig(const GetConfigCallback& done) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  DaemonController::GetConfigCallback wrapped_done = base::Bind(
      &DaemonController::InvokeConfigCallbackAndScheduleNext, this, done);
  base::Closure request = base::Bind(
      &DaemonController::DoGetConfig, this, wrapped_done);
  ServiceOrQueueRequest(request);
}

void DaemonController::CheckPermission(bool it2me, BoolCallback callback) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  return delegate_->CheckPermission(it2me, std::move(callback));
}

void DaemonController::SetConfigAndStart(
    std::unique_ptr<base::DictionaryValue> config,
    bool consent,
    const CompletionCallback& done) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  DaemonController::CompletionCallback wrapped_done = base::Bind(
      &DaemonController::InvokeCompletionCallbackAndScheduleNext, this, done);
  base::Closure request = base::Bind(
      &DaemonController::DoSetConfigAndStart, this, base::Passed(&config),
      consent, wrapped_done);
  ServiceOrQueueRequest(request);
}

void DaemonController::UpdateConfig(
    std::unique_ptr<base::DictionaryValue> config,
    const CompletionCallback& done) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  DaemonController::CompletionCallback wrapped_done = base::Bind(
      &DaemonController::InvokeCompletionCallbackAndScheduleNext, this, done);
  base::Closure request = base::Bind(
      &DaemonController::DoUpdateConfig, this, base::Passed(&config),
      wrapped_done);
  ServiceOrQueueRequest(request);
}

void DaemonController::Stop(const CompletionCallback& done) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  DaemonController::CompletionCallback wrapped_done = base::Bind(
      &DaemonController::InvokeCompletionCallbackAndScheduleNext, this, done);
  base::Closure request = base::Bind(
      &DaemonController::DoStop, this, wrapped_done);
  ServiceOrQueueRequest(request);
}

void DaemonController::GetUsageStatsConsent(
    const GetUsageStatsConsentCallback& done) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  DaemonController::GetUsageStatsConsentCallback wrapped_done = base::Bind(
      &DaemonController::InvokeConsentCallbackAndScheduleNext, this, done);
  base::Closure request = base::Bind(
      &DaemonController::DoGetUsageStatsConsent, this, wrapped_done);
  ServiceOrQueueRequest(request);
}

DaemonController::~DaemonController() {
  // Make sure |delegate_| is deleted on the background thread.
  delegate_task_runner_->DeleteSoon(FROM_HERE, delegate_.release());

  // Stop the thread.
  delegate_task_runner_ = nullptr;
  caller_task_runner_->DeleteSoon(FROM_HERE, delegate_thread_.release());
}

void DaemonController::DoGetConfig(const GetConfigCallback& done) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  std::unique_ptr<base::DictionaryValue> config = delegate_->GetConfig();
  caller_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(done, std::move(config)));
}

void DaemonController::DoSetConfigAndStart(
    std::unique_ptr<base::DictionaryValue> config,
    bool consent,
    const CompletionCallback& done) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  delegate_->SetConfigAndStart(std::move(config), consent, done);
}

void DaemonController::DoUpdateConfig(
    std::unique_ptr<base::DictionaryValue> config,
    const CompletionCallback& done) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  delegate_->UpdateConfig(std::move(config), done);
}

void DaemonController::DoStop(const CompletionCallback& done) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  delegate_->Stop(done);
}

void DaemonController::DoGetUsageStatsConsent(
    const GetUsageStatsConsentCallback& done) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  DaemonController::UsageStatsConsent consent =
      delegate_->GetUsageStatsConsent();
  caller_task_runner_->PostTask(FROM_HERE, base::BindOnce(done, consent));
}

void DaemonController::InvokeCompletionCallbackAndScheduleNext(
    const CompletionCallback& done,
    AsyncResult result) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &DaemonController::InvokeCompletionCallbackAndScheduleNext, this,
            done, result));
    return;
  }

  done.Run(result);
  ScheduleNext();
}

void DaemonController::InvokeConfigCallbackAndScheduleNext(
    const GetConfigCallback& done,
    std::unique_ptr<base::DictionaryValue> config) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  done.Run(std::move(config));
  ScheduleNext();
}

void DaemonController::InvokeConsentCallbackAndScheduleNext(
    const GetUsageStatsConsentCallback& done,
    const UsageStatsConsent& consent) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  done.Run(consent);
  ScheduleNext();
}

void DaemonController::ScheduleNext() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  pending_requests_.pop();
  ServiceNextRequest();
}

void DaemonController::ServiceOrQueueRequest(const base::Closure& request) {
  bool servicing_request = !pending_requests_.empty();
  pending_requests_.push(request);
  if (!servicing_request)
    ServiceNextRequest();
}

void DaemonController::ServiceNextRequest() {
  if (!pending_requests_.empty())
    delegate_task_runner_->PostTask(FROM_HERE, pending_requests_.front());
}

}  // namespace remoting

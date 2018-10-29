// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/embedded_instance_manager.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"

namespace service_manager {

EmbeddedInstanceManager::EmbeddedInstanceManager(
    const base::StringPiece& name,
    const EmbeddedServiceInfo& info,
    const base::Closure& quit_closure)
    : name_(name.as_string()),
      factory_callback_(info.factory),
      use_own_thread_(!info.task_runner && info.use_own_thread),
      message_loop_type_(info.message_loop_type),
      thread_priority_(info.thread_priority),
      quit_closure_(quit_closure),
      quit_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      service_task_runner_(info.task_runner) {
  if (!use_own_thread_ && !service_task_runner_)
    service_task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

void EmbeddedInstanceManager::BindServiceRequest(
    service_manager::mojom::ServiceRequest request) {
  DCHECK_CALLED_ON_VALID_THREAD(runner_thread_checker_);

  if (use_own_thread_ && !thread_) {
    // Start a new thread if necessary.
    thread_.reset(new base::Thread(name_));
    base::Thread::Options options;
    options.message_loop_type = message_loop_type_;
    options.priority = thread_priority_;
    thread_->StartWithOptions(options);
    service_task_runner_ = thread_->task_runner();
  }

  DCHECK(service_task_runner_);
  service_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedInstanceManager::BindServiceRequestOnServiceSequence,
                 this, base::Passed(&request)));
}

void EmbeddedInstanceManager::ShutDown() {
  DCHECK_CALLED_ON_VALID_THREAD(runner_thread_checker_);
  if (!service_task_runner_)
    return;
  // Any extant ServiceContexts must be destroyed on the application thread.
  if (service_task_runner_->RunsTasksInCurrentSequence()) {
    QuitOnServiceSequence();
  } else {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&EmbeddedInstanceManager::QuitOnServiceSequence, this));
  }
  thread_.reset();
}

EmbeddedInstanceManager::~EmbeddedInstanceManager() {
  // If this instance had its own thread, it MUST be explicitly destroyed by
  // QuitOnRunnerThread() by the time this destructor is run.
  DCHECK(!thread_);
}

void EmbeddedInstanceManager::BindServiceRequestOnServiceSequence(
    service_manager::mojom::ServiceRequest request) {
  DCHECK(service_task_runner_->RunsTasksInCurrentSequence());

  int instance_id = next_instance_id_++;

  std::unique_ptr<service_manager::ServiceContext> context =
      std::make_unique<service_manager::ServiceContext>(factory_callback_.Run(),
                                                        std::move(request));

  service_manager::ServiceContext* raw_context = context.get();
  context->SetQuitClosure(
      base::Bind(&EmbeddedInstanceManager::OnInstanceLost, this, instance_id));
  contexts_.insert(std::make_pair(raw_context, std::move(context)));
  id_to_context_map_.insert(std::make_pair(instance_id, raw_context));
}

void EmbeddedInstanceManager::OnInstanceLost(int instance_id) {
  DCHECK(service_task_runner_->RunsTasksInCurrentSequence());

  auto id_iter = id_to_context_map_.find(instance_id);
  CHECK(id_iter != id_to_context_map_.end());

  auto context_iter = contexts_.find(id_iter->second);
  CHECK(context_iter != contexts_.end());
  contexts_.erase(context_iter);
  id_to_context_map_.erase(id_iter);

  // If we've lost the last instance, run the quit closure.
  if (contexts_.empty())
    QuitOnServiceSequence();
}

void EmbeddedInstanceManager::QuitOnServiceSequence() {
  DCHECK(service_task_runner_->RunsTasksInCurrentSequence());

  contexts_.clear();
  if (quit_task_runner_->RunsTasksInCurrentSequence()) {
    QuitOnRunnerThread();
  } else {
    quit_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&EmbeddedInstanceManager::QuitOnRunnerThread, this));
  }
}

void EmbeddedInstanceManager::QuitOnRunnerThread() {
  DCHECK_CALLED_ON_VALID_THREAD(runner_thread_checker_);
  if (thread_) {
    thread_.reset();
    service_task_runner_ = nullptr;
  }
  quit_closure_.Run();
}

}  // namespace service_manager

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/traced_process_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "services/tracing/public/cpp/base_agent.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "services/tracing/public/cpp/trace_event_agent.h"
#include "services/tracing/public/mojom/constants.mojom.h"

namespace tracing {

// static
TracedProcessImpl* TracedProcessImpl::GetInstance() {
  static base::NoDestructor<TracedProcessImpl> traced_process;
  return traced_process.get();
}

TracedProcessImpl::TracedProcessImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

TracedProcessImpl::~TracedProcessImpl() = default;

void TracedProcessImpl::ResetTracedProcessReceiver() {
  if (task_runner_ && !task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&TracedProcessImpl::ResetTracedProcessReceiver,
                       base::Unretained(this)));
    return;
  }

  receiver_.reset();
}

void TracedProcessImpl::OnTracedProcessRequest(
    mojo::PendingReceiver<mojom::TracedProcess> receiver) {
  if (task_runner_ && !task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&TracedProcessImpl::OnTracedProcessRequest,
                                  base::Unretained(this), std::move(receiver)));
    return;
  }

  // We only need one binding per process. If a new binding request is made,
  // ignore it.
  if (receiver_.is_bound())
    return;

  receiver_.Bind(std::move(receiver));
}

mojo::Remote<mojom::SystemTracingService>&
TracedProcessImpl::system_tracing_service() {
  // |system_tracing_service_| can only be used on the Perfetto task runner.
  auto task_runner =
      PerfettoTracedProcess::GetTaskRunner()->GetOrCreateTaskRunner();
  DCHECK(task_runner && task_runner->RunsTasksInCurrentSequence());
  return system_tracing_service_;
}

void TracedProcessImpl::EnableSystemTracingService(
    mojo::PendingRemote<mojom::SystemTracingService> remote) {
  auto task_runner =
      PerfettoTracedProcess::GetTaskRunner()->GetOrCreateTaskRunner();
  if (!task_runner->RunsTasksInCurrentSequence()) {
    // |system_tracing_service_| is bound on the Perfetto task runner.
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&TracedProcessImpl::EnableSystemTracingService,
                       base::Unretained(this), std::move(remote)));
    return;
  }

  system_tracing_service_.Bind(std::move(remote), nullptr);
}

// SetTaskRunner must be called before we start receiving
// any OnTracedProcessRequest calls.
void TracedProcessImpl::SetTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DCHECK(!receiver_.is_bound());
  DCHECK(!task_runner_);
  task_runner_ = task_runner;
}

void TracedProcessImpl::RegisterAgent(BaseAgent* agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock lock(agents_lock_);
  agents_.insert(agent);
}

void TracedProcessImpl::UnregisterAgent(BaseAgent* agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock lock(agents_lock_);
  agents_.erase(agent);
}

void TracedProcessImpl::ConnectToTracingService(
    mojom::ConnectToTracingRequestPtr request,
    ConnectToTracingServiceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Acknowledge this message so the tracing service knows it was dispatched in
  // this process.
  std::move(callback).Run();

  // Tracing requires a running ThreadPool; disable tracing
  // for processes without it.
  if (!base::ThreadPoolInstance::Get()) {
    return;
  }

  // Ensure the TraceEventAgent has been created.
  TraceEventAgent::GetInstance();

  PerfettoTracedProcess::Get()->ConnectProducer(
      std::move(request->perfetto_service));
}

void TracedProcessImpl::GetCategories(std::set<std::string>* category_set) {
  base::AutoLock lock(agents_lock_);
  for (BaseAgent* agent : agents_) {
    agent->GetCategories(category_set);
  }
}

}  // namespace tracing

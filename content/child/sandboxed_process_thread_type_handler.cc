// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/sandboxed_process_thread_type_handler.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "content/child/child_thread_impl.h"
#include "content/common/thread_type_switcher.mojom.h"

namespace content {

namespace {

SandboxedProcessThreadTypeHandler* g_sandboxed_process_thread_type_handler =
    nullptr;

}  // namespace

SandboxedProcessThreadTypeHandler::SandboxedProcessThreadTypeHandler() {
  // `thread_type_switcher_receiver_` will be bound when ChildThreadImpl is
  // available.
  thread_type_switcher_receiver_ =
      thread_type_switcher_.BindNewPipeAndPassReceiver();
  base::PlatformThread::SetThreadTypeDelegate(this);
}

SandboxedProcessThreadTypeHandler::~SandboxedProcessThreadTypeHandler() {
  base::PlatformThread::SetThreadTypeDelegate(nullptr);
}

// static
void SandboxedProcessThreadTypeHandler::Create() {
  DCHECK(!g_sandboxed_process_thread_type_handler);

  g_sandboxed_process_thread_type_handler =
      new SandboxedProcessThreadTypeHandler();
}

// static
void SandboxedProcessThreadTypeHandler::NotifyMainChildThreadCreated() {
  if (g_sandboxed_process_thread_type_handler) {
    g_sandboxed_process_thread_type_handler->ConnectThreadTypeSwitcher();
  }
}

// static
SandboxedProcessThreadTypeHandler* SandboxedProcessThreadTypeHandler::Get() {
  return g_sandboxed_process_thread_type_handler;
}

bool SandboxedProcessThreadTypeHandler::HandleThreadTypeChange(
    base::PlatformThreadId thread_id,
    base::ThreadType thread_type) {
  // Invoked from arbitrary threads. Coalesce changes and flush them to the
  // browser as a single batched IPC, instead of sending one IPC per thread.
  {
    base::AutoLock lock(lock_);
    pending_changes_[thread_id.raw()] = thread_type;
  }
  // If `task_runner_` isn't set yet, the changes accumulate and are flushed by
  // OnSwitcherConnected() once the pipe is bound (until then a plain
  // SharedRemote would only buffer the calls anyway).
  PostFlushIfNeeded();
  return true;
}

void SandboxedProcessThreadTypeHandler::ConnectThreadTypeSwitcher() {
  ChildThreadImpl* main_thread = ChildThreadImpl::current();
  DCHECK(main_thread);
  DCHECK(main_thread->main_thread_runner()->RunsTasksInCurrentSequence());

  main_thread->BindHostReceiverBatched(
      std::move(thread_type_switcher_receiver_));
  OnSwitcherConnected(main_thread->main_thread_runner());
}

void SandboxedProcessThreadTypeHandler::OnSwitcherConnected(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  {
    base::AutoLock lock(lock_);
    task_runner_ = std::move(task_runner);
  }
  PostFlushIfNeeded();
}

void SandboxedProcessThreadTypeHandler::PostFlushIfNeeded() {
  scoped_refptr<base::SequencedTaskRunner> task_runner;
  {
    base::AutoLock lock(lock_);
    if (pending_changes_.empty() || flush_scheduled_ || !task_runner_) {
      return;
    }
    flush_scheduled_ = true;
    task_runner = task_runner_;
  }
  // Post with `lock_` released so this call out to the task scheduler cannot
  // re-enter HandleThreadTypeChange() (which takes `lock_`) on this thread.
  // base::Unretained() is safe: this is a process-wide singleton (see Create())
  // that is never destroyed, so it outlives any posted task. A WeakPtr can't be
  // used here because HandleThreadTypeChange() runs on arbitrary threads while
  // WeakPtr is sequence-bound.
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&SandboxedProcessThreadTypeHandler::FlushThreadTypeChanges,
                     base::Unretained(this)));
}

mojo::PendingReceiver<mojom::ThreadTypeSwitcher>
SandboxedProcessThreadTypeHandler::PassReceiverForTesting() {
  return std::move(thread_type_switcher_receiver_);
}

void SandboxedProcessThreadTypeHandler::FlushThreadTypeChanges() {
  base::flat_map<int32_t, base::ThreadType> changes;
  {
    base::AutoLock lock(lock_);
    changes.swap(pending_changes_);
    flush_scheduled_ = false;
  }
  if (changes.empty()) {
    return;
  }
  std::vector<mojom::ThreadTypeChangePtr> batch;
  batch.reserve(changes.size());
  for (const auto& [thread_id, thread_type] : changes) {
    batch.push_back(mojom::ThreadTypeChange::New(thread_id, thread_type));
  }
  thread_type_switcher_->SetThreadTypes(std::move(batch));
}

}  // namespace content

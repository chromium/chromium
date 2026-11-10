// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SANDBOXED_PROCESS_THREAD_TYPE_HANDLER_H_
#define CONTENT_CHILD_SANDBOXED_PROCESS_THREAD_TYPE_HANDLER_H_

#include "base/containers/flat_map.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_type_delegate.h"
#include "content/common/content_export.h"
#include "content/common/thread_type_switcher.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace content {

// This class handles thread type changes for sandboxed processes, which
// supports proxying the thread type changes to browser process.
class CONTENT_EXPORT SandboxedProcessThreadTypeHandler
    : public base::ThreadTypeDelegate {
 public:
  SandboxedProcessThreadTypeHandler(const SandboxedProcessThreadTypeHandler&) =
      delete;
  SandboxedProcessThreadTypeHandler& operator=(
      const SandboxedProcessThreadTypeHandler&) = delete;

  ~SandboxedProcessThreadTypeHandler() override;

  // Creates a SandboxedProcessThreadTypeHandler instance and stores it to
  // g_instance. Make sure the g_instance doesn't exist before creation.
  static void Create();

  // Invoked when ChildThread is created. If g_instance exists,
  // g_instance.ConnectThreadTypeSwitcher() is called.
  static void NotifyMainChildThreadCreated();

  // Returns nullptr if Create() hasn't been called, e.g. in an unsandboxed
  // process.
  static SandboxedProcessThreadTypeHandler* Get();

  // Overridden from base::ThreadTypeDelegate.
  bool HandleThreadTypeChange(base::PlatformThreadId thread_id,
                              base::ThreadType thread_type) override;

 private:
  friend class SandboxedProcessThreadTypeHandlerTest;

  SandboxedProcessThreadTypeHandler();

  // Use the ChildThread (which must be valid) to bind `thread_type_switcher_`.
  void ConnectThreadTypeSwitcher();

  // Binds `thread_type_switcher_` to `task_runner`, records it as the task
  // runner used to flush batched changes and flushes any changes that
  // accumulated before the switcher was connected. Called from the sequence
  // `task_runner` belongs to.
  void OnSwitcherConnected(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Posts FlushThreadTypeChanges() to `task_runner_` if there are pending
  // changes, the switcher is connected, and a flush isn't already scheduled.
  // Must be called WITHOUT holding `lock_`: it acquires the lock to make the
  // decision but posts the task with the lock released, so the call out to the
  // task scheduler can never re-enter HandleThreadTypeChange() under the lock.
  void PostFlushIfNeeded();

  // Sends all `pending_changes_` to the browser as a single batched IPC. Runs
  // on `task_runner_`.
  void FlushThreadTypeChanges();

  // Returns the receiver end of `thread_type_switcher_` so a test can bind it
  // to a fake implementation in place of the browser.
  mojo::PendingReceiver<mojom::ThreadTypeSwitcher> PassReceiverForTesting();

  // Bound by OnSwitcherConnected(). Until then, the remote end of the pipe is
  // held by `pending_thread_type_switcher_`, as binding it here needs a task
  // runner that may not exist yet when the handler is created.
  mojo::SharedRemote<mojom::ThreadTypeSwitcher> thread_type_switcher_;
  mojo::PendingRemote<mojom::ThreadTypeSwitcher> pending_thread_type_switcher_;
  // Holds the ThreadTypeSwitcher receiver until it can be bound by the browser
  // process via ChildThreadImpl.
  mojo::PendingReceiver<mojom::ThreadTypeSwitcher>
      thread_type_switcher_receiver_;

  // HandleThreadTypeChange() is invoked from arbitrary threads, so coalescing
  // state is lock-protected. Pending changes are keyed by raw thread id, so a
  // later change to the same thread naturally replaces an earlier one. They are
  // flushed to the browser as a single batched IPC on `task_runner_`, which
  // coalesces the burst of per-thread changes during process startup.
  base::Lock lock_;
  base::flat_map<int32_t, base::ThreadType> pending_changes_ GUARDED_BY(lock_);
  bool flush_scheduled_ GUARDED_BY(lock_) = false;
  scoped_refptr<base::SequencedTaskRunner> task_runner_ GUARDED_BY(lock_);
};

}  // namespace content

#endif  // CONTENT_CHILD_SANDBOXED_PROCESS_THREAD_TYPE_HANDLER_H_

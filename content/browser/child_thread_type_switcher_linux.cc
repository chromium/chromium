// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_thread_type_switcher_linux.h"

#include "base/linux_util.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "content/public/browser/child_process_launcher_utils.h"

namespace content {

namespace {

void SetThreadTypeOnLauncherThread(base::ProcessId peer_pid,
                                   base::PlatformThreadId ns_tid,
                                   base::ThreadType thread_type) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());

  bool ns_pid_supported = false;
  pid_t peer_tid = base::FindThreadID(peer_pid, ns_tid, &ns_pid_supported);
  if (peer_tid == -1) {
    if (ns_pid_supported) {
      DVLOG(1) << "Could not find tid";
    }
    return;
  }

  if (peer_tid == peer_pid && thread_type != base::ThreadType::kDefault &&
      thread_type != base::ThreadType::kDisplayCritical) {
    // TODO(crbug.com/40226692): Consider reporting with ReceivedBadMessage().
    DLOG(WARNING) << "Changing main thread type to another value than "
                  << "kDefault or kDisplayCritical isn't allowed";
    return;
  }

  base::PlatformThread::SetThreadType(peer_pid, peer_tid, thread_type,
                                      base::IsViaIPC(true));
}

}  // namespace

ChildThreadTypeSwitcher::ChildThreadTypeSwitcher() = default;

ChildThreadTypeSwitcher::~ChildThreadTypeSwitcher() = default;

bool ChildThreadTypeSwitcher::Bind(
    mojo::PendingReceiver<mojom::ThreadTypeSwitcher> receiver) {
  if (receiver_.is_bound()) {
    return false;
  }
  receiver_.Bind(std::move(receiver));
  if (child_pid_ == base::kNullProcessId) {
    receiver_.Pause();
  }
  return true;
}

void ChildThreadTypeSwitcher::SetPid(base::ProcessId child_pid) {
  DCHECK_EQ(child_pid_, base::kNullProcessId);
  child_pid_ = child_pid;
  if (receiver_.is_bound()) {
    receiver_.Resume();
  }
}

void ChildThreadTypeSwitcher::SetThreadType(int32_t ns_tid,
                                            base::ThreadType thread_type) {
  // Post this task to process launcher task runner. All thread type changes
  // (nice value, c-group setting) of renderer process would be performed on the
  // same sequence as renderer process priority changes, to guarantee that
  // there's no race of c-group manipulations.
  GetProcessLauncherTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SetThreadTypeOnLauncherThread, child_pid_,
                     static_cast<base::PlatformThreadId>(ns_tid), thread_type));
}

}  // namespace content

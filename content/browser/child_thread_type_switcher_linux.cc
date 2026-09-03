// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_thread_type_switcher_linux.h"

#include "base/linux_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_handle.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "content/public/browser/child_process_launcher_utils.h"

namespace content {

namespace {

void SetThreadTypeOnLauncherThread(base::ProcessId peer_pid,
                                   base::PlatformThreadId ns_tid,
                                   base::ThreadType thread_type) {
  CHECK(CurrentlyOnProcessLauncherTaskRunner(), base::NotFatalUntil::M159);

  bool ns_pid_supported = false;
  pid_t peer_tid =
      base::FindThreadID(peer_pid, ns_tid.raw(), &ns_pid_supported);
  if (peer_tid == -1) {
    if (ns_pid_supported) {
      DVLOG(1) << "Could not find tid";
    }
    return;
  }

  if (peer_tid == peer_pid && thread_type != base::ThreadType::kDefault &&
      thread_type != base::ThreadType::kPresentation &&
      thread_type != base::ThreadType::kAudioProcessing) {
    // TODO(crbug.com/40226692): Consider reporting with ReceivedBadMessage().
    DLOG(WARNING) << "Changing main thread type to another value than "
                  << "kDefault, kInteractive or kPresentation isn't allowed";
    return;
  }

  base::PlatformThread::SetThreadType(
      peer_pid, base::PlatformThreadId(peer_tid), thread_type);
}

void SetThreadTypesOnLauncherThread(
    base::ProcessId peer_pid,
    std::vector<mojom::ThreadTypeChangePtr> changes) {
  CHECK(CurrentlyOnProcessLauncherTaskRunner(), base::NotFatalUntil::M159);
  for (const auto& change : changes) {
    SetThreadTypeOnLauncherThread(
        peer_pid, base::PlatformThreadId(change->platform_thread_id),
        change->thread_type);
  }
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
  CHECK_EQ(child_pid_, base::kNullProcessId, base::NotFatalUntil::M159);
  child_pid_ = child_pid;
  if (receiver_.is_bound()) {
    receiver_.Resume();
  }
}

void ChildThreadTypeSwitcher::SetThreadTypes(
    std::vector<mojom::ThreadTypeChangePtr> changes) {
  // The mojom carries thread ids which must match the platform ThreadId size
  // (32-bit in this case).
  static_assert(sizeof(decltype(mojom::ThreadTypeChange::platform_thread_id)) ==
                sizeof(base::PlatformThreadId));

  // Record batch size for monitoring. Using the macro variant to avoid
  // acquiring a lock here. See
  // https://chromium.googlesource.com/chromium/src/tools/+/HEAD/metrics/histograms/README.md#coding-emitting-to-histograms.
  UMA_HISTOGRAM_COUNTS_100("Process.ThreadTypeSwitcher.BatchSize",
                           changes.size());

  // Apply the whole batch on the process launcher task runner with a single
  // PostTask. All thread type changes (nice value, c-group setting) of the
  // child process are performed on the same sequence as the child process's
  // priority changes, to guarantee there's no race of c-group manipulations.
  GetProcessLauncherTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SetThreadTypesOnLauncherThread, child_pid_,
                                std::move(changes)));
}

}  // namespace content

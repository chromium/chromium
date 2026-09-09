// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_thread_type_switcher_linux.h"

#include <vector>

#include "base/containers/flat_map.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "content/public/browser/child_process_launcher_utils.h"

namespace content {

namespace {

bool IsAllowedForMainThread(base::ThreadType thread_type) {
  return thread_type == base::ThreadType::kDefault ||
         thread_type == base::ThreadType::kPresentation ||
         thread_type == base::ThreadType::kAudioProcessing;
}

}  // namespace

// Translates the thread ids sent by the child, which are relative to its PID
// namespace, to thread ids in the browser's namespace and applies the changes.
// Finding the thread for a namespaced id means reading
// /proc/<pid>/task/*/status until one matches, so translations are remembered
// and only re-validated (one status read) when used again.
class ChildThreadTypeSwitcher::LauncherThreadState {
 public:
  explicit LauncherThreadState(base::ProcessId peer_pid)
      : peer_pid_(peer_pid) {}
  ~LauncherThreadState() = default;

  void SetThreadTypes(std::vector<mojom::ThreadTypeChangePtr> changes) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    bool refreshed = false;
    for (const auto& change : changes) {
      const pid_t ns_tid = change->platform_thread_id;
      // Translations read during this batch are used as they are; older ones
      // are re-validated first.
      pid_t peer_tid = GetCachedThreadId(ns_tid, /*validate=*/!refreshed);
      if (peer_tid == -1 && !refreshed) {
        // Unknown (or stale) thread: rescan the process once per batch. New
        // threads usually arrive together, so this also resolves the rest of
        // the batch.
        RefreshThreadIds();
        refreshed = true;
        peer_tid = GetCachedThreadId(ns_tid, /*validate=*/false);
      }
      if (peer_tid == -1) {
        DVLOG(1) << "Could not find tid";
        continue;
      }
      SetThreadType(base::PlatformThreadId(peer_tid), change->thread_type);
    }
  }

 private:
  // Returns the cached translation of `ns_tid`, or -1 if there is none. With
  // `validate`, the entry is only returned if the thread it names still exists
  // and still has that id in the child's namespace.
  pid_t GetCachedThreadId(pid_t ns_tid, bool validate) {
    auto it = thread_ids_.find(ns_tid);
    if (it == thread_ids_.end()) {
      return -1;
    }
    if (validate &&
        base::GetNamespaceThreadId(peer_pid_, it->second) != ns_tid) {
      // The thread exited (and its id may have been reused since).
      thread_ids_.erase(it);
      return -1;
    }
    return it->second;
  }

  void RefreshThreadIds() {
    thread_ids_.clear();
    std::vector<pid_t> tids;
    if (!base::GetThreadsForProcess(peer_pid_, &tids)) {
      return;
    }
    for (pid_t tid : tids) {
      const pid_t ns_tid = base::GetNamespaceThreadId(peer_pid_, tid);
      if (ns_tid != -1) {
        thread_ids_[ns_tid] = tid;
      }
    }
  }

  void SetThreadType(base::PlatformThreadId peer_tid,
                     base::ThreadType thread_type) {
    if (peer_tid.raw() == peer_pid_ && !IsAllowedForMainThread(thread_type)) {
      // TODO(crbug.com/40226692): Consider reporting with ReceivedBadMessage().
      DLOG(WARNING) << "Changing main thread type to another value than "
                    << "kDefault, kInteractive or kPresentation isn't allowed";
      return;
    }
    base::PlatformThread::SetThreadType(peer_pid_, peer_tid, thread_type);
  }

  const base::ProcessId peer_pid_;
  // Thread id in the child's PID namespace -> thread id in ours.
  base::flat_map<pid_t, pid_t> thread_ids_;
  SEQUENCE_CHECKER(sequence_checker_);
};

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
  if (!launcher_thread_state_) {
    CHECK_NE(child_pid_, base::kNullProcessId);
    launcher_thread_state_.emplace(GetTaskRunner(), child_pid_);
  }
  launcher_thread_state_.AsyncCall(&LauncherThreadState::SetThreadTypes)
      .WithArgs(std::move(changes));
}

scoped_refptr<base::SequencedTaskRunner>
ChildThreadTypeSwitcher::GetTaskRunner() {
  return GetProcessLauncherTaskRunner();
}

}  // namespace content

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/thread_helpers.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "sandbox/linux/services/proc_util.h"

namespace sandbox {

namespace {

const char kAssertSingleThreadedError[] =
    "Current process is not mono-threaded!";
const char kAssertThreadDoesNotAppearInProcFS[] =
    "Started thread does not appear in /proc";
const char kAssertThreadDoesNotDisappearInProcFS[] =
    "Stopped thread does not disappear in /proc";

bool IsSingleThreadedImpl(int proc_fd) {
  CHECK_LE(0, proc_fd);
  struct stat task_stat;
  int fstat_ret = fstatat(proc_fd, "self/task/", &task_stat, 0);
  PCHECK(0 == fstat_ret);

  // At least "..", "." and the current thread should be present.
  CHECK_LE(3UL, task_stat.st_nlink);
  // Counting threads via /proc/self/task could be racy. For the purpose of
  // determining if the current proces is monothreaded it works: if at any
  // time it becomes monothreaded, it'll stay so.
  return task_stat.st_nlink == 3;
}

bool IsThreadPresentInProcFS(int proc_fd,
                             const std::string& thread_id_dir_str) {
  struct stat task_stat;
  const int fstat_ret =
      fstatat(proc_fd, thread_id_dir_str.c_str(), &task_stat, 0);
  if (fstat_ret < 0) {
    PCHECK(ENOENT == errno);
    return false;
  }
  return true;
}

bool IsNotThreadPresentInProcFS(int proc_fd,
                                const std::string& thread_id_dir_str) {
  return !IsThreadPresentInProcFS(proc_fd, thread_id_dir_str);
}

// Run |cb| in a loop until it returns false. Every time |cb| runs, sleep
// for an exponentially increasing amount of time. |cb| is expected to return
// false very quickly and this will crash if it doesn't happen within ~64ms on
// Debug builds (2s on Release builds).
// This is guaranteed to not sleep more than twice as much as the bare minimum
// amount of time.
void RunWhileTrue(const base::RepeatingCallback<bool(void)>& cb,
                  const char* message) {
#if defined(NDEBUG)
  // In Release mode, crash after 30 iterations, which means having spent
  // roughly 2s in
  // nanosleep(2) cumulatively.
  const unsigned int kMaxIterations = 30U;
#else
  // In practice, this never goes through more than a couple iterations. In
  // debug mode, crash after 64ms (+ eventually 25 times the granularity of
  // the clock) in nanosleep(2). This ensures that this is not becoming too
  // slow.
  const unsigned int kMaxIterations = 25U;
#endif

  // Run |cb| with an exponential back-off, sleeping 2^iterations nanoseconds
  // in nanosleep(2).
  // Note: the clock may not allow for nanosecond granularity, in this case the
  // first iterations would sleep a tiny bit more instead, which would not
  // change the calculations significantly.
  for (unsigned int i = 0; i < kMaxIterations; ++i) {
    if (!cb.Run()) {
      return;
    }

    // Increase the waiting time exponentially.
    struct timespec ts = {0, 1L << i /* nanoseconds */};
    PCHECK(0 == HANDLE_EINTR(nanosleep(&ts, &ts)));
  }

  LOG(FATAL) << message << " (iterations: " << kMaxIterations << ")";
}

bool IsMultiThreaded(int proc_fd) {
  return !ThreadHelpers::IsSingleThreaded(proc_fd);
}

enum class ThreadAction { Start, Stop };

bool ChangeThreadStateAndWatchProcFS(
    int proc_fd, base::Thread* thread, ThreadAction action) {
  DCHECK_LE(0, proc_fd);
  DCHECK(thread);
  DCHECK(action == ThreadAction::Start || action == ThreadAction::Stop);

  base::RepeatingCallback<bool(void)> cb;
  const char* message;

  if (action == ThreadAction::Start) {
    // Should start the thread before calling thread_id().
    if (!thread->Start())
      return false;
  }

  const base::PlatformThreadId thread_id = thread->GetThreadId();
  const std::string thread_id_dir_str =
      "self/task/" + base::NumberToString(thread_id) + "/";

  if (action == ThreadAction::Stop) {
    // The target thread should exist in /proc.
    DCHECK(IsThreadPresentInProcFS(proc_fd, thread_id_dir_str));
    thread->Stop();
  }

  // The kernel is at liberty to wake the thread id futex before updating
  // /proc. Start() above or following Stop(), the thread is started or joined,
  // but entries in /proc may not have been updated.
  if (action == ThreadAction::Start) {
    cb = base::BindRepeating(&IsNotThreadPresentInProcFS, proc_fd,
                             thread_id_dir_str);
    message = kAssertThreadDoesNotAppearInProcFS;
  } else {
    cb = base::BindRepeating(&IsThreadPresentInProcFS, proc_fd,
                             thread_id_dir_str);
    message = kAssertThreadDoesNotDisappearInProcFS;
  }
  RunWhileTrue(cb, message);

  DCHECK_EQ(action == ThreadAction::Start,
            IsThreadPresentInProcFS(proc_fd, thread_id_dir_str));

  return true;
}

}  // namespace

// static
bool ThreadHelpers::IsSingleThreaded(int proc_fd) {
  DCHECK_LE(0, proc_fd);
  return IsSingleThreadedImpl(proc_fd);
}

// static
bool ThreadHelpers::IsSingleThreaded() {
  base::ScopedFD task_fd(ProcUtil::OpenProc());
  return IsSingleThreaded(task_fd.get());
}

// static
void ThreadHelpers::AssertSingleThreaded(int proc_fd) {
  DCHECK_LE(0, proc_fd);
  const base::RepeatingCallback<bool(void)> cb =
      base::BindRepeating(&IsMultiThreaded, proc_fd);
  RunWhileTrue(cb, kAssertSingleThreadedError);
}

void ThreadHelpers::AssertSingleThreaded() {
  base::ScopedFD task_fd(ProcUtil::OpenProc());
  AssertSingleThreaded(task_fd.get());
}

// static
bool ThreadHelpers::StartThreadAndWatchProcFS(int proc_fd,
                                              base::Thread* thread) {
  return ChangeThreadStateAndWatchProcFS(proc_fd, thread, ThreadAction::Start);
}

// static
bool ThreadHelpers::StopThreadAndWatchProcFS(int proc_fd,
                                             base::Thread* thread) {
  return ChangeThreadStateAndWatchProcFS(proc_fd, thread, ThreadAction::Stop);
}

// static
const char* ThreadHelpers::GetAssertSingleThreadedErrorMessageForTests() {
  return kAssertSingleThreadedError;
}

}  // namespace sandbox

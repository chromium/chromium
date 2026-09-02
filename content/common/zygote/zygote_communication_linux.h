// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ZYGOTE_ZYGOTE_COMMUNICATION_LINUX_H_
#define CONTENT_COMMON_ZYGOTE_ZYGOTE_COMMUNICATION_LINUX_H_

#include <sys/types.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/platform_file.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "content/common/content_export.h"

namespace base {
class CommandLine;
class Pickle;
}  // namespace base

namespace content {

// Completes the launch of a zygote: blocks until the zygote process is ready to
// accept commands and returns its pid.
using ZygoteLaunchCompletionCallback = base::OnceCallback<pid_t()>;

// Starts a zygote process for |cmd_line| and provides the control fd for it.
// The returned callback must be run before the zygote is used.
using ZygoteLaunchCallback =
    base::OnceCallback<ZygoteLaunchCompletionCallback(base::CommandLine*,
                                                      base::ScopedFD*)>;

// Handles interprocess communication with the Linux zygote process. The zygote
// does not use standard Chrome IPC or mojo, see:
// https://chromium.googlesource.com/chromium/src/+/main/docs/linux/sandbox_ipc.md
class CONTENT_EXPORT ZygoteCommunication {
 public:
  enum class ZygoteType { kSandboxed, kUnsandboxed };
  explicit ZygoteCommunication(ZygoteType type);
  ~ZygoteCommunication();

  // Launches the zygote process. Waiting for it to finish starting up is
  // deferred until the zygote is first used, or EnsureLaunchFinished().
  void Init(ZygoteLaunchCallback launcher);

  // Blocks until the zygote process has finished starting up.
  void EnsureLaunchFinished();

  // Reinitialize logging. Needed on ChromeOS, which switches to a log file
  // in the user's home directory once they log in.
  void ReinitializeLogging(uint32_t logging_dest,
                           base::PlatformFile log_file_fd);

  // Tries to start a process of type indicated by process_type.
  // Returns its pid on success, otherwise base::kNullProcessHandle;
  pid_t ForkRequest(const std::vector<std::string>& command_line,
                    const base::FileHandleMappingVector& mapping,
                    const std::string& process_type);

  void EnsureProcessTerminated(pid_t process);

  // Should be called every time a Zygote child died.
  void ZygoteChildDied(pid_t process);

  // Get the termination status (and, optionally, the exit code) of
  // the process. |exit_code| is set to the exit code of the child
  // process. (|exit_code| may be NULL.)
  // Unfortunately the Zygote can not accurately figure out if a process
  // is already dead without waiting synchronously for it.
  // |known_dead| should be set to true when we already know that the process
  // is dead. When |known_dead| is false, processes could be seen as
  // still running, even when they're not. When |known_dead| is true, the
  // process will be SIGKILL-ed first (which should have no effect if it was
  // really dead). This is to prevent a waiting waitpid() from blocking in
  // a single-threaded Zygote. See https://crbug.com/157458.
  base::TerminationStatus GetTerminationStatus(base::ProcessHandle handle,
                                               bool known_dead,
                                               int* exit_code);

  // Returns the sandbox status of this zygote.
  int GetSandboxStatus();

 private:
  // Should be called every time a Zygote child is born.
  void ZygoteChildBorn(pid_t process);

  // Read the reply from the zygote.
  ssize_t ReadReply(void* buf, size_t buf_len);

  // Sends |data| to the zygote via |control_fd_|.  If |fds| is non-NULL, the
  // included file descriptors will also be passed.  The caller is responsible
  // for acquiring |control_lock_|.
  bool SendMessage(const base::Pickle& data, const std::vector<int>* fds);

  // Get the sandbox status from the zygote.
  ssize_t ReadSandboxStatus();

  // Same as EnsureLaunchFinished(), for callers that hold |control_lock_|.
  void EnsureLaunchFinishedLocked() EXCLUSIVE_LOCKS_REQUIRED(control_lock_);

  // Indicates whether the Zygote starts unsandboxed or not.
  const ZygoteType type_;

  base::ScopedFD control_fd_;  // the socket to the zygote.
  // A lock protecting all communication with the zygote. This lock must be
  // acquired before sending a command and released after the result has been
  // received.
  base::Lock control_lock_;
  // Completes the launch of the zygote process; null once it has run.
  ZygoteLaunchCompletionCallback finish_launch_ GUARDED_BY(control_lock_);
  // The pid of the zygote.
  pid_t pid_;
  // The list of running zygote children.
  std::set<pid_t> list_of_running_zygote_children_;
  // The lock to guard the list of running zygote children.
  base::Lock child_tracking_lock_;
  int sandbox_status_;
  bool have_read_sandbox_status_word_;
  // Set to true when the zygote is initialized successfully.
  bool init_;
};

}  // namespace content

#endif  // CONTENT_COMMON_ZYGOTE_ZYGOTE_COMMUNICATION_LINUX_H_

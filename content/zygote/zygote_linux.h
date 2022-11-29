// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ZYGOTE_ZYGOTE_LINUX_H_
#define CONTENT_ZYGOTE_ZYGOTE_LINUX_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/small_map.h"
#include "base/files/platform_file.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/posix/global_descriptors.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"

namespace base {
class PickleIterator;
}

namespace content {

class ZygoteForkDelegate;

// This is the object which implements the zygote. The ZygoteMain function,
// which is called from ChromeMain, simply constructs one of these objects and
// runs it.
class Zygote {
 public:
  Zygote(int sandbox_flags,
         std::vector<std::unique_ptr<ZygoteForkDelegate>> helpers,
         const base::GlobalDescriptors::Descriptor& ipc_backchannel);
  ~Zygote();

  bool ProcessRequests();

 private:
  struct ZygoteProcessInfo {
    // Pid from inside the Zygote's PID namespace.
    base::ProcessHandle internal_pid;
    // Keeps track of which fork delegate helper the process was started from.
    raw_ptr<ZygoteForkDelegate> started_from_helper;
    // Records when the browser requested the zygote to reap this process.
    base::TimeTicks time_of_reap_request;
    // Notes whether the zygote has sent SIGKILL to this process.
    bool sent_sigkill;
  };
  using ZygoteProcessMap =
      base::small_map<std::map<base::ProcessHandle, ZygoteProcessInfo>>;

  // Retrieve a ZygoteProcessInfo from the process_info_map_.
  // Returns true and write to process_info if |pid| can be found, return
  // false otherwise.
  bool GetProcessInfo(base::ProcessHandle pid, ZygoteProcessInfo* process_info);

  // Returns true if the SUID sandbox is active.
  bool UsingSUIDSandbox() const;
  // Returns true if the NS sandbox is active.
  bool UsingNSSandbox() const;

  // ---------------------------------------------------------------------------
  // Requests from the browser...

  // Read and process a request from the browser. Returns true if we are in a
  // new process and thus need to unwind back into ChromeMain.
  bool HandleRequestFromBrowser(int fd);

  void HandleReapRequest(int fd, base::PickleIterator iter);

  // Get the termination status of |real_pid|. |real_pid| is the PID as it
  // appears outside of the sandbox.
  // Return true if it managed to get the termination status and return the
  // status in |status| and the exit code in |exit_code|.
  bool GetTerminationStatus(base::ProcessHandle real_pid,
                            bool known_dead,
                            base::TerminationStatus* status,
                            int* exit_code);

  void HandleGetTerminationStatus(int fd, base::PickleIterator iter);

  // This is equivalent to fork(), except that, when using the SUID sandbox, it
  // returns the real PID of the child process as it appears outside the
  // sandbox, rather than returning the PID inside the sandbox.  The child's
  // real PID is determined by having it call
  // service_manager::SendZygoteChildPing(int) using the |pid_oracle|
  // descriptor.
  // Finally, when using a ZygoteForkDelegate helper, |uma_name|, |uma_sample|,
  // and |uma_boundary_value| may be set if the helper wants to make a UMA
  // report via UMA_HISTOGRAM_ENUMERATION.
  int ForkWithRealPid(const std::string& process_type,
                      const std::vector<std::string>& args,
                      const base::GlobalDescriptors::Mapping& fd_mapping,
                      base::ScopedFD pid_oracle,
                      std::string* uma_name,
                      int* uma_sample,
                      int* uma_boundary_value);

  // Unpacks process type and arguments from |iter| and forks a new process.
  // Returns -1 on error, otherwise returns twice, returning 0 to the child
  // process and the child process ID to the parent process, like fork().
  base::ProcessId ReadArgsAndFork(base::PickleIterator iter,
                                  std::vector<base::ScopedFD> fds,
                                  std::string* uma_name,
                                  int* uma_sample,
                                  int* uma_boundary_value);

  // Handle a 'fork' request from the browser: this means that the browser
  // wishes to start a new renderer. Returns true if we are in a new process,
  // otherwise writes the child_pid back to the browser via |fd|. Writes a
  // child_pid of -1 on error.
  bool HandleForkRequest(int fd,
                         base::PickleIterator iter,
                         std::vector<base::ScopedFD> fds);

  bool HandleGetSandboxStatus(int fd, base::PickleIterator iter);

  // Handle a logging reinitialization request from the browser.
  // Needed on ChromeOS, which switches to a log file in the user's
  // home directory once they log in.
  void HandleReinitializeLoggingRequest(base::PickleIterator iter,
                                        std::vector<base::ScopedFD> fds);

  // Attempt to reap the child process by calling waitpid, and return
  // whether successful.  If the process has not terminated within
  // 2 seconds of its reap request, send it SIGKILL.
  bool ReapChild(const base::TimeTicks& now, ZygoteProcessInfo* child);

  // Attempt to reap all outstanding children in |to_reap_|.
  void ReapChildren();

  // The Zygote needs to keep some information about each process. Most
  // notably what the PID of the process is inside the PID namespace of
  // the Zygote and whether or not a process was started by the
  // ZygoteForkDelegate helper.
  ZygoteProcessMap process_info_map_;

  const int sandbox_flags_;
  std::vector<std::unique_ptr<ZygoteForkDelegate>> helpers_;

  // Count of how many fork delegates for which we've invoked InitialUMA().
  size_t initial_uma_index_;

  // The vector contains the child processes that need to be reaped.
  std::vector<ZygoteProcessInfo> to_reap_;

  // Sandbox IPC channel for renderers to invoke services from the browser. See
  // https://chromium.googlesource.com/chromium/src/+/main/docs/linux/sandbox_ipc.md
  base::GlobalDescriptors::Descriptor ipc_backchannel_;
};

}  // namespace content

#endif  // CONTENT_ZYGOTE_ZYGOTE_LINUX_H_

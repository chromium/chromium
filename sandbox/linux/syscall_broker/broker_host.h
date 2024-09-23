// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_BROKER_HOST_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_BROKER_HOST_H_

#include <optional>

#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "sandbox/linux/syscall_broker/broker_channel.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_sandbox_config.h"

namespace sandbox {

namespace syscall_broker {

class BrokerSimpleMessage;

// The BrokerHost class should be embedded in a (presumably not sandboxed)
// process. It will honor IPC requests from a BrokerClient sent over
// |ipc_channel| according to |broker_permission_list|.
class BrokerHost {
 public:
  BrokerHost(const BrokerSandboxConfig& policy,
             BrokerChannel::EndPoint ipc_channel,
             pid_t sandboxed_process_pid);

  BrokerHost(const BrokerHost&) = delete;
  BrokerHost& operator=(const BrokerHost&) = delete;

  ~BrokerHost();

  // Receive system call requests and handle them forevermore.
  void LoopAndHandleRequests();

 private:
  [[nodiscard]] std::optional<std::string> RewritePathname(
      const char* pathname);
  [[nodiscard]] std::optional<std::pair<const char*, int>> GetPathAndFlags(
      BrokerSimpleMessage* message);

  void AccessFileForIPC(const char* requested_filename,
                        int mode,
                        BrokerSimpleMessage* reply);
  void MkdirFileForIPC(const char* requested_filename,
                       int mode,
                       BrokerSimpleMessage* reply);
  void OpenFileForIPC(const char* requested_filename,
                      int flags,
                      BrokerSimpleMessage* reply,
                      base::ScopedFD* opened_file);
  void RenameFileForIPC(const char* old_filename,
                        const char* new_filename,
                        BrokerSimpleMessage* reply);
  void ReadlinkFileForIPC(const char* filename, BrokerSimpleMessage* reply);
  void RmdirFileForIPC(const char* requested_filename,
                       BrokerSimpleMessage* reply);
  void StatFileForIPC(BrokerCommand command_type,
                      const char* requested_filename,
                      bool follow_links,
                      BrokerSimpleMessage* reply);
  void UnlinkFileForIPC(const char* requested_filename,
                        BrokerSimpleMessage* message);
  void InotifyAddWatchForIPC(base::ScopedFD inotify_fd,
                             const char* requested_filename,
                             uint32_t mask,
                             BrokerSimpleMessage* message);

  bool HandleRemoteCommand(BrokerSimpleMessage* message,
                           base::span<base::ScopedFD> recv_fds,
                           BrokerSimpleMessage* reply,
                           base::ScopedFD* opened_file);

  const raw_ref<const BrokerSandboxConfig> policy_;
  const BrokerChannel::EndPoint ipc_channel_;
  const pid_t sandboxed_process_pid_;
};

}  // namespace syscall_broker

}  // namespace sandbox

#endif  //  SANDBOX_LINUX_SYSCALL_BROKER_BROKER_HOST_H_

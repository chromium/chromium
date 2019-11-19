// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/zygote/zygote_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/global_descriptors.h"
#include "base/posix/unix_domain_socket.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "sandbox/linux/services/credentials.h"
#include "sandbox/linux/services/namespace_sandbox.h"
#include "services/service_manager/embedder/descriptors.h"
#include "services/service_manager/embedder/result_codes.h"
#include "services/service_manager/embedder/set_process_title.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/sandbox/linux/sandbox_linux.h"
#include "services/service_manager/sandbox/sandbox.h"
#include "services/service_manager/zygote/common/send_zygote_child_ping_linux.h"
#include "services/service_manager/zygote/common/zygote_commands_linux.h"
#include "services/service_manager/zygote/common/zygote_fork_delegate_linux.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

// See
// https://chromium.googlesource.com/chromium/src/+/master/docs/linux_zygote.md

namespace service_manager {

namespace {

// NOP function. See below where this handler is installed.
void SIGCHLDHandler(int signal) {}

int LookUpFd(const base::GlobalDescriptors::Mapping& fd_mapping, uint32_t key) {
  for (size_t index = 0; index < fd_mapping.size(); ++index) {
    if (fd_mapping[index].key == key)
      return fd_mapping[index].fd;
  }
  return -1;
}

void KillAndReap(pid_t pid, ZygoteForkDelegate* helper) {
  if (helper) {
    // Helper children may be forked in another PID namespace, so |pid| might
    // be meaningless to us; or we just might not be able to directly send it
    // signals.  So we can't kill it.
    // Additionally, we're not its parent, so we can't reap it anyway.
    // TODO(mdempsky): Extend the ZygoteForkDelegate API to handle this.
    LOG(WARNING) << "Unable to kill or reap helper children";
    return;
  }

  // Kill the child process in case it's not already dead, so we can safely
  // perform a blocking wait.
  PCHECK(0 == kill(pid, SIGKILL));
  PCHECK(pid == HANDLE_EINTR(waitpid(pid, nullptr, 0)));
}

}  // namespace

Zygote::Zygote(int sandbox_flags,
               std::vector<std::unique_ptr<ZygoteForkDelegate>> helpers,
               const base::GlobalDescriptors::Descriptor& ipc_backchannel)
    : sandbox_flags_(sandbox_flags),
      helpers_(std::move(helpers)),
      initial_uma_index_(0),
      to_reap_(),
      ipc_backchannel_(ipc_backchannel) {}

Zygote::~Zygote() {}

bool Zygote::ProcessRequests() {
  // A SOCK_SEQPACKET socket is installed in fd 3. We get commands from the
  // browser on it.
  // A SOCK_DGRAM is installed in fd 5. This is the sandbox IPC channel.
  // See
  // https://chromium.googlesource.com/chromium/src/+/master/docs/linux_sandbox_ipc.md

  // We need to accept SIGCHLD, even though our handler is a no-op because
  // otherwise we cannot wait on children. (According to POSIX 2001.)
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = &SIGCHLDHandler;
  PCHECK(sigaction(SIGCHLD, &action, nullptr) == 0);

  // Block SIGCHLD until a child might be ready to reap.
  sigset_t sigset;
  sigset_t orig_sigmask;
  PCHECK(sigemptyset(&sigset) == 0);
  PCHECK(sigaddset(&sigset, SIGCHLD) == 0);
  PCHECK(sigprocmask(SIG_BLOCK, &sigset, &orig_sigmask) == 0);

  if (UsingSUIDSandbox() || UsingNSSandbox()) {
    // Let the ZygoteHost know we are ready to go.
    // The receiving code is in
    // content/browser/zygote_host/zygote_host_impl_linux.cc.
    bool r = base::UnixDomainSocket::SendMsg(
        kZygoteSocketPairFd, kZygoteHelloMessage, sizeof(kZygoteHelloMessage),
        std::vector<int>());
#if defined(OS_CHROMEOS)
    LOG_IF(WARNING, !r) << "Sending zygote magic failed";
    // Exit normally on chromeos because session manager may send SIGTERM
    // right after the process starts and it may fail to send zygote magic
    // number to browser process.
    if (!r)
      _exit(service_manager::RESULT_CODE_NORMAL_EXIT);
#else
    CHECK(r) << "Sending zygote magic failed";
#endif
  }

  sigset_t ppoll_sigmask = orig_sigmask;
  PCHECK(sigdelset(&ppoll_sigmask, SIGCHLD) == 0);
  struct pollfd pfd;
  pfd.fd = kZygoteSocketPairFd;
  pfd.events = POLLIN;

  struct timespec timeout;
  timeout.tv_sec = 2;
  timeout.tv_nsec = 0;

  for (;;) {
    struct timespec* timeout_ptr = nullptr;
    if (!to_reap_.empty())
      timeout_ptr = &timeout;
    int rc = ppoll(&pfd, 1, timeout_ptr, &ppoll_sigmask);
    PCHECK(rc >= 0 || errno == EINTR);
    ReapChildren();

    if (pfd.revents & POLLIN) {
      // This function call can return multiple times, once per fork().
      if (HandleRequestFromBrowser(kZygoteSocketPairFd)) {
        PCHECK(sigprocmask(SIG_SETMASK, &orig_sigmask, nullptr) == 0);
        return true;
      }
    }
  }
  // The loop should not be exited unless a request was successfully processed.
  NOTREACHED();
  return false;
}

bool Zygote::ReapChild(const base::TimeTicks& now, ZygoteProcessInfo* child) {
  pid_t pid = child->internal_pid;
  pid_t r = HANDLE_EINTR(waitpid(pid, nullptr, WNOHANG));
  if (r > 0) {
    if (r != pid) {
      DLOG(ERROR) << "While waiting for " << pid
                  << " to terminate, "
                     "waitpid returned "
                  << r;
    }
    return r == pid;
  }
  if ((now - child->time_of_reap_request).InSeconds() < 2) {
    return false;
  }
  // If the process has been requested reaped >= 2 seconds ago, kill it.
  if (!child->sent_sigkill) {
    if (kill(pid, SIGKILL) != 0)
      DPLOG(ERROR) << "Sending SIGKILL to process " << pid << " failed";

    child->sent_sigkill = true;
  }
  return false;
}

void Zygote::ReapChildren() {
  base::TimeTicks now = base::TimeTicks::Now();
  std::vector<ZygoteProcessInfo>::iterator it = to_reap_.begin();
  while (it != to_reap_.end()) {
    if (ReapChild(now, &(*it))) {
      it = to_reap_.erase(it);
    } else {
      it++;
    }
  }
}

bool Zygote::GetProcessInfo(base::ProcessHandle pid,
                            ZygoteProcessInfo* process_info) {
  DCHECK(process_info);
  const ZygoteProcessMap::const_iterator it = process_info_map_.find(pid);
  if (it == process_info_map_.end()) {
    return false;
  }
  *process_info = it->second;
  return true;
}

bool Zygote::UsingSUIDSandbox() const {
  return sandbox_flags_ & service_manager::SandboxLinux::kSUID;
}

bool Zygote::UsingNSSandbox() const {
  return sandbox_flags_ & service_manager::SandboxLinux::kUserNS;
}

bool Zygote::HandleRequestFromBrowser(int fd) {
  std::vector<base::ScopedFD> fds;
  char buf[kZygoteMaxMessageLength];
  const ssize_t len =
      base::UnixDomainSocket::RecvMsg(fd, buf, sizeof(buf), &fds);

  if (len == 0 || (len == -1 && errno == ECONNRESET)) {
    // EOF from the browser. We should die.
    // TODO(eugenis): call __sanititizer_cov_dump() here to obtain code
    // coverage for the Zygote. Currently it's not possible because of
    // confusion over who is responsible for closing the file descriptor.
    _exit(0);
    return false;
  }

  if (len == -1) {
    PLOG(ERROR) << "Error reading message from browser";
    return false;
  }

  base::Pickle pickle(buf, len);
  base::PickleIterator iter(pickle);

  int kind;
  if (iter.ReadInt(&kind)) {
    switch (kind) {
      case kZygoteCommandFork:
        // This function call can return multiple times, once per fork().
        return HandleForkRequest(fd, iter, std::move(fds));

      case kZygoteCommandReap:
        if (!fds.empty())
          break;
        HandleReapRequest(fd, iter);
        return false;
      case kZygoteCommandGetTerminationStatus:
        if (!fds.empty())
          break;
        HandleGetTerminationStatus(fd, iter);
        return false;
      case kZygoteCommandGetSandboxStatus:
        HandleGetSandboxStatus(fd, iter);
        return false;
      case kZygoteCommandForkRealPID:
        // This shouldn't happen in practice, but some failure paths in
        // HandleForkRequest (e.g., if ReadArgsAndFork fails during depickling)
        // could leave this command pending on the socket.
        LOG(ERROR) << "Unexpected real PID message from browser";
        NOTREACHED();
        return false;
      default:
        NOTREACHED();
        break;
    }
  }

  LOG(WARNING) << "Error parsing message from browser";
  return false;
}

void Zygote::HandleReapRequest(int fd, base::PickleIterator iter) {
  base::ProcessId child;

  if (!iter.ReadInt(&child)) {
    LOG(WARNING) << "Error parsing reap request from browser";
    return;
  }

  ZygoteProcessInfo child_info;
  if (!GetProcessInfo(child, &child_info)) {
    LOG(ERROR) << "Child not found!";
    NOTREACHED();
    return;
  }
  child_info.time_of_reap_request = base::TimeTicks::Now();

  if (!child_info.started_from_helper) {
    to_reap_.push_back(child_info);
  } else {
    // For processes from the helper, send a GetTerminationStatus request
    // with known_dead set to true.
    // This is not perfect, as the process may be killed instantly, but is
    // better than ignoring the request.
    base::TerminationStatus status;
    int exit_code;
    bool got_termination_status =
        GetTerminationStatus(child, true /* known_dead */, &status, &exit_code);
    DCHECK(got_termination_status);
  }
  process_info_map_.erase(child);
}

bool Zygote::GetTerminationStatus(base::ProcessHandle real_pid,
                                  bool known_dead,
                                  base::TerminationStatus* status,
                                  int* exit_code) {
  ZygoteProcessInfo child_info;
  if (!GetProcessInfo(real_pid, &child_info)) {
    LOG(ERROR) << "Zygote::GetTerminationStatus for unknown PID " << real_pid;
    NOTREACHED();
    return false;
  }
  // We know about |real_pid|.
  const base::ProcessHandle child = child_info.internal_pid;
  if (child_info.started_from_helper) {
    if (!child_info.started_from_helper->GetTerminationStatus(
            child, known_dead, status, exit_code)) {
      return false;
    }
  } else {
    // Handle the request directly.
    if (known_dead) {
      *status = base::GetKnownDeadTerminationStatus(child, exit_code);
    } else {
      // We don't know if the process is dying, so get its status but don't
      // wait.
      *status = base::GetTerminationStatus(child, exit_code);
    }
  }
  // Successfully got a status for |real_pid|.
  if (*status != base::TERMINATION_STATUS_STILL_RUNNING) {
    // Time to forget about this process.
    process_info_map_.erase(real_pid);
  }

  if (WIFEXITED(*exit_code)) {
    const int exit_status = WEXITSTATUS(*exit_code);
    if (exit_status == sandbox::NamespaceSandbox::SignalExitCode(SIGINT) ||
        exit_status == sandbox::NamespaceSandbox::SignalExitCode(SIGTERM)) {
      *status = base::TERMINATION_STATUS_PROCESS_WAS_KILLED;
    }
  }

  return true;
}

void Zygote::HandleGetTerminationStatus(int fd, base::PickleIterator iter) {
  bool known_dead;
  base::ProcessHandle child_requested;

  if (!iter.ReadBool(&known_dead) || !iter.ReadInt(&child_requested)) {
    LOG(WARNING) << "Error parsing GetTerminationStatus request "
                 << "from browser";
    return;
  }

  base::TerminationStatus status;
  int exit_code;

  bool got_termination_status =
      GetTerminationStatus(child_requested, known_dead, &status, &exit_code);
  if (!got_termination_status) {
    // Assume that if we can't find the child in the sandbox, then
    // it terminated normally.
    NOTREACHED();
    status = base::TERMINATION_STATUS_NORMAL_TERMINATION;
    exit_code = service_manager::RESULT_CODE_NORMAL_EXIT;
  }

  base::Pickle write_pickle;
  write_pickle.WriteInt(static_cast<int>(status));
  write_pickle.WriteInt(exit_code);
  ssize_t written =
      HANDLE_EINTR(write(fd, write_pickle.data(), write_pickle.size()));
  if (written != static_cast<ssize_t>(write_pickle.size()))
    PLOG(ERROR) << "write";
}

int Zygote::ForkWithRealPid(const std::string& process_type,
                            const base::GlobalDescriptors::Mapping& fd_mapping,
                            const std::string& channel_id,
                            base::ScopedFD pid_oracle,
                            std::string* uma_name,
                            int* uma_sample,
                            int* uma_boundary_value) {
  ZygoteForkDelegate* helper = nullptr;
  for (auto i = helpers_.begin(); i != helpers_.end(); ++i) {
    if ((*i)->CanHelp(process_type, uma_name, uma_sample, uma_boundary_value)) {
      helper = i->get();
      break;
    }
  }

  base::ScopedFD read_pipe, write_pipe;
  base::ProcessId pid = 0;
  if (helper) {
    int mojo_channel_fd = LookUpFd(fd_mapping, kMojoIPCChannel);
    if (mojo_channel_fd < 0) {
      DLOG(ERROR) << "Failed to find kMojoIPCChannel in FD mapping";
      return -1;
    }
    std::vector<int> fds;
    fds.push_back(mojo_channel_fd);   // kBrowserFDIndex
    fds.push_back(pid_oracle.get());  // kPIDOracleFDIndex
    pid = helper->Fork(process_type, fds, channel_id);

    // Helpers should never return in the child process.
    CHECK_NE(pid, 0);
  } else {
    PCHECK(base::CreatePipe(&read_pipe, &write_pipe));
    if (sandbox_flags_ & service_manager::SandboxLinux::kPIDNS &&
        sandbox_flags_ & service_manager::SandboxLinux::kUserNS) {
      pid = sandbox::NamespaceSandbox::ForkInNewPidNamespace(
          /*drop_capabilities_in_child=*/true);
    } else {
      pid = sandbox::Credentials::ForkAndDropCapabilitiesInChild();
    }
  }

  if (pid == 0) {
    // In the child process.

    // If the process is the init process inside a PID namespace, it must have
    // explicit signal handlers.
    if (getpid() == 1) {
      static const int kTerminationSignals[] = {
          SIGINT, SIGTERM, SIGHUP, SIGQUIT, SIGABRT, SIGPIPE, SIGUSR1, SIGUSR2};
      for (const int sig : kTerminationSignals) {
        sandbox::NamespaceSandbox::InstallTerminationSignalHandler(
            sig, sandbox::NamespaceSandbox::SignalExitCode(sig));
      }
    }

    write_pipe.reset();

    // Ping the PID oracle socket so the browser can find our PID.
    CHECK(SendZygoteChildPing(pid_oracle.get()));

    // Now read back our real PID from the zygote.
    base::ProcessId real_pid;
    if (!base::ReadFromFD(read_pipe.get(), reinterpret_cast<char*>(&real_pid),
                          sizeof(real_pid))) {
      LOG(FATAL) << "Failed to synchronise with parent zygote process";
    }
    if (real_pid <= 0) {
      LOG(FATAL) << "Invalid pid from parent zygote";
    }
    // Sandboxed processes need to send the global, non-namespaced PID when
    // setting up an IPC channel to their parent.
    IPC::Channel::SetGlobalPid(real_pid);
    // Force the real PID so chrome event data have a PID that corresponds
    // to system trace event data.
    base::trace_event::TraceLog::GetInstance()->SetProcessID(
        static_cast<int>(real_pid));
    base::InitUniqueIdForProcessInPidNamespace(real_pid);
    return 0;
  }

  // In the parent process.
  if (pid < 0) {
    // Fork failed.
    return -1;
  }

  read_pipe.reset();
  pid_oracle.reset();

  // Always receive a real PID from the zygote host, though it might
  // be invalid (see below).
  base::ProcessId real_pid = -1;
  {
    std::vector<base::ScopedFD> recv_fds;
    char buf[kZygoteMaxMessageLength];
    const ssize_t len = base::UnixDomainSocket::RecvMsg(
        kZygoteSocketPairFd, buf, sizeof(buf), &recv_fds);

    if (len > 0) {
      CHECK(recv_fds.empty());

      base::Pickle pickle(buf, len);
      base::PickleIterator iter(pickle);

      int kind;
      CHECK(iter.ReadInt(&kind));
      CHECK(kind == kZygoteCommandForkRealPID);
      CHECK(iter.ReadInt(&real_pid));
    }
  }

  // If we successfully forked a child, but it crashed without sending
  // a message to the browser, the browser won't have found its PID.
  if (real_pid < 0) {
    KillAndReap(pid, helper);
    return -1;
  }

  // If we're not using a helper, send the PID back to the child process.
  if (!helper) {
    ssize_t written =
        HANDLE_EINTR(write(write_pipe.get(), &real_pid, sizeof(real_pid)));
    if (written != sizeof(real_pid)) {
      KillAndReap(pid, helper);
      return -1;
    }
  }

  // Now set-up this process to be tracked by the Zygote.
  if (process_info_map_.find(real_pid) != process_info_map_.end()) {
    LOG(ERROR) << "Already tracking PID " << real_pid;
    NOTREACHED();
  }
  process_info_map_[real_pid].internal_pid = pid;
  process_info_map_[real_pid].started_from_helper = helper;

  return real_pid;
}

base::ProcessId Zygote::ReadArgsAndFork(base::PickleIterator iter,
                                        std::vector<base::ScopedFD> fds,
                                        std::string* uma_name,
                                        int* uma_sample,
                                        int* uma_boundary_value) {
  std::vector<std::string> args;
  int argc = 0;
  int numfds = 0;
  base::GlobalDescriptors::Mapping mapping;
  std::string process_type;
  std::string channel_id;
  const std::string channel_id_prefix =
      std::string("--") +
      service_manager::switches::kServiceRequestChannelToken + std::string("=");

  if (!iter.ReadString(&process_type))
    return -1;
  if (!iter.ReadInt(&argc))
    return -1;

  for (int i = 0; i < argc; ++i) {
    std::string arg;
    if (!iter.ReadString(&arg))
      return -1;
    args.push_back(arg);
    if (arg.compare(0, channel_id_prefix.length(), channel_id_prefix) == 0)
      channel_id = arg.substr(channel_id_prefix.length());
  }

  // timezone_id is obtained from ICU in zygote host so that it can't be
  // invalid. For an unknown reason, if an invalid ID is passed down here, the
  // worst result would be that timezone would be set to Etc/Unknown.
  base::string16 timezone_id;
  if (!iter.ReadString16(&timezone_id))
    return -1;
  icu::TimeZone::adoptDefault(icu::TimeZone::createTimeZone(
      icu::UnicodeString(FALSE, timezone_id.data(), timezone_id.length())));

  if (!iter.ReadInt(&numfds))
    return -1;
  if (numfds != static_cast<int>(fds.size()))
    return -1;

  // First FD is the PID oracle socket.
  if (fds.size() < 1)
    return -1;
  base::ScopedFD pid_oracle(std::move(fds[0]));

  // Remaining FDs are for the global descriptor mapping.
  for (int i = 1; i < numfds; ++i) {
    base::GlobalDescriptors::Key key;
    if (!iter.ReadUInt32(&key))
      return -1;
    mapping.push_back(base::GlobalDescriptors::Descriptor(key, fds[i].get()));
  }

  mapping.push_back(ipc_backchannel_);

  // Returns twice, once per process.
  base::ProcessId child_pid =
      ForkWithRealPid(process_type, mapping, channel_id, std::move(pid_oracle),
                      uma_name, uma_sample, uma_boundary_value);
  if (!child_pid) {
    // This is the child process.

    // Our socket from the browser.
    PCHECK(0 == IGNORE_EINTR(close(kZygoteSocketPairFd)));

    // Pass ownership of file descriptors from fds to GlobalDescriptors.
    for (base::ScopedFD& fd : fds)
      ignore_result(fd.release());
    base::GlobalDescriptors::GetInstance()->Reset(mapping);

    // Reset the process-wide command line to our new command line.
    base::CommandLine::Reset();
    base::CommandLine::Init(0, nullptr);
    base::CommandLine::ForCurrentProcess()->InitFromArgv(args);

    // Update the process title. The argv was already cached by the call to
    // SetProcessTitleFromCommandLine in ChromeMain, so we can pass NULL here
    // (we don't have the original argv at this point).
    service_manager::SetProcessTitleFromCommandLine(nullptr);
  } else if (child_pid < 0) {
    LOG(ERROR) << "Zygote could not fork: process_type " << process_type
               << " numfds " << numfds << " child_pid " << child_pid;
  }
  return child_pid;
}

bool Zygote::HandleForkRequest(int fd,
                               base::PickleIterator iter,
                               std::vector<base::ScopedFD> fds) {
  std::string uma_name;
  int uma_sample;
  int uma_boundary_value;
  base::ProcessId child_pid = ReadArgsAndFork(iter, std::move(fds), &uma_name,
                                              &uma_sample, &uma_boundary_value);
  if (child_pid == 0)
    return true;
  // If there's no UMA report for this particular fork, then check if any
  // helpers have an initial UMA report for us to send instead.
  while (uma_name.empty() && initial_uma_index_ < helpers_.size()) {
    helpers_[initial_uma_index_++]->InitialUMA(&uma_name, &uma_sample,
                                               &uma_boundary_value);
  }
  // Must always send reply, as ZygoteHost blocks while waiting for it.
  base::Pickle reply_pickle;
  reply_pickle.WriteInt(child_pid);
  reply_pickle.WriteString(uma_name);
  if (!uma_name.empty()) {
    reply_pickle.WriteInt(uma_sample);
    reply_pickle.WriteInt(uma_boundary_value);
  }
  if (HANDLE_EINTR(write(fd, reply_pickle.data(), reply_pickle.size())) !=
      static_cast<ssize_t>(reply_pickle.size()))
    PLOG(ERROR) << "write";
  return false;
}

bool Zygote::HandleGetSandboxStatus(int fd, base::PickleIterator iter) {
  if (HANDLE_EINTR(write(fd, &sandbox_flags_, sizeof(sandbox_flags_))) !=
      sizeof(sandbox_flags_)) {
    PLOG(ERROR) << "write";
  }

  return false;
}

}  // namespace service_manager

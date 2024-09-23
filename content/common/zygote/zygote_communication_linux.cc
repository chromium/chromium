// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/341324165): Fix and remove.
#pragma allow_unsafe_buffers
#endif

#include "content/common/zygote/zygote_communication_linux.h"

#include <string.h>
#include <sys/socket.h>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/platform_file.h"
#include "base/i18n/unicodestring.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket.h"
#include "content/common/zygote/zygote_commands_linux.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "sandbox/policy/switches.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace content {

ZygoteCommunication::ZygoteCommunication(ZygoteType type)
    : type_(type),
      pid_(),
      sandbox_status_(0),
      have_read_sandbox_status_word_(false),
      init_(false) {}

ZygoteCommunication::~ZygoteCommunication() {}

bool ZygoteCommunication::SendMessage(const base::Pickle& data,
                                      const std::vector<int>* fds) {
  DCHECK(control_fd_.is_valid());
  CHECK(data.size() <= kZygoteMaxMessageLength)
      << "Trying to send too-large message to zygote (sending " << data.size()
      << " bytes, max is " << kZygoteMaxMessageLength << ")";
  CHECK(!fds || fds->size() <= base::UnixDomainSocket::kMaxFileDescriptors)
      << "Trying to send message with too many file descriptors to zygote "
      << "(sending " << fds->size() << ", max is "
      << base::UnixDomainSocket::kMaxFileDescriptors << ")";

  return base::UnixDomainSocket::SendMsg(control_fd_.get(), data.data(),
                                         data.size(),
                                         fds ? *fds : std::vector<int>());
}

ssize_t ZygoteCommunication::ReadSandboxStatus() {
  DCHECK(control_fd_.is_valid());
  // At startup we send a kZygoteCommandGetSandboxStatus request to the zygote,
  // but don't wait for the reply. Thus, the first time that we read from the
  // zygote, we get the reply to that request.
  ssize_t bytes_read = HANDLE_EINTR(
      read(control_fd_.get(), &sandbox_status_, sizeof(sandbox_status_)));
  if (bytes_read != sizeof(sandbox_status_)) {
    return -1;
  }
  return bytes_read;
}

ssize_t ZygoteCommunication::ReadReply(void* buf, size_t buf_len) {
  DCHECK(control_fd_.is_valid());
  if (!have_read_sandbox_status_word_) {
    if (ReadSandboxStatus() == -1) {
      return -1;
    }
    have_read_sandbox_status_word_ = true;
    base::UmaHistogramSparse("Linux.SandboxStatus", sandbox_status_);
  }

  return HANDLE_EINTR(read(control_fd_.get(), buf, buf_len));
}

void ZygoteCommunication::ReinitializeLogging(
    uint32_t logging_dest,
    base::PlatformFile raw_log_file_fd) {
  DCHECK(init_);

  base::Pickle pickle;
  pickle.WriteInt(kZygoteCommandReinitializeLogging);
  pickle.WriteUInt32(logging_dest);
  std::vector<int> fds = {raw_log_file_fd};

  base::AutoLock lock(control_lock_);
  if (!SendMessage(pickle, &fds))
    DLOG(WARNING) << "Unable to reinitialize logging";
}

pid_t ZygoteCommunication::ForkRequest(
    const std::vector<std::string>& argv,
    const base::FileHandleMappingVector& mapping,
    const std::string& process_type) {
  DCHECK(init_);

  base::Pickle pickle;
  int raw_socks[2];
  PCHECK(0 == socketpair(AF_UNIX, SOCK_SEQPACKET, 0, raw_socks));
  base::ScopedFD my_sock(raw_socks[0]);
  base::ScopedFD peer_sock(raw_socks[1]);
  CHECK(base::UnixDomainSocket::EnableReceiveProcessId(my_sock.get()));

  pickle.WriteInt(kZygoteCommandFork);
  pickle.WriteString(process_type);
  pickle.WriteInt(argv.size());
  for (std::vector<std::string>::const_iterator i = argv.begin();
       i != argv.end(); ++i)
    pickle.WriteString(*i);
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createDefault());
  icu::UnicodeString timezone_id;
  pickle.WriteString16(
      base::i18n::UnicodeStringToString16(timezone->getID(timezone_id)));

  // Fork requests contain one file descriptor for the PID oracle, and one
  // more for each file descriptor mapping for the child process.
  const size_t num_fds_to_send = 1 + mapping.size();
  pickle.WriteInt(num_fds_to_send);

  std::vector<int> fds;

  // First FD to send is peer_sock.
  // TODO(morrita): Ideally, this should be part of the mapping so that
  // PosixFileDescriptorInfo can manages its lifetime.
  fds.push_back(peer_sock.get());

  // The rest come from mapping.
  for (const auto& item : mapping) {
    fds.push_back(item.first);
    pickle.WriteUInt32(item.second);
  }

  // Sanity check that we've populated |fds| correctly.
  DCHECK_EQ(num_fds_to_send, fds.size());

  pid_t pid;
  {
    base::AutoLock lock(control_lock_);
    if (!SendMessage(pickle, &fds))
      return base::kNullProcessHandle;
    peer_sock.reset();

    {
      char buf[sizeof(kZygoteChildPingMessage) + 1];
      std::vector<base::ScopedFD> recv_fds;
      base::ProcessId real_pid;

      ssize_t n = base::UnixDomainSocket::RecvMsgWithPid(
          my_sock.get(), buf, sizeof(buf), &recv_fds, &real_pid);
      if (n != sizeof(kZygoteChildPingMessage) ||
          0 != memcmp(buf, kZygoteChildPingMessage,
                      sizeof(kZygoteChildPingMessage))) {
        // Zygote children should still be trustworthy when they're supposed to
        // ping us, so something's broken if we don't receive a valid ping.
        LOG(ERROR) << "Did not receive ping from zygote child";
        NOTREACHED_IN_MIGRATION();
        real_pid = -1;
      }
      my_sock.reset();

      // Always send PID back to zygote.
      base::Pickle pid_pickle;
      pid_pickle.WriteInt(kZygoteCommandForkRealPID);
      pid_pickle.WriteInt(real_pid);
      if (!SendMessage(pid_pickle, nullptr))
        return base::kNullProcessHandle;
    }

    // Read the reply, which pickles the PID and an optional UMA enumeration.
    static const unsigned kMaxReplyLength = 2048;
    char buf[kMaxReplyLength];
    const ssize_t len = ReadReply(buf, sizeof(buf));

    base::Pickle reply_pickle = base::Pickle::WithUnownedBuffer(
        base::as_bytes(base::span(buf, base::checked_cast<size_t>(len))));
    base::PickleIterator iter(reply_pickle);
    if (len <= 0 || !iter.ReadInt(&pid))
      return base::kNullProcessHandle;

    // If there is a nonempty UMA name string, then there is a UMA
    // enumeration to record.
    std::string uma_name;
    int uma_sample;
    int uma_boundary_value;
    if (iter.ReadString(&uma_name) && !uma_name.empty() &&
        iter.ReadInt(&uma_sample) && iter.ReadInt(&uma_boundary_value)) {
      // We cannot use the UMA_HISTOGRAM_ENUMERATION macro here,
      // because that's only for when the name is the same every time.
      // Here we're using whatever name we got from the other side.
      // But since it's likely that the same one will be used repeatedly
      // (even though it's not guaranteed), we cache it here.
      static base::HistogramBase* uma_histogram;
      if (!uma_histogram || uma_histogram->histogram_name() != uma_name) {
        uma_histogram = base::LinearHistogram::FactoryGet(
            uma_name, 1, uma_boundary_value, uma_boundary_value + 1,
            base::HistogramBase::kUmaTargetedHistogramFlag);
      }
      uma_histogram->Add(uma_sample);
    }

    if (pid <= 0)
      return base::kNullProcessHandle;
  }

  ZygoteChildBorn(pid);
  return pid;
}

void ZygoteCommunication::EnsureProcessTerminated(pid_t process) {
  DCHECK(init_);
  base::Pickle pickle;

  pickle.WriteInt(kZygoteCommandReap);
  pickle.WriteInt(process);
  if (!SendMessage(pickle, nullptr))
    LOG(ERROR) << "Failed to send Reap message to zygote";
  ZygoteChildDied(process);
}

void ZygoteCommunication::ZygoteChildBorn(pid_t process) {
  base::AutoLock lock(child_tracking_lock_);
  bool new_element_inserted =
      list_of_running_zygote_children_.insert(process).second;
  DCHECK(new_element_inserted);
}

void ZygoteCommunication::ZygoteChildDied(pid_t process) {
  base::AutoLock lock(child_tracking_lock_);
  size_t num_erased = list_of_running_zygote_children_.erase(process);
  DCHECK_EQ(1U, num_erased);
}

void ZygoteCommunication::Init(
    base::OnceCallback<pid_t(base::CommandLine*, base::ScopedFD*)> launcher) {
  CHECK(!init_);

  base::FilePath chrome_path;
  CHECK(base::PathService::Get(base::FILE_EXE, &chrome_path));

  base::CommandLine cmd_line(chrome_path);
  cmd_line.AppendSwitchASCII(switches::kProcessType, switches::kZygoteProcess);

  if (type_ == ZygoteType::kUnsandboxed)
    cmd_line.AppendSwitch(sandbox::policy::switches::kNoZygoteSandbox);

  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  if (browser_command_line.HasSwitch(switches::kZygoteCmdPrefix)) {
    cmd_line.PrependWrapper(
        browser_command_line.GetSwitchValueNative(switches::kZygoteCmdPrefix));
  }
  // Append any switches from the service manager that need to be forwarded on
  // to the zygote/renderers.
  static const char* const kForwardSwitches[] = {
      sandbox::policy::switches::kAllowSandboxDebugging,
      switches::kDisableInProcessStackTraces,
      sandbox::policy::switches::kDisableSeccompFilterSandbox,
      sandbox::policy::switches::kNoSandbox,
  };
  cmd_line.CopySwitchesFrom(browser_command_line, kForwardSwitches);

  pid_ = std::move(launcher).Run(&cmd_line, &control_fd_);

  base::Pickle pickle;
  pickle.WriteInt(kZygoteCommandGetSandboxStatus);
  if (!SendMessage(pickle, nullptr))
    LOG(FATAL) << "Cannot communicate with zygote";

  init_ = true;
}

base::TerminationStatus ZygoteCommunication::GetTerminationStatus(
    base::ProcessHandle handle,
    bool known_dead,
    int* exit_code) {
  DCHECK(init_);
  base::Pickle pickle;
  pickle.WriteInt(kZygoteCommandGetTerminationStatus);
  pickle.WriteBool(known_dead);
  pickle.WriteInt(handle);

  static const unsigned kMaxMessageLength = 128;
  char buf[kMaxMessageLength];
  ssize_t len;
  {
    base::AutoLock lock(control_lock_);
    if (!SendMessage(pickle, nullptr))
      LOG(ERROR) << "Failed to send GetTerminationStatus message to zygote";
    len = ReadReply(buf, sizeof(buf));
  }

  // Set this now to handle the error cases.
  if (exit_code)
    *exit_code = RESULT_CODE_NORMAL_EXIT;
  int status = base::TERMINATION_STATUS_NORMAL_TERMINATION;

  if (len == -1) {
    PLOG(WARNING) << "Error reading message from zygote";
  } else if (len == 0) {
    LOG(WARNING) << "Socket closed prematurely.";
  } else {
    base::Pickle read_pickle = base::Pickle::WithUnownedBuffer(
        base::as_bytes(base::span(buf, base::checked_cast<size_t>(len))));
    int tmp_status, tmp_exit_code;
    base::PickleIterator iter(read_pickle);
    if (!iter.ReadInt(&tmp_status) || !iter.ReadInt(&tmp_exit_code)) {
      LOG(WARNING)
          << "Error parsing GetTerminationStatus response from zygote.";
    } else {
      if (exit_code)
        *exit_code = tmp_exit_code;
      status = tmp_status;
    }
  }

  if (status != base::TERMINATION_STATUS_STILL_RUNNING) {
    ZygoteChildDied(handle);
  }
  return static_cast<base::TerminationStatus>(status);
}

int ZygoteCommunication::GetSandboxStatus() {
  if (have_read_sandbox_status_word_) {
    return sandbox_status_;
  }
  if (ReadSandboxStatus() == -1) {
    return 0;
  }
  have_read_sandbox_status_word_ = true;
  base::UmaHistogramSparse("Linux.SandboxStatus", sandbox_status_);
  return sandbox_status_;
}

}  // namespace content

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/terminal_session.h"

#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "remoting/base/logging.h"
#include "remoting/host/terminal_process_monitor_linux.h"
#include "remoting/host/terminal_session_manager.h"

namespace remoting {

namespace {

constexpr std::string_view kTmx2Path = "/usr/bin/tmx2";
constexpr std::string_view kTmuxSessionPrefix = "chrome-remote-desktop-";
constexpr std::string_view kTmuxSocketName = "chrome-remote-desktop";

std::string GetTmuxSessionName(int32_t id) {
  return base::StrCat({kTmuxSessionPrefix, base::NumberToString(id)});
}

base::FilePath FindTmx2Path() {
  // Only tmx2 is supported for terminal sessions. It's impossible to have tmx2
  // installed without tmux also being installed.
  base::FilePath tmx2_path(kTmx2Path);
  if (base::PathExists(tmx2_path)) {
    return tmx2_path;
  }
  return base::FilePath();
}

void KillTmuxSession(int32_t id) {
  base::FilePath tmx2_path = FindTmx2Path();
  if (!tmx2_path.empty()) {
    std::vector<std::string> tmux_args = {
        tmx2_path.value(), "-L", std::string(kTmuxSocketName),
        "kill-session", "-t", GetTmuxSessionName(id)};
    base::Process process =
        base::LaunchProcess(tmux_args, base::LaunchOptions());
    if (process.IsValid()) {
      base::EnsureProcessTerminated(std::move(process));
    }
  }
}

void TerminateProcessInBackground(base::Process process) {
  process.Terminate(0, false);
  base::EnsureProcessTerminated(std::move(process));
}

std::optional<pid_t> GetTmuxPaneShellPid(int32_t id) {
  base::FilePath tmx2_path = FindTmx2Path();
  if (tmx2_path.empty()) {
    return std::nullopt;
  }

  std::string output;
  std::vector<std::string> args = {
      tmx2_path.value(), "-L", std::string(kTmuxSocketName),
      "display-message", "-p", "-t",
      GetTmuxSessionName(id), "-F", "#{pane_pid}"};

  if (!base::GetAppOutput(args, &output)) {
    return std::nullopt;
  }

  output = base::TrimWhitespaceASCII(output, base::TRIM_ALL);
  int int_pid;
  if (base::StringToInt(output, &int_pid) && int_pid > 0) {
    return static_cast<pid_t>(int_pid);
  }
  return std::nullopt;
}

// PreExecDelegate to set up the PTY session in the child process. It creates
// a new session leader and attaches the process to the PTY.
class TerminalPreExecDelegate : public base::LaunchOptions::PreExecDelegate {
 public:
  TerminalPreExecDelegate() = default;
  ~TerminalPreExecDelegate() override = default;

  void RunAsyncSafe() override {
    setsid();
    ioctl(STDIN_FILENO, TIOCSCTTY, 0);
  }
};

base::Process LaunchShellProcess(int32_t id, base::ScopedFD subsidiary_fd) {
  base::FilePath tmx2_path = FindTmx2Path();
  // If tmx2 is not available, then we cannot launch the terminal session.
  if (tmx2_path.empty()) {
    LOG(ERROR)
        << "tmx2 binary not found. Cannot launch terminal session.";
    return base::Process();
  }

  std::vector<std::string> tmux_cmd = {
    tmx2_path.value(),
    "-L", std::string(kTmuxSocketName),
    "new-session", "-A", "-s", GetTmuxSessionName(id), ";",
    "set-option", "set-titles", "on", ";",
    "set-option", "set-titles-string", "#T"
  };

  base::LaunchOptions options;
  options.allow_new_privs = true;
  options.fds_to_remap.emplace_back(subsidiary_fd.get(), STDIN_FILENO);
  options.fds_to_remap.emplace_back(subsidiary_fd.get(), STDOUT_FILENO);
  options.fds_to_remap.emplace_back(subsidiary_fd.get(), STDERR_FILENO);

  TerminalPreExecDelegate delegate;
  options.pre_exec_delegate = &delegate;
  options.environment["TERM"] = "xterm-256color";

  return base::LaunchProcess(tmux_cmd, options);
}

class TerminalSessionLinux : public TerminalSession {
 public:
  TerminalSessionLinux(
      TerminalSessionManager::OutputCallback output_cb,
      TerminalSessionManager::ExitCallback exit_cb,
      TerminalSessionManager::ProcessInfoCallback process_info_cb,
      int32_t id)
      : output_callback_(std::move(output_cb)),
        exit_callback_(std::move(exit_cb)),
        process_info_callback_(std::move(process_info_cb)),
        id_(id),
        writer_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

  ~TerminalSessionLinux() override { CleanupLocalSession(); }

  // Start the terminal session. This will start a new PTY session and launch a
  // bash process in the subsidiary end of the PTY.
  bool Start() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::ScopedFD pty_fd(
        HANDLE_EINTR(posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC)));
    if (!pty_fd.is_valid()) {
      PLOG(ERROR) << "posix_openpt failed";
      return false;
    }
    if (grantpt(pty_fd.get()) != 0) {
      PLOG(ERROR) << "grantpt failed";
      return false;
    }
    if (unlockpt(pty_fd.get()) != 0) {
      PLOG(ERROR) << "unlockpt failed";
      return false;
    }

    char subsidiary_name[TTY_NAME_MAX];
    if (ptsname_r(pty_fd.get(), subsidiary_name, sizeof(subsidiary_name)) !=
        0) {
      PLOG(ERROR) << "ptsname_r failed";
      return false;
    }

    base::ScopedFD subsidiary_fd(
        HANDLE_EINTR(open(subsidiary_name, O_RDWR | O_NOCTTY | O_CLOEXEC)));
    if (!subsidiary_fd.is_valid()) {
      PLOG(ERROR) << "open subsidiary_fd failed";
      return false;
    }

    struct termios ios;
    if (tcgetattr(subsidiary_fd.get(), &ios) == 0) {
      ios.c_iflag |= IUTF8;
      tcsetattr(subsidiary_fd.get(), TCSANOW, &ios);
    }

    pty_fd_ = std::move(pty_fd);

    writer_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&LaunchShellProcess, id_, std::move(subsidiary_fd)),
        base::BindOnce(
            [](base::WeakPtr<TerminalSessionLinux> weak_this,
               scoped_refptr<base::SequencedTaskRunner> writer_task_runner,
               base::Process process) {
              if (weak_this) {
                weak_this->OnProcessLaunched(std::move(process));
              } else if (process.IsValid()) {
                writer_task_runner->PostTask(
                    FROM_HERE, base::BindOnce(&TerminateProcessInBackground,
                                              std::move(process)));
              }
            },
            weak_factory_.GetWeakPtr(), writer_task_runner_));
    return true;
  }

  void OnProcessLaunched(base::Process process) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!process.IsValid()) {
      LOG(ERROR) << "Failed to launch terminal shell process asynchronously";
      CleanupLocalSession();
      if (exit_callback_) {
        std::move(exit_callback_).Run(id_);
      }
      return;
    }
    // If the session was detached or terminated before the process was
    // launched, terminate the process and return.
    if (detached_ || terminated_) {
      if (writer_task_runner_) {
        writer_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&TerminateProcessInBackground,
                                      std::move(process)));
      }
      return;
    }
    // process_ will never be valid here since it's only set by
    // OnProcessLaunched(), which is only called once by Start().
    CHECK(!process_.IsValid());
    process_ = std::move(process);
    WatchOutput();
  }

  static void WriteToPtyManager(int fd, std::string payload) {
    base::span<const char> remaining(payload);
    while (!remaining.empty()) {
      ssize_t bytes_written =
          HANDLE_EINTR(write(fd, remaining.data(), remaining.size()));
      if (bytes_written <= 0) {
        PLOG(WARNING) << "write to PTY manager failed";
        return;
      }
      remaining = remaining.subspan(static_cast<size_t>(bytes_written));
    }
  }

  // Write terminal input to the Manager end of PTY.
  void Write(const std::string& data) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!pty_fd_.is_valid()) {
      LOG(ERROR)
          << "Write called before successful Start() or after Terminate()";
      return;
    }

    // Post the write task to the dedicated sequenced task runner.
    // Pass the raw file descriptor integer from pty_fd_.
    writer_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&TerminalSessionLinux::WriteToPtyManager,
                                  pty_fd_.get(), data));
  }

  // Resizes the terminal window (rows and columns) of the PTY.
  void Resize(uint32_t width, uint32_t height) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!pty_fd_.is_valid()) {
      LOG(WARNING) << "Resize called with invalid pty_fd_";
      return;
    }
    struct winsize ws;
    ws.ws_col = width;
    ws.ws_row = height;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    if (ioctl(pty_fd_.get(), TIOCSWINSZ, &ws) != 0) {
      PLOG(ERROR) << "ioctl(TIOCSWINSZ) failed";
    }
  }

  // Terminates the terminal session and stops the output watcher.
  // Called when the user specifically closes a terminal session.
  void Terminate() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (terminated_) {
      return;
    }
    terminated_ = true;
    if (writer_task_runner_) {
      writer_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&KillTmuxSession, id_));
    }
    CleanupLocalSession();
  }

  // Detaches from the terminal session destroying the terminal emulator process
  // but leaving the tmux server session intact to allow for reconnection.
  void Detach() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CleanupLocalSession();
  }

 private:
  void CleanupLocalSession() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (detached_) {
      return;
    }
    detached_ = true;
    output_watcher_.reset();
    process_monitor_.reset();
    if (process_.IsValid() && writer_task_runner_) {
      writer_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&TerminateProcessInBackground,
                                    std::move(process_)));
    }
    if (pty_fd_.is_valid() && writer_task_runner_) {
      // Post the destruction of pty_fd_ to the writer task runner
      // to ensure it's closed after all pending writes are done.
      writer_task_runner_->PostTask(
          FROM_HERE, base::BindOnce([](base::ScopedFD fd) { fd.reset(); },
                                    std::move(pty_fd_)));
    }
  }

  // Watches the PTY manager file descriptor for readable data.
  void WatchOutput() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!pty_fd_.is_valid()) {
      LOG(ERROR) << "WatchOutput called with invalid pty_fd_";
      return;
    }
    output_watcher_ = base::FileDescriptorWatcher::WatchReadable(
        pty_fd_.get(),
        base::BindRepeating(&TerminalSessionLinux::OnOutputCanRead,
                            weak_factory_.GetWeakPtr()));
  }

  void OnOutputCanRead() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    char buffer[4096];

    if (!pty_fd_.is_valid()) {
      LOG(ERROR) << "OnOutputCanRead called with invalid pty_fd_";
      output_watcher_.reset();
      if (exit_callback_) {
        std::move(exit_callback_).Run(id_);
      }
      return;
    }

    // If the PTY pipe has more than 4k of data, this will read it in chunks.
    // The FileDescriptorWatcher will notify again to read the remaining data.
    ssize_t bytes_read =
        HANDLE_EINTR(read(pty_fd_.get(), buffer, sizeof(buffer)));
    if (bytes_read > 0) {
      // Retrieve the shell PID if it hasn't been retrieved yet.
      // This is done once when output is first received since that means that
      // the tmux pane has been successfully created.
      if (!shell_pid_retrieval_started_) {
        shell_pid_retrieval_started_ = true;
        base::ThreadPool::PostTaskAndReplyWithResult(
            FROM_HERE,
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
            base::BindOnce(&GetTmuxPaneShellPid, id_),
            base::BindOnce(&TerminalSessionLinux::OnShellPidRetrieved,
                           weak_factory_.GetWeakPtr()));
      }
      output_callback_.Run(id_, std::string(buffer, bytes_read));
    } else {
      if (bytes_read < 0) {
        PLOG(ERROR) << "read from PTY manager failed";
      } else {
        HOST_LOG << "PTY manager reached EOF - normal exit";
      }
      output_watcher_.reset();
      if (exit_callback_) {
        std::move(exit_callback_).Run(id_);
      }
    }
  }

  void OnShellPidRetrieved(std::optional<pid_t> pid) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (detached_ || terminated_) {
      HOST_LOG << "OnShellPidRetrieved called after detach or terminate, "
                  "ignoring";
      return;
    }
    if (pid) {
      shell_pid_ = *pid;
      HOST_LOG << "Retrieved shell PID " << *pid
               << " for terminal session " << id_;
      if (process_info_callback_) {
        process_monitor_ = std::make_unique<TerminalProcessMonitorLinux>(
            *pid,
            base::BindRepeating(&TerminalSessionLinux::OnProcessInfoChanged,
                                weak_factory_.GetWeakPtr()));
        process_monitor_->StartPolling();
      }
    } else {
      LOG(WARNING) << "Failed to retrieve shell PID for tmux terminal " << id_;
    }
  }

  void OnProcessInfoChanged(
      bool is_active,
      const std::optional<std::string>& process_name) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (detached_ || terminated_) {
      HOST_LOG << "OnProcessInfoChanged called after detach or terminate, "
                  "ignoring";
      return;
    }
    if (process_info_callback_) {
      std::string_view process_name_view = "";
      if (process_name) {
        process_name_view = *process_name;
      }
      process_info_callback_.Run(id_, is_active, process_name_view);
    }
  }

  base::ScopedFD pty_fd_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::Process process_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<base::FileDescriptorWatcher::Controller> output_watcher_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<TerminalProcessMonitorLinux> process_monitor_
      GUARDED_BY_CONTEXT(sequence_checker_);
  TerminalSessionManager::OutputCallback output_callback_;
  TerminalSessionManager::ExitCallback exit_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  TerminalSessionManager::ProcessInfoCallback process_info_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  int32_t id_;
  bool detached_ = false;
  bool terminated_ = false;
  scoped_refptr<base::SequencedTaskRunner> writer_task_runner_;
  std::optional<pid_t> shell_pid_ GUARDED_BY_CONTEXT(sequence_checker_);
  bool shell_pid_retrieval_started_
      GUARDED_BY_CONTEXT(sequence_checker_) = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<TerminalSessionLinux> weak_factory_{this};
};

}  // namespace

// static
std::unique_ptr<TerminalSession> TerminalSession::Create(
    TerminalSessionManager::OutputCallback output_cb,
    TerminalSessionManager::ExitCallback exit_cb,
    TerminalSessionManager::ProcessInfoCallback process_info_cb,
    int32_t id) {
  return std::make_unique<TerminalSessionLinux>(
      std::move(output_cb), std::move(exit_cb), std::move(process_info_cb),
      id);
}

// static
std::vector<int32_t> TerminalSession::GetPersistentTerminalIds() {
  // This is a blocking call (uses PathExists and GetAppOutput).
  base::FilePath tmx2_path = FindTmx2Path();
  if (tmx2_path.empty()) {
    return {};
  }

  std::string output;
  std::vector<std::string> args = {
      tmx2_path.value(), "-L", std::string(kTmuxSocketName),
      "list-sessions",   "-F", "#{session_name}"};
  if (!base::GetAppOutput(args, &output)) {
    return {};
  }

  std::vector<std::string_view> lines = base::SplitStringPiece(
      output, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  std::vector<int32_t> restored_ids;
  for (std::string_view line : lines) {
    if (line.starts_with(kTmuxSessionPrefix)) {
      std::string_view id_str = line.substr(kTmuxSessionPrefix.size());
      int32_t id;
      if (base::StringToInt(id_str, &id)) {
        restored_ids.push_back(id);
      }
    }
  }
  return restored_ids;
}

}  // namespace remoting

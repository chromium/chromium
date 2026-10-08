// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/terminal_session.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_paths.h"
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
#include "base/path_service.h"
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
#include "base/types/expected.h"
#include "remoting/base/logging.h"
#include "remoting/host/terminal_error.h"
#include "remoting/host/terminal_process_monitor_linux.h"
#include "remoting/host/terminal_session_manager.h"

namespace remoting {

namespace {

constexpr std::string_view kTmx2Path = "/usr/bin/tmx2";
constexpr std::string_view kTmuxSessionPrefix = "chrome-remote-desktop-";
constexpr std::string_view kTmuxSocketName = "chrome-remote-desktop";

// When tmux attaches a VT100-like client, `tty_send_requests()` emits Primary
// DA (`\x1b[c`), Secondary DA (`\x1b[>c`), and Extended DA (`\x1b[>q`) queries
// and starts a 5-second timeout (`TTY_QUERY_TIMEOUT`). If the client's
// responses arrive after the timeout (e.g. due to reconnect or scrollback
// processing latency), tmux rejects them as device attribute responses and
// forwards the raw escape sequences as literal keystrokes into the shell pane.
// Intercepting the queries on the host and replying locally on the PTY avoids
// the network round-trip and immediately satisfies `TTY_ALL_REQUEST_FLAGS` in
// tmux (preventing tmux's 500ms `escape-time` query-wait override).
constexpr std::string_view kTmuxDeviceAttributesQuery = "\x1b[c\x1b[>c\x1b[>q";
constexpr char kTmuxDeviceAttributesReply[] =
    "\x1b[?1;2c\x1b[>0;276;0c\x1bP>|xterm.js\x1b\\";

std::string GetTmuxSessionName(int32_t id) {
  return base::StrCat({kTmuxSessionPrefix, base::NumberToString(id)});
}

base::unexpected<TerminalError> LogAndReturnError(TerminalError error) {
  LOG(ERROR) << error;
  return base::unexpected(std::move(error));
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
        "kill-session",    "-t", GetTmuxSessionName(id)};
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
      tmx2_path.value(),      "-L", std::string(kTmuxSocketName),
      "display-message",      "-p", "-t",
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

std::string GetTmuxScrollback(int32_t id) {
  base::FilePath tmx2_path = FindTmx2Path();
  if (tmx2_path.empty()) {
    return std::string();
  }

  // Capture full pane history with -epJ flags.
  std::string scrollback_output;
  std::vector<std::string> args = {tmx2_path.value(),
                                   "-L",
                                   std::string(kTmuxSocketName),
                                   "capture-pane",
                                   "-epJ",
                                   "-S",
                                   "-",
                                   "-t",
                                   GetTmuxSessionName(id)};

  if (!base::GetAppOutput(args, &scrollback_output) ||
      scrollback_output.empty()) {
    return std::string();
  }

  // Trim trailing newlines so the terminal does not scroll an extra line
  // (which would push the top visible line into scrollback and cause a
  // duplicate line when tmux redraws).
  std::string_view trimmed =
      base::TrimString(scrollback_output, "\r\n", base::TRIM_TRAILING);
  if (trimmed.empty()) {
    return std::string();
  }
  scrollback_output.resize(trimmed.size());

  // Normalize line endings to CRLF. Direct callback output bypasses the
  // PTY's automatic ONLCR translation, causing terminal staircasing.
  //
  // A two-step replacement (\r\n -> \n, then \n -> \r\n) is used to ensure
  // idempotency: it converts any mixed or standalone '\n' to '\r\n' while
  // preventing already-CRLF lines from turning into duplicate carriage returns
  // (\r\r\n), unlike naive ONLCR prefixing.
  base::ReplaceSubstringsAfterOffset(&scrollback_output, 0, "\r\n", "\n");
  base::ReplaceSubstringsAfterOffset(&scrollback_output, 0, "\n", "\r\n");

  return scrollback_output;
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

base::expected<base::Process, TerminalError> LaunchShellProcess(
    int32_t id,
    base::ScopedFD subsidiary_fd) {
  base::FilePath tmx2_path = FindTmx2Path();
  // If tmx2 is not available, then we cannot launch the terminal session.
  if (tmx2_path.empty()) {
    return base::unexpected(TerminalError(
        FROM_HERE, TerminalError::Reason::kTmuxMissing,
        base::StrCat(
            {kTmx2Path, " not found. Cannot launch terminal session."})));
  }

  static constexpr std::pair<std::string_view, std::string_view>
      kTmuxOptions[] = {
          // Disable the alternate screen buffer (smcup/rmcup) for xterm* so
          // output flows into the outer terminal's (xterm.js) native scrollback
          // buffer.
          {"terminal-overrides", "xterm*:smcup@:rmcup@"},
          // Tell tmux the outer terminal (xterm.js) supports OSC 8 hyperlinks
          // so that they are forwarded to the client instead of being stripped.
          // Use an explicit array index so that repeated launches against the
          // same tmux server don't keep appending duplicate entries.
          {"terminal-features[1000]", "xterm*:hyperlinks"},
          // Forward pane title changes to the outer terminal via OSC escape
          // sequences so the CRD client can update the tab title.
          {"set-titles", "on"},
          // Use only the inner pane's title (#T) without tmux's default
          // session/window prefix.
          {"set-titles-string", "#T"},
          // Hide the tmux status bar so the persistence session is visually
          // transparent.
          {"status", "off"},
          // Disable tmux mouse capture so the outer terminal (xterm.js) handles
          // native text selection and scrolling directly.
          {"mouse", "off"},
          // Disable the prefix key on the outer tmux server so that shortcuts
          // (such as Ctrl+b d to detach from an inner tmux session) are
          // forwarded to the shell instead of detaching the CRD terminal tab.
          {"prefix", "None"},
          {"prefix2", "None"},
      };

  // clang-format off
  std::vector<std::string> tmux_cmd = {
      tmx2_path.value(),
      // Ignore the user's ~/.tmux.conf so custom keybindings, status bars, or
      // plugins do not affect the outer CRD persistence wrapper.
      "-f", "/dev/null",
      // Use the CRD-specific tmux socket name to avoid conflicts with other
      // tmux sessions.
      "-L", std::string(kTmuxSocketName),
  };
  for (const auto& [name, value] : kTmuxOptions) {
    tmux_cmd.insert(tmux_cmd.end(), {"set-option", "-g", std::string(name),
                                     std::string(value), ";"});
  }
  tmux_cmd.insert(
      tmux_cmd.end(),
      {
          "unbind-key", "-a", "-T", "root", ";",
          "new-session", "-A", "-s", GetTmuxSessionName(id),
          "-e", base::StrCat({"CRD_TERMINAL_ID=", base::NumberToString(id)}),
          "-e", "CLI_GRAPHICS=iterm2",
          // Unset TMUX and TMUX_PANE before exec'ing the user's login shell so
          // that users can create or attach to nested tmux/tmx2 sessions
          // without hitting "sessions should be nested with care, unset $TMUX
          // to force". Using exec preserves #{pane_pid} as the login shell PID
          // for process monitoring.
          "unset TMUX TMUX_PANE; exec \"${SHELL:-/bin/bash}\" -l", ";",
      });
  // clang-format on

  base::LaunchOptions options;
  base::FilePath home_dir;
  if (base::PathService::Get(base::DIR_HOME, &home_dir)) {
    options.current_directory = std::move(home_dir);
  }
  options.allow_new_privs = true;
  options.fds_to_remap.emplace_back(subsidiary_fd.get(), STDIN_FILENO);
  options.fds_to_remap.emplace_back(subsidiary_fd.get(), STDOUT_FILENO);
  options.fds_to_remap.emplace_back(subsidiary_fd.get(), STDERR_FILENO);

  TerminalPreExecDelegate delegate;
  options.pre_exec_delegate = &delegate;
  options.environment["TERM"] = "xterm-256color";

  base::Process process = base::LaunchProcess(tmux_cmd, options);
  if (!process.IsValid()) {
    return base::unexpected(
        TerminalError(FROM_HERE, TerminalError::Reason::kLaunchFailed,
                      "Failed to launch terminal shell process"));
  }
  return process;
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
  void Start(StartCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!start_callback_);
    base::expected<base::ScopedFD, TerminalError> subsidiary_fd = OpenPty();
    if (!subsidiary_fd.has_value()) {
      std::move(callback).Run(
          base::unexpected(std::move(subsidiary_fd).error()));
      return;
    }
    start_callback_ = std::move(callback);

    writer_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&LaunchShellProcess, id_,
                       std::move(subsidiary_fd).value()),
        base::BindOnce(
            [](base::WeakPtr<TerminalSessionLinux> weak_this,
               scoped_refptr<base::SequencedTaskRunner> writer_task_runner,
               base::expected<base::Process, TerminalError> result) {
              if (weak_this) {
                weak_this->OnProcessLaunched(std::move(result));
              } else if (result.has_value()) {
                writer_task_runner->PostTask(
                    FROM_HERE, base::BindOnce(&TerminateProcessInBackground,
                                              std::move(result).value()));
              }
            },
            weak_factory_.GetWeakPtr(), writer_task_runner_));
  }

  void OnProcessLaunched(base::expected<base::Process, TerminalError> result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Note that the owner may delete `this` in response to `callback`, so it
    // must be run last.
    StartCallback callback = std::move(start_callback_);
    // If the session was detached or terminated before the process was
    // launched, terminate the process (if any) and return without running
    // `callback`.
    if (detached_ || terminated_) {
      if (result.has_value() && writer_task_runner_) {
        writer_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&TerminateProcessInBackground,
                                      std::move(result).value()));
      }
      return;
    }
    if (!result.has_value()) {
      LOG(ERROR) << result.error();
      CleanupLocalSession();
      std::move(callback).Run(base::unexpected(std::move(result).error()));
      return;
    }
    // process_ will never be valid here since it's only set by
    // OnProcessLaunched(), which is only called once by Start().
    CHECK(!process_.IsValid());
    process_ = std::move(result).value();

    // Asynchronously retrieve and forward existing scrollback history before
    // starting to watch live PTY output to avoid stream interleaving. Sequence
    // through writer_task_runner_ to preserve order with lifecycle commands.
    // Since this is asynchronous, `callback` is guaranteed to run before any
    // output is delivered.
    writer_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&GetTmuxScrollback, id_),
        base::BindOnce(&TerminalSessionLinux::OnScrollbackRetrieved,
                       weak_factory_.GetWeakPtr()));

    std::move(callback).Run(base::ok());
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
      writer_task_runner_->PostTask(FROM_HERE,
                                    base::BindOnce(&KillTmuxSession, id_));
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
  // Opens a new PTY, storing the manager end in `pty_fd_` and returning the
  // subsidiary end.
  base::expected<base::ScopedFD, TerminalError> OpenPty() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::ScopedFD pty_fd(
        HANDLE_EINTR(posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC)));
    if (!pty_fd.is_valid()) {
      return LogAndReturnError(TerminalError::FromSystemError(
          FROM_HERE, TerminalError::Reason::kPtyError, "posix_openpt", errno));
    }
    if (grantpt(pty_fd.get()) != 0) {
      return LogAndReturnError(TerminalError::FromSystemError(
          FROM_HERE, TerminalError::Reason::kPtyError, "grantpt", errno));
    }
    if (unlockpt(pty_fd.get()) != 0) {
      return LogAndReturnError(TerminalError::FromSystemError(
          FROM_HERE, TerminalError::Reason::kPtyError, "unlockpt", errno));
    }

    char subsidiary_name[TTY_NAME_MAX];
    int ptsname_result =
        ptsname_r(pty_fd.get(), subsidiary_name, sizeof(subsidiary_name));
    if (ptsname_result != 0) {
      // ptsname_r returns the error code rather than setting errno.
      return LogAndReturnError(TerminalError::FromSystemError(
          FROM_HERE, TerminalError::Reason::kPtyError, "ptsname_r",
          ptsname_result));
    }

    base::ScopedFD subsidiary_fd(
        HANDLE_EINTR(open(subsidiary_name, O_RDWR | O_NOCTTY | O_CLOEXEC)));
    if (!subsidiary_fd.is_valid()) {
      return LogAndReturnError(TerminalError::FromSystemError(
          FROM_HERE, TerminalError::Reason::kPtyError, "open subsidiary PTY",
          errno));
    }

    struct termios ios;
    if (tcgetattr(subsidiary_fd.get(), &ios) == 0) {
      ios.c_iflag |= IUTF8;
      tcsetattr(subsidiary_fd.get(), TCSANOW, &ios);
    }

    pty_fd_ = std::move(pty_fd);
    return subsidiary_fd;
  }

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
          FROM_HERE,
          base::BindOnce(&TerminateProcessInBackground, std::move(process_)));
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
      std::string output(buffer, bytes_read);
      if (!device_attributes_handled_) {
        size_t pos = output.find(kTmuxDeviceAttributesQuery);
        if (pos != std::string::npos) {
          device_attributes_handled_ = true;
          output.erase(pos, kTmuxDeviceAttributesQuery.size());
          Write(kTmuxDeviceAttributesReply);
        }
      }
      if (!output.empty()) {
        output_callback_.Run(id_, std::move(output));
      }
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

  void OnScrollbackRetrieved(std::string scrollback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (detached_ || terminated_) {
      HOST_LOG
          << "OnScrollbackRetrieved called after detach or terminate, ignoring";
      return;
    }
    base::WeakPtr<TerminalSessionLinux> weak_this = weak_factory_.GetWeakPtr();
    if (!scrollback.empty() && output_callback_) {
      output_callback_.Run(id_, std::move(scrollback));
    }
    // Re-verify session validity and state before starting output watcher in
    // case output_callback_ synchronously destroyed the instance
    // or triggered Detach()/Terminate().
    if (!weak_this || weak_this->detached_ || weak_this->terminated_) {
      return;
    }
    WatchOutput();
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
      HOST_LOG << "Retrieved shell PID " << *pid << " for terminal session "
               << id_;
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

  void OnProcessInfoChanged(bool is_active,
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
  // Set by Start() while the shell process is being launched.
  StartCallback start_callback_ GUARDED_BY_CONTEXT(sequence_checker_);
  TerminalSessionManager::ExitCallback exit_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  TerminalSessionManager::ProcessInfoCallback process_info_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  int32_t id_;
  bool detached_ = false;
  bool terminated_ = false;
  scoped_refptr<base::SequencedTaskRunner> writer_task_runner_;
  std::optional<pid_t> shell_pid_ GUARDED_BY_CONTEXT(sequence_checker_);
  bool shell_pid_retrieval_started_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;
  bool device_attributes_handled_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<TerminalSessionLinux> weak_factory_{this};
};

}  // namespace

// static
bool TerminalSession::IsSupported() {
  return true;
}

// static
std::unique_ptr<TerminalSession> TerminalSession::Create(
    TerminalSessionManager::OutputCallback output_cb,
    TerminalSessionManager::ExitCallback exit_cb,
    TerminalSessionManager::ProcessInfoCallback process_info_cb,
    int32_t id) {
  return std::make_unique<TerminalSessionLinux>(
      std::move(output_cb), std::move(exit_cb), std::move(process_info_cb), id);
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

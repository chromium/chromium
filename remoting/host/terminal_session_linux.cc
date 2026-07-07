// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/terminal_session.h"

#include <fcntl.h>
// #include <limits.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "remoting/host/terminal_session_manager.h"

namespace remoting {

namespace {

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

class TerminalSessionLinux : public TerminalSession {
 public:
  TerminalSessionLinux(TerminalSessionManager::OutputCallback output_cb,
                       TerminalSessionManager::ExitCallback exit_cb,
                       int32_t id)
      : output_callback_(std::move(output_cb)),
        exit_callback_(std::move(exit_cb)),
        id_(id),
        writer_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

  ~TerminalSessionLinux() override { Terminate(); }

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

    base::CommandLine cmd(base::FilePath("/bin/bash"));
    base::LaunchOptions options;
    options.fds_to_remap.emplace_back(subsidiary_fd.get(), STDIN_FILENO);
    options.fds_to_remap.emplace_back(subsidiary_fd.get(), STDOUT_FILENO);
    options.fds_to_remap.emplace_back(subsidiary_fd.get(), STDERR_FILENO);

    TerminalPreExecDelegate delegate;
    options.pre_exec_delegate = &delegate;
    options.environment["TERM"] = "xterm-256color";

    base::Process process = base::LaunchProcess(cmd, options);
    if (!process.IsValid()) {
      LOG(ERROR) << "Failed to launch terminal shell process";
      return false;
    }

    pty_fd_ = std::move(pty_fd);
    process_ = std::move(process);

    WatchOutput();
    return true;
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
  void Terminate() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    output_watcher_.reset();
    if (process_.IsValid()) {
      process_.Terminate(0, false);
      base::EnsureProcessTerminated(std::move(process_));
    }
    if (pty_fd_.is_valid() && writer_task_runner_) {
      // Post the destruction of pty_fd_ to the writer task runner
      // to ensure it's closed after all pending writes are done.
      writer_task_runner_->PostTask(
          FROM_HERE, base::BindOnce([](base::ScopedFD fd) { fd.reset(); },
                                    std::move(pty_fd_)));
    }
  }

 private:
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
      output_callback_.Run(id_, std::string(buffer, bytes_read));
    } else {
      if (bytes_read < 0) {
        PLOG(ERROR) << "read from PTY manager failed";
      } else {
        LOG(INFO) << "PTY manager reached EOF - normal exit";
      }
      output_watcher_.reset();
      if (exit_callback_) {
        std::move(exit_callback_).Run(id_);
      }
    }
  }

  base::ScopedFD pty_fd_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::Process process_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<base::FileDescriptorWatcher::Controller> output_watcher_
      GUARDED_BY_CONTEXT(sequence_checker_);
  TerminalSessionManager::OutputCallback output_callback_;
  TerminalSessionManager::ExitCallback exit_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  int32_t id_;

  scoped_refptr<base::SequencedTaskRunner> writer_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<TerminalSessionLinux> weak_factory_{this};
};

}  // namespace

// static
std::unique_ptr<TerminalSession> TerminalSession::Create(
    TerminalSessionManager::OutputCallback output_cb,
    TerminalSessionManager::ExitCallback exit_cb,
    int32_t id) {
  return std::make_unique<TerminalSessionLinux>(std::move(output_cb),
                                                std::move(exit_cb), id);
}

}  // namespace remoting

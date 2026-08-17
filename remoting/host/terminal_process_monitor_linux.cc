// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/terminal_process_monitor_linux.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"

namespace remoting {

namespace {

using ProcessCheckResult = TerminalProcessMonitorLinux::ProcessCheckResult;

constexpr base::TimeDelta kPollingInterval = base::Milliseconds(200);

// Trims leading and trailing whitespace, checks that the name is non-empty and
// valid UTF-8, and replaces control characters with '?'.
//
// Returns std::nullopt if the name is empty or not valid UTF-8.
std::optional<std::string> SanitizeProcessName(
    std::string_view raw_process_name) {
  std::string_view trimmed_name =
      base::TrimWhitespaceASCII(raw_process_name, base::TRIM_ALL);
  if (trimmed_name.empty() || !base::IsStringUTF8(trimmed_name)) {
    return std::nullopt;
  }
  std::string sanitized_name(trimmed_name);
  std::ranges::replace_if(
      sanitized_name,
      [](char c) {
        return base::IsAsciiControl(static_cast<unsigned char>(c));
      },
      '?');
  return sanitized_name;
}

std::optional<std::string> GetProcessName(base::ProcessId pid) {
  if (pid <= 0) {
    return std::nullopt;
  }
  base::FilePath comm_path =
      base::FilePath("/proc").Append(base::NumberToString(pid)).Append("comm");
  std::string comm_content;
  if (!base::ReadFileToString(comm_path, &comm_content)) {
    return std::nullopt;
  }
  return SanitizeProcessName(comm_content);
}

// Queries procfs for status and name of any active child process running under
// the shell.
//
// Note: /proc/<pid>/task/<pid>/children requires the Linux kernel to be
// compiled with CONFIG_CHECKPOINT_RESTORE (which enables CONFIG_PROC_CHILDREN).
// This is enabled by default in all standard modern Linux distributions
// (Debian, Ubuntu, Fedora, ChromeOS, etc.). If the kernel lacks this config,
// reading the children file fails gracefully and falls back to treating the
// session as inactive (shell only).
ProcessCheckResult GetProcessState(base::ProcessId shell_pid) {
  if (shell_pid <= 0) {
    return ProcessCheckResult();
  }

  std::optional<std::string> shell_name = GetProcessName(shell_pid);
  std::string shell_pid_str = base::NumberToString(shell_pid);
  base::FilePath children_path =
      base::FilePath("/proc")
          .Append(shell_pid_str)
          .Append("task")
          .Append(shell_pid_str)
          .Append("children");

  std::string children_content;
  if (!base::ReadFileToString(children_path, &children_content)) {
    return ProcessCheckResult{
        .is_active = false,
        .process_name = std::move(shell_name),
    };
  }

  std::vector<std::string_view> child_pids = base::SplitStringPiece(
      children_content, base::kWhitespaceASCII, base::TRIM_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);

  // If long-running processes are launched in the background, multiple child
  // PIDs may be present. We iterate in reverse so the most recently spawned
  // child is inspected first.
  for (std::string_view child_pid_str : base::Reversed(child_pids)) {
    base::ProcessId child_pid = 0;
    if (!base::StringToInt(child_pid_str, &child_pid) || child_pid <= 0) {
      continue;
    }
    std::optional<std::string> child_name = GetProcessName(child_pid);
    return ProcessCheckResult{
        .is_active = true,
        .process_name = std::move(child_name),
    };
  }

  return ProcessCheckResult{
      .is_active = false,
      .process_name = std::move(shell_name),
  };
}

}  // namespace

TerminalProcessMonitorLinux::TerminalProcessMonitorLinux(
    base::ProcessId shell_pid,
    ProcessInfoCallback callback)
    : shell_pid_(shell_pid),
      callback_(std::move(callback)),
      polling_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

TerminalProcessMonitorLinux::~TerminalProcessMonitorLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopPolling();
}

void TerminalProcessMonitorLinux::StartPolling() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!callback_) {
    return;
  }
  last_reported_result_.reset();
  timer_.Start(FROM_HERE, kPollingInterval,
               base::BindRepeating(&TerminalProcessMonitorLinux::PollProcess,
                                   weak_factory_.GetWeakPtr()));
  // Trigger immediate poll so initial state is reported without delay.
  PollProcess();
}

void TerminalProcessMonitorLinux::StopPolling() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.Stop();
  is_check_pending_ = false;
  weak_factory_.InvalidateWeakPtrs();
}

void TerminalProcessMonitorLinux::PollProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_check_pending_) {
    return;
  }

  is_check_pending_ = true;

  polling_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GetProcessState, shell_pid_),
      base::BindOnce(&TerminalProcessMonitorLinux::OnProcessChecked,
                     weak_factory_.GetWeakPtr()));
}

void TerminalProcessMonitorLinux::OnProcessChecked(ProcessCheckResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_check_pending_ = false;

  // Report the process info if this is the first report, if the process
  // activity has changed, or if the title has changed.
  if (!last_reported_result_.has_value() || *last_reported_result_ != result) {
    if (callback_) {
      callback_.Run(result.is_active, result.process_name);
    }
    last_reported_result_ = std::move(result);
  }
}

}  // namespace remoting

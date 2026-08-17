// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_TERMINAL_PROCESS_MONITOR_LINUX_H_
#define REMOTING_HOST_TERMINAL_PROCESS_MONITOR_LINUX_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"

namespace remoting {

// Monitors the process hierarchy of a Linux terminal session to detect running
// active processes and report changes in activity or process name.
// TerminalSessionLinux monitors the shell lifetime and exit via PTY EOF.
class TerminalProcessMonitorLinux {
 public:
  using ProcessInfoCallback = base::RepeatingCallback<
      void(bool is_active, const std::optional<std::string>& process_name)>;

  struct ProcessCheckResult {
    bool is_active = false;
    std::optional<std::string> process_name;

    bool operator==(const ProcessCheckResult& other) const = default;
  };

  TerminalProcessMonitorLinux(base::ProcessId shell_pid,
                              ProcessInfoCallback callback);
  ~TerminalProcessMonitorLinux();

  TerminalProcessMonitorLinux(const TerminalProcessMonitorLinux&) = delete;
  TerminalProcessMonitorLinux& operator=(const TerminalProcessMonitorLinux&) =
      delete;

  // Starts periodic polling of the shell process state. An immediate check
  // is posted to report the initial state as soon as possible.
  void StartPolling();

  // Stops periodic polling and cancels any pending callbacks.
  void StopPolling();

 private:
  void PollProcess();
  void OnProcessChecked(ProcessCheckResult result);

  base::ProcessId shell_pid_ = 0;
  ProcessInfoCallback callback_;
  std::optional<ProcessCheckResult> last_reported_result_;
  bool is_check_pending_ = false;

  base::RepeatingTimer timer_;
  scoped_refptr<base::SequencedTaskRunner> polling_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<TerminalProcessMonitorLinux> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_TERMINAL_PROCESS_MONITOR_LINUX_H_

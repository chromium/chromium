// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_TERMINATION_INFO_H_
#define CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_TERMINATION_INFO_H_

#include "base/process/kill.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/result_codes.h"

#if defined(OS_ANDROID)
#include "base/android/child_process_binding_types.h"
#endif

namespace content {

struct CONTENT_EXPORT ChildProcessTerminationInfo {
  ChildProcessTerminationInfo();
  ChildProcessTerminationInfo(const ChildProcessTerminationInfo& other);
  ~ChildProcessTerminationInfo();

  base::TerminationStatus status = base::TERMINATION_STATUS_NORMAL_TERMINATION;

  // If |status| is TERMINATION_STATUS_LAUNCH_FAILED then |exit_code| will
  // contain a platform specific launch failure error code. Otherwise, it will
  // contain the exit code for the process (e.g. status from waitpid if on
  // posix, from GetExitCodeProcess on Windows).
  int exit_code = service_manager::RESULT_CODE_NORMAL_EXIT;

  // Time delta between 1) the process start and 2) the time when
  // ChildProcessTerminationInfo is computed.
  base::TimeDelta uptime = base::TimeDelta::Max();

  // Populated only for renderer process. True if there are any visible
  // clients at the time of process death.
  bool renderer_has_visible_clients = false;

  // Populated only for renderer process. True if
  // RenderProcessHost::GetFrameDepth is bigger than 0. Note this is not exactly
  // the same as not having main frames.
  bool renderer_was_subframe = false;

#if defined(OS_ANDROID)
  // True if child service has strong or moderate binding at time of death.
  base::android::ChildBindingState binding_state =
      base::android::ChildBindingState::UNBOUND;

  // True if child service was explicitly killed by browser.
  bool was_killed_intentionally_by_browser = false;

  // True if the child shut itself down cleanly by quitting the main runloop.
  bool clean_exit = false;

  // Counts of remaining child processes with corresponding binding.
  int remaining_process_with_strong_binding = 0;
  int remaining_process_with_moderate_binding = 0;
  int remaining_process_with_waived_binding = 0;

  // Eg lowest ranked process at time of death should have value 0.
  // Valid values are non-negative.
  // -1 means could not be obtained due to threading restrictions.
  // -2 means not applicable because process is not ranked.
  int best_effort_reverse_rank = -1;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_TERMINATION_INFO_H_

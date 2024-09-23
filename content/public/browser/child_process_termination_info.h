// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_TERMINATION_INFO_H_
#define CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_TERMINATION_INFO_H_

#include <optional>

#include "base/process/kill.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/result_codes.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/child_process_binding_types.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
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
  int exit_code = RESULT_CODE_NORMAL_EXIT;

#if BUILDFLAG(IS_ANDROID)
  // Populated only for renderer process. True if there are any visible
  // clients at the time of process death.
  bool renderer_has_visible_clients = false;

  // Populated only for renderer process. True if
  // RenderProcessHost::GetFrameDepth is bigger than 0. Note this is not exactly
  // the same as not having main frames.
  bool renderer_was_subframe = false;

  // Child service binding state at time of death.
  base::android::ChildBindingState binding_state =
      base::android::ChildBindingState::UNBOUND;

  // True if child service was explicitly killed by browser.
  bool was_killed_intentionally_by_browser = false;

  // True if child process threw an exception before calling into main.
  bool threw_exception_during_init = false;

  // True if the child shut itself down cleanly by quitting the main runloop.
  bool clean_exit = false;
#endif

#if BUILDFLAG(IS_WIN)
  // The LastError if there was a failure to launch the process.
  DWORD last_error;
#endif

#if !BUILDFLAG(IS_ANDROID)
  // The cumulative CPU usage of this process, if available.
  std::optional<base::TimeDelta> cpu_usage;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_TERMINATION_INFO_H_

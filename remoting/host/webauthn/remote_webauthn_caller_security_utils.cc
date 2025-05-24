// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_caller_security_utils.h"

#include <optional>

#include "base/environment.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/process/process_handle.h"
#include "base/strings/cstring_view.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#include "base/containers/fixed_flat_set.h"
#include "base/files/file_path.h"
#include "remoting/host/base/process_util.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/win/scoped_handle.h"
#include "remoting/host/win/trust_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include <Security/Security.h>
#include <mach/kern_return.h>
#include <unistd.h>

#include <array>

#include "remoting/host/mac/trust_util.h"
#endif

namespace remoting {
namespace {

#if !defined(NDEBUG)

// No static variables needed for debug builds.

#elif BUILDFLAG(IS_LINUX)

constexpr auto kAllowedCallerPrograms =
    base::MakeFixedFlatSet<base::FilePath::StringViewType>({
        "/opt/google/chrome/chrome",
        "/opt/google/chrome-beta/chrome",
        "/opt/google/chrome-canary/chrome",
        "/opt/google/chrome-unstable/chrome",
    });

#elif BUILDFLAG(IS_WIN)

// Names of environment variables that store the path to directories where apps
// are installed.
constexpr auto kAppsDirectoryEnvVars =
    base::MakeFixedFlatSet<base::cstring_view>({
        "PROGRAMFILES",

        // May happen if Chrome is upgraded from a 32-bit version.
        "PROGRAMFILES(X86)",

        // Refers to "C:\Program Files" if current process is 32-bit.
        "ProgramW6432",

        // For per-user installations.
        "LOCALAPPDATA",
    });

// Relative to the Program Files directory.
constexpr auto kAllowedCallerPrograms =
    base::MakeFixedFlatSet<base::FilePath::StringViewType>({
        L"Google\\Chrome\\Application\\chrome.exe",
        L"Google\\Chrome Beta\\Application\\chrome.exe",
        L"Google\\Chrome SxS\\Application\\chrome.exe",
        L"Google\\Chrome Dev\\Application\\chrome.exe",
    });

#elif BUILDFLAG(IS_MAC)

constexpr auto kAllowedIdentifiers = std::to_array<const std::string_view>(
    {"com.google.Chrome", "com.google.Chrome.beta", "com.google.Chrome.dev",
     "com.google.Chrome.canary"});

#endif

}  // namespace

bool IsLaunchedByTrustedProcess() {
#if !defined(NDEBUG)
  // Just return true on debug builds for the convenience of development.
  return true;
#elif BUILDFLAG(IS_LINUX)
  base::ProcessId parent_pid =
      base::GetParentProcessId(base::GetCurrentProcessHandle());
  // Note that on Linux the process image may no longer exist in its original
  // path. This will happen when Chrome has been updated but hasn't been
  // relaunched. The path can still be trusted since it's not spoofable even if
  // it's no longer pointing to the current Chrome binary.
  base::FilePath parent_image_path = GetProcessImagePath(parent_pid);
  return kAllowedCallerPrograms.contains(parent_image_path.value());
#elif BUILDFLAG(IS_WIN)
  auto environment = base::Environment::Create();

  base::ProcessId parent_pid =
      base::GetParentProcessId(base::GetCurrentProcessHandle());
  base::FilePath parent_image_path = GetProcessImagePath(parent_pid);

  // On Windows, Chrome launches native messaging hosts via cmd for stdio
  // communication. See:
  //   chrome/browser/extensions/api/messaging/native_process_launcher_win.cc
  // Therefore, we check if the parent is cmd and skip to the grandparent if
  // that's the case. It's possible to do stdio communications without cmd, so
  // we don't require the parent to always be cmd.

  // COMSPEC is generally "C:\WINDOWS\system32\cmd.exe". Note that the casing
  // does not match the actual file path's casing.
  std::optional<std::string> comspec_utf8 = environment->GetVar("COMSPEC");
  if (comspec_utf8.has_value()) {
    base::FilePath::StringType comspec = base::UTF8ToWide(comspec_utf8.value());
    if (base::FilePath::CompareEqualIgnoreCase(parent_image_path.value(),
                                               comspec)) {
      // Skip to the grandparent.
      base::win::ScopedHandle parent_handle(
          OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, parent_pid));
      if (parent_handle.is_valid()) {
        parent_pid = base::GetParentProcessId(parent_handle.Get());
        parent_image_path = GetProcessImagePath(parent_pid);
      } else {
        PLOG(ERROR) << "Failed to query parent info.";
      }
    }
  } else {
    LOG(ERROR) << "COMSPEC is not set";
  }

  // Check if the caller's image path is allowlisted.
  for (base::cstring_view apps_dir_env_var : kAppsDirectoryEnvVars) {
    std::optional<std::string> apps_dir_path_utf8 =
        environment->GetVar(apps_dir_env_var);
    if (!apps_dir_path_utf8.has_value()) {
      continue;
    }
    auto apps_dir_path =
        base::FilePath::FromUTF8Unsafe(apps_dir_path_utf8.value());
    if (!apps_dir_path.IsParent(parent_image_path)) {
      continue;
    }
    for (const base::FilePath::StringViewType& allowed_caller_program :
         kAllowedCallerPrograms) {
      base::FilePath allowed_caller_program_full_path =
          apps_dir_path.Append(allowed_caller_program);
      if (base::FilePath::CompareEqualIgnoreCase(
              parent_image_path.value(),
              allowed_caller_program_full_path.value())) {
        // Caller's path is allowlisted, now also check if it's properly signed.
        return IsBinaryTrusted(parent_image_path);
      }
    }
  }
  // Caller's path is not allowlisted.
  return false;
#elif BUILDFLAG(IS_MAC)
  // TODO: crbug.com/410903981 - move away from PID-based security checks, which
  // might be susceptible of PID reuse attacks, if Apple provides APIs to query
  // parent process audit token without using a PPID.
  base::ProcessId parent_pid = getppid();
  kern_return_t kern_return;
  task_name_t task;

  kern_return = task_name_for_pid(mach_task_self(), parent_pid, &task);
  if (kern_return != KERN_SUCCESS) {
    LOG(ERROR) << "Failed to get task name for parent PID " << parent_pid
               << ": " << kern_return;
    return false;
  }

  mach_msg_type_number_t size = TASK_AUDIT_TOKEN_COUNT;
  audit_token_t audit_token;
  kern_return =
      task_info(task, TASK_AUDIT_TOKEN, (task_info_t)&audit_token, &size);
  mach_port_deallocate(mach_task_self(), task);
  if (kern_return != KERN_SUCCESS) {
    LOG(ERROR) << "Failed to get audit token for parent PID " << parent_pid
               << ": " << kern_return;
    return false;
  }

  return IsProcessTrusted(audit_token, kAllowedIdentifiers);
#else  // Unsupported platform
  NOTIMPLEMENTED();
  return true;
#endif
}

}  // namespace remoting

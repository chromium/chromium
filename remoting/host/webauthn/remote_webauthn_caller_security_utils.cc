// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_caller_security_utils.h"

#include <optional>

#include "base/base_paths.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
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

// Keys for base::PathService to retrieve paths to directories where apps are
// installed.
constexpr int kAppsDirectoryPathKeys[] = {
    base::DIR_PROGRAM_FILES,
    base::DIR_PROGRAM_FILESX86,
    base::DIR_PROGRAM_FILES6432,
    base::DIR_LOCAL_APP_DATA,
};

// Relative to the Program Files directory.
constexpr auto kAllowedCallerPrograms =
    base::MakeFixedFlatSet<base::FilePath::StringViewType>({
        L"Google\\Chrome\\Application\\chrome.exe",
        L"Google\\Chrome Beta\\Application\\chrome.exe",
        L"Google\\Chrome SxS\\Application\\chrome.exe",
        L"Google\\Chrome Dev\\Application\\chrome.exe",
    });

// Helper to verify that the cmd.exe binary is located in a trusted system
// directory.
bool IsSystemCmd(const base::FilePath& process_image_path) {
  if (!base::FilePath::CompareEqualIgnoreCase(
          process_image_path.BaseName().value(), L"cmd.exe")) {
    return false;
  }

  base::FilePath process_dir = process_image_path.DirName();

  base::FilePath system_dir;
  if (base::PathService::Get(base::DIR_SYSTEM, &system_dir)) {
    if (base::FilePath::CompareEqualIgnoreCase(process_dir.value(),
                                               system_dir.value())) {
      return true;
    }
  }

  // Also check for the native system directory if it's different (e.g.
  // SysWOW64 vs System32).
  base::FilePath windows_dir;
  if (base::PathService::Get(base::DIR_WINDOWS, &windows_dir)) {
    if (base::FilePath::CompareEqualIgnoreCase(
            process_dir.value(), windows_dir.Append(L"System32").value()) ||
        base::FilePath::CompareEqualIgnoreCase(
            process_dir.value(), windows_dir.Append(L"SysWOW64").value())) {
      return true;
    }
  }

  return false;
}

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
  base::ProcessId parent_pid =
      base::GetParentProcessId(base::GetCurrentProcessHandle());
  base::FilePath parent_image_path = GetProcessImagePath(parent_pid);

  // On Windows, Chrome launches native messaging hosts via cmd for stdio
  // communication. See:
  //   chrome/browser/extensions/api/messaging/native_process_launcher_win.cc
  // Therefore, we check if the parent is cmd and skip to the grandparent if
  // that's the case. It's possible to do stdio communications without cmd, so
  // we don't require the parent to always be cmd.

  // Use IsSystemCmd() for comparison as a 64-bit Chrome launches a 64-bit
  // cmd.exe which is in C:\Windows\System32, but for a 32-bit native messaging
  // host, C:\Windows\System32 will be redirected to C:\Windows\SysWOW64, so we
  // perform case-insensitive directory checks.
  if (IsSystemCmd(parent_image_path)) {
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

  // Check if the caller's image path is allowlisted.
  for (int path_key : kAppsDirectoryPathKeys) {
    base::FilePath apps_dir_path;
    if (!base::PathService::Get(path_key, &apps_dir_path)) {
      continue;
    }
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

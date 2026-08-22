// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/crashpad/crashpad/handler/handler_main.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <stdlib.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include <grp.h>
#include <linux/capability.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "remoting/base/crash/crashpad_database_manager.h"
#include "remoting/base/passwd_utils.h"
#include "remoting/base/username.h"

namespace {

class CrashpadHandlerLogger : public remoting::CrashpadDatabaseManager::Logger {
 public:
  void Log(std::string_view message) const override { LOG(INFO) << message; }
  void LogError(std::string_view message) const override {
    LOG(ERROR) << message;
  }
};

void DropPrivilegesToCrashpadUser() {
  if (getuid() != 0) {
    LOG(WARNING) << "Crashpad handler is not running as root (" << getuid()
                 << "). Skipping privilege dropping to "
                 << remoting::GetCrashpadProcessUsername();
    return;
  }

  // 1. Drop supplementary groups.
  if (setgroups(0, nullptr) != 0) {
    PLOG(FATAL) << "setgroups(0, nullptr) failed";
  }

  auto user_info =
      remoting::GetPasswdUserInfo(remoting::GetCrashpadProcessUsername());
  if (!user_info.has_value()) {
    LOG(FATAL) << "Failed to find user "
               << remoting::GetCrashpadProcessUsername() << ": "
               << user_info.error();
  }

  // Preserve capabilities across UID/GID change.
  if (prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0) != 0) {
    PLOG(FATAL) << "prctl(PR_SET_KEEPCAPS) failed";
  }

  if (setresgid(user_info->gid, user_info->gid, user_info->gid) != 0 ||
      setresuid(user_info->uid, user_info->uid, user_info->uid) != 0) {
    PLOG(FATAL) << "Failed to drop UID/GID to "
                << remoting::GetCrashpadProcessUsername();
  }

  // 2. Retain required capabilities in effective and permitted sets:
  // - `CAP_SYS_PTRACE`: Allows inspecting process memory, stack, registers, and
  //   thread state of crashed processes across different UIDs.
  // - `CAP_KILL`: Allows sending `SIGCONT` via `tgkill` to wake up crashed
  //   client processes after the minidump is written.
  // - `CAP_DAC_READ_SEARCH`: Allows reading `/proc/<pid>/auxv` to locate ELF
  //   `AT_PHDR` program headers for module resolution without requiring write
  //   permissions.
  struct __user_cap_header_struct hdr = {
      .version = _LINUX_CAPABILITY_VERSION_3,
      .pid = 0,
  };
  struct __user_cap_data_struct data[_LINUX_CAPABILITY_U32S_3] = {};
  data[CAP_TO_INDEX(CAP_SYS_PTRACE)].effective |= CAP_TO_MASK(CAP_SYS_PTRACE);
  data[CAP_TO_INDEX(CAP_SYS_PTRACE)].permitted |= CAP_TO_MASK(CAP_SYS_PTRACE);
  data[CAP_TO_INDEX(CAP_KILL)].effective |= CAP_TO_MASK(CAP_KILL);
  data[CAP_TO_INDEX(CAP_KILL)].permitted |= CAP_TO_MASK(CAP_KILL);
  data[CAP_TO_INDEX(CAP_DAC_READ_SEARCH)].effective |=
      CAP_TO_MASK(CAP_DAC_READ_SEARCH);
  data[CAP_TO_INDEX(CAP_DAC_READ_SEARCH)].permitted |=
      CAP_TO_MASK(CAP_DAC_READ_SEARCH);
  data[CAP_TO_INDEX(CAP_SYS_PTRACE)].inheritable = 0;

  if (syscall(SYS_capset, &hdr, data) != 0) {
    PLOG(FATAL) << "Failed to retain capabilities via SYS_capset";
  }

  // 3. Clear keepcaps and prevent gaining new privileges via SUID.
  if (prctl(PR_SET_KEEPCAPS, 0, 0, 0, 0) != 0) {
    PLOG(FATAL) << "prctl(PR_SET_KEEPCAPS, 0) failed";
  }
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
    PLOG(FATAL) << "prctl(PR_SET_NO_NEW_PRIVS, 1) failed";
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  DropPrivilegesToCrashpadUser();

  base::CommandLine::Init(argc, argv);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch("database")) {
    base::FilePath database_path = command_line->GetSwitchValuePath("database");
    CrashpadHandlerLogger logger;
    remoting::CrashpadDatabaseManager database_manager(logger);
    if (database_manager.InitializeCrashpadDatabase(database_path)) {
      database_manager.EnableReportUploads();
      database_manager.LogCompletedCrashpadReports();
      database_manager.LogPendingCrashpadReports();
      database_manager.CleanupCompletedCrashpadReports();
    } else {
      LOG(ERROR) << "Failed to initialize Crashpad database at "
                 << database_path;
    }
  }

  return crashpad::HandlerMain(argc, argv,
                               /*user_stream_data_sources=*/nullptr);
}

#elif BUILDFLAG(IS_WIN)

namespace {

int HandlerMainAdaptor(int argc, char* argv[]) {
  return crashpad::HandlerMain(argc, argv,
                               /*user_stream_data_sources=*/nullptr);
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
  // Convert wide strings to skinny strings.
  auto argv_as_utf8 = base::HeapArray<char*>::Uninit(argc + 1);
  std::vector<std::string> storage;
  storage.reserve(argc);
  auto argv_span = UNSAFE_BUFFERS(base::span<wchar_t*>(
      argv, static_cast<size_t>(argc)));  // SAFETY: argv,argc come from os.
  for (int i = 0; i < argc; ++i) {
    storage.push_back(base::WideToUTF8(argv_span[i]));
    argv_as_utf8[i] = &storage[i][0];
  }
  argv_as_utf8[argc] = nullptr;
  return HandlerMainAdaptor(argc, argv_as_utf8.data());
}

#endif  // BUILDFLAG(IS_LINUX)

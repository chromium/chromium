// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <ios>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/containers/heap_array.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/access_token.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "build/build_config.h"
#include "remoting/base/branding.h"
#include "remoting/base/crash/crashpad_database_manager.h"
#include "remoting/base/crash/crashpad_win.h"
#include "remoting/base/logging.h"
#include "remoting/base/scoped_sc_handle_win.h"

namespace remoting {
namespace {

constexpr char kHelpSwitch[] = "help";
constexpr char kQuestionSwitch[] = "?";
constexpr char kStatusSwitch[] = "status";
constexpr char kDbPathSwitch[] = "db-path";
constexpr char kCrashPidSwitch[] = "crash-pid";

// Switches consumed by base logging (see logging::InitLogging()).
constexpr char kVerboseLoggingSwitch[] = "v";
constexpr char kVmoduleSwitch[] = "vmodule";

constexpr std::string_view kKnownSwitches[] = {
    kHelpSwitch,     kQuestionSwitch,       kStatusSwitch,  kDbPathSwitch,
    kCrashPidSwitch, kVerboseLoggingSwitch, kVmoduleSwitch,
};

constexpr wchar_t kCrashpadSubdir[] = L"crashpad";

// When the Crashpad handler does not respond to a crash, the Crashpad client in
// the target process waits 60 seconds (kMillisecondsUntilTerminate in
// crashpad_client_win.cc) and then terminates itself with
// kTerminationCodeCrashNoDump. Wait longer than that so the tester observes the
// exit code rather than timing out first.
constexpr DWORD kCrashProcessWaitTimeoutMs = 90'000;

constexpr int kSuccessExitCode = 0;
constexpr int kUsageExitCode = 1;
constexpr int kErrorExitCode = 2;

class ConsoleCrashpadLogger final : public CrashpadDatabaseManager::Logger {
 public:
  void Log(std::string_view message) const override {
    std::cout << "  " << message << '\n';
  }
  void LogError(std::string_view message) const override {
    std::cerr << "  ERROR: " << message << '\n';
  }
};

void PrintUsage(std::ostream& out, const base::FilePath& program) {
  out << "\nUsage: " << base::WideToUTF8(program.BaseName().value())
      << " [options]\n\n"
      << "Options:\n"
      << "  --status            Print active HKLM Crashpad overrides and\n"
      << "                      recent completed/pending crash reports.\n"
      << "  --db-path=<path>    Inspect a specific Crashpad database path\n"
      << "                      (requires --status).\n"
      << "  --crash-pid=<pid>   Trigger an unhandled access violation in\n"
      << "                      target process <pid> via CreateRemoteThread\n"
      << "                      and wait for the process to exit. The exit\n"
      << "                      code does not show whether Crashpad captured\n"
      << "                      the crash; use --status to look for a new\n"
      << "                      report.\n"
      << "  --help, --?         Show this usage message.\n\n"
      << "Note: Crashing or inspecting LocalSystem host processes requires an\n"
      << "elevated Administrator (or SYSTEM) command prompt.\n\n"
      << "Note: The registry key and per-user database path are fixed at\n"
      << "build time by branding. Use a tester built with the same\n"
      << "branding (official or Chromium) as the installed host;\n"
      << "otherwise --status reads a different key and per-user directory\n"
      << "than the host uses.\n";
}

// Returns false and reports each unrecognized switch or positional argument.
bool ValidateCommandLine(const base::CommandLine& cmd) {
  bool ok = true;
  for (const auto& [name, value] : cmd.GetSwitches()) {
    if (!std::ranges::contains(kKnownSwitches, std::string_view(name))) {
      std::cerr << "ERROR: Unknown switch --" << name << ".\n";
      ok = false;
    }
  }
  if (!cmd.GetArgs().empty()) {
    std::cerr << "ERROR: Unexpected positional arguments.\n";
    ok = false;
  }
  return ok;
}

std::optional<base::FilePath> QueryInstalledServiceDatabasePath() {
  ScopedScHandle scm(
      ::OpenSCManagerW(nullptr, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CONNECT));
  if (!scm.is_valid()) {
    return std::nullopt;
  }
  ScopedScHandle service(
      ::OpenServiceW(scm.get(), kWindowsServiceName, SERVICE_QUERY_CONFIG));
  if (!service.is_valid()) {
    return std::nullopt;
  }

  DWORD bytes_needed = 0;
  if (!::QueryServiceConfigW(service.get(), nullptr, 0, &bytes_needed) &&
      ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return std::nullopt;
  }

  auto buffer = base::HeapArray<uint8_t>::Uninit(bytes_needed);
  auto* config = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.data());
  if (!::QueryServiceConfigW(service.get(), config, bytes_needed,
                             &bytes_needed) ||
      !config->lpBinaryPathName) {
    return std::nullopt;
  }

  const base::FilePath service_exe =
      base::CommandLine::FromString(config->lpBinaryPathName).GetProgram();
  if (service_exe.empty() || !base::PathExists(service_exe)) {
    return std::nullopt;
  }
  return service_exe.DirName().Append(kCrashpadSubdir);
}

// Prints the Crashpad registry overrides. Returns false if the registry key
// exists but could not be opened.
bool PrintRegistryStatus() {
  std::cout << "Registry overrides (HKLM\\"
            << base::WideToUTF8(kCrashpadRegistryKeyPath) << "):\n";
  base::win::RegKey key;
  const LONG open_res =
      key.Open(HKEY_LOCAL_MACHINE, kCrashpadRegistryKeyPath, KEY_READ);
  if (open_res != ERROR_SUCCESS) {
    if (open_res == ERROR_FILE_NOT_FOUND) {
      std::cout
          << "  <not configured> (using production upload URL and default "
             "rate limit)\n";
      return true;
    }
    std::cerr << "  ERROR: Unable to open registry key (error=" << open_res
              << ")\n";
    return false;
  }

  // The host reads these values once, in CrashpadWin::Initialize(), so the
  // values shown here can differ from what a running host was started with.
  std::cout << "  (Read by the host at startup; restart the host to apply "
               "changes.)\n";

  // ReadRegistryConfig() logs each value it rejects. Silence those log lines
  // because every rejected value is also reported in the output below.
  const int previous_min_log_level = logging::GetMinLogLevel();
  logging::SetMinLogLevel(logging::LOGGING_FATAL);
  const CrashpadWin::RegistryConfig config = CrashpadWin::ReadRegistryConfig();
  logging::SetMinLogLevel(previous_min_log_level);
  if (config.upload_url.has_value()) {
    std::cout << "  UploadUrl: " << *config.upload_url << '\n';
  } else {
    DWORD url_bytes = 0;
    DWORD url_type = 0;
    const LONG url_res =
        key.ReadValue(kCrashpadUploadUrlValue, nullptr, &url_bytes, &url_type);
    if (url_res == ERROR_FILE_NOT_FOUND) {
      std::cout << "  UploadUrl: <default production URL>\n";
    } else if (url_res != ERROR_SUCCESS || url_type != REG_SZ) {
      std::cout << "  UploadUrl: <unreadable or non-REG_SZ (error=" << url_res
                << ", type=" << url_type
                << ")> (ignored; using production URL)\n";
    } else {
      std::cout
          << "  UploadUrl: <invalid REG_SZ value> (ignored; using production "
             "URL)\n";
    }
  }

  DWORD rate_val = 0;
  DWORD rate_bytes = sizeof(rate_val);
  DWORD rate_type = 0;
  const LONG rate_res = key.ReadValue(kCrashpadDisableRateLimitValue, &rate_val,
                                      &rate_bytes, &rate_type);
  if (rate_res != ERROR_FILE_NOT_FOUND &&
      (rate_res != ERROR_SUCCESS || rate_type != REG_DWORD ||
       rate_bytes != sizeof(rate_val))) {
    std::cout << "  DisableRateLimit: <unreadable or non-REG_DWORD (error="
              << rate_res << ", type=" << rate_type
              << ")> (ignored; using false)\n";
  } else {
    std::cout << "  DisableRateLimit: "
              << (config.disable_rate_limit ? "true (--no-rate-limit)"
                                            : "false")
              << '\n';
  }
  return true;
}

// Returns true if `db_path` contains both subdirectories that Crashpad creates
// when it initializes a database on Windows.
bool HasCrashpadDatabaseMarkers(const base::FilePath& db_path) {
  return base::DirectoryExists(db_path.Append(L"reports")) &&
         base::DirectoryExists(db_path.Append(L"attachments"));
}

bool PrintDatabaseStatus(std::string_view label,
                         const base::FilePath& db_path,
                         bool is_explicit_path) {
  std::cout << '\n'
            << label << " (" << base::WideToUTF8(db_path.value()) << "):\n";
  if (db_path.empty()) {
    std::cerr << "  ERROR: Database path is empty.\n";
    return false;
  }

  base::win::ScopedHandle dir_handle(
      ::CreateFileW(db_path.value().c_str(), FILE_LIST_DIRECTORY,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    /*lpSecurityAttributes=*/nullptr, OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS, /*hTemplateFile=*/nullptr));
  if (!dir_handle.is_valid()) {
    const DWORD error = ::GetLastError();
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
      if (is_explicit_path) {
        std::cerr << "  ERROR: Database directory does not exist.\n";
        return false;
      }
      std::cout << "  <directory does not exist>\n";
      return true;
    }
    std::cerr << "  ERROR: Unable to open directory (Win32 error " << error
              << ").\n";
    if (error == ERROR_ACCESS_DENIED) {
      std::cerr << "  Hint: Run from an elevated Administrator prompt to "
                   "inspect the system Crashpad database.\n";
    }
    return false;
  }

  BY_HANDLE_FILE_INFORMATION file_info = {};
  if (!::GetFileInformationByHandle(dir_handle.get(), &file_info) ||
      !(file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
    std::cerr << "  ERROR: Path is not a directory.\n";
    return false;
  }
  dir_handle.Close();

  // CrashReportDatabaseWin::InitializeWithoutCreating() still creates the
  // reports and attachments subdirectories inside an existing directory, and
  // GetCompletedReports() creates the metadata file via OPEN_ALWAYS. Require
  // both subdirectories to exist first so inspection never creates directories
  // and only writes the metadata file into a directory that already has the
  // Crashpad database layout.
  if (!HasCrashpadDatabaseMarkers(db_path)) {
    if (is_explicit_path) {
      std::cerr << "  ERROR: Directory is not an initialized Crashpad "
                   "database.\n";
      return false;
    }
    std::cout << "  <not initialized>\n";
    return true;
  }

  ConsoleCrashpadLogger logger;
  CrashpadDatabaseManager db_manager(logger);
  if (!db_manager.InitializeCrashpadDatabaseWithoutCreating(db_path)) {
    return false;
  }
  db_manager.LogCompletedCrashpadReports();
  db_manager.LogPendingCrashpadReports();
  return true;
}

int RunStatus(const base::CommandLine& cmd) {
  bool ok = PrintRegistryStatus();

  if (cmd.HasSwitch(kDbPathSwitch)) {
    const base::FilePath explicit_path = cmd.GetSwitchValuePath(kDbPathSwitch);
    if (explicit_path.empty()) {
      std::cerr << "ERROR: --db-path requires a non-empty path.\n";
      return kUsageExitCode;
    }
    ok = PrintDatabaseStatus("Crashpad database", explicit_path,
                             /*is_explicit_path=*/true) &&
         ok;
    return ok ? kSuccessExitCode : kErrorExitCode;
  }

  base::FilePath system_db_path =
      QueryInstalledServiceDatabasePath().value_or(base::FilePath());
  if (system_db_path.empty()) {
    base::FilePath assets_dir;
    if (base::PathService::Get(base::DIR_ASSETS, &assets_dir)) {
      system_db_path = assets_dir.Append(kCrashpadSubdir);
    }
  }

  if (!system_db_path.empty()) {
    ok = PrintDatabaseStatus("System Crashpad database", system_db_path,
                             /*is_explicit_path=*/false) &&
         ok;
  }

  if (!IsCurrentProcessLocalSystem()) {
    const base::FilePath user_data_dir = GetPerUserDataDir();
    if (!user_data_dir.empty()) {
      const base::FilePath user_db_path = user_data_dir.Append(kCrashpadSubdir);
      if (user_db_path != system_db_path) {
        ok = PrintDatabaseStatus("Per-user Crashpad database", user_db_path,
                                 /*is_explicit_path=*/false) &&
             ok;
      }
    }
  }

  return ok ? kSuccessExitCode : kErrorExitCode;
}

constexpr USHORT GetCurrentBuildMachine() {
#if defined(ARCH_CPU_X86_64)
  return IMAGE_FILE_MACHINE_AMD64;
#elif defined(ARCH_CPU_ARM64)
  return IMAGE_FILE_MACHINE_ARM64;
#elif defined(ARCH_CPU_X86)
  return IMAGE_FILE_MACHINE_I386;
#else
#error Unsupported Windows CPU architecture
#endif
}

std::optional<USHORT> GetTargetProcessMachine(HANDLE process) {
  // ProcessMachineTypeInfo (Windows 11 / Server 2022+) reports
  // IMAGE_FILE_MACHINE_AMD64 for x64-emulated processes on ARM64, whereas
  // IsWow64Process2 reports IMAGE_FILE_MACHINE_UNKNOWN because x64 emulation is
  // not WOW64.
  PROCESS_MACHINE_INFORMATION machine_info = {};
  if (::GetProcessInformation(process, ProcessMachineTypeInfo, &machine_info,
                              sizeof(machine_info))) {
    return machine_info.ProcessMachine;
  }

  // Fallback for Windows 10 (where ARM64 has no x64 emulation).
  USHORT process_machine = IMAGE_FILE_MACHINE_UNKNOWN;
  USHORT native_machine = IMAGE_FILE_MACHINE_UNKNOWN;
  if (::IsWow64Process2(process, &process_machine, &native_machine)) {
    return process_machine == IMAGE_FILE_MACHINE_UNKNOWN ? native_machine
                                                         : process_machine;
  }
  return std::nullopt;
}

// Exit codes that a Crashpad client terminates its process with when the crash
// could not be handled normally. These mirror crashpad::TerminationCodes in
// third_party/crashpad/crashpad/util/win/termination_codes.h.
constexpr DWORD kCrashpadTerminationCodeCrashNoDump = 0xFFFF7001;
constexpr DWORD kCrashpadTerminationCodeSnapshotFailed = 0xFFFF7002;
constexpr DWORD kCrashpadTerminationCodeNotConnectedToHandler = 0xFFFF7003;

// Returns a description of `exit_code` if it is a Crashpad termination code, or
// an empty string otherwise.
std::string_view DescribeCrashpadTerminationCode(DWORD exit_code) {
  switch (exit_code) {
    case kCrashpadTerminationCodeCrashNoDump:
      return " This is Crashpad's kTerminationCodeCrashNoDump: the crash "
             "handler did not respond, so the client terminated itself.";
    case kCrashpadTerminationCodeSnapshotFailed:
      return " This is Crashpad's kTerminationCodeSnapshotFailed: the crash "
             "handler could not take the initial process snapshot.";
    case kCrashpadTerminationCodeNotConnectedToHandler:
      return " This is Crashpad's kTerminationCodeNotConnectedToHandler: the "
             "process was never registered with the crash handler.";
    default:
      return std::string_view();
  }
}

int CrashProcessByPid(std::string_view pid_str) {
  unsigned int pid = 0;
  if (!base::StringToUint(pid_str, &pid) || pid == 0) {
    LOG(ERROR) << "Invalid process PID: " << pid_str;
    return kUsageExitCode;
  }
  if (pid == ::GetCurrentProcessId()) {
    LOG(ERROR) << "Refusing to crash current tester process (PID " << pid
               << ").";
    return kUsageExitCode;
  }

  // Enable SeDebugPrivilege when present (e.g. elevated Administrator prompt)
  // so OpenProcess can obtain PROCESS_CREATE_THREAD / VM rights on LocalSystem
  // and LocalService host processes whose default DACLs only grant read access
  // to Administrators.
  if (auto token = base::win::AccessToken::FromCurrentProcess(
          /*impersonation=*/false, TOKEN_ADJUST_PRIVILEGES);
      !token.has_value() ||
      !token->SetPrivilege(SE_DEBUG_NAME, /*enable=*/true).has_value()) {
    VLOG(1) << "SeDebugPrivilege was not enabled; continuing with default "
               "token access.";
  }

  constexpr DWORD kDesiredAccess =
      PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
      PROCESS_VM_WRITE | PROCESS_VM_READ | SYNCHRONIZE;
  base::win::ScopedHandle process(
      ::OpenProcess(kDesiredAccess, /*bInheritHandle=*/FALSE, pid));
  if (!process.is_valid()) {
    const DWORD error = ::GetLastError();
    PLOG(ERROR) << "Failed to open process " << pid;
    if (error == ERROR_ACCESS_DENIED) {
      std::cerr << "Hint: Run from an elevated Administrator prompt, or as "
                   "SYSTEM (e.g. 'psexec -s') when targeting LocalSystem host "
                   "processes.\n";
    }
    return kErrorExitCode;
  }

  BOOL is_critical = FALSE;
  if (::IsProcessCritical(process.get(), &is_critical) && is_critical) {
    LOG(ERROR) << "Refusing to crash critical system process (PID " << pid
               << ").";
    return kUsageExitCode;
  }

  const std::optional<USHORT> target_machine =
      GetTargetProcessMachine(process.get());
  if (!target_machine.has_value()) {
    PLOG(ERROR) << "Failed to determine architecture of target PID " << pid;
    return kErrorExitCode;
  }
  constexpr USHORT kSelfMachine = GetCurrentBuildMachine();
  if (*target_machine != kSelfMachine) {
    LOG(ERROR) << "Architecture mismatch between tester (0x" << std::hex
               << kSelfMachine << ") and target PID " << std::dec << pid
               << " (0x" << std::hex << *target_machine << std::dec << ").";
    return kErrorExitCode;
  }

  // Resolve kernel32!EnterCriticalSection, which forwards to
  // ntdll!RtlEnterCriticalSection — a CFG-valid export mapped at the same base
  // address in every same-architecture process in the boot session. Calling it
  // with lpParameter == nullptr raises a user-mode EXCEPTION_ACCESS_VIOLATION
  // that reaches RtlUserThreadStart's unhandled exception filter (Crashpad).
  HMODULE kernel32 = ::GetModuleHandleW(L"kernel32.dll");
  auto crash_start_routine = reinterpret_cast<LPTHREAD_START_ROUTINE>(
      kernel32 ? ::GetProcAddress(kernel32, "EnterCriticalSection") : nullptr);
  if (!crash_start_routine) {
    PLOG(ERROR) << "Failed to resolve kernel32!EnterCriticalSection";
    return kErrorExitCode;
  }

  DWORD thread_id = 0;
  base::win::ScopedHandle thread(
      ::CreateRemoteThread(process.get(), nullptr, 0, crash_start_routine,
                           /*lpParameter=*/nullptr, 0, &thread_id));
  if (!thread.is_valid()) {
    const DWORD error = ::GetLastError();
    PLOG(ERROR) << "Failed to create remote thread in process " << pid;
    // The error code that a cross-session target produces has not been
    // confirmed, so the hint is worded as one possible cause. It is skipped for
    // access denied, which the log line above already explains.
    if (error != ERROR_ACCESS_DENIED) {
      std::cerr << "Hint: One possible cause is a target that runs in a "
                   "different session (e.g. the session 0 host service). If "
                   "that applies, retry from that session, for example as "
                   "SYSTEM via 'psexec -s'.\n";
    }
    return kErrorExitCode;
  }

  const DWORD wait_res =
      ::WaitForSingleObject(process.get(), kCrashProcessWaitTimeoutMs);
  if (wait_res != WAIT_OBJECT_0) {
    LOG(ERROR) << "Timed out or failed waiting for target PID " << pid
               << " to terminate after crash injection (wait_res=" << wait_res
               << ").";
    return kErrorExitCode;
  }

  DWORD exit_code = 0;
  if (!::GetExitCodeProcess(process.get(), &exit_code)) {
    PLOG(ERROR) << "Failed to query exit code of crashed PID " << pid;
    return kErrorExitCode;
  }

  std::cout << "Triggered access violation in PID " << pid << " (remote thread "
            << thread_id << ", exit code 0x" << std::hex << std::uppercase
            << exit_code << std::dec << std::nouppercase << ").\n";
  // An EXCEPTION_ACCESS_VIOLATION exit code shows that the injected crash
  // terminated the process. It does not show that Crashpad captured it: a
  // process without a Crashpad client exits with the same code.
  if (exit_code != EXCEPTION_ACCESS_VIOLATION) {
    LOG(WARNING) << "Target process exited with unexpected code 0x" << std::hex
                 << std::uppercase << exit_code << std::dec << std::nouppercase
                 << " (expected 0xC0000005 EXCEPTION_ACCESS_VIOLATION)."
                 << DescribeCrashpadTerminationCode(exit_code);
  }
  return kSuccessExitCode;
}

int CrashpadTesterMain(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::AtExitManager exit_manager;
  InitHostLogging();

  const base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(kHelpSwitch) || cmd->HasSwitch(kQuestionSwitch)) {
    PrintUsage(std::cout, cmd->GetProgram());
    return kSuccessExitCode;
  }

  if (!ValidateCommandLine(*cmd)) {
    PrintUsage(std::cerr, cmd->GetProgram());
    return kUsageExitCode;
  }

  const bool has_status = cmd->HasSwitch(kStatusSwitch);
  const bool has_crash_pid = cmd->HasSwitch(kCrashPidSwitch);
  if (cmd->HasSwitch(kDbPathSwitch) && !has_status) {
    std::cerr << "ERROR: --db-path can only be used with --status.\n";
    PrintUsage(std::cerr, cmd->GetProgram());
    return kUsageExitCode;
  }
  if (!has_status && !has_crash_pid) {
    PrintUsage(std::cerr, cmd->GetProgram());
    return kUsageExitCode;
  }

  if (has_crash_pid) {
    const int rc = CrashProcessByPid(cmd->GetSwitchValueASCII(kCrashPidSwitch));
    if (rc != kSuccessExitCode) {
      return rc;
    }
  }

  if (has_status) {
    return RunStatus(*cmd);
  }

  return kSuccessExitCode;
}

}  // namespace
}  // namespace remoting

int main(int argc, char** argv) {
  return remoting::CrashpadTesterMain(argc, argv);
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <initializer_list>
#include <ios>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

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
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
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
constexpr char kSetStagingSwitch[] = "set-staging";
constexpr char kSetUploadUrlSwitch[] = "set-upload-url";
constexpr char kDisableRateLimitSwitch[] = "disable-rate-limit";
constexpr char kEnableRateLimitSwitch[] = "enable-rate-limit";
constexpr char kClearOverridesSwitch[] = "clear-overrides";
constexpr char kRestartServiceSwitch[] = "restart-service";

// Switches consumed by base logging (see logging::InitLogging()).
constexpr char kVerboseLoggingSwitch[] = "v";
constexpr char kVmoduleSwitch[] = "vmodule";

constexpr std::string_view kKnownSwitches[] = {
    kHelpSwitch,           kQuestionSwitch,         kStatusSwitch,
    kDbPathSwitch,         kCrashPidSwitch,         kSetStagingSwitch,
    kSetUploadUrlSwitch,   kDisableRateLimitSwitch, kEnableRateLimitSwitch,
    kClearOverridesSwitch, kRestartServiceSwitch,   kVerboseLoggingSwitch,
    kVmoduleSwitch,
};

constexpr char kStagingCrashpadUploadUrl[] =
    "https://clients2.google.com/cr/staging_report";
constexpr wchar_t kCrashpadSubdir[] = L"crashpad";

// When the Crashpad handler does not respond to a crash, the Crashpad client in
// the target process waits 60 seconds (kMillisecondsUntilTerminate in
// crashpad_client_win.cc) and then terminates itself with
// kTerminationCodeCrashNoDump. Wait longer than that so the tester observes the
// exit code rather than timing out first.
constexpr DWORD kCrashProcessWaitTimeoutMs = 90'000;

constexpr base::TimeDelta kServiceTransitionTimeout = base::Seconds(30);
constexpr base::TimeDelta kServicePollInterval = base::Milliseconds(250);

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
  const std::string service_name = base::WideToUTF8(kWindowsServiceName);
  out << "\nUsage: " << base::WideToUTF8(program.BaseName().value())
      << " [options]\n\n"
      << "Inspection & crash injection:\n"
      << "  --status                Print active HKLM Crashpad overrides and\n"
      << "                          recent completed/pending crash reports.\n"
      << "  --db-path=<path>        Inspect a specific Crashpad database path\n"
      << "                          (requires --status).\n"
      << "  --crash-pid=<pid>       Trigger an unhandled access violation in\n"
      << "                          target process <pid> via\n"
      << "                          CreateRemoteThread and wait for the\n"
      << "                          process to exit. The exit code does not\n"
      << "                          show whether Crashpad captured the crash;\n"
      << "                          use --status to look for a new report.\n\n"
      << "Registry override & service helpers (require Administrator):\n"
      << "  --set-staging           Set HKLM UploadUrl to Google's staging\n"
      << "                          endpoint:\n"
      << "                          " << kStagingCrashpadUploadUrl << "\n"
      << "  --set-upload-url=<url>  Validate and set HKLM UploadUrl (https://\n"
      << "                          for remote hosts; http:// allowed only\n"
      << "                          for 127.0.0.1 or localhost).\n"
      << "  --disable-rate-limit    Set HKLM DisableRateLimit=1\n"
      << "                          (--no-rate-limit).\n"
      << "  --enable-rate-limit     Remove HKLM DisableRateLimit override.\n"
      << "  --clear-overrides       Remove HKLM Crashpad UploadUrl and\n"
      << "                          DisableRateLimit overrides.\n"
      << "  --restart-service       Stop and restart the '" << service_name
      << "' service\n"
      << "                          if running (no-op if already stopped) so\n"
      << "                          remoting_crashpad_handler.exe reloads\n"
      << "                          registry overrides.\n"
      << "  --help, --?             Show this usage message.\n\n"
      << "Flags may be combined and run in a fixed order: registry override\n"
      << "changes, --restart-service, --crash-pid, then --status. Execution\n"
      << "stops on the first failure (any earlier registry writes remain\n"
      << "applied). Mutually exclusive flags: --set-staging with\n"
      << "--set-upload-url; --disable-rate-limit with --enable-rate-limit;\n"
      << "--clear-overrides with any --set-* or --*-rate-limit flag; and\n"
      << "--restart-service with --crash-pid.\n\n"
      << "Note: Crashing or inspecting LocalSystem host processes requires an\n"
      << "elevated Administrator (or SYSTEM) command prompt.\n\n"
      << "Note: Use a tester built with the same branding (official or\n"
      << "Chromium) and bitness (32-bit vs. 64-bit) as the installed host.\n"
      << "Branding determines the HKLM subkey and per-user database path,\n"
      << "and bitness determines the registry view (native vs. WOW6432Node);\n"
      << "a mismatch causes --status, --set-*, --*-rate-limit, and\n"
      << "--clear-overrides to read or modify a different registry key than\n"
      << "the host uses.\n";
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
    ok = PrintDatabaseStatus("Crashpad database",
                             cmd.GetSwitchValuePath(kDbPathSwitch),
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

std::optional<unsigned int> ParseTargetPid(std::string_view pid_str) {
  unsigned int pid = 0;
  if (!base::StringToUint(pid_str, &pid) || pid == 0) {
    LOG(ERROR) << "Invalid process PID: " << pid_str;
    return std::nullopt;
  }
  if (pid == ::GetCurrentProcessId()) {
    LOG(ERROR) << "Refusing to crash current tester process (PID " << pid
               << ").";
    return std::nullopt;
  }
  return pid;
}

int CrashProcessByPid(unsigned int pid) {
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

bool ValidateUploadUrlForCli(std::string_view url) {
  if (url.size() > kMaxCrashpadUploadUrlChars) {
    std::cerr << "Invalid upload URL: exceeds maximum length of "
              << kMaxCrashpadUploadUrlChars << " characters.\n";
    return false;
  }
  if (!IsValidCrashpadUploadUrlOverride(url)) {
    std::cerr << "Invalid upload URL: " << url << '\n'
              << "Expected lowercase https://<host>[:port]/<path> (or "
                 "http://127.0.0.1[:port]/<path> / "
                 "http://localhost[:port]/<path>) without userinfo or '#' "
                 "fragments.\n";
    return false;
  }
  return true;
}

bool SetUploadUrlOverride(std::string_view url) {
  if (!ValidateUploadUrlForCli(url)) {
    return false;
  }

  base::win::RegKey key;
  const LONG create_res =
      key.Create(HKEY_LOCAL_MACHINE, kCrashpadRegistryKeyPath, KEY_SET_VALUE);
  if (create_res != ERROR_SUCCESS) {
    ::SetLastError(create_res);
    PLOG(ERROR) << "Failed to open HKLM\\" << kCrashpadRegistryKeyPath
                << " for writing (run as Administrator)";
    return false;
  }

  const std::wstring wide_url = base::UTF8ToWide(url);
  const LONG write_res =
      key.WriteValue(kCrashpadUploadUrlValue, wide_url.c_str());
  if (write_res != ERROR_SUCCESS) {
    ::SetLastError(write_res);
    PLOG(ERROR) << "Failed to write " << kCrashpadUploadUrlValue;
    return false;
  }

  std::cout << "Set HKLM\\" << base::WideToUTF8(kCrashpadRegistryKeyPath)
            << "\\" << base::WideToUTF8(kCrashpadUploadUrlValue) << " = " << url
            << '\n';
  return true;
}

bool SetDisableRateLimitOverride(bool disable) {
  base::win::RegKey key;
  if (disable) {
    const LONG create_res =
        key.Create(HKEY_LOCAL_MACHINE, kCrashpadRegistryKeyPath, KEY_SET_VALUE);
    if (create_res != ERROR_SUCCESS) {
      ::SetLastError(create_res);
      PLOG(ERROR) << "Failed to open HKLM\\" << kCrashpadRegistryKeyPath
                  << " for writing (run as Administrator)";
      return false;
    }
    const LONG write_res =
        key.WriteValue(kCrashpadDisableRateLimitValue, DWORD{1});
    if (write_res != ERROR_SUCCESS) {
      ::SetLastError(write_res);
      PLOG(ERROR) << "Failed to write " << kCrashpadDisableRateLimitValue;
      return false;
    }
    std::cout << "Set HKLM\\" << base::WideToUTF8(kCrashpadRegistryKeyPath)
              << "\\" << base::WideToUTF8(kCrashpadDisableRateLimitValue)
              << " = 1\n";
    const int previous_min_log_level = logging::GetMinLogLevel();
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
    const bool has_upload_url =
        CrashpadWin::ReadRegistryConfig().upload_url.has_value();
    logging::SetMinLogLevel(previous_min_log_level);
    if (!has_upload_url) {
      std::cout << "Warning: No valid UploadUrl override is set; crashes will "
                   "upload unthrottled to the production endpoint. Run "
                   "--clear-overrides after testing.\n";
    } else {
      std::cout << "Note: Override persists across host restarts; run "
                   "--clear-overrides after testing.\n";
    }
    return true;
  }

  const LONG open_res =
      key.Open(HKEY_LOCAL_MACHINE, kCrashpadRegistryKeyPath, KEY_SET_VALUE);
  if (open_res == ERROR_FILE_NOT_FOUND) {
    std::cout << "HKLM\\" << base::WideToUTF8(kCrashpadRegistryKeyPath)
              << " is not set.\n";
    return true;
  }
  if (open_res != ERROR_SUCCESS) {
    ::SetLastError(open_res);
    PLOG(ERROR) << "Failed to open HKLM\\" << kCrashpadRegistryKeyPath
                << " for writing (run as Administrator)";
    return false;
  }
  const LONG del_res = key.DeleteValue(kCrashpadDisableRateLimitValue);
  if (del_res == ERROR_FILE_NOT_FOUND) {
    std::cout << "HKLM\\" << base::WideToUTF8(kCrashpadRegistryKeyPath) << "\\"
              << base::WideToUTF8(kCrashpadDisableRateLimitValue)
              << " is not set.\n";
    return true;
  }
  if (del_res != ERROR_SUCCESS) {
    ::SetLastError(del_res);
    PLOG(ERROR) << "Failed to delete " << kCrashpadDisableRateLimitValue;
    return false;
  }
  std::cout << "Removed HKLM\\" << base::WideToUTF8(kCrashpadRegistryKeyPath)
            << "\\" << base::WideToUTF8(kCrashpadDisableRateLimitValue) << '\n';
  return true;
}

bool ClearRegistryOverrides() {
  base::win::RegKey key;
  const LONG open_res =
      key.Open(HKEY_LOCAL_MACHINE, kCrashpadRegistryKeyPath, KEY_SET_VALUE);
  if (open_res == ERROR_FILE_NOT_FOUND) {
    std::cout << "No Crashpad registry overrides present under HKLM\\"
              << base::WideToUTF8(kCrashpadRegistryKeyPath) << ".\n";
    return true;
  }
  if (open_res != ERROR_SUCCESS) {
    ::SetLastError(open_res);
    PLOG(ERROR) << "Failed to open HKLM\\" << kCrashpadRegistryKeyPath
                << " (run as Administrator)";
    return false;
  }

  // Delete override values explicitly first as a fallback in case the crashpad
  // subkey contains unexpected child subkeys that prevent RegDeleteKeyW.
  for (const wchar_t* value_name :
       {kCrashpadUploadUrlValue, kCrashpadDisableRateLimitValue}) {
    const LONG del_res = key.DeleteValue(value_name);
    if (del_res != ERROR_SUCCESS && del_res != ERROR_FILE_NOT_FOUND) {
      ::SetLastError(del_res);
      PLOG(ERROR) << "Failed to delete registry value " << value_name;
      return false;
    }
  }
  key.Close();

  const LONG del_key_res =
      ::RegDeleteKeyW(HKEY_LOCAL_MACHINE, kCrashpadRegistryKeyPath);
  if (del_key_res != ERROR_SUCCESS && del_key_res != ERROR_FILE_NOT_FOUND) {
    LOG(WARNING) << "Cleared Crashpad override values, but could not delete "
                    "subkey HKLM\\"
                 << kCrashpadRegistryKeyPath << " (error " << del_key_res
                 << ").";
  } else {
    std::cout << "Cleared Crashpad registry overrides under HKLM\\"
              << base::WideToUTF8(kCrashpadRegistryKeyPath) << ".\n";
  }
  return true;
}

// Mirrors the stop loop in remoting::installer::StopHostService()
// (remoting/host/installer/win/setup/service_util.cc), kept self-contained here
// to avoid pulling installer dependencies into this standalone tool.
bool StopServiceAndWait(SC_HANDLE service) {
  SERVICE_STATUS status = {};
  bool stop_sent = false;
  if (::ControlService(service, SERVICE_CONTROL_STOP, &status)) {
    stop_sent = true;
  } else {
    const DWORD error = ::GetLastError();
    if (error != ERROR_SERVICE_NOT_ACTIVE &&
        error != ERROR_SERVICE_CANNOT_ACCEPT_CTRL) {
      PLOG(ERROR) << "Failed to stop service '" << kWindowsServiceName << "'";
      return false;
    }
  }

  const base::TimeTicks stop_deadline =
      base::TimeTicks::Now() + kServiceTransitionTimeout;
  while (true) {
    if (!::QueryServiceStatus(service, &status)) {
      PLOG(ERROR) << "Failed to query status for service '"
                  << kWindowsServiceName << "'";
      return false;
    }
    if (status.dwCurrentState == SERVICE_STOPPED) {
      return true;
    }
    if (base::TimeTicks::Now() >= stop_deadline) {
      LOG(ERROR) << "Timed out waiting for service '" << kWindowsServiceName
                 << "' to stop.";
      return false;
    }
    // A service in SERVICE_START_PENDING rejects SERVICE_CONTROL_STOP with
    // ERROR_SERVICE_CANNOT_ACCEPT_CTRL; re-send the stop request once it
    // finishes starting and transitions out of SERVICE_START_PENDING.
    if (!stop_sent && status.dwCurrentState != SERVICE_START_PENDING &&
        status.dwCurrentState != SERVICE_STOP_PENDING) {
      if (::ControlService(service, SERVICE_CONTROL_STOP, &status)) {
        stop_sent = true;
      } else {
        const DWORD error = ::GetLastError();
        if (error == ERROR_SERVICE_NOT_ACTIVE) {
          return true;
        }
        if (error != ERROR_SERVICE_CANNOT_ACCEPT_CTRL) {
          PLOG(ERROR) << "Failed to stop service '" << kWindowsServiceName
                      << "'";
          return false;
        }
      }
    }
    base::PlatformThread::Sleep(kServicePollInterval);
  }
}

bool StartServiceAndWait(SC_HANDLE service) {
  if (!::StartServiceW(service, 0, nullptr) &&
      ::GetLastError() != ERROR_SERVICE_ALREADY_RUNNING) {
    PLOG(ERROR) << "Failed to start service '" << kWindowsServiceName << "'";
    return false;
  }

  SERVICE_STATUS status = {};
  const base::TimeTicks start_deadline =
      base::TimeTicks::Now() + kServiceTransitionTimeout;
  while (base::TimeTicks::Now() < start_deadline) {
    if (!::QueryServiceStatus(service, &status)) {
      PLOG(ERROR) << "Failed to query status for service '"
                  << kWindowsServiceName << "'";
      return false;
    }
    if (status.dwCurrentState == SERVICE_RUNNING) {
      return true;
    }
    if (status.dwCurrentState == SERVICE_STOPPED) {
      LOG(ERROR) << "Service '" << kWindowsServiceName
                 << "' stopped unexpectedly during startup (win32_exit_code="
                 << status.dwWin32ExitCode
                 << ", service_exit_code=" << status.dwServiceSpecificExitCode
                 << ").";
      return false;
    }
    base::PlatformThread::Sleep(kServicePollInterval);
  }

  LOG(ERROR) << "Timed out waiting for service '" << kWindowsServiceName
             << "' to reach SERVICE_RUNNING.";
  return false;
}

bool RestartHostService() {
  ScopedScHandle scm(
      ::OpenSCManagerW(nullptr, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CONNECT));
  if (!scm.is_valid()) {
    PLOG(ERROR) << "Failed to open Service Control Manager";
    return false;
  }

  ScopedScHandle service(
      ::OpenServiceW(scm.get(), kWindowsServiceName,
                     SERVICE_STOP | SERVICE_START | SERVICE_QUERY_STATUS));
  if (!service.is_valid()) {
    const DWORD error = ::GetLastError();
    PLOG(ERROR) << "Failed to open service '" << kWindowsServiceName << "'";
    if (error == ERROR_ACCESS_DENIED) {
      std::cerr << "Hint: Run from an elevated Administrator prompt.\n";
    }
    return false;
  }

  SERVICE_STATUS status = {};
  if (!::QueryServiceStatus(service.get(), &status)) {
    PLOG(ERROR) << "Failed to query status for service '" << kWindowsServiceName
                << "'";
    return false;
  }
  const std::string service_name = base::WideToUTF8(kWindowsServiceName);
  if (status.dwCurrentState == SERVICE_STOPPED) {
    // Do not start a deliberately stopped/disabled host; overrides will take
    // effect automatically the next time the service is started.
    std::cout << "Service '" << service_name
              << "' is not running; overrides will apply on next start.\n";
    return true;
  }

  if (!StopServiceAndWait(service.get())) {
    return false;
  }

  if (!StartServiceAndWait(service.get())) {
    std::cerr << "WARNING: Service '" << service_name
              << "' was stopped and is NOT running. Start it manually from an "
                 "elevated prompt with: sc start "
              << service_name << '\n';
    return false;
  }

  std::cout << "Restarted service '" << service_name << "'.\n";
  return true;
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

  const bool has_set_staging = cmd->HasSwitch(kSetStagingSwitch);
  const bool has_set_upload_url = cmd->HasSwitch(kSetUploadUrlSwitch);
  const bool has_disable_rate_limit = cmd->HasSwitch(kDisableRateLimitSwitch);
  const bool has_enable_rate_limit = cmd->HasSwitch(kEnableRateLimitSwitch);
  const bool has_clear_overrides = cmd->HasSwitch(kClearOverridesSwitch);
  const bool has_restart_service = cmd->HasSwitch(kRestartServiceSwitch);
  const bool has_status = cmd->HasSwitch(kStatusSwitch);
  const bool has_crash_pid = cmd->HasSwitch(kCrashPidSwitch);
  if (cmd->HasSwitch(kDbPathSwitch)) {
    if (!has_status) {
      std::cerr << "ERROR: --db-path can only be used with --status.\n";
      PrintUsage(std::cerr, cmd->GetProgram());
      return kUsageExitCode;
    }
    if (cmd->GetSwitchValuePath(kDbPathSwitch).empty()) {
      std::cerr << "ERROR: --db-path requires a non-empty path.\n";
      PrintUsage(std::cerr, cmd->GetProgram());
      return kUsageExitCode;
    }
  }

  const bool has_registry_mutation =
      has_set_staging || has_set_upload_url || has_disable_rate_limit ||
      has_enable_rate_limit || has_clear_overrides;

  if (!has_registry_mutation && !has_restart_service && !has_status &&
      !has_crash_pid) {
    PrintUsage(std::cerr, cmd->GetProgram());
    return kUsageExitCode;
  }

  if (has_set_staging && has_set_upload_url) {
    std::cerr << "ERROR: --set-staging cannot be combined with "
                 "--set-upload-url.\n";
    PrintUsage(std::cerr, cmd->GetProgram());
    return kUsageExitCode;
  }
  if (has_disable_rate_limit && has_enable_rate_limit) {
    std::cerr << "ERROR: --disable-rate-limit cannot be combined with "
                 "--enable-rate-limit.\n";
    PrintUsage(std::cerr, cmd->GetProgram());
    return kUsageExitCode;
  }
  if (has_clear_overrides) {
    for (const auto& [flag_present, flag_name] :
         std::initializer_list<std::pair<bool, std::string_view>>{
             {has_set_staging, kSetStagingSwitch},
             {has_set_upload_url, kSetUploadUrlSwitch},
             {has_disable_rate_limit, kDisableRateLimitSwitch},
             {has_enable_rate_limit, kEnableRateLimitSwitch},
         }) {
      if (flag_present) {
        std::cerr << "ERROR: --clear-overrides cannot be combined with --"
                  << flag_name << ".\n";
        PrintUsage(std::cerr, cmd->GetProgram());
        return kUsageExitCode;
      }
    }
  }

  if (has_restart_service && has_crash_pid) {
    std::cerr << "ERROR: --restart-service cannot be combined with "
                 "--crash-pid (restarting the service invalidates existing "
                 "host PIDs).\n";
    PrintUsage(std::cerr, cmd->GetProgram());
    return kUsageExitCode;
  }

  // Validate all user-supplied values before performing any registry or SCM
  // side effects.
  std::string custom_upload_url;
  if (has_set_upload_url) {
    // GetSwitchValueASCII() returns an empty string for non-ASCII input, which
    // would drop the rejected value from the error message.
    custom_upload_url = cmd->GetSwitchValueUTF8(kSetUploadUrlSwitch);
    if (!ValidateUploadUrlForCli(custom_upload_url)) {
      return kUsageExitCode;
    }
  }

  std::optional<unsigned int> target_pid;
  if (has_crash_pid) {
    target_pid = ParseTargetPid(cmd->GetSwitchValueASCII(kCrashPidSwitch));
    if (!target_pid.has_value()) {
      return kUsageExitCode;
    }
  }

  if (has_clear_overrides && !ClearRegistryOverrides()) {
    return kErrorExitCode;
  }
  if (has_set_staging && !SetUploadUrlOverride(kStagingCrashpadUploadUrl)) {
    return kErrorExitCode;
  }
  if (has_set_upload_url && !SetUploadUrlOverride(custom_upload_url)) {
    return kErrorExitCode;
  }
  if (has_disable_rate_limit && !SetDisableRateLimitOverride(true)) {
    return kErrorExitCode;
  }
  if (has_enable_rate_limit && !SetDisableRateLimitOverride(false)) {
    return kErrorExitCode;
  }

  if (has_restart_service) {
    if (!RestartHostService()) {
      return kErrorExitCode;
    }
  } else if (has_registry_mutation) {
    std::cout << "Note: Pass --restart-service (or restart the '"
              << base::WideToUTF8(kWindowsServiceName)
              << "' service) so remoting_crashpad_handler.exe reloads updated "
                 "overrides.\n";
  }

  if (target_pid.has_value()) {
    const int rc = CrashProcessByPid(*target_pid);
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

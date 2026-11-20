// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/crashpad_win.h"

#include <windows.h>

#include <crtdbg.h>

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/win/access_control_list.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#include "base/win/wrapped_window_proc.h"
#include "build/build_config.h"
#include "remoting/base/crash/crashpad_database_manager.h"
#include "remoting/base/logging.h"
#include "remoting/base/version.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"

namespace remoting {

namespace {

constexpr wchar_t kChromotingCrashpadHandler[] =
    L"remoting_crashpad_handler.exe";
constexpr char kDefaultCrashpadUploadUrl[] =
    "https://clients2.google.com/cr/report";

int __cdecl OnWindowProcedureException(EXCEPTION_POINTERS* exinfo) {
  crashpad::CrashpadClient::DumpAndCrash(exinfo);
  return EXCEPTION_CONTINUE_SEARCH;
}

// Temporarily clears STD_INPUT_HANDLE and STD_OUTPUT_HANDLE while spawning
// remoting_crashpad_handler.exe synchronously so CrashpadClient::StartHandler()
// does not add Chrome's inheritable Native Messaging stdin/stdout pipes to the
// handler's PROC_THREAD_ATTRIBUTE_HANDLE_LIST (leaving STD_ERROR_HANDLE
// intact).
class ScopedClearStdIoHandles {
 public:
  ScopedClearStdIoHandles()
      : std_input_(::GetStdHandle(STD_INPUT_HANDLE)),
        std_output_(::GetStdHandle(STD_OUTPUT_HANDLE)) {
    if (std_input_ != nullptr && std_input_ != INVALID_HANDLE_VALUE) {
      ::SetStdHandle(STD_INPUT_HANDLE, nullptr);
    }
    if (std_output_ != nullptr && std_output_ != INVALID_HANDLE_VALUE) {
      ::SetStdHandle(STD_OUTPUT_HANDLE, nullptr);
    }
  }

  ScopedClearStdIoHandles(const ScopedClearStdIoHandles&) = delete;
  ScopedClearStdIoHandles& operator=(const ScopedClearStdIoHandles&) = delete;

  ~ScopedClearStdIoHandles() {
    if (std_input_ != nullptr && std_input_ != INVALID_HANDLE_VALUE) {
      ::SetStdHandle(STD_INPUT_HANDLE, std_input_);
    }
    if (std_output_ != nullptr && std_output_ != INVALID_HANDLE_VALUE) {
      ::SetStdHandle(STD_OUTPUT_HANDLE, std_output_);
    }
  }

 private:
  const HANDLE std_input_;
  const HANDLE std_output_;
};

bool SetupSystemCrashpadDirectory(const base::FilePath& database_path) {
  CHECK(!database_path.empty());
  if (!base::CreateDirectory(database_path.DirName())) {
    LOG(ERROR) << "Failed to create parent directory for system Crashpad "
                  "database: "
               << database_path.DirName();
    return false;
  }

  // Restrict the system Crashpad database directory (under DIR_ASSETS) to
  // LocalSystem and Administrators with a protected DACL so crash dumps do not
  // inherit Read & Execute permissions granted to Users or ALL APPLICATION
  // PACKAGES on the parent installation directory.
  auto sd = base::win::SecurityDescriptor::FromSddl(
      L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)");
  if (!sd.has_value()) {
    PLOG(ERROR) << "Failed to parse SDDL for system Crashpad database "
                   "directory";
    return false;
  }
  auto self_relative_sd = sd->ToSelfRelative();
  if (!self_relative_sd.has_value()) {
    PLOG(ERROR) << "Failed to create self-relative security descriptor for "
                   "system Crashpad database directory";
    return false;
  }

  SECURITY_ATTRIBUTES sa = {
      .nLength = sizeof(sa),
      .lpSecurityDescriptor = self_relative_sd->get(),
      .bInheritHandle = FALSE,
  };
  if (::CreateDirectoryW(database_path.value().c_str(), &sa)) {
    return true;
  }
  if (::GetLastError() != ERROR_ALREADY_EXISTS) {
    PLOG(ERROR) << "Failed to create system Crashpad database directory: "
                << database_path;
    return false;
  }

  // Harden pre-existing directories on upgrade.
  const DWORD attributes = ::GetFileAttributesW(database_path.value().c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    PLOG(ERROR) << "Failed to query attributes of existing system Crashpad "
                   "database path: "
                << database_path;
    return false;
  }
  if (!(attributes & FILE_ATTRIBUTE_DIRECTORY) ||
      (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
    LOG(ERROR) << "Existing system Crashpad database path is not a regular "
                  "directory: "
               << database_path;
    return false;
  }
  if (!sd->WriteToFile(database_path, DACL_SECURITY_INFORMATION)) {
    PLOG(ERROR) << "Failed to set DACL on system Crashpad database directory: "
                << database_path;
    return false;
  }
  return true;
}

bool GrantLocalServiceAccessToHandlerPipe(
    const std::wstring& handler_ipc_pipe) {
  if (handler_ipc_pipe.empty()) {
    return false;
  }

  // Retry on ERROR_PIPE_BUSY using the same pattern as Crashpad's
  // SendToCrashHandlerServer(), bounded so a wedged handler cannot stall
  // HostMain() before DaemonProcessMain() connects to the Windows SCM.
  // ExceptionHandlerServer only creates kPipeInstances = 2 server pipe
  // instances, and StartHandler()'s initial ping may still be disconnecting on
  // one instance when we open the pipe here.
  //
  // Note: Opening and closing the pipe handle here without sending a
  // ClientToServerMessage causes ExceptionHandlerServer in
  // remoting_crashpad_handler.exe to log one expected broken/closing-pipe read
  // error on daemon startup.
  constexpr base::TimeDelta kPipeOpenTimeout = base::Seconds(5);
  const base::TimeTicks deadline = base::TimeTicks::Now() + kPipeOpenTimeout;

  base::win::ScopedHandle pipe_handle;
  for (;;) {
    pipe_handle.Set(::CreateFileW(
        handler_ipc_pipe.c_str(), READ_CONTROL | WRITE_DAC, /*dwShareMode=*/0,
        /*lpSecurityAttributes=*/nullptr, OPEN_EXISTING,
        SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION,
        /*hTemplateFile=*/nullptr));
    if (pipe_handle.is_valid()) {
      break;
    }
    if (::GetLastError() != ERROR_PIPE_BUSY) {
      PLOG(WARNING) << "Failed to open Crashpad handler pipe to update DACL: "
                    << handler_ipc_pipe;
      return false;
    }
    const base::TimeDelta remaining = deadline - base::TimeTicks::Now();
    if (!remaining.is_positive()) {
      LOG(WARNING) << "Timed out waiting for busy Crashpad handler pipe to "
                      "update DACL: "
                   << handler_ipc_pipe;
      return false;
    }
    // WaitNamedPipeW treats 0 as NMPWAIT_USE_DEFAULT_WAIT, so clamp to >= 1 ms.
    const DWORD wait_ms = std::clamp<DWORD>(
        base::saturated_cast<DWORD>(remaining.InMilliseconds()), 1,
        base::saturated_cast<DWORD>(kPipeOpenTimeout.InMilliseconds()));
    if (!::WaitNamedPipeW(handler_ipc_pipe.c_str(), wait_ms)) {
      if (::GetLastError() == ERROR_SEM_TIMEOUT) {
        LOG(WARNING) << "Timed out waiting for busy Crashpad handler pipe to "
                        "update DACL: "
                     << handler_ipc_pipe;
      } else {
        PLOG(WARNING) << "Failed waiting for Crashpad handler pipe to update "
                         "DACL: "
                      << handler_ipc_pipe;
      }
      return false;
    }
  }

  // GetSecurityInfo/SetSecurityInfo require SE_KERNEL_OBJECT for named pipe
  // handles.
  auto sd = base::win::SecurityDescriptor::FromHandle(
      pipe_handle.get(), base::win::SecurityObjectType::kKernel,
      DACL_SECURITY_INFORMATION);
  if (!sd.has_value() ||
      !sd->SetDaclEntry(base::win::WellKnownSid::kLocalService,
                        base::win::SecurityAccessMode::kGrant,
                        GENERIC_READ | GENERIC_WRITE,
                        /*inheritance=*/0) ||
      !sd->WriteToHandle(pipe_handle.get(),
                         base::win::SecurityObjectType::kKernel,
                         DACL_SECURITY_INFORMATION)) {
    PLOG(WARNING) << "Failed to grant LocalService access to Crashpad handler "
                     "pipe: "
                  << handler_ipc_pipe;
    return false;
  }
  return true;
}

}  // namespace

CrashpadWin::CrashpadWin() = default;

CrashpadWin::~CrashpadWin() = default;

bool CrashpadWin::Initialize() {
  if (initialization_attempted_) {
    if (crashpad_client_) {
      LOG(ERROR) << "Crashpad is already initialized with pipe: "
                 << crashpad_client_->GetHandlerIPCPipe();
    } else {
      LOG(ERROR) << "Crashpad initialization was already attempted.";
    }
    return false;
  }
  initialization_attempted_ = true;

  const base::FilePath database_path = GetCrashpadDatabasePath();
  if (database_path.empty()) {
    LOG(ERROR) << "Failed to get Crashpad database directory path.";
    return false;
  }

  const bool is_system = IsCurrentProcessLocalSystem();
  if (is_system && !SetupSystemCrashpadDirectory(database_path)) {
    return false;
  }

  CrashpadDatabaseManager database(*this);
  if (!database.InitializeCrashpadDatabase(database_path)) {
    LOG(ERROR) << "Failed to initialize database for Crashpad";
    return false;
  }

  // We only initialize crash handling if the user has consented to record and
  // upload reports, so we can simply enable it here.
  if (!database.EnableReportUploads()) {
    LOG(WARNING) << "Unable to enable Crashpad uploads.";
  }

  database.LogCompletedCrashpadReports();
  database.LogPendingCrashpadReports();
  database.CleanupCompletedCrashpadReports();

  // Leave metrics_path empty because this option is not used (or supported) on
  // non-Chromium builds.
  base::FilePath metrics_path;

  std::map<std::string, std::string> annotations;
  // "Chromoting_Win" is the product name registered on the crash server for
  // Windows Chrome Remote Desktop hosts; keep this exact value so reports are
  // routed to the registered product configuration.
  annotations["prod"] = "Chromoting_Win";
  annotations["ver"] = REMOTING_VERSION_STRING;
#if defined(ARCH_CPU_X86)
  annotations["plat"] = "Win32";
#elif defined(ARCH_CPU_X86_64)
  annotations["plat"] = "Win64";
#endif

  const RegistryConfig registry_config = ReadRegistryConfig();
  const std::string upload_url =
      registry_config.upload_url.value_or(kDefaultCrashpadUploadUrl);
  if (registry_config.upload_url.has_value()) {
    LOG(WARNING) << "Overriding Crashpad upload URL via HKLM\\"
                 << kCrashpadRegistryKeyPath << ": " << upload_url;
  }

  std::vector<std::string> arguments;
  // Make sure Crashpad's generate_dump tool includes monitor-self annotations.
  // This creates a second crashpad instance that monitors the handler so it can
  // report crashes in the handler.
  arguments.push_back("--monitor-self-annotation=ptype=crashpad-handler");
  if (registry_config.disable_rate_limit) {
    arguments.push_back("--no-rate-limit");
    LOG(WARNING) << "Disabling Crashpad upload rate limit via HKLM\\"
                 << kCrashpadRegistryKeyPath;
  }

  base::FilePath handler_path;
  if (!GetCrashpadHandlerPath(&handler_path)) {
    return false;
  }

  auto client = std::make_unique<crashpad::CrashpadClient>();
  bool handler_started = false;
  {
    ScopedClearStdIoHandles clear_stdio_handles;
    handler_started = client->StartHandler(
        handler_path, database_path, metrics_path, upload_url, annotations,
        arguments, /*restartable=*/false, /*asynchronous_start=*/false);
  }
  if (!handler_started) {
    LOG(ERROR) << "Failed to start Crashpad handler.";
    return false;
  }

  crashpad_client_ = std::move(client);
  const std::wstring handler_ipc_pipe = crashpad_client_->GetHandlerIPCPipe();

  if (is_system) {
    local_service_pipe_access_granted_ =
        GrantLocalServiceAccessToHandlerPipe(handler_ipc_pipe);
  }

  ConfigureProcessExceptionHandling();

  HOST_LOG << "Crashpad handler started.";
  return true;
}

bool CrashpadWin::InitializeClient(const std::wstring& handler_pipe) {
  if (handler_pipe.empty()) {
    LOG(ERROR) << "Handler pipe is empty.";
    return false;
  }
  if (initialization_attempted_) {
    if (crashpad_client_) {
      LOG(ERROR) << "Crashpad is already initialized with pipe: "
                 << crashpad_client_->GetHandlerIPCPipe();
    } else {
      LOG(ERROR) << "Crashpad initialization was already attempted.";
    }
    return false;
  }
  initialization_attempted_ = true;

  auto client = std::make_unique<crashpad::CrashpadClient>();
  if (!client->SetHandlerIPCPipe(handler_pipe)) {
    LOG(ERROR) << "Failed to register with Crashpad handler pipe: "
               << handler_pipe;
    return false;
  }

  crashpad_client_ = std::move(client);
  ConfigureProcessExceptionHandling();
  HOST_LOG << "Crashpad client initialized.";
  return true;
}

std::wstring CrashpadWin::GetHandlerIPCPipe() const {
  return crashpad_client_ ? crashpad_client_->GetHandlerIPCPipe()
                          : std::wstring();
}

bool CrashpadWin::CanUnprivilegedClientsConnect() const {
  return crashpad_client_ && local_service_pipe_access_granted_;
}

// static
CrashpadWin& CrashpadWin::GetInstance() {
  static base::NoDestructor<CrashpadWin> instance;
  return *instance;
}

// static
CrashpadWin::RegistryConfig CrashpadWin::ReadRegistryConfig() {
  RegistryConfig config;
  base::win::RegKey crashpad_reg_key;
  const LONG open_res = crashpad_reg_key.Open(
      HKEY_LOCAL_MACHINE, kCrashpadRegistryKeyPath, KEY_READ);
  if (open_res != ERROR_SUCCESS) {
    if (open_res != ERROR_FILE_NOT_FOUND) {
      LOG(ERROR) << "Failed to open HKLM\\" << kCrashpadRegistryKeyPath
                 << " (error=" << open_res << ")";
    }
    return config;
  }

  // Matches base::win::RegKey::ReadValue's 1024-wchar limit (including NUL);
  // longer values return ERROR_MORE_DATA and are rejected below.
  std::array<wchar_t, kMaxCrashpadUploadUrlChars + 1> raw_url;
  DWORD url_bytes = sizeof(raw_url);
  DWORD url_type = 0;
  const LONG url_res = crashpad_reg_key.ReadValue(
      kCrashpadUploadUrlValue, raw_url.data(), &url_bytes, &url_type);
  if (url_res != ERROR_FILE_NOT_FOUND) {
    if (url_res != ERROR_SUCCESS || url_type != REG_SZ ||
        url_bytes % sizeof(wchar_t) != 0) {
      LOG(ERROR) << "Ignoring unreadable or non-REG_SZ Crashpad UploadUrl in "
                    "HKLM\\"
                 << kCrashpadRegistryKeyPath << " (error=" << url_res
                 << ", type=" << url_type << ")";
    } else {
      size_t url_len = url_bytes / sizeof(wchar_t);
      if (url_len > 0 && raw_url[url_len - 1] == L'\0') {
        --url_len;
      }
      std::string upload_url;
      if (base::WideToUTF8(std::wstring_view(raw_url.data(), url_len),
                           &upload_url) &&
          IsValidCrashpadUploadUrlOverride(upload_url)) {
        config.upload_url = std::move(upload_url);
      } else {
        LOG(ERROR) << "Ignoring invalid Crashpad UploadUrl in HKLM\\"
                   << kCrashpadRegistryKeyPath;
      }
    }
  }

  DWORD disable_rate_limit = 0;
  DWORD rate_bytes = sizeof(disable_rate_limit);
  DWORD rate_type = 0;
  const LONG rate_res =
      crashpad_reg_key.ReadValue(kCrashpadDisableRateLimitValue,
                                 &disable_rate_limit, &rate_bytes, &rate_type);
  if (rate_res != ERROR_FILE_NOT_FOUND) {
    if (rate_res == ERROR_SUCCESS && rate_type == REG_DWORD &&
        rate_bytes == sizeof(disable_rate_limit)) {
      config.disable_rate_limit = (disable_rate_limit != 0);
    } else {
      LOG(ERROR)
          << "Ignoring unreadable or non-REG_DWORD Crashpad DisableRateLimit "
             "in HKLM\\"
          << kCrashpadRegistryKeyPath << " (error=" << rate_res
          << ", type=" << rate_type << ")";
    }
  }

  return config;
}

// static
bool CrashpadWin::GrantLocalServiceAccessToHandlerPipeForTesting(
    const std::wstring& handler_ipc_pipe) {
  return GrantLocalServiceAccessToHandlerPipe(handler_ipc_pipe);
}

// private

void CrashpadWin::ConfigureProcessExceptionHandling() {
  _CrtSetReportMode(_CRT_ASSERT, 0);

  base::win::WinProcExceptionFilter exception_filter =
      base::win::SetWinProcExceptionFilter(&OnWindowProcedureException);
  LOG_IF(DFATAL,
         exception_filter && exception_filter != &OnWindowProcedureException)
      << "Exception filter already present";
}

// CrashpadDatabaseManager::Logger overrides
void CrashpadWin::Log(std::string_view message) const {
  HOST_LOG << message;
}

void CrashpadWin::LogError(std::string_view message) const {
  LOG(ERROR) << message;
}

bool CrashpadWin::GetCrashpadHandlerPath(base::FilePath* handler_path) {
  if (!base::PathService::Get(base::DIR_EXE, handler_path)) {
    LOG(ERROR) << "Unable to get exe dir for crashpad handler";
    return false;
  }
  *handler_path = handler_path->Append(kChromotingCrashpadHandler);
  return true;
}

}  // namespace remoting

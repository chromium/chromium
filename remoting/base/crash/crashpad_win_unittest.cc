// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/crashpad_win.h"

#include <windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/multiprocess_test.h"
#include "base/test/run_until.h"
#include "base/test/scoped_path_override.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_reg_util_win.h"
#include "base/test/test_timeouts.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "base/win/access_control_list.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "remoting/base/branding.h"
#include "remoting/base/crash/crash_reporting_crashpad.h"
#include "remoting/base/crash/crashpad_database_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/util/win/address_types.h"
#include "third_party/crashpad/crashpad/util/win/exception_handler_server.h"
#include "third_party/zlib/google/compression_utils.h"

namespace remoting {
namespace {

constexpr char kTestHandlerPipeSwitch[] = "test-crashpad-handler-pipe";
constexpr char kTestLocalAppDataSwitch[] = "test-crashpad-local-app-data";
constexpr char kTestUploadUrlSwitch[] = "test-crashpad-upload-url";
constexpr char kTestShutdownEventSwitch[] = "test-crashpad-shutdown-event";
constexpr char kTestParentPidSwitch[] = "test-crashpad-parent-pid";

std::wstring CreateUniqueTestPipeName() {
  return L"\\\\.\\pipe\\remoting_crashpad_test_" +
         base::ASCIIToWide(base::UnguessableToken::Create().ToString());
}

class ScopedTestExceptionHandlerServer {
 public:
  ScopedTestExceptionHandlerServer() = default;
  ScopedTestExceptionHandlerServer(const ScopedTestExceptionHandlerServer&) =
      delete;
  ScopedTestExceptionHandlerServer& operator=(
      const ScopedTestExceptionHandlerServer&) = delete;

  ~ScopedTestExceptionHandlerServer() {
    if (thread_.HasStartBeenAttempted() && !thread_.HasBeenJoined()) {
      server_.Stop();
      thread_.Join();
    }
  }

  bool Start(const std::wstring& pipe_name) {
    server_.SetPipeName(pipe_name);
    thread_.Start();
    return started_event_.TimedWait(TestTimeouts::action_timeout());
  }

 private:
  class TestServerDelegate final
      : public crashpad::ExceptionHandlerServer::Delegate {
   public:
    explicit TestServerDelegate(base::WaitableEvent& started_event)
        : started_event_(started_event) {}

    void ExceptionHandlerServerStarted() override { started_event_->Signal(); }

    unsigned int ExceptionHandlerServerException(
        HANDLE process,
        crashpad::WinVMAddress exception_information_address,
        crashpad::WinVMAddress debug_critical_section_address) override {
      return 0;
    }

   private:
    const raw_ref<base::WaitableEvent> started_event_;
  };

  class ServerThread final : public base::SimpleThread {
   public:
    ServerThread(crashpad::ExceptionHandlerServer& server,
                 crashpad::ExceptionHandlerServer::Delegate& delegate)
        : base::SimpleThread("TestCrashpadExceptionHandlerServer"),
          server_(server),
          delegate_(delegate) {}

    void Run() override { server_->Run(&*delegate_); }

   private:
    const raw_ref<crashpad::ExceptionHandlerServer> server_;
    const raw_ref<crashpad::ExceptionHandlerServer::Delegate> delegate_;
  };

  base::WaitableEvent started_event_;
  TestServerDelegate delegate_{started_event_};
  crashpad::ExceptionHandlerServer server_{/*persistent=*/true};
  ServerThread thread_{server_, delegate_};
};

struct AllowedAceInfo {
  BYTE flags;
  ACCESS_MASK mask;
  base::win::Sid sid;
};

std::optional<base::win::SecurityDescriptor> ReadPipeSecurityDescriptor(
    const std::wstring& pipe_name) {
  base::win::ScopedHandle pipe_handle;
  for (;;) {
    pipe_handle.Set(
        ::CreateFileW(pipe_name.c_str(), READ_CONTROL, /*dwShareMode=*/0,
                      /*lpSecurityAttributes=*/nullptr, OPEN_EXISTING,
                      SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION,
                      /*hTemplateFile=*/nullptr));
    if (pipe_handle.is_valid()) {
      break;
    }
    if (::GetLastError() != ERROR_PIPE_BUSY ||
        !::WaitNamedPipeW(
            pipe_name.c_str(),
            base::saturated_cast<DWORD>(
                TestTimeouts::action_timeout().InMilliseconds()))) {
      return std::nullopt;
    }
  }

  return base::win::SecurityDescriptor::FromHandle(
      pipe_handle.get(), base::win::SecurityObjectType::kKernel,
      DACL_SECURITY_INFORMATION | LABEL_SECURITY_INFORMATION);
}

std::vector<AllowedAceInfo> GetAllowedAces(base::win::AccessControlList& acl) {
  std::vector<AllowedAceInfo> result;
  ACL* raw_acl = acl.get();
  if (!raw_acl) {
    return result;
  }
  for (DWORD i = 0; i < raw_acl->AceCount; ++i) {
    void* raw_ace = nullptr;
    if (!::GetAce(raw_acl, i, &raw_ace)) {
      ADD_FAILURE() << "GetAce failed at index " << i;
      continue;
    }
    const auto* header = static_cast<const ACE_HEADER*>(raw_ace);
    if (header->AceType != ACCESS_ALLOWED_ACE_TYPE) {
      continue;
    }
    auto* ace = static_cast<ACCESS_ALLOWED_ACE*>(raw_ace);
    auto sid = base::win::Sid::FromPSID(&ace->SidStart);
    if (!sid.has_value()) {
      ADD_FAILURE() << "Invalid SID at index " << i;
      continue;
    }
    result.push_back({
        .flags = header->AceFlags,
        .mask = ace->Mask,
        .sid = std::move(*sid),
    });
  }
  return result;
}

std::optional<ACCESS_MASK> FindAllowedMaskForSid(
    const std::vector<AllowedAceInfo>& aces,
    const base::win::Sid& target_sid,
    BYTE expected_flags = 0) {
  for (const auto& ace : aces) {
    if (ace.sid == target_sid && ace.flags == expected_flags) {
      return ace.mask;
    }
  }
  return std::nullopt;
}

}  // namespace

TEST(CrashpadWinTest, GetHandlerPipeBeforeInit) {
  EXPECT_TRUE(GetCrashpadHandlerPipe().empty());
  EXPECT_FALSE(CanUnprivilegedCrashpadClientsConnect());
}

TEST(CrashpadWinTest, InitializeClientWithEmptyPipe) {
  EXPECT_FALSE(InitializeCrashpadClient(L""));
  EXPECT_TRUE(GetCrashpadHandlerPipe().empty());
}

MULTIPROCESS_TEST_MAIN(InitializeClientWithNonexistentPipeChild) {
  if (InitializeCrashpadClient(
          L"\\\\.\\pipe\\nonexistent_remoting_crashpad_test_pipe")) {
    return 1;
  }
  if (!GetCrashpadHandlerPipe().empty()) {
    return 2;
  }
  // A retry after a failed initialization must also fail cleanly without
  // re-entering CrashpadClient global initialization.
  if (InitializeCrashpadClient(
          L"\\\\.\\pipe\\nonexistent_remoting_crashpad_test_pipe")) {
    return 3;
  }
  return 0;
}

TEST(CrashpadWinTest, InitializeClientWithNonexistentPipe) {
  base::CommandLine command_line(
      base::GetMultiProcessTestChildBaseCommandLine());
  base::Process test_child_process = base::SpawnMultiProcessTestChild(
      "InitializeClientWithNonexistentPipeChild", command_line,
      base::LaunchOptions());
  int rv = -1;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      test_child_process, TestTimeouts::action_timeout(), &rv));
  EXPECT_EQ(0, rv);
}

MULTIPROCESS_TEST_MAIN(InitializeClientAndGettersChild) {
  const std::wstring pipe_name =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          kTestHandlerPipeSwitch);
  if (pipe_name.empty()) {
    return 1;
  }
  if (!InitializeCrashpadClient(pipe_name)) {
    return 2;
  }
  if (GetCrashpadHandlerPipe() != pipe_name) {
    return 3;
  }
  // A second initialization attempt must fail while preserving the pipe name.
  if (InitializeCrashpadClient(pipe_name)) {
    return 4;
  }
  if (GetCrashpadHandlerPipe() != pipe_name) {
    return 5;
  }
  if (CanUnprivilegedCrashpadClientsConnect()) {
    return 6;
  }
  return 0;
}

TEST(CrashpadWinTest, InitializeClientAndGetters) {
  const std::wstring pipe_name = CreateUniqueTestPipeName();
  ScopedTestExceptionHandlerServer server;
  ASSERT_TRUE(server.Start(pipe_name));

  base::CommandLine command_line(
      base::GetMultiProcessTestChildBaseCommandLine());
  command_line.AppendSwitchNative(kTestHandlerPipeSwitch, pipe_name);
  base::Process test_child_process = base::SpawnMultiProcessTestChild(
      "InitializeClientAndGettersChild", command_line, base::LaunchOptions());
  int rv = -1;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      test_child_process, TestTimeouts::action_timeout(), &rv));
  EXPECT_EQ(0, rv);
}

TEST(CrashpadWinTest, GrantLocalServiceAccessToHandlerPipe) {
  const std::wstring pipe_name = CreateUniqueTestPipeName();
  ScopedTestExceptionHandlerServer server;
  ASSERT_TRUE(server.Start(pipe_name));

  auto before_sd = ReadPipeSecurityDescriptor(pipe_name);
  ASSERT_TRUE(before_sd.has_value());
  ASSERT_TRUE(before_sd->dacl().has_value());
  auto before_label = before_sd->ToSddl(LABEL_SECURITY_INFORMATION);
  ASSERT_TRUE(before_label.has_value());
  // Windows sets SE_SACL_AUTO_INHERITED (S:AI) on the named pipe's SACL.
  EXPECT_NE(before_label->find(L"(ML;;;;;S-1-16-0)"), std::wstring::npos)
      << *before_label;

  const std::vector<AllowedAceInfo> before_aces =
      GetAllowedAces(*before_sd->dacl());
  const base::win::Sid local_system_sid(base::win::WellKnownSid::kLocalSystem);
  const base::win::Sid app_packages_sid(
      base::win::WellKnownSid::kAllApplicationPackages);
  const base::win::Sid local_service_sid(
      base::win::WellKnownSid::kLocalService);
  ASSERT_TRUE(FindAllowedMaskForSid(before_aces, local_system_sid).has_value());
  ASSERT_TRUE(FindAllowedMaskForSid(before_aces, app_packages_sid).has_value());
  EXPECT_FALSE(
      FindAllowedMaskForSid(before_aces, local_service_sid).has_value());

  ASSERT_TRUE(
      CrashpadWin::GrantLocalServiceAccessToHandlerPipeForTesting(pipe_name));

  auto after_sd = ReadPipeSecurityDescriptor(pipe_name);
  ASSERT_TRUE(after_sd.has_value());
  ASSERT_TRUE(after_sd->dacl().has_value());
  auto after_label = after_sd->ToSddl(LABEL_SECURITY_INFORMATION);
  ASSERT_TRUE(after_label.has_value());
  EXPECT_EQ(*after_label, *before_label);

  const std::vector<AllowedAceInfo> after_aces =
      GetAllowedAces(*after_sd->dacl());
  for (const auto& before_ace : before_aces) {
    auto preserved_mask =
        FindAllowedMaskForSid(after_aces, before_ace.sid, before_ace.flags);
    ASSERT_TRUE(preserved_mask.has_value());
    EXPECT_EQ(*preserved_mask, before_ace.mask);
  }
  EXPECT_TRUE(FindAllowedMaskForSid(after_aces, local_system_sid).has_value());
  EXPECT_TRUE(FindAllowedMaskForSid(after_aces, app_packages_sid).has_value());

  auto local_service_mask =
      FindAllowedMaskForSid(after_aces, local_service_sid);
  ASSERT_TRUE(local_service_mask.has_value());
  GENERIC_MAPPING file_generic_mapping = {
      .GenericRead = FILE_GENERIC_READ,
      .GenericWrite = FILE_GENERIC_WRITE,
      .GenericExecute = FILE_GENERIC_EXECUTE,
      .GenericAll = FILE_ALL_ACCESS,
  };
  ACCESS_MASK mapped_mask = *local_service_mask;
  ::MapGenericMask(&mapped_mask, &file_generic_mapping);
  constexpr ACCESS_MASK kExpectedReadWrite =
      FILE_GENERIC_READ | FILE_GENERIC_WRITE;
  EXPECT_EQ(mapped_mask & kExpectedReadWrite, kExpectedReadWrite);
}

TEST(CrashpadWinTest, ReadRegistryConfig) {
  registry_util::RegistryOverrideManager registry_override;
  ASSERT_NO_FATAL_FAILURE(
      registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE));

  // Absent key returns empty/default config.
  CrashpadWin::RegistryConfig config = CrashpadWin::ReadRegistryConfig();
  EXPECT_FALSE(config.upload_url.has_value());
  EXPECT_FALSE(config.disable_rate_limit);

  base::win::RegKey key;
  ASSERT_EQ(
      key.Create(HKEY_LOCAL_MACHINE, kCrashpadRegistryKeyPath, KEY_SET_VALUE),
      ERROR_SUCCESS);

  // Valid HTTPS staging URL and rate-limit override.
  ASSERT_EQ(key.WriteValue(kCrashpadUploadUrlValue,
                           L"https://clients2.google.com/cr/staging_report"),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kCrashpadDisableRateLimitValue, DWORD{1}),
            ERROR_SUCCESS);
  config = CrashpadWin::ReadRegistryConfig();
  EXPECT_EQ(config.upload_url, "https://clients2.google.com/cr/staging_report");
  EXPECT_TRUE(config.disable_rate_limit);

  // Valid loopback HTTP URLs for local upload testing.
  ASSERT_EQ(key.WriteValue(kCrashpadUploadUrlValue,
                           L"http://127.0.0.1:8080/cr/report"),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kCrashpadDisableRateLimitValue, DWORD{0}),
            ERROR_SUCCESS);
  config = CrashpadWin::ReadRegistryConfig();
  EXPECT_EQ(config.upload_url, "http://127.0.0.1:8080/cr/report");
  EXPECT_FALSE(config.disable_rate_limit);

  ASSERT_EQ(key.WriteValue(kCrashpadUploadUrlValue,
                           L"http://localhost:9000/cr/report"),
            ERROR_SUCCESS);
  config = CrashpadWin::ReadRegistryConfig();
  EXPECT_EQ(config.upload_url, "http://localhost:9000/cr/report");

  // Disallowed or malformed URLs are ignored (full URL matrix is covered by
  // CrashpadDatabaseManagerTest.IsValidCrashpadUploadUrlOverride).
  const wchar_t* const kInvalidUrls[] = {
      L"",
      L"http://example.com/cr/report",
      L"https://clients2.google.com",
  };
  for (const wchar_t* invalid_url : kInvalidUrls) {
    ASSERT_EQ(key.WriteValue(kCrashpadUploadUrlValue, invalid_url),
              ERROR_SUCCESS);
    config = CrashpadWin::ReadRegistryConfig();
    EXPECT_FALSE(config.upload_url.has_value()) << invalid_url;
  }

  // REG_SZ without a trailing NUL terminator is accepted.
  constexpr std::wstring_view kUnterminatedUrl =
      L"https://clients2.google.com/cr/staging_report";
  ASSERT_EQ(key.WriteValue(kCrashpadUploadUrlValue, kUnterminatedUrl.data(),
                           base::checked_cast<DWORD>(kUnterminatedUrl.size() *
                                                     sizeof(wchar_t)),
                           REG_SZ),
            ERROR_SUCCESS);
  config = CrashpadWin::ReadRegistryConfig();
  EXPECT_EQ(config.upload_url, "https://clients2.google.com/cr/staging_report");

  // REG_SZ containing an embedded NUL is rejected.
  constexpr wchar_t kEmbeddedNulUrl[] =
      L"https://clients2.google.com/cr/report\0extra";
  ASSERT_EQ(key.WriteValue(kCrashpadUploadUrlValue, kEmbeddedNulUrl,
                           sizeof(kEmbeddedNulUrl), REG_SZ),
            ERROR_SUCCESS);
  config = CrashpadWin::ReadRegistryConfig();
  EXPECT_FALSE(config.upload_url.has_value());

  // REG_SZ exceeding the 1024-wchar buffer limit (ERROR_MORE_DATA) is rejected.
  const std::wstring overlong_url =
      L"https://clients2.google.com/" + std::wstring(1024, L'a');
  ASSERT_EQ(key.WriteValue(kCrashpadUploadUrlValue, overlong_url.c_str()),
            ERROR_SUCCESS);
  config = CrashpadWin::ReadRegistryConfig();
  EXPECT_FALSE(config.upload_url.has_value());

  // Wrong value type (REG_EXPAND_SZ instead of REG_SZ) for UploadUrl is
  // ignored.
  constexpr wchar_t kExpandUrl[] =
      L"https://clients2.google.com/cr/staging_report";
  ASSERT_EQ(key.WriteValue(kCrashpadUploadUrlValue, kExpandUrl,
                           sizeof(kExpandUrl), REG_EXPAND_SZ),
            ERROR_SUCCESS);
  config = CrashpadWin::ReadRegistryConfig();
  EXPECT_FALSE(config.upload_url.has_value());

  // Non-zero DWORD values other than 1 also enable DisableRateLimit.
  ASSERT_EQ(key.WriteValue(kCrashpadDisableRateLimitValue, DWORD{2}),
            ERROR_SUCCESS);
  config = CrashpadWin::ReadRegistryConfig();
  EXPECT_TRUE(config.disable_rate_limit);

  // Wrong value types (REG_SZ, 4-byte REG_BINARY, REG_QWORD) for
  // DisableRateLimit are ignored.
  ASSERT_EQ(key.WriteValue(kCrashpadDisableRateLimitValue, L"1"),
            ERROR_SUCCESS);
  config = CrashpadWin::ReadRegistryConfig();
  EXPECT_FALSE(config.disable_rate_limit);

  const DWORD binary_one = 1;
  ASSERT_EQ(key.WriteValue(kCrashpadDisableRateLimitValue, &binary_one,
                           sizeof(binary_one), REG_BINARY),
            ERROR_SUCCESS);
  config = CrashpadWin::ReadRegistryConfig();
  EXPECT_FALSE(config.disable_rate_limit);

  const uint64_t qword_one = 1;
  ASSERT_EQ(key.WriteValue(kCrashpadDisableRateLimitValue, &qword_one,
                           sizeof(qword_one), REG_QWORD),
            ERROR_SUCCESS);
  config = CrashpadWin::ReadRegistryConfig();
  EXPECT_FALSE(config.disable_rate_limit);
}

DWORD WINAPI RaiseAccessViolationThreadProc(void*) {
  ::RaiseException(EXCEPTION_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE, 0,
                   nullptr);
  return 0;
}

MULTIPROCESS_TEST_MAIN(EndToEndCrashingClientChild) {
  ::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);

  const std::wstring pipe_name =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          kTestHandlerPipeSwitch);
  if (pipe_name.empty()) {
    return 1;
  }
  if (!InitializeCrashpadClient(pipe_name)) {
    return 2;
  }

  // Simulate an unhandled SEH access violation on a dedicated Win32 thread so
  // kernel32!BaseThreadInitThunk dispatches directly to Crashpad's
  // UnhandledExceptionFilter without GTest/CRT main-thread wrappers.
  base::win::ScopedHandle thread(::CreateThread(
      nullptr, 0, &RaiseAccessViolationThreadProc, nullptr, 0, nullptr));
  if (!thread.is_valid()) {
    return 3;
  }
  ::WaitForSingleObject(thread.get(), INFINITE);
  return 0;
}

int OverrideRegistryAndInitializeCrashpad(const std::wstring& upload_url) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  if (testing::Test::HasFatalFailure()) {
    return 10;
  }

  base::win::RegKey key;
  if (key.Create(HKEY_LOCAL_MACHINE, kCrashpadRegistryKeyPath, KEY_SET_VALUE) !=
      ERROR_SUCCESS) {
    return 11;
  }
  if (key.WriteValue(kCrashpadUploadUrlValue, upload_url.c_str()) !=
      ERROR_SUCCESS) {
    return 12;
  }
  // Verify the registry override is in effect before starting the handler so a
  // regression in ReadRegistryConfig() never uploads a test minidump to the
  // default remote endpoint.
  if (CrashpadWin::ReadRegistryConfig().upload_url !=
      base::WideToUTF8(upload_url)) {
    return 13;
  }

  InitializeCrashpadReporting();
  return 0;
}

MULTIPROCESS_TEST_MAIN(EndToEndSupervisorChild) {
  const base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  const base::FilePath local_app_data =
      cmd->GetSwitchValuePath(kTestLocalAppDataSwitch);
  const std::wstring upload_url =
      cmd->GetSwitchValueNative(kTestUploadUrlSwitch);
  const std::wstring shutdown_event_name =
      cmd->GetSwitchValueNative(kTestShutdownEventSwitch);
  uint32_t parent_pid = 0;
  if (local_app_data.empty() || upload_url.empty() ||
      shutdown_event_name.empty() ||
      !base::StringToUint(cmd->GetSwitchValueASCII(kTestParentPidSwitch),
                          &parent_pid) ||
      parent_pid == 0) {
    return 1;
  }
  base::win::ScopedHandle parent_process(
      ::OpenProcess(SYNCHRONIZE, FALSE, parent_pid));
  if (!parent_process.is_valid()) {
    return 2;
  }

  base::ScopedPathOverride local_app_data_override(
      base::DIR_LOCAL_APP_DATA, local_app_data, /*is_absolute=*/true,
      /*create=*/true);

  if (const int init_rc = OverrideRegistryAndInitializeCrashpad(upload_url);
      init_rc != 0) {
    return init_rc;
  }

  const std::wstring handler_pipe = GetCrashpadHandlerPipe();
  if (handler_pipe.empty()) {
    return 3;
  }

  base::CommandLine client_cmd(base::GetMultiProcessTestChildBaseCommandLine());
  client_cmd.RemoveSwitch(switches::kTestChildProcess);
  client_cmd.RemoveSwitch(kTestLocalAppDataSwitch);
  client_cmd.RemoveSwitch(kTestUploadUrlSwitch);
  client_cmd.RemoveSwitch(kTestShutdownEventSwitch);
  client_cmd.RemoveSwitch(kTestParentPidSwitch);
  client_cmd.AppendSwitchNative(kTestHandlerPipeSwitch, handler_pipe);
  base::Process crashing_child = base::SpawnMultiProcessTestChild(
      "EndToEndCrashingClientChild", client_cmd, base::LaunchOptions());
  int client_exit_code = 0;
  // Wait action_timeout() so a hung crashing child causes the supervisor to
  // return exit code 4 before the parent's 2 * action_timeout() upload wait
  // expires.
  if (!base::WaitForMultiprocessTestChildExit(
          crashing_child, TestTimeouts::action_timeout(), &client_exit_code)) {
    base::TerminateMultiProcessTestChild(crashing_child, 0, /*wait=*/true);
    return 4;
  }
  if (static_cast<DWORD>(client_exit_code) != EXCEPTION_ACCESS_VIOLATION) {
    return 5;
  }

  base::win::ScopedHandle shutdown_event(
      ::OpenEventW(SYNCHRONIZE, FALSE, shutdown_event_name.c_str()));
  if (!shutdown_event.is_valid()) {
    return 6;
  }
  const std::array<HANDLE, 2> wait_handles = {shutdown_event.get(),
                                              parent_process.get()};
  const DWORD wait_result = ::WaitForMultipleObjects(
      static_cast<DWORD>(wait_handles.size()), wait_handles.data(),
      /*bWaitAll=*/FALSE,
      base::saturated_cast<DWORD>(
          (2 * TestTimeouts::action_max_timeout()).InMilliseconds()));
  if (wait_result == WAIT_OBJECT_0) {
    return 0;
  }
  return wait_result == WAIT_OBJECT_0 + 1 ? 8 : 7;
}

TEST(CrashpadWinTest, EndToEndClientCrashAndUploadViaRegistryOverride) {
#if defined(ADDRESS_SANITIZER)
  GTEST_SKIP() << "Intentional SEH crash tests are not supported under ASAN.";
#else
  static constexpr char kMockCrashReportId[] = "mock_crash_report_id_42";

  if (IsCurrentProcessLocalSystem()) {
    GTEST_SKIP() << "Test requires a non-SYSTEM token so DIR_LOCAL_APP_DATA "
                    "isolates the Crashpad database.";
  }

  base::test::SingleThreadTaskEnvironment task_environment;

  base::FilePath exe_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_dir));
  ASSERT_TRUE(
      base::PathExists(exe_dir.Append(L"remoting_crashpad_handler.exe")));

  base::ScopedTempDir temp_local_app_data;
  ASSERT_TRUE(temp_local_app_data.CreateUniqueTempDir());

  std::atomic<bool> upload_captured{false};
  base::test::TestFuture<net::test_server::HttpRequest> upload_future;

  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(base::BindRepeating(
      [](std::atomic<bool>* captured,
         base::RepeatingCallback<void(net::test_server::HttpRequest)> on_upload,
         const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (!base::StartsWith(request.relative_url, "/cr/report",
                              base::CompareCase::SENSITIVE)) {
          return nullptr;
        }
        if (!captured->exchange(true)) {
          on_upload.Run(request);
        }
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_OK);
        response->set_content_type("text/plain");
        response->set_content(kMockCrashReportId);
        return response;
      },
      &upload_captured, upload_future.GetSequenceBoundRepeatingCallback()));
  ASSERT_TRUE(test_server.Start());

  const std::wstring shutdown_event_name =
      L"Local\\remoting_crashpad_test_shutdown_" +
      base::ASCIIToWide(base::UnguessableToken::Create().ToString());
  base::win::ScopedHandle shutdown_event(
      ::CreateEventW(nullptr, /*bManualReset=*/TRUE, /*bInitialState=*/FALSE,
                     shutdown_event_name.c_str()));
  ASSERT_TRUE(shutdown_event.is_valid());

  base::CommandLine supervisor_cmd(
      base::GetMultiProcessTestChildBaseCommandLine());
  supervisor_cmd.AppendSwitchPath(kTestLocalAppDataSwitch,
                                  temp_local_app_data.GetPath());
  supervisor_cmd.AppendSwitchASCII(kTestUploadUrlSwitch,
                                   test_server.GetURL("/cr/report").spec());
  supervisor_cmd.AppendSwitchNative(kTestShutdownEventSwitch,
                                    shutdown_event_name);
  supervisor_cmd.AppendSwitchASCII(
      kTestParentPidSwitch, base::NumberToString(::GetCurrentProcessId()));

  base::Process supervisor_process = base::SpawnMultiProcessTestChild(
      "EndToEndSupervisorChild", supervisor_cmd, base::LaunchOptions());
  ASSERT_TRUE(supervisor_process.IsValid());
  base::ScopedClosureRunner supervisor_cleanup(base::BindOnce(
      [](HANDLE event, base::Process* proc) {
        ::SetEvent(event);
        int cleanup_exit_code = 0;
        if (!base::WaitForMultiprocessTestChildExit(
                *proc, TestTimeouts::action_timeout(), &cleanup_exit_code)) {
          LOG(ERROR) << "Timed out waiting for supervisor child; terminating.";
          base::TerminateMultiProcessTestChild(*proc, 0, /*wait=*/true);
        } else if (cleanup_exit_code != 0) {
          LOG(ERROR) << "Supervisor child exited with code "
                     << cleanup_exit_code;
        }
      },
      shutdown_event.get(), &supervisor_process));

  // Periodically wake the task environment while RunUntil() polls external
  // child process exit and external Crashpad database file updates.
  base::RepeatingTimer poll_timer;
  poll_timer.Start(FROM_HERE, base::Milliseconds(25), base::DoNothing());

  std::optional<int> early_supervisor_exit_code;
  auto has_supervisor_exited_early = [&]() {
    if (early_supervisor_exit_code.has_value()) {
      return true;
    }
    int code = 0;
    if (base::WaitForMultiprocessTestChildExit(supervisor_process,
                                               base::TimeDelta(), &code)) {
      early_supervisor_exit_code = code;
      return true;
    }
    return false;
  };

  {
    // Allow 2 * action_timeout() (still well under the 45s test launcher
    // timeout) for 3-process startup, minidump capture, and WinHTTP upload.
    base::test::ScopedRunLoopTimeout upload_timeout(
        FROM_HERE, 2 * TestTimeouts::action_timeout());
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return upload_future.IsReady() || has_supervisor_exited_early();
    }));
  }
  ASSERT_FALSE(early_supervisor_exit_code.has_value())
      << "Supervisor child exited early with code "
      << *early_supervisor_exit_code;

  const net::test_server::HttpRequest& request = upload_future.Get();
  EXPECT_EQ(request.method, net::test_server::METHOD_POST);
  EXPECT_NE(request.relative_url.find("product=Chromoting_Win"),
            std::string::npos)
      << request.relative_url;
  auto ua_it = request.headers.find("User-Agent");
  ASSERT_NE(ua_it, request.headers.end());
  EXPECT_TRUE(base::StartsWith(ua_it->second, "Crashpad/",
                               base::CompareCase::SENSITIVE))
      << ua_it->second;

  std::string body = request.content;
  auto encoding_it = request.headers.find("Content-Encoding");
  if (encoding_it != request.headers.end() && encoding_it->second == "gzip") {
    ASSERT_TRUE(compression::GzipUncompress(request.content, &body));
  }
  EXPECT_NE(body.find("Chromoting_Win"), std::string::npos);
  EXPECT_NE(body.find("MDMP"), std::string::npos);

  // Compute the expected Crashpad database path inside temp_local_app_data and
  // wait briefly for remoting_crashpad_handler.exe to finish recording the
  // completed upload before shutting down the supervisor child.
  base::FilePath expected_db_path;
  {
    base::ScopedPathOverride local_app_data_override(
        base::DIR_LOCAL_APP_DATA, temp_local_app_data.GetPath(),
        /*is_absolute=*/true, /*create=*/false);
    expected_db_path = GetPerUserDataDir().Append(L"crashpad");
  }
  auto db = crashpad::CrashReportDatabase::InitializeWithoutCreating(
      expected_db_path);
  ASSERT_NE(db, nullptr);

  std::vector<crashpad::CrashReportDatabase::Report> completed_reports;
  ASSERT_TRUE(base::test::RunUntil([&]() {
    if (has_supervisor_exited_early()) {
      return true;
    }
    completed_reports.clear();
    return db->GetCompletedReports(&completed_reports) ==
               crashpad::CrashReportDatabase::kNoError &&
           !completed_reports.empty() && completed_reports[0].uploaded;
  }));
  ASSERT_FALSE(early_supervisor_exit_code.has_value())
      << "Supervisor child exited early with code "
      << *early_supervisor_exit_code;

  ASSERT_EQ(completed_reports.size(), 1u);
  EXPECT_TRUE(completed_reports[0].uploaded);
  EXPECT_EQ(completed_reports[0].id, kMockCrashReportId);
  db.reset();

  ::SetEvent(shutdown_event.get());
  int supervisor_exit_code = -1;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      supervisor_process, TestTimeouts::action_timeout(),
      &supervisor_exit_code));
  supervisor_cleanup.ReplaceClosure(base::DoNothing());
  EXPECT_EQ(supervisor_exit_code, 0);

  // remoting_crashpad_handler.exe exits asynchronously once its last client
  // (supervisor_process) disconnects from the handler pipe; retry deleting the
  // temp directory briefly in case the handler is still closing DB files.
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return temp_local_app_data.Delete(); }));
  poll_timer.Stop();
#endif  // defined(ADDRESS_SANITIZER)
}

}  // namespace remoting

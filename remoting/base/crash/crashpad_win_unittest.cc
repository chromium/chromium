// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/crashpad_win.h"

#include <windows.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/memory/raw_ref.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/threading/simple_thread.h"
#include "base/unguessable_token.h"
#include "base/win/access_control_list.h"
#include "base/win/scoped_handle.h"
#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#include "remoting/base/crash/crash_reporting_crashpad.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/crashpad/crashpad/util/win/address_types.h"
#include "third_party/crashpad/crashpad/util/win/exception_handler_server.h"

namespace remoting {
namespace {

constexpr char kTestHandlerPipeSwitch[] = "test-crashpad-handler-pipe";

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

}  // namespace remoting

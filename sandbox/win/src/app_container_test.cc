// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <winsock2.h>

#include <iphlpapi.h>

#include <sddl.h>

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process_info.h"
#include "base/rand_util.h"
#include "base/scoped_native_library.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/win/access_token.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_localalloc.h"
#include "base/win/scoped_process_information.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "sandbox/features.h"
#include "sandbox/win/src/app_container_base.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/tests/common/controller.h"
#include "sandbox/win/tests/common/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

const wchar_t kAppContainerSid[] =
    L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
    L"924012148-2839372144";

// Some tests depend on a timeout happening (e.g. to detect if firewall blocks a
// TCP/UDP connection from App Container). This does timeout intentionally for
// some tests, so can't be too long or test execution takes too long, but needs
// to be long enough to verify that the connection really did fail.
DWORD network_timeout() {
  return base::Seconds(1).InMilliseconds();
}

// Timeout on waiting for a process to start. Since this should always succeed,
// this can be longer than the network timeout above without unnecessarily
// slowing down the test execution time.
DWORD process_start_timeout() {
  return base::Seconds(10).InMilliseconds();
}

std::wstring GenerateRandomPackageName() {
  return base::StringPrintf(L"%016lX%016lX", base::RandUint64(),
                            base::RandUint64());
}

const char* TokenTypeToName(bool impersonation) {
  return impersonation ? "Impersonation Token" : "Primary Token";
}

void CheckToken(const absl::optional<base::win::AccessToken>& token,
                bool impersonation,
                PSECURITY_CAPABILITIES security_capabilities,
                bool restricted) {
  ASSERT_TRUE(token);
  EXPECT_EQ(restricted, token->IsRestricted())
      << TokenTypeToName(impersonation);
  EXPECT_TRUE(token->IsAppContainer()) << TokenTypeToName(impersonation);
  EXPECT_EQ(token->IsImpersonation(), impersonation)
      << TokenTypeToName(impersonation);
  if (impersonation) {
    EXPECT_FALSE(token->IsIdentification()) << TokenTypeToName(impersonation);
  }

  absl::optional<base::win::Sid> package_sid = token->AppContainerSid();
  ASSERT_TRUE(package_sid) << TokenTypeToName(impersonation);
  EXPECT_TRUE(package_sid->Equal(security_capabilities->AppContainerSid))
      << TokenTypeToName(impersonation);

  std::vector<base::win::AccessToken::Group> capabilities =
      token->Capabilities();
  ASSERT_EQ(capabilities.size(), security_capabilities->CapabilityCount)
      << TokenTypeToName(impersonation);
  for (size_t index = 0; index < capabilities.size(); ++index) {
    EXPECT_EQ(capabilities[index].GetAttributes(),
              security_capabilities->Capabilities[index].Attributes)
        << TokenTypeToName(impersonation);
    EXPECT_TRUE(capabilities[index].GetSid().Equal(
        security_capabilities->Capabilities[index].Sid))
        << TokenTypeToName(impersonation);
  }
}

void CheckProcessToken(HANDLE process,
                       PSECURITY_CAPABILITIES security_capabilities,
                       bool restricted) {
  CheckToken(base::win::AccessToken::FromProcess(process), false,
             security_capabilities, restricted);
}

void CheckThreadToken(HANDLE thread,
                      PSECURITY_CAPABILITIES security_capabilities,
                      bool restricted) {
  CheckToken(base::win::AccessToken::FromThread(thread), true,
             security_capabilities, restricted);
}

// Check for LPAC using an access check. We could query for a security attribute
// but that's undocumented and has the potential to change.
void CheckLpacToken(HANDLE process) {
  HANDLE token_handle;
  ASSERT_TRUE(::OpenProcessToken(process, TOKEN_ALL_ACCESS, &token_handle));
  base::win::ScopedHandle token(token_handle);
  ASSERT_TRUE(
      ::DuplicateToken(token.Get(), ::SecurityImpersonation, &token_handle));
  token.Set(token_handle);
  PSECURITY_DESCRIPTOR security_desc_ptr;
  // AC is AllPackages, S-1-15-2-2 is AllRestrictedPackages. An LPAC token
  // will get granted access of 2, where as a normal AC token will get 3.
  ASSERT_TRUE(::ConvertStringSecurityDescriptorToSecurityDescriptor(
      L"O:SYG:SYD:(A;;0x3;;;WD)(A;;0x1;;;AC)(A;;0x2;;;S-1-15-2-2)",
      SDDL_REVISION_1, &security_desc_ptr, nullptr));
  base::win::ScopedLocalAlloc security_desc =
      base::win::TakeLocalAlloc(security_desc_ptr);
  GENERIC_MAPPING generic_mapping = {};
  PRIVILEGE_SET priv_set = {};
  DWORD priv_set_length = sizeof(PRIVILEGE_SET);
  DWORD granted_access;
  BOOL access_status;
  ASSERT_TRUE(::AccessCheck(security_desc.get(), token.Get(), MAXIMUM_ALLOWED,
                            &generic_mapping, &priv_set, &priv_set_length,
                            &granted_access, &access_status));
  ASSERT_TRUE(access_status);
  ASSERT_EQ(DWORD{2}, granted_access);
}

// Generate a unique sandbox AC profile for the appcontainer based on the SHA1
// hash of the appcontainer_id. This does not need to be secure so using SHA1
// isn't a security concern.
std::wstring GetAppContainerProfileName() {
  std::string sandbox_base_name = std::string("cr.sb.net");
  // Create a unique app container ID for the test case. This ensures that if
  // multiple tests are running concurrently they don't mess with each other's
  // app containers.
  std::string appcontainer_id(
      testing::UnitTest::GetInstance()->current_test_info()->test_case_name());
  appcontainer_id +=
      testing::UnitTest::GetInstance()->current_test_info()->name();
  auto sha1 = base::SHA1HashString(appcontainer_id);
  std::string profile_name = base::StrCat(
      {sandbox_base_name, base::HexEncode(sha1.data(), sha1.size())});
  // CreateAppContainerProfile requires that the profile name is at most 64
  // characters but 50 on WCOS systems.  The size of sha1 is a constant 40, so
  // validate that the base names are sufficiently short that the total length
  // is valid on all systems.
  DCHECK_LE(profile_name.length(), 50U);
  return base::UTF8ToWide(profile_name);
}

// Adds an app container policy similar to network service.
ResultCode AddNetworkAppContainerPolicy(TargetPolicy* policy) {
  std::wstring profile_name = GetAppContainerProfileName();
  ResultCode ret = policy->AddAppContainerProfile(profile_name.c_str(), true);
  if (SBOX_ALL_OK != ret)
    return ret;
  ret = policy->SetTokenLevel(USER_UNPROTECTED, USER_UNPROTECTED);
  if (SBOX_ALL_OK != ret)
    return ret;
  scoped_refptr<AppContainer> app_container = policy->GetAppContainer();

  constexpr const wchar_t* kBaseCapsSt[] = {
      L"lpacChromeInstallFiles", L"registryRead", L"lpacIdentityServices",
      L"lpacCryptoServices"};
  constexpr const base::win::WellKnownCapability kBaseCapsWK[] = {
      base::win::WellKnownCapability::kPrivateNetworkClientServer,
      base::win::WellKnownCapability::kInternetClient,
      base::win::WellKnownCapability::kEnterpriseAuthentication};

  for (const auto* cap : kBaseCapsSt) {
    if (!app_container->AddCapability(cap)) {
      DLOG(ERROR) << "AppContainerProfile::AddCapability() failed";
      return SBOX_ERROR_CREATE_APPCONTAINER_CAPABILITY;
    }
  }

  for (const auto cap : kBaseCapsWK) {
    if (!app_container->AddCapability(cap)) {
      DLOG(ERROR) << "AppContainerProfile::AddCapability() failed";
      return SBOX_ERROR_CREATE_APPCONTAINER_CAPABILITY;
    }
  }

  app_container->SetEnableLowPrivilegeAppContainer(true);

  return SBOX_ALL_OK;
}

int InitWinsock() {
  WORD winsock_ver = MAKEWORD(2, 2);
  WSAData wsa_data;
  return WSAStartup(winsock_ver, &wsa_data);
}

class WSAEventHandleTraits {
 public:
  typedef HANDLE Handle;

  WSAEventHandleTraits() = delete;
  WSAEventHandleTraits(const WSAEventHandleTraits&) = delete;
  WSAEventHandleTraits& operator=(const WSAEventHandleTraits&) = delete;

  static bool CloseHandle(HANDLE handle) {
    return ::WSACloseEvent(handle) == TRUE;
  }
  static bool IsHandleValid(HANDLE handle) {
    return handle != INVALID_HANDLE_VALUE;
  }
  static HANDLE NullHandle() { return INVALID_HANDLE_VALUE; }
};

typedef base::win::GenericScopedHandle<WSAEventHandleTraits,
                                       base::win::DummyVerifierTraits>
    ScopedWSAEventHandle;

class SocketHandleTraits {
 public:
  typedef SOCKET Handle;

  SocketHandleTraits() = delete;
  SocketHandleTraits(const SocketHandleTraits&) = delete;
  SocketHandleTraits& operator=(const SocketHandleTraits&) = delete;

  static bool CloseHandle(SOCKET handle) { return ::closesocket(handle) == 0; }
  static bool IsHandleValid(SOCKET handle) { return handle != INVALID_SOCKET; }
  static SOCKET NullHandle() { return INVALID_SOCKET; }
};

class DummySocketVerifierTraits {
 public:
  using Handle = SOCKET;

  DummySocketVerifierTraits() = delete;
  DummySocketVerifierTraits(const DummySocketVerifierTraits&) = delete;
  DummySocketVerifierTraits& operator=(const DummySocketVerifierTraits&) =
      delete;

  static void StartTracking(SOCKET handle,
                            const void* owner,
                            const void* pc1,
                            const void* pc2) {}
  static void StopTracking(SOCKET handle,
                           const void* owner,
                           const void* pc1,
                           const void* pc2) {}
};

typedef base::win::GenericScopedHandle<SocketHandleTraits,
                                       DummySocketVerifierTraits>
    ScopedSocketHandle;

class AppContainerTest : public ::testing::Test {
 public:
  void SetUp() override {
    if (!features::IsAppContainerSandboxSupported())
      return;
    package_name_ = GenerateRandomPackageName();
    broker_services_ = GetBroker();
    policy_ = broker_services_->CreatePolicy();
    ASSERT_EQ(SBOX_ALL_OK,
              policy_->SetProcessMitigations(MITIGATION_HEAP_TERMINATE));
    ASSERT_EQ(SBOX_ALL_OK,
              policy_->AddAppContainerProfile(package_name_.c_str(), true));
    // For testing purposes we known the base class so cast directly.
    container_ =
        static_cast<AppContainerBase*>(policy_->GetAppContainer().get());
  }

  void TearDown() override {
    if (scoped_process_info_.IsValid())
      ::TerminateProcess(scoped_process_info_.process_handle(), 0);
    if (container_)
      AppContainerBase::Delete(package_name_.c_str());
  }

 protected:
  void CreateProcess() {
    // Get the path to the sandboxed app.
    wchar_t prog_name[MAX_PATH] = {};
    ASSERT_NE(DWORD{0}, ::GetModuleFileNameW(nullptr, prog_name, MAX_PATH));

    PROCESS_INFORMATION process_info = {};
    ResultCode last_warning = SBOX_ALL_OK;
    DWORD last_error = 0;
    ResultCode result = broker_services_->SpawnTarget(
        prog_name, prog_name, policy_, &last_warning, &last_error,
        &process_info);
    ASSERT_EQ(SBOX_ALL_OK, result) << "Last Error: " << last_error;
    scoped_process_info_.Set(process_info);
  }

  std::wstring package_name_;
  raw_ptr<BrokerServices> broker_services_;
  scoped_refptr<AppContainerBase> container_;
  scoped_refptr<TargetPolicy> policy_;
  base::win::ScopedProcessInformation scoped_process_info_;
};

// A Very Simple UDP test server.
// Binds to all interfaces on a random port, and then waits to receive some
// data, then sends it back to the peer.
class UDPEchoServer {
 public:
  UDPEchoServer() = default;
  ~UDPEchoServer();

  // Start server.
  bool Start();
  // Get the listening port. Must be called after Start().
  int GetPort();
  // Gets the event that the child process should signal before trying to
  // connect to the server. This handle is only valid for the lifetime of the
  // UDPEchoServer. Must be called after Start().
  HANDLE GetProcessSignalEvent();

 private:
  void RecvTask();
  ScopedSocketHandle socket_;
  int port_;
  base::test::TaskEnvironment environment_;
  base::win::ScopedHandle trigger_event_;
};

UDPEchoServer::~UDPEchoServer() {
  // Make sure to drain threads before destructing.
  environment_.RunUntilIdle();
}

bool UDPEchoServer::Start() {
  SOCKET s = ::WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0,
                         WSA_FLAG_OVERLAPPED);
  if (s == INVALID_SOCKET)
    return false;
  socket_ = ScopedSocketHandle(s);

  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  // Pick a random port.
  server.sin_port = 0;

  int ret =
      bind(socket_.Get(), reinterpret_cast<sockaddr*>(&server), sizeof(server));
  if (ret == SOCKET_ERROR)
    return false;

  struct sockaddr_in bound_server;
  int bound_server_len = sizeof(bound_server);
  ret = getsockname(socket_.Get(), reinterpret_cast<sockaddr*>(&bound_server),
                    &bound_server_len);
  if (ret == SOCKET_ERROR)
    return false;
  port_ = ntohs(bound_server.sin_port);
  trigger_event_.Set(::CreateEvent(nullptr, /*bManualReset=*/TRUE,
                                   /*bInitialState=*/FALSE, nullptr));
  return base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(&UDPEchoServer::RecvTask, base::Unretained(this)));
}

void UDPEchoServer::RecvTask() {
  char buf[1024];
  WSABUF wsa_buf;
  wsa_buf.buf = buf;
  wsa_buf.len = sizeof(buf);
  struct sockaddr_in other;
  int other_len = sizeof(other);

  HANDLE recv_event_handle = WSACreateEvent();
  ASSERT_NE(WSA_INVALID_EVENT, recv_event_handle);
  ScopedWSAEventHandle recv_event(recv_event_handle);

  OVERLAPPED read_overlapped = {};
  read_overlapped.hEvent = recv_event.Get();
  DWORD flags = 0;
  int ret = WSARecvFrom(socket_.Get(), &wsa_buf, 1, nullptr, &flags,
                        reinterpret_cast<sockaddr*>(&other), &other_len,
                        &read_overlapped, nullptr);
  // Return value of 0 means operation completed immediately which should never
  // happen. SOCKET_ERROR returned means operation is pending.
  ASSERT_EQ(SOCKET_ERROR, ret);
  // Operation should be pending.
  ASSERT_EQ(WSA_IO_PENDING, ::WSAGetLastError());

  // Wait for the target process to have certainly started.
  DWORD wait =
      WaitForSingleObject(trigger_event_.Get(), process_start_timeout());

  // Wait to receive data from the child process.
  wait = WaitForSingleObject(recv_event.Get(), network_timeout());

  if (wait != WAIT_OBJECT_0)
    return;  // No connections. Expected for certain types of tests.

  DWORD num_bytes_recv = 0;
  flags = 0;
  BOOL overlapped_result = WSAGetOverlappedResult(
      socket_.Get(), &read_overlapped, &num_bytes_recv, FALSE, &flags);
  ASSERT_TRUE(overlapped_result);

  // Now reply.
  HANDLE send_event_handle = WSACreateEvent();
  ASSERT_NE(WSA_INVALID_EVENT, send_event_handle);
  ScopedWSAEventHandle send_event(send_event_handle);

  OVERLAPPED send_overlapped = {};
  read_overlapped.hEvent = send_event.Get();

  ret = WSASendTo(socket_.Get(), &wsa_buf, 1, nullptr, 0,
                  reinterpret_cast<sockaddr*>(&other), other_len,
                  &send_overlapped, nullptr);
  // Return value of 0 means operation completed immediately, which means data
  // was successfully sent to the peer.
  if (ret == 0)
    return;
  // If not, the operation should be pending.
  ASSERT_EQ(WSA_IO_PENDING, ::WSAGetLastError());
  // Wait for send.
  wait = WaitForSingleObject(send_event.Get(), network_timeout());
  // Send should always succeed in a timely manner.
  EXPECT_EQ(wait, WAIT_OBJECT_0);
}

int UDPEchoServer::GetPort() {
  return port_;
}

HANDLE UDPEchoServer::GetProcessSignalEvent() {
  return trigger_event_.Get();
}

}  // namespace

SBOX_TESTS_COMMAND int AppContainerEvent_Open(int argc, wchar_t** argv) {
  if (argc != 1)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  base::win::ScopedHandle event_open(
      ::OpenEvent(EVENT_ALL_ACCESS, false, argv[0]));
  DWORD error_open = ::GetLastError();

  if (event_open.IsValid())
    return SBOX_TEST_SUCCEEDED;

  if (ERROR_ACCESS_DENIED == error_open || ERROR_BAD_PATHNAME == error_open ||
      ERROR_FILE_NOT_FOUND == error_open) {
    return SBOX_TEST_DENIED;
  }

  return SBOX_TEST_FAILED;
}

TEST_F(AppContainerTest, DenyOpenEventForLowBox) {
  if (!features::IsAppContainerSandboxSupported())
    return;

  TestRunner runner(JobLevel::kUnprotected, USER_UNPROTECTED, USER_UNPROTECTED);

  EXPECT_EQ(SBOX_ALL_OK, runner.GetPolicy()->SetLowBox(kAppContainerSid));
  // Run test once, this ensures the app container directory exists, we
  // ignore the result.
  runner.RunTest(L"AppContainerEvent_Open test");
  std::wstring event_name = L"AppContainerNamedObjects\\";
  event_name += kAppContainerSid;
  event_name += L"\\test";

  base::win::ScopedHandle event(
      ::CreateEvent(nullptr, false, false, event_name.c_str()));
  ASSERT_TRUE(event.IsValid());

  TestRunner runner2(JobLevel::kUnprotected, USER_UNPROTECTED,
                     USER_UNPROTECTED);
  EXPECT_EQ(SBOX_TEST_DENIED, runner2.RunTest(L"AppContainerEvent_Open test"));
}

TEST_F(AppContainerTest, CheckIncompatibleOptions) {
  if (!container_)
    return;
  EXPECT_EQ(SBOX_ERROR_BAD_PARAMS,
            policy_->SetIntegrityLevel(INTEGRITY_LEVEL_UNTRUSTED));
  EXPECT_EQ(SBOX_ERROR_BAD_PARAMS, policy_->SetLowBox(kAppContainerSid));

  MitigationFlags expected_mitigations = 0;
  MitigationFlags expected_delayed = MITIGATION_HEAP_TERMINATE;
  sandbox::ResultCode expected_result = SBOX_ERROR_BAD_PARAMS;

  if (base::win::GetVersion() >= base::win::Version::WIN10_RS5) {
    expected_mitigations = MITIGATION_HEAP_TERMINATE;
    expected_delayed = 0;
    expected_result = SBOX_ALL_OK;
  }

  EXPECT_EQ(expected_mitigations, policy_->GetProcessMitigations());
  EXPECT_EQ(expected_delayed, policy_->GetDelayedProcessMitigations());
  EXPECT_EQ(expected_result,
            policy_->SetProcessMitigations(MITIGATION_HEAP_TERMINATE));
}

TEST_F(AppContainerTest, NoCapabilities) {
  if (!container_)
    return;

  policy_->SetTokenLevel(USER_UNPROTECTED, USER_UNPROTECTED);
  policy_->SetJobLevel(JobLevel::kNone, 0);

  CreateProcess();
  auto security_capabilities = container_->GetSecurityCapabilities();

  CheckProcessToken(scoped_process_info_.process_handle(),
                    security_capabilities.get(), FALSE);
  CheckThreadToken(scoped_process_info_.thread_handle(),
                   security_capabilities.get(), FALSE);
}

TEST_F(AppContainerTest, NoCapabilitiesRestricted) {
  if (!container_)
    return;

  policy_->SetTokenLevel(USER_LOCKDOWN, USER_RESTRICTED_SAME_ACCESS);
  policy_->SetJobLevel(JobLevel::kNone, 0);

  CreateProcess();
  auto security_capabilities = container_->GetSecurityCapabilities();

  CheckProcessToken(scoped_process_info_.process_handle(),
                    security_capabilities.get(), TRUE);
  CheckThreadToken(scoped_process_info_.thread_handle(),
                   security_capabilities.get(), TRUE);
}

TEST_F(AppContainerTest, WithCapabilities) {
  if (!container_)
    return;

  container_->AddCapability(base::win::WellKnownCapability::kInternetClient);
  container_->AddCapability(
      base::win::WellKnownCapability::kInternetClientServer);
  policy_->SetTokenLevel(USER_UNPROTECTED, USER_UNPROTECTED);
  policy_->SetJobLevel(JobLevel::kNone, 0);

  CreateProcess();
  auto security_capabilities = container_->GetSecurityCapabilities();

  CheckProcessToken(scoped_process_info_.process_handle(),
                    security_capabilities.get(), FALSE);
  CheckThreadToken(scoped_process_info_.thread_handle(),
                   security_capabilities.get(), FALSE);
}

TEST_F(AppContainerTest, WithCapabilitiesRestricted) {
  if (!container_)
    return;

  container_->AddCapability(base::win::WellKnownCapability::kInternetClient);
  container_->AddCapability(
      base::win::WellKnownCapability::kInternetClientServer);
  policy_->SetTokenLevel(USER_LOCKDOWN, USER_RESTRICTED_SAME_ACCESS);
  policy_->SetJobLevel(JobLevel::kNone, 0);

  CreateProcess();
  auto security_capabilities = container_->GetSecurityCapabilities();

  CheckProcessToken(scoped_process_info_.process_handle(),
                    security_capabilities.get(), TRUE);
  CheckThreadToken(scoped_process_info_.thread_handle(),
                   security_capabilities.get(), TRUE);
}

TEST_F(AppContainerTest, WithImpersonationCapabilities) {
  if (!container_)
    return;

  container_->AddCapability(base::win::WellKnownCapability::kInternetClient);
  container_->AddCapability(
      base::win::WellKnownCapability::kInternetClientServer);
  container_->AddImpersonationCapability(
      base::win::WellKnownCapability::kPrivateNetworkClientServer);
  container_->AddImpersonationCapability(
      base::win::WellKnownCapability::kPicturesLibrary);
  policy_->SetTokenLevel(USER_UNPROTECTED, USER_UNPROTECTED);
  policy_->SetJobLevel(JobLevel::kNone, 0);

  CreateProcess();
  auto security_capabilities = container_->GetSecurityCapabilities();

  CheckProcessToken(scoped_process_info_.process_handle(),
                    security_capabilities.get(), FALSE);
  SecurityCapabilities impersonation_security_capabilities(
      container_->GetPackageSid(), container_->GetImpersonationCapabilities());
  CheckThreadToken(scoped_process_info_.thread_handle(),
                   &impersonation_security_capabilities, FALSE);
}

TEST_F(AppContainerTest, NoCapabilitiesLPAC) {
  if (!features::IsAppContainerSandboxSupported())
    return;

  container_->SetEnableLowPrivilegeAppContainer(true);
  policy_->SetTokenLevel(USER_UNPROTECTED, USER_UNPROTECTED);
  policy_->SetJobLevel(JobLevel::kNone, 0);

  CreateProcess();
  auto security_capabilities = container_->GetSecurityCapabilities();

  CheckProcessToken(scoped_process_info_.process_handle(),
                    security_capabilities.get(), FALSE);
  CheckThreadToken(scoped_process_info_.thread_handle(),
                   security_capabilities.get(), FALSE);
  CheckLpacToken(scoped_process_info_.process_handle());
}

SBOX_TESTS_COMMAND int LoadDLL(int argc, wchar_t** argv) {
  // Library here doesn't matter as long as it's in the output directory: re-use
  // one from another sbox test.
  base::ScopedNativeLibrary test_dll(
      base::FilePath(FILE_PATH_LITERAL("sbox_integration_test_win_proc.exe")));
  if (test_dll.is_valid())
    return SBOX_TEST_SUCCEEDED;
  return SBOX_TEST_FAILED;
}

SBOX_TESTS_COMMAND int CheckIsAppContainer(int argc, wchar_t** argv) {
  if (base::IsCurrentProcessInAppContainer())
    return SBOX_TEST_SUCCEEDED;
  return SBOX_TEST_FAILED;
}

// Attempts to create a TCP socket,  connect to specified address on a specified
// port.
//
// First parameter should contain the host. Second parameter should contain the
// port to connect to. Third parameter indicates whether the socket should be
// brokered (1) or created in-process (0).
//
// SBOX_TEST_INVALID_PARAMETER - Invalid number of parameters.
//
// SBOX_TEST_FIRST_ERROR - Could not create socket from call to WSASocket or
// socket broker operation.
//
// SBOX_TEST_SECOND_ERROR - Could not call successfully perform a non-blocking
// TCP connect().
//
// SBOX_TEST_FIFTH_ERROR - Could not acquire target services.
//
// SBOX_TEST_SIXTH_ERROR - Could not create necessary event for overlapped
// Connect operation.
//
// SBOX_TEST_SEVENTH_ERROR - Could not call WSAEventSelect on connect event.
//
// SBOX_TEST_TIMED_OUT - The connect timed out. This might be the correct result
// for certain types of tests e.g. when App Container is blocking either a TCP
// connect.
//
// This function can also return a WSAError if Winsock fails to initialize
// correctly.
SBOX_TESTS_COMMAND int Socket_CreateTCP(int argc, wchar_t** argv) {
  int init_status = InitWinsock();
  if (init_status != ERROR_SUCCESS)
    return init_status;
  SOCKET socket_handle = INVALID_SOCKET;

  if (argc < 3)
    return SBOX_TEST_INVALID_PARAMETER;

  if (::_wtoi(argv[2]) == 1) {
    TargetServices* target_services = SandboxFactory::GetTargetServices();
    if (!target_services)
      return SBOX_TEST_FIFTH_ERROR;
    socket_handle = target_services->CreateBrokeredSocket(AF_INET, SOCK_STREAM,
                                                          IPPROTO_TCP);
  } else {
    socket_handle = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0,
                                WSA_FLAG_OVERLAPPED);
  }

  if (socket_handle == INVALID_SOCKET)
    return SBOX_TEST_FIRST_ERROR;

  ScopedSocketHandle socket(socket_handle);

  sockaddr_in local_service = {};
  local_service.sin_family = AF_INET;
  std::string hostname = base::WideToUTF8(argv[0]);
  local_service.sin_addr.s_addr = inet_addr(hostname.c_str());
  local_service.sin_port = htons(::_wtoi(argv[1]));

  HANDLE connect_event_handle = WSACreateEvent();
  if (connect_event_handle == WSA_INVALID_EVENT)
    return SBOX_TEST_SIXTH_ERROR;

  ScopedWSAEventHandle connect_event(connect_event_handle);
  // For TCP sockets, wait on the connect.
  int event_select =
      WSAEventSelect(socket.Get(), connect_event.Get(), FD_CONNECT);

  if (event_select)
    return SBOX_TEST_SEVENTH_ERROR;

  int ret = ::connect(socket.Get(), reinterpret_cast<sockaddr*>(&local_service),
                      sizeof(local_service));
  if (ret != SOCKET_ERROR || WSAGetLastError() != WSAEWOULDBLOCK) {
    return SBOX_TEST_SECOND_ERROR;
  }
  // Non-blocking socket, always returns SOCKET_ERROR and sets WSAlastError to
  // WSAEWOULDBLOCK.
  // Wait for the connect to succeed.
  DWORD wait = WaitForSingleObject(connect_event.Get(), network_timeout());

  if (wait != WAIT_OBJECT_0)
    return SBOX_TEST_TIMED_OUT;

  return SBOX_TEST_SUCCEEDED;
}

// Attempts to create a UDP socket, connect to specified address on a specified
// port, and transmit/receive data.
//
// First parameter should contain the host. Second parameter should contain the
// port to connect to. Third parameter contains an event that should be
// signalled when the process is about to make the connection. Fourth parameter
// indicates whether the socket should be brokered (1) or created in-process
// (0).
//
// Returns:
//
// SBOX_TEST_INVALID_PARAMETER - Invalid number of parameters.
//
// SBOX_TEST_FIRST_ERROR - Could not create socket from call to WSASocket or
// socket broker operation.
//
// SBOX_TEST_THIRD_ERROR - Could not successfully perform a non-blocking UDP
// sendto().
//
// SBOX_TEST_FOURTH_ERROR - Could not successfully perform a non-blocking UDP
// recv().
//
// SBOX_TEST_FIFTH_ERROR - Could not acquire target services.
//
// SBOX_TEST_SIXTH_ERROR - Could not create necessary event for overlapped
// SendTo operation.
//
// SBOX_TEST_SEVENTH_ERROR - Could not create necessary event for overlapped
// Recv operation.
//
// SBOX_TEST_TIMED_OUT - One of the above operations (connect, sendto, recv)
// timed out. This might be the correct result for certain types of tests e.g.
// when App Container is blocking either a TCP connect or UDP recv.
//
// This function can also return a WSAError if Winsock fails to initialize
// correctly.
SBOX_TESTS_COMMAND int Socket_CreateUDP(int argc, wchar_t** argv) {
  int init_status = InitWinsock();
  if (init_status != ERROR_SUCCESS)
    return init_status;
  SOCKET socket_handle = INVALID_SOCKET;

  if (argc < 4)
    return SBOX_TEST_INVALID_PARAMETER;

  // Set the event that the UDP server is waiting for.
  ::SetEvent(base::win::Uint32ToHandle(::_wtoi(argv[2])));

  if (::_wtoi(argv[3]) == 1) {
    TargetServices* target_services = SandboxFactory::GetTargetServices();
    if (!target_services)
      return SBOX_TEST_FIFTH_ERROR;
    socket_handle =
        target_services->CreateBrokeredSocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  } else {
    socket_handle = ::WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0,
                                WSA_FLAG_OVERLAPPED);
  }

  if (socket_handle == INVALID_SOCKET)
    return SBOX_TEST_FIRST_ERROR;

  ScopedSocketHandle socket(socket_handle);
  sockaddr_in local_service = {};
  local_service.sin_family = AF_INET;
  std::string hostname = base::WideToUTF8(argv[0]);
  local_service.sin_addr.s_addr = inet_addr(hostname.c_str());
  local_service.sin_port = htons(::_wtoi(argv[1]));

  char data[] = "hello";

  WSABUF write_buffer = {};
  write_buffer.buf = data;
  write_buffer.len = sizeof(data);
  HANDLE send_event_handle = WSACreateEvent();
  if (send_event_handle == WSA_INVALID_EVENT)
    return SBOX_TEST_SIXTH_ERROR;
  ScopedWSAEventHandle send_event(send_event_handle);
  OVERLAPPED write_overlapped = {};
  write_overlapped.hEvent = send_event.Get();
  int ret = ::WSASendTo(socket.Get(), &write_buffer, 1, nullptr, 0,
                        reinterpret_cast<sockaddr*>(&local_service),
                        sizeof(local_service), &write_overlapped, nullptr);
  if (ret == 0) {
    // Operation completed immediately!
  } else {
    // Winsock should return WSA_IO_PENDING and we wait on the event.
    if (WSAGetLastError() != WSA_IO_PENDING)
      return SBOX_TEST_THIRD_ERROR;
    DWORD wait = WaitForSingleObject(send_event.Get(), network_timeout());

    if (wait != WAIT_OBJECT_0)
      return SBOX_TEST_TIMED_OUT;
  }

  // Now try to read the response.
  WSABUF read_buffer = {};
  char recv_buf[10] = {};
  read_buffer.buf = recv_buf;
  read_buffer.len = sizeof(recv_buf);
  HANDLE read_event_handle = WSACreateEvent();
  if (read_event_handle == WSA_INVALID_EVENT)
    return SBOX_TEST_SEVENTH_ERROR;
  ScopedWSAEventHandle read_event(read_event_handle);
  OVERLAPPED read_overlapped = {};
  read_overlapped.hEvent = read_event.Get();
  DWORD flags = MSG_PARTIAL;
  ret = ::WSARecv(socket.Get(), &read_buffer, 1, nullptr, &flags,
                  &read_overlapped, nullptr);
  if (ret == 0) {
    // Operation completed immediately!
  } else {
    // Winsock should return WSA_IO_PENDING and we wait on the event.
    if (WSAGetLastError() != WSA_IO_PENDING) {
      return SBOX_TEST_FOURTH_ERROR;
    }
    DWORD wait = WaitForSingleObject(read_event.Get(), network_timeout());

    if (wait != WAIT_OBJECT_0)
      return SBOX_TEST_TIMED_OUT;
  }

  return SBOX_TEST_SUCCEEDED;
}

TEST(AppContainerLaunchTest, CheckLPACACE) {
  if (!features::IsAppContainerSandboxSupported())
    return;
  TestRunner runner;
  AddNetworkAppContainerPolicy(runner.GetPolicy());

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"LoadDLL"));

  AppContainerBase::Delete(GetAppContainerProfileName().c_str());
}

TEST(AppContainerLaunchTest, IsAppContainer) {
  if (!features::IsAppContainerSandboxSupported())
    return;
  TestRunner runner;
  AddNetworkAppContainerPolicy(runner.GetPolicy());

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckIsAppContainer"));

  AppContainerBase::Delete(GetAppContainerProfileName().c_str());
}

TEST(AppContainerLaunchTest, IsNotAppContainer) {
  TestRunner runner;

  EXPECT_EQ(SBOX_TEST_FAILED, runner.RunTest(L"CheckIsAppContainer"));
}

class SocketBrokerTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          ::testing::tuple</* using app container */ bool,
                           /* using socket brokering */ bool,
                           /* connect to real adapter */ bool,
                           /* add brokering rule */ bool>> {
 public:
  void SetUp() override {
    ASSERT_EQ(ERROR_SUCCESS, InitWinsock());
    SetUpSandboxPolicy();
  }

  void TearDown() override {
    if (IsTestInAppContainer())
      AppContainerBase::Delete(GetAppContainerProfileName().c_str());
  }

 protected:
  bool IsTestInAppContainer() { return ::testing::get<0>(GetParam()); }
  bool IsTestUsingBrokeredSockets() { return ::testing::get<1>(GetParam()); }
  bool IsTestConnectingToRealAdapter() { return ::testing::get<2>(GetParam()); }
  bool ShouldBrokerRuleBeAdded() { return ::testing::get<3>(GetParam()); }
  SboxTestResult GetExpectedTestResult() {
    if (!IsTestInAppContainer())
      return SBOX_TEST_SUCCEEDED;

    if (IsTestUsingBrokeredSockets()) {
      if (ShouldBrokerRuleBeAdded())
        return SBOX_TEST_SUCCEEDED;
      return SBOX_TEST_FIRST_ERROR;
    }

    return SBOX_TEST_TIMED_OUT;
  }

  std::wstring GetTestHostName() {
    if (!IsTestConnectingToRealAdapter())
      return L"127.0.0.1";
    // Try and obtain a local network address.
    // MSDN recommends a 15KB buffer for the first try at GetAdaptersAddresses.
    size_t buffer_size = 16384;
    std::unique_ptr<char[]> adapter_info(new char[buffer_size]);
    PIP_ADAPTER_ADDRESSES adapter_addrs =
        reinterpret_cast<PIP_ADAPTER_ADDRESSES>(adapter_info.get());
    ULONG flags = (GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_ANYCAST |
                   GAA_FLAG_SKIP_MULTICAST);
    ULONG ret = 0;
    do {
      adapter_info.reset(new char[buffer_size]);
      adapter_addrs =
          reinterpret_cast<PIP_ADAPTER_ADDRESSES>(adapter_info.get());
      ret = GetAdaptersAddresses(AF_INET, flags, 0, adapter_addrs,
                                 reinterpret_cast<PULONG>(&buffer_size));
    } while (ret == ERROR_BUFFER_OVERFLOW);
    if (ret != ERROR_SUCCESS)
      return std::wstring();
    while (adapter_addrs) {
      if (adapter_addrs->OperStatus == IfOperStatusUp) {
        PIP_ADAPTER_UNICAST_ADDRESS address =
            adapter_addrs->FirstUnicastAddress;
        for (; address; address = address->Next) {
          if (address->Address.lpSockaddr->sa_family != AF_INET)
            continue;
          sockaddr_in* ipv4_addr =
              reinterpret_cast<sockaddr_in*>(address->Address.lpSockaddr);
          return base::UTF8ToWide(inet_ntoa(ipv4_addr->sin_addr));
        }
      }
      adapter_addrs = adapter_addrs->Next;
    }
    return std::wstring();
  }

  void SetUpSandboxPolicy() {
    if (IsTestInAppContainer()) {
      AddNetworkAppContainerPolicy(runner_.GetPolicy());
    } else {
      TargetPolicy* policy = runner_.GetPolicy();
      policy->SetTokenLevel(USER_RESTRICTED_SAME_ACCESS, USER_LIMITED);
    }
    if (ShouldBrokerRuleBeAdded()) {
      TargetPolicy* policy = runner_.GetPolicy();
      policy->AddRule(TargetPolicy::SUBSYS_SOCKET,
                      TargetPolicy::SOCKET_ALLOW_BROKER, nullptr);
    }
  }

 protected:
  TestRunner runner_;
};

TEST_P(SocketBrokerTest, SocketBrokerTestUDP) {
  // Some APIs, such as named capabilities, needed to create the network service
  // sandbox require Windows 10 RS2.
  if (!features::IsAppContainerSandboxSupported())
    return;

  UDPEchoServer server;
  ASSERT_TRUE(server.Start());

  std::wstring hostname = GetTestHostName();
  ASSERT_TRUE(!hostname.empty());
  runner_.GetPolicy()->AddHandleToShare(server.GetProcessSignalEvent());
  EXPECT_EQ(
      GetExpectedTestResult(),
      runner_.RunTest(base::StringPrintf(L"Socket_CreateUDP %ls %d %d %d",
                                         hostname.c_str(), server.GetPort(),
                                         server.GetProcessSignalEvent(),
                                         IsTestUsingBrokeredSockets() ? 1 : 0)
                          .c_str()));
}

TEST_P(SocketBrokerTest, SocketBrokerTestTCP) {
  // Some APIs, such as named capabilities, needed to create the network service
  // sandbox require Windows 10 RS2.
  if (!features::IsAppContainerSandboxSupported())
    return;

  std::wstring hostname = GetTestHostName();
  ASSERT_TRUE(!hostname.empty());
  EXPECT_EQ(
      GetExpectedTestResult(),
      runner_.RunTest(base::StringPrintf(L"Socket_CreateTCP %ls 445 %d",
                                         hostname.c_str(),
                                         IsTestUsingBrokeredSockets() ? 1 : 0)
                          .c_str()));
}

INSTANTIATE_TEST_SUITE_P(
    AppContainerBrokered,
    SocketBrokerTest,
    ::testing::Combine(
        /* using app container */ ::testing::Values(true),
        /* using socket brokering */ ::testing::Values(true),
        /* connect to real adapter */ ::testing::Values(true, false),
        /* add brokering rule */ ::testing::Values(true, false)));
INSTANTIATE_TEST_SUITE_P(
    AppContainerNonBrokered,
    SocketBrokerTest,
    ::testing::Combine(
        /* using app container */ ::testing::Values(true),
        /* using socket brokering */ ::testing::Values(false),
        /* connect to real adapter */ ::testing::Values(true, false),
        /* add brokering rule */ ::testing::Values(true, false)));
INSTANTIATE_TEST_SUITE_P(
    NoAppContainerNonBrokered,
    SocketBrokerTest,
    ::testing::Combine(
        /* using app container */ ::testing::Values(false),
        /* using socket brokering */ ::testing::Values(false),
        /* connect to real adapter */ ::testing::Values(true, false),
        /* add brokering rule */ ::testing::Values(true, false)));

}  // namespace sandbox

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/win/tests/common/controller.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/dcheck_is_on.h"
#include "base/functional/callback.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/app_container.h"
#include "sandbox/win/src/sandbox_factory.h"

namespace {

// Used by the machinery that counts how many processes the sandbox is tracking.
HANDLE g_no_targets_event = nullptr;

// Helper to track the number of live processes, sets an event when there are
// none and resets it when one process is added.
class TargetTracker : public sandbox::BrokerServicesTargetTracker {
 public:
  TargetTracker(HANDLE no_targets) : no_targets_event_(no_targets) {
    // We create this in a test thread but it is only accessed on the sandbox
    // internal events thread.
    DETACH_FROM_SEQUENCE(target_events_sequence_);
    ::ResetEvent(no_targets_event_);
  }
  TargetTracker(const TargetTracker&) = delete;
  TargetTracker& operator=(const TargetTracker&) = delete;
  ~TargetTracker() override {}

  void OnTargetAdded() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(target_events_sequence_);
    ++targets_;
    if (1 == targets_) {
      ::ResetEvent(no_targets_event_);
    }
  }
  void OnTargetRemoved() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(target_events_sequence_);
    CHECK_NE(targets_, 0U);
    --targets_;
    if (targets_ == 0) {
      ::SetEvent(no_targets_event_);
    }
  }

 private:
  // Event is owned by the test framework but we can set it.
  const HANDLE no_targets_event_;
  // Number of processes we're tracking (both directly associated with a
  // TargetPolicy in a job, and those launched by tracked jobs).
  size_t targets_ GUARDED_BY_CONTEXT(target_events_sequence_) = 0;
  SEQUENCE_CHECKER(target_events_sequence_);
};

bool IsProcessRunning(HANDLE process) {
  DWORD exit_code = 0;
  if (::GetExitCodeProcess(process, &exit_code))
    return exit_code == STILL_ACTIVE;
  return false;
}

bool WaitForAllTargetsInternal() {
  ::WaitForSingleObject(g_no_targets_event, INFINITE);
  return true;
}

}  // namespace

namespace sandbox {

// Constructs a full path to a file inside the system32 folder.
std::wstring MakePathToSys32(const wchar_t* name, bool is_obj_man_path) {
  wchar_t windows_path[MAX_PATH] = {0};
  if (0 == ::GetSystemWindowsDirectoryW(windows_path, MAX_PATH))
    return std::wstring();

  std::wstring full_path(windows_path);
  if (full_path.empty())
    return full_path;

  if (is_obj_man_path)
    full_path.insert(0, L"\\??\\");

  full_path += L"\\system32\\";
  full_path += name;
  return full_path;
}

// Constructs a full path to a file inside the syswow64 folder.
std::wstring MakePathToSysWow64(const wchar_t* name, bool is_obj_man_path) {
  wchar_t windows_path[MAX_PATH] = {0};
  if (0 == ::GetSystemWindowsDirectoryW(windows_path, MAX_PATH))
    return std::wstring();

  std::wstring full_path(windows_path);
  if (full_path.empty())
    return full_path;

  if (is_obj_man_path)
    full_path.insert(0, L"\\??\\");

  full_path += L"\\SysWOW64\\";
  full_path += name;
  return full_path;
}

std::wstring MakePathToSys(const wchar_t* name, bool is_obj_man_path) {
  return (base::win::OSInfo::GetInstance()->IsWowX86OnAMD64())
             ? MakePathToSysWow64(name, is_obj_man_path)
             : MakePathToSys32(name, is_obj_man_path);
}

// This delegate is required for initializing BrokerServices and configures it
// to use synchronous launching.
class TestBrokerServicesDelegateImpl : public BrokerServicesDelegate {
 public:
  bool ParallelLaunchEnabled() override { return false; }

  void ParallelLaunchPostTaskAndReplyWithResult(
      const base::Location& from_here,
      base::OnceCallback<CreateTargetResult()> task,
      base::OnceCallback<void(CreateTargetResult)> reply) override {
    // This function is only used for parallel launching and should not get
    // called.
    CHECK(false);
  }

  void BeforeTargetProcessCreateOnCreationThread(
      const void* trace_id) override {}

  void AfterTargetProcessCreateOnCreationThread(const void* trace_id,
                                                DWORD process_id) override {}
};

BrokerServices* GetBroker() {
  static BrokerServices* broker = SandboxFactory::GetBrokerServices();
  static bool is_initialized = false;

  if (!broker) {
    return NULL;
  }

  if (!is_initialized) {
    g_no_targets_event = ::CreateEventW(nullptr, true, false, nullptr);
    if (!g_no_targets_event) {
      return nullptr;
    }

    auto tracker = std::make_unique<TargetTracker>(g_no_targets_event);
    if (SBOX_ALL_OK != broker->InitForTesting(  // IN-TEST
                           std::make_unique<TestBrokerServicesDelegateImpl>(),
                           std::move(tracker))) {
      return nullptr;
    }

    is_initialized = true;
  }

  return broker;
}

TestRunner::TestRunner(JobLevel job_level,
                       TokenLevel startup_token,
                       TokenLevel main_token)
    : is_init_(false),
      is_async_(false),
      no_sandbox_(false),
      disable_csrss_(true),
      target_process_id_(0) {
  broker_ = nullptr;
  policy_.reset();
  timeout_ = TestTimeouts::test_launcher_timeout();
  state_ = AFTER_REVERT;
  is_async_= false;
  kill_on_destruction_ = true;
  target_process_id_ = 0;

  broker_ = GetBroker();
  if (!broker_)
    return;

  policy_ = broker_->CreatePolicy();
  if (!policy_)
    return;

  auto result = policy_->GetConfig()->SetJobLevel(job_level, 0);
  if (result != SBOX_ALL_OK)
    return;
  result = policy_->GetConfig()->SetTokenLevel(startup_token, main_token);
  if (result != SBOX_ALL_OK)
    return;

  is_init_ = true;
}

TestRunner::TestRunner()
    : TestRunner(JobLevel::kLockdown,
                 USER_RESTRICTED_SAME_ACCESS,
                 USER_LOCKDOWN) {}

TargetPolicy* TestRunner::GetPolicy() {
  return policy_.get();
}

TestRunner::~TestRunner() {
  if (target_process_.is_valid() && kill_on_destruction_) {
    ::TerminateProcess(target_process_.get(), 0);
  }
}

bool TestRunner::WaitForAllTargets() {
  return WaitForAllTargetsInternal();
}

bool TestRunner::AllowFileAccess(FileSemantics semantics,
                                 const wchar_t* pattern) {
  if (!is_init_)
    return false;

  if (policy_->GetConfig()->IsConfigured())
    return false;

  return (SBOX_ALL_OK ==
          policy_->GetConfig()->AllowFileAccess(semantics, pattern));
}

bool TestRunner::AddRuleSys32(FileSemantics semantics, const wchar_t* pattern) {
  if (!is_init_)
    return false;

  std::wstring win32_path = MakePathToSys32(pattern, false);
  if (win32_path.empty())
    return false;

  if (!AllowFileAccess(semantics, win32_path.c_str())) {
    return false;
  }

  if (!base::win::OSInfo::GetInstance()->IsWowX86OnAMD64())
    return true;

  win32_path = MakePathToSysWow64(pattern, false);
  if (win32_path.empty())
    return false;

  return AllowFileAccess(semantics, win32_path.c_str());
}

int TestRunner::RunTest(const wchar_t* command) {
  DCHECK_LE(MAX_STATE, 10);

  wchar_t state_number[2];
  state_number[0] = static_cast<wchar_t>(L'0' + state_);
  state_number[1] = L'\0';
  std::wstring full_command(state_number);
  full_command += L" ";
  full_command += command;

  return InternalRunTest(full_command.c_str());
}

int TestRunner::InternalRunTest(const wchar_t* command) {
  if (!is_init_)
    return SBOX_TEST_FAILED_TO_RUN_TEST;

  // For simplicity TestRunner supports only one process per instance.
  if (target_process_.is_valid()) {
    if (IsProcessRunning(target_process_.get())) {
      return SBOX_TEST_FAILED_TO_RUN_TEST;
    }
    target_process_.Close();
    target_process_id_ = 0;
  }

  if (disable_csrss_) {
    auto* config = policy_->GetConfig();
    if (config->GetAppContainer() == nullptr) {
      config->SetDisconnectCsrss();
    }
  }

  // Get the path to the sandboxed process.
  wchar_t prog_name[MAX_PATH];
  GetModuleFileNameW(NULL, prog_name, MAX_PATH);

  // Launch the sandboxed process.
  DWORD last_error = ERROR_SUCCESS;
  PROCESS_INFORMATION target = {0};

  std::wstring arguments(L"\"");
  arguments += prog_name;
  arguments += L"\" -child";
  arguments += no_sandbox_ ? L"-no-sandbox " : L" ";
  arguments += command;

  ResultCode result = SBOX_ALL_OK;
  if (no_sandbox_) {
    STARTUPINFO startup_info = {sizeof(STARTUPINFO)};
    if (!::CreateProcessW(prog_name, &arguments[0], NULL, NULL, FALSE, 0,
                          NULL, NULL, &startup_info, &target)) {
      return SBOX_ERROR_GENERIC;
    }
  } else {
    result = broker_->SpawnTarget(prog_name, arguments.c_str(),
                                  std::move(policy_), &last_error, &target);
  }

  if (SBOX_ALL_OK != result)
    return SBOX_TEST_FAILED_TO_RUN_TEST;

  FILETIME creation_time, exit_time, kernel_time, user_time;
  // Can never fail. If it does, then something really bad has happened.
  CHECK(::GetProcessTimes(target.hProcess, &creation_time, &exit_time,
                          &kernel_time, &user_time));

  // Execution times should be zero. If not, something has changed in Windows.
  CHECK_EQ(0, base::TimeDelta::FromFileTime(user_time).InMicroseconds());
  CHECK_EQ(0, base::TimeDelta::FromFileTime(kernel_time).InMicroseconds());

  ::ResumeThread(target.hThread);

  // For an asynchronous run we don't bother waiting.
  if (is_async_) {
    target_process_.Set(target.hProcess);
    target_process_id_ = target.dwProcessId;
    ::CloseHandle(target.hThread);
    return SBOX_TEST_SUCCEEDED;
  }

  if (::IsDebuggerPresent()) {
    // Don't kill the target process on a time-out while we are debugging.
    timeout_ = base::TimeDelta::Max();
  }

  if (WAIT_TIMEOUT == ::WaitForSingleObject(target.hProcess, timeout_ms())) {
    ::TerminateProcess(target.hProcess, static_cast<UINT>(SBOX_TEST_TIMED_OUT));
    ::CloseHandle(target.hProcess);
    ::CloseHandle(target.hThread);
    return SBOX_TEST_TIMED_OUT;
  }

  DWORD exit_code = static_cast<DWORD>(SBOX_TEST_LAST_RESULT);
  if (!::GetExitCodeProcess(target.hProcess, &exit_code)) {
    ::CloseHandle(target.hProcess);
    ::CloseHandle(target.hThread);
    return SBOX_TEST_FAILED_TO_RUN_TEST;
  }

  ::CloseHandle(target.hProcess);
  ::CloseHandle(target.hThread);

  return exit_code;
}

void TestRunner::SetTimeout(DWORD timeout_ms) {
  SetTimeout(timeout_ms == INFINITE ? base::TimeDelta::Max()
                                    : base::Milliseconds(timeout_ms));
}

void TestRunner::SetTimeout(base::TimeDelta timeout) {
  // We do not take -ve timeouts.
  DCHECK(timeout >= base::TimeDelta());
  // We need millisecond DWORDS but also cannot take exactly INFINITE,
  // for that should supply ::Max().
  DCHECK(timeout.is_inf() || timeout < base::Milliseconds(UINT_MAX));
  timeout_ = timeout;
}

DWORD TestRunner::timeout_ms() {
  if (timeout_.is_inf())
    return INFINITE;
  else
    return static_cast<DWORD>(timeout_.InMilliseconds());
}

void TestRunner::SetTestState(SboxTestsState desired_state) {
  state_ = desired_state;
}

// This is the main procedure for the target (child) application. We'll find out
// the target test and call it.
// We expect the arguments to be:
//  argv[1] = "-child"
//  argv[2] = SboxTestsState when to run the command
//  argv[3] = command to run
//  argv[4...] = command arguments.
int DispatchCall(int argc, wchar_t **argv) {
  if (argc < 4)
    return SBOX_TEST_INVALID_PARAMETER;

  // We hard code two tests to avoid dispatch failures.
  if (0 == _wcsicmp(argv[3], L"wait")) {
      Sleep(INFINITE);
      return SBOX_TEST_TIMED_OUT;
  }

  if (0 == _wcsicmp(argv[3], L"ping"))
      return SBOX_TEST_PING_OK;

  // If the caller shared a shared memory handle with us attempt to open it
  // in read only mode and sleep infinitely if we succeed.
  if (0 == _wcsicmp(argv[3], L"shared_memory_handle")) {
    HANDLE raw_handle = nullptr;
    std::string_view test_contents = "Hello World";
    base::StringToUint(base::AsStringPiece16(argv[4]),
                       reinterpret_cast<unsigned int*>(&raw_handle));
    if (raw_handle == nullptr)
      return SBOX_TEST_INVALID_PARAMETER;
    // First extract the handle to the platform-native ScopedHandle.
    base::win::ScopedHandle scoped_handle(raw_handle);
    if (!scoped_handle.is_valid()) {
      return SBOX_TEST_INVALID_PARAMETER;
    }
    // Then convert to the low-level chromium region.
    base::subtle::PlatformSharedMemoryRegion platform_region =
        base::subtle::PlatformSharedMemoryRegion::Take(
            std::move(scoped_handle),
            base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly,
            test_contents.size(), base::UnguessableToken::Create());
    // Finally wrap the low-level region in the shared memory API.
    base::ReadOnlySharedMemoryRegion region =
        base::ReadOnlySharedMemoryRegion::Deserialize(
            std::move(platform_region));
    if (!region.IsValid())
      return SBOX_TEST_INVALID_PARAMETER;
    base::ReadOnlySharedMemoryMapping view = region.Map();
    if (!view.IsValid())
      return SBOX_TEST_INVALID_PARAMETER;

    const std::string contents(view.GetMemoryAsSpan<char>().data());
    if (contents != test_contents)
      return SBOX_TEST_INVALID_PARAMETER;
    Sleep(INFINITE);
    return SBOX_TEST_TIMED_OUT;
  }

  SboxTestsState state = static_cast<SboxTestsState>(_wtoi(argv[2]));
  if ((state <= MIN_STATE) || (state >= MAX_STATE))
    return SBOX_TEST_INVALID_PARAMETER;

  HMODULE module;
  if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<wchar_t*>(&DispatchCall), &module))
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  std::string command_name = base::SysWideToMultiByte(argv[3], CP_UTF8);
  CommandFunction command = reinterpret_cast<CommandFunction>(
                                ::GetProcAddress(module, command_name.c_str()));
  if (!command)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  if (BEFORE_INIT == state)
    return command(argc - 4, argv + 4);
  else if (EVERY_STATE == state)
    command(argc - 4, argv + 4);

  TargetServices* target = SandboxFactory::GetTargetServices();
  if (target) {
    if (SBOX_ALL_OK != target->Init())
      return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

    if (BEFORE_REVERT == state)
      return command(argc - 4, argv + 4);
    else if (EVERY_STATE == state)
      command(argc - 4, argv + 4);

#if defined(ADDRESS_SANITIZER)
    // Bind and leak dbghelp.dll before the token is lowered, otherwise
    // AddressSanitizer will crash when trying to symbolize a report.
    if (!LoadLibraryA("dbghelp.dll"))
      return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
#endif

    target->LowerToken();
  } else if (0 != _wcsicmp(argv[1], L"-child-no-sandbox")) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  return command(argc - 4, argv + 4);
}

}  // namespace sandbox

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/tests/common/controller.h"

#include <windows.h>

#include <memory>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/app_container.h"
#include "sandbox/win/src/sandbox_factory.h"

namespace sandbox {

namespace {

// Helper to track the number of live processes, sets an event when there are
// none and resets it when one process is added.
class TargetTracker : public sandbox::BrokerServicesTargetTracker {
 public:
  TargetTracker() {
    // We create this in a test thread but it is only accessed on the sandbox
    // internal events thread.
    DETACH_FROM_SEQUENCE(target_events_sequence_);
    GetEvent().Reset();
  }
  TargetTracker(const TargetTracker&) = delete;
  TargetTracker& operator=(const TargetTracker&) = delete;
  ~TargetTracker() override {}

  void OnTargetAdded() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(target_events_sequence_);
    ++targets_;
    if (1 == targets_) {
      GetEvent().Reset();
    }
  }
  void OnTargetRemoved() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(target_events_sequence_);
    CHECK_NE(targets_, 0U);
    --targets_;
    if (targets_ == 0) {
      GetEvent().Signal();
    }
  }

  static void WaitForAllTargets() { GetEvent().Wait(); }

 private:
  // Number of processes we're tracking (both directly associated with a
  // TargetPolicy in a job, and those launched by tracked jobs).
  size_t targets_ GUARDED_BY_CONTEXT(target_events_sequence_) = 0;
  SEQUENCE_CHECKER(target_events_sequence_);

  // Used by the machinery that counts how many processes the sandbox is
  // tracking.
  static base::WaitableEvent& GetEvent() {
    static base::NoDestructor<base::WaitableEvent> instance;
    return *instance;
  }
};

constexpr char kChildSwitch[] = "child";
constexpr char kNoSandboxSwitch[] = "no-sandbox";
constexpr char kStateSwitch[] = "state";
constexpr char kCommandSwitch[] = "cmd";

std::wstring MakePathToSysBase(std::wstring_view name,
                               std::wstring_view sysname,
                               bool is_obj_man_path) {
  base::FilePath windows_path;
  if (!base::PathService::Get(base::DIR_WINDOWS, &windows_path)) {
    return {};
  }

  std::wstring full_path = windows_path.Append(sysname).Append(name).value();
  if (is_obj_man_path) {
    full_path.insert(0, L"\\??\\");
  }
  return full_path;
}

std::wstring MakePathToSys32(std::wstring_view name, bool is_obj_man_path) {
  return MakePathToSysBase(name, L"system32", is_obj_man_path);
}

std::wstring MakePathToSysWow64(std::wstring_view name, bool is_obj_man_path) {
  return MakePathToSysBase(name, L"SysWOW64", is_obj_man_path);
}

using CommandType =
    base::RepeatingCallback<int(base::span<const std::wstring>)>;

CommandType BindCommand(const std::string& command_name) {
  HMODULE module;
  if (!::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<wchar_t*>(&DispatchCall),
                           &module)) {
    return {};
  }

  FARPROC func = ::GetProcAddress(module, command_name.c_str());
  if (!func) {
    return {};
  }

  return base::BindRepeating(reinterpret_cast<CommandType::RunType*>(func));
}

}  // namespace

namespace internal {

template <>
std::string ToString(const std::wstring& value) {
  return base::WideToUTF8(value);
}

std::string ToString(const wchar_t* value) {
  return base::WideToUTF8(value);
}

}  // namespace internal

std::wstring MakePathToSys(std::wstring_view name, bool is_obj_man_path) {
  return (base::win::OSInfo::GetInstance()->IsWowX86OnAMD64())
             ? MakePathToSysWow64(name, is_obj_man_path)
             : MakePathToSys32(name, is_obj_man_path);
}

// This delegate is required for initializing BrokerServices and configures it
// to use synchronous launching.
class TestBrokerServicesDelegateImpl : public BrokerServicesDelegate {
 public:
  void ParallelLaunchPostTaskAndReplyWithResult(
      const base::Location& from_here,
      base::OnceCallback<CreateTargetResult()> task,
      base::OnceCallback<void(CreateTargetResult)> reply) override {
    base::ThreadPool::PostTaskAndReplyWithResult(
        from_here,
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
        std::move(task), std::move(reply));
  }

  void BeforeTargetProcessCreateOnCreationThread(
      const void* trace_id) override {}

  void AfterTargetProcessCreateOnCreationThread(const void* trace_id,
                                                DWORD process_id) override {}
  void OnCreateThreadActionCreateFailure(DWORD last_error) override {}
  void OnCreateThreadActionDuplicateFailure(DWORD last_error) override {}
};

BrokerServices* GetBroker() {
  static BrokerServices* instance = []() {
    BrokerServices* broker = SandboxFactory::GetBrokerServices();
    return SBOX_ALL_OK ==
                   broker->InitForTesting(  // IN-TEST
                       std::make_unique<TestBrokerServicesDelegateImpl>(),
                       std::make_unique<TargetTracker>())
               ? broker
               : nullptr;
  }();
  return instance;
}

// static
base::CommandLine TestRunnerBase::CreateCommandLine(
    std::string_view command,
    base::span<const std::string> args,
    SboxTestsState state,
    bool no_sandbox) {
  // Get the path to the sandboxed process.
  base::FilePath prog_name;
  CHECK(base::PathService::Get(base::FILE_EXE, &prog_name));
  base::CommandLine cmd_line(prog_name);
  cmd_line.AppendSwitch(kChildSwitch);
  if (no_sandbox) {
    cmd_line.AppendSwitch(kNoSandboxSwitch);
  }
  DCHECK_LE(MAX_STATE, 10);
  cmd_line.AppendSwitchASCII(kStateSwitch,
                             base::NumberToString(static_cast<int>(state)));
  cmd_line.AppendSwitchUTF8(kCommandSwitch, command);
  cmd_line.AppendArg("--");
  for (const auto& arg : args) {
    cmd_line.AppendArg(arg);
  }

  return cmd_line;
}

TestRunnerBase::TestRunnerBase(std::string_view command,
                               JobLevel job_level,
                               TokenLevel startup_token,
                               TokenLevel main_token)
    : command_(command), timeout_(TestTimeouts::test_launcher_timeout()) {
  broker_ = GetBroker();
  CHECK(broker_);
  policy_ = broker_->CreatePolicy();
  CHECK(policy_);
  CHECK_EQ(SBOX_ALL_OK, policy_->GetConfig()->SetJobLevel(job_level, 0));
  CHECK_EQ(SBOX_ALL_OK,
           policy_->GetConfig()->SetTokenLevel(startup_token, main_token));
}

TestRunnerBase::~TestRunnerBase() = default;

TargetPolicy* TestRunnerBase::GetPolicy() {
  return policy_.get();
}

TargetConfig* TestRunnerBase::GetConfig() {
  return GetPolicy()->GetConfig();
}

bool TestRunnerBase::WaitForAllTargets() {
  TargetTracker::WaitForAllTargets();
  return true;
}

bool TestRunnerBase::AllowFileAccess(FileSemantics semantics,
                                     std::wstring_view pattern) {
  if (policy_->GetConfig()->IsConfigured()) {
    return false;
  }
  return (SBOX_ALL_OK ==
          policy_->GetConfig()->AllowFileAccess(semantics, pattern));
}

bool TestRunnerBase::AddRuleSys32(FileSemantics semantics,
                                  std::wstring_view pattern) {
  std::wstring win32_path = MakePathToSys32(pattern, false);
  if (win32_path.empty()) {
    return false;
  }
  if (!AllowFileAccess(semantics, win32_path.c_str())) {
    return false;
  }

  if (!base::win::OSInfo::GetInstance()->IsWowX86OnAMD64()) {
    return true;
  }
  win32_path = MakePathToSysWow64(pattern, false);
  if (win32_path.empty()) {
    return false;
  }
  return AllowFileAccess(semantics, win32_path.c_str());
}

base::Process TestRunnerBase::CreateTestProcess(
    base::span<const std::string> args) {
  if (disable_csrss_) {
    auto* config = policy_->GetConfig();
    if (config->GetAppContainer() == nullptr) {
      config->SetDisconnectCsrss();
    }
  }

  // Launch the sandboxed process
  auto cmd_line = CreateCommandLine(command_, args, state_, no_sandbox_);
  return no_sandbox_ ? base::LaunchProcess(cmd_line, {})
                     : LaunchSandboxProcess(cmd_line);
}

int TestRunnerBase::WaitForResult(const base::Process& process) const {
  auto timeout = ::IsDebuggerPresent() ? base::TimeDelta::Max() : timeout_;
  int exit_code = SBOX_TEST_SUCCEEDED;
  if (!process.WaitForExitWithTimeout(timeout, &exit_code)) {
    return SBOX_TEST_TIMED_OUT;
  }

  return exit_code;
}

base::Process TestRunnerBase::LaunchSandboxProcess(
    const base::CommandLine& cmd_line) {
  ResultCode result = SBOX_ALL_OK;
  DWORD last_error = ERROR_SUCCESS;
  base::win::ScopedProcessInformation proc_info;
  base::test::TaskEnvironment task_environment;
  base::test::TestFuture<base::win::ScopedProcessInformation, DWORD, ResultCode>
      test_future;
  broker_->SpawnTargetAsync(cmd_line, std::move(policy_),
                            test_future.GetCallback());
  std::tie(proc_info, last_error, result) = test_future.Take();

  if (SBOX_ALL_OK != result) {
    return base::Process();
  }

  FILETIME creation_time, exit_time, kernel_time, user_time;
  // Can never fail. If it does, then something really bad has happened.
  CHECK(::GetProcessTimes(proc_info.process_handle(), &creation_time,
                          &exit_time, &kernel_time, &user_time));

  // Execution times should be zero. If not, something has changed in Windows.
  CHECK_EQ(0, base::TimeDelta::FromFileTime(user_time).InMicroseconds());
  CHECK_EQ(0, base::TimeDelta::FromFileTime(kernel_time).InMicroseconds());

  ::ResumeThread(proc_info.thread_handle());
  return base::Process(proc_info.TakeProcessHandle());
}

void TestRunnerBase::SetTimeout(DWORD timeout_ms) {
  SetTimeout(timeout_ms == INFINITE ? base::TimeDelta::Max()
                                    : base::Milliseconds(timeout_ms));
}

void TestRunnerBase::SetTimeout(base::TimeDelta timeout) {
  // We do not take -ve timeouts.
  DCHECK(timeout >= base::TimeDelta());
  // We need millisecond DWORDS but also cannot take exactly INFINITE,
  // for that should supply ::Max().
  DCHECK(timeout.is_inf() || timeout < base::Milliseconds(UINT_MAX));
  timeout_ = timeout;
}

bool IsChildProcessForTesting() {
  // Initialize the singleton command line.
  base::CommandLine::Init(0, nullptr);
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  return cmd_line->HasSwitch(kChildSwitch);
}

// This is the main procedure for the target (child) application. We'll find out
// the target test and call it.
// We expect the arguments to be:
//  argv[1] = "--child"
//  argv[2] = "--state=SboxTestsState" when to run the command
//  argv[3] = "--cmd=command" the command to run
//  argv[4] = "--" argument separator.
//  argv[5...] = command arguments.
int DispatchCall() {
  // Ensure the singleton command line is initialized.
  base::CommandLine::Init(0, nullptr);
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  CHECK(cmd_line->HasSwitch(kChildSwitch));

  const auto command_name = cmd_line->GetSwitchValueUTF8(kCommandSwitch);
  if (command_name.empty()) {
    return SBOX_TEST_INVALID_PARAMETER;
  }
  auto args = cmd_line->GetArgs();
  // We hard code two tests to avoid dispatch failures.
  if (command_name == sandbox::WaitCommandTestRunner::type::kTestName) {
    ::Sleep(INFINITE);
    return SBOX_TEST_TIMED_OUT;
  }

  if (command_name == PingCommandTestRunner::type::kTestName) {
    return SBOX_TEST_PING_OK;
  }

  int state_value;
  if (!base::StringToInt(cmd_line->GetSwitchValueASCII(kStateSwitch),
                         &state_value)) {
    return SBOX_TEST_INVALID_PARAMETER;
  }
  SboxTestsState state = static_cast<SboxTestsState>(state_value);
  if ((state <= MIN_STATE) || (state >= MAX_STATE)) {
    return SBOX_TEST_INVALID_PARAMETER;
  }

  HMODULE module;
  if (!::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<wchar_t*>(&DispatchCall),
                           &module)) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  auto command = BindCommand(command_name);
  if (!command) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  if (BEFORE_INIT == state) {
    return command.Run(args);
  } else if (EVERY_STATE == state) {
    command.Run(args);
  }
  TargetServices* target = SandboxFactory::GetTargetServices();
  if (target) {
    if (SBOX_ALL_OK != target->Init()) {
      return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
    }
    if (BEFORE_REVERT == state) {
      return command.Run(args);
    } else if (EVERY_STATE == state) {
      command.Run(args);
    }
#if defined(ADDRESS_SANITIZER) || CHECK_WILL_STREAM()
    // Bind and leak dbghelp.dll before the token is lowered, otherwise some
    // child process will fail with the wrong error code when they fail to
    // symbolize a stack while crashing.
    if (!LoadLibraryA("dbghelp.dll")) {
      return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
    }
#endif

    target->LowerToken();
  } else if (!cmd_line->HasSwitch(kNoSandboxSwitch)) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  return command.Run(args);
}

}  // namespace sandbox

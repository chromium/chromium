// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mac/agent_process_broker.h"

#include <inttypes.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "remoting/host/mac/agent_process_broker_client.h"
#include "remoting/host/mojom/agent_process_broker.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace remoting {

namespace {

using testing::_;
using testing::Return;

static constexpr char kRemotingTestAgentProcessName[] =
    "RemotingTestAgentProcess";

static constexpr char kServerNameSwitch[] = "server-name";
static constexpr char kAgentStateFilePathSwitch[] = "state-file";

static constexpr char kAgentStateAwaiting[] = "awaiting";
static constexpr char kAgentStateResumed[] = "resumed";
static constexpr char kAgentStateSuspended[] = "suspended";

// A struct that holds both the real process object and the path of the agent
// state file.
struct Process {
  base::Process process;
  base::FilePath agent_state_file_path;
};

// A test AgentProcess implementation that simply writes state changes to
// `agent_state_file_path`. It will immediately write `kAgentStateAwaiting`
// when the object is constructed.
class TestAgentProcess : public mojom::AgentProcess {
 public:
  explicit TestAgentProcess(const base::FilePath& agent_state_file_path);
  ~TestAgentProcess() override;

  void ResumeProcess() override;
  void SuspendProcess() override;

 private:
  void WriteAgentState(std::string_view state);

  base::File agent_state_file_;
};

TestAgentProcess::TestAgentProcess(
    const base::FilePath& agent_state_file_path) {
  agent_state_file_ = base::File(
      agent_state_file_path, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  EXPECT_TRUE(agent_state_file_.IsValid());
  WriteAgentState(kAgentStateAwaiting);
}

TestAgentProcess::~TestAgentProcess() = default;

void TestAgentProcess::ResumeProcess() {
  WriteAgentState(kAgentStateResumed);
}

void TestAgentProcess::SuspendProcess() {
  WriteAgentState(kAgentStateSuspended);
}

void TestAgentProcess::WriteAgentState(std::string_view state) {
  agent_state_file_.SetLength(state.size());
  agent_state_file_.Write(0, base::as_byte_span(state));
  agent_state_file_.Flush();
}

}  // namespace

class AgentProcessBrokerTest : public testing::Test {
 public:
  AgentProcessBrokerTest();
  ~AgentProcessBrokerTest() override;

 protected:
  Process LaunchTestAgentProcess(bool is_root);
  // Returns nullopt if the process has exited.
  std::optional<std::string> GetTestAgentState(const Process& process);
  // Returns false if the process has exited.
  bool WaitForTestAgentState(const Process& process, std::string_view state);

  base::MockCallback<AgentProcessBroker::IsRootProcessGetter>
      is_root_process_getter_;
  std::unique_ptr<AgentProcessBroker> agent_process_broker_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  mojo::NamedPlatformChannel::ServerName server_name_;
  base::ScopedTempDir temp_dir_;
};

AgentProcessBrokerTest::AgentProcessBrokerTest() {
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  server_name_ = mojo::NamedPlatformChannel::ServerNameFromUTF8(
      base::StringPrintf("remoting_agent_process_broker_test_server.%" PRIu64,
                         base::RandUint64()));
  agent_process_broker_ = base::WrapUnique(new AgentProcessBroker(
      server_name_,
      base::BindRepeating(
          [](const named_mojo_ipc_server::ConnectionInfo&) { return true; }),
      is_root_process_getter_.Get()));
  agent_process_broker_->Start();
}

AgentProcessBrokerTest::~AgentProcessBrokerTest() = default;

Process AgentProcessBrokerTest::LaunchTestAgentProcess(bool is_root) {
  base::FilePath agent_state_file_path;
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(),
                                             &agent_state_file_path));
  EXPECT_CALL(is_root_process_getter_, Run(_)).WillOnce(Return(is_root));
  base::CommandLine cmd_line = base::GetMultiProcessTestChildBaseCommandLine();
  cmd_line.AppendSwitchNative(kServerNameSwitch, server_name_);
  cmd_line.AppendSwitchPath(kAgentStateFilePathSwitch, agent_state_file_path);
  base::RunLoop run_loop;
  agent_process_broker_->set_on_agent_process_launched_for_testing(
      run_loop.QuitClosure());
  base::Process process = base::SpawnMultiProcessTestChild(
      kRemotingTestAgentProcessName, cmd_line, /* options= */ {});
  run_loop.Run();
  return {
      .process = std::move(process),
      .agent_state_file_path = agent_state_file_path,
  };
}

std::optional<std::string> AgentProcessBrokerTest::GetTestAgentState(
    const Process& process) {
  if (process.process.WaitForExitWithTimeout(base::TimeDelta(), nullptr)) {
    // Process has exited.
    return std::nullopt;
  }
  base::File file(process.agent_state_file_path,
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  std::vector<char> buffer(
      std::max({sizeof(kAgentStateAwaiting), sizeof(kAgentStateResumed),
                sizeof(kAgentStateSuspended)}));
  std::optional<size_t> num_bytes_read =
      file.Read(0, base::as_writable_byte_span(buffer));
  if (!num_bytes_read.has_value()) {
    return std::nullopt;
  }
  return std::string(buffer.data(), *num_bytes_read);
}

bool AgentProcessBrokerTest::WaitForTestAgentState(const Process& process,
                                                   std::string_view state) {
  base::RunLoop run_loop;
  bool result;
  base::RepeatingClosure quit_loop_on_state = base::BindLambdaForTesting([&]() {
    std::optional<std::string> agent_state = GetTestAgentState(process);
    if (!agent_state.has_value() || *agent_state == state) {
      result = agent_state.has_value();
      run_loop.Quit();
      return;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, quit_loop_on_state, base::Milliseconds(10));
  });
  quit_loop_on_state.Run();
  run_loop.Run();
  return result;
}

TEST_F(AgentProcessBrokerTest, UserAgentProcessOnly_ResumedImmediately) {
  auto process = LaunchTestAgentProcess(/* is_root= */ false);
  ASSERT_TRUE(WaitForTestAgentState(process, kAgentStateResumed));
}

TEST_F(AgentProcessBrokerTest, RootAgentProcessOnly_ResumedImmediately) {
  auto process = LaunchTestAgentProcess(/* is_root= */ true);
  ASSERT_TRUE(WaitForTestAgentState(process, kAgentStateResumed));
}

TEST_F(AgentProcessBrokerTest,
       UserAgentProcessCloseAndRelaunch_ResumedImmediately) {
  auto p1 = LaunchTestAgentProcess(/* is_root= */ false);
  ASSERT_TRUE(WaitForTestAgentState(p1, kAgentStateResumed));
  ASSERT_TRUE(p1.process.Terminate(0, true));

  auto p2 = LaunchTestAgentProcess(/* is_root= */ false);
  ASSERT_TRUE(WaitForTestAgentState(p2, kAgentStateResumed));
}

TEST_F(AgentProcessBrokerTest,
       RootAgentProcessCloseAndRelaunch_ResumedImmediately) {
  auto p1 = LaunchTestAgentProcess(/* is_root= */ true);
  ASSERT_TRUE(WaitForTestAgentState(p1, kAgentStateResumed));
  ASSERT_TRUE(p1.process.Terminate(0, true));

  auto p2 = LaunchTestAgentProcess(/* is_root= */ true);
  ASSERT_TRUE(WaitForTestAgentState(p2, kAgentStateResumed));
}

TEST_F(AgentProcessBrokerTest,
       UserAgentProcessAfterUserAgentProcess_SecondProcessClosedImmediately) {
  auto p1 = LaunchTestAgentProcess(/* is_root= */ false);
  ASSERT_TRUE(WaitForTestAgentState(p1, kAgentStateResumed));

  auto p2 = LaunchTestAgentProcess(/* is_root= */ false);
  ASSERT_TRUE(p2.process.WaitForExit(nullptr));
}

TEST_F(AgentProcessBrokerTest,
       RootAgentProcessAfterRootAgentProcess_SecondProcessClosedImmediately) {
  auto p1 = LaunchTestAgentProcess(/* is_root= */ true);
  ASSERT_TRUE(WaitForTestAgentState(p1, kAgentStateResumed));

  auto p2 = LaunchTestAgentProcess(/* is_root= */ true);
  ASSERT_TRUE(p2.process.WaitForExit(nullptr));
}

TEST_F(AgentProcessBrokerTest,
       UserAgentProcessAfterRootAgentProcess_RootProcessSuspended) {
  auto root_process = LaunchTestAgentProcess(/* is_root= */ true);
  ASSERT_TRUE(WaitForTestAgentState(root_process, kAgentStateResumed));

  auto user_process = LaunchTestAgentProcess(/* is_root= */ false);
  ASSERT_TRUE(WaitForTestAgentState(user_process, kAgentStateResumed));
  ASSERT_TRUE(WaitForTestAgentState(root_process, kAgentStateSuspended));
}

TEST_F(
    AgentProcessBrokerTest,
    RootAgentProcessAfterUserAgentProcess_RootProcessResumedAfterUserProcessExited) {
  auto user_process = LaunchTestAgentProcess(/* is_root= */ false);
  ASSERT_TRUE(WaitForTestAgentState(user_process, kAgentStateResumed));

  auto root_process = LaunchTestAgentProcess(/* is_root= */ true);
  ASSERT_TRUE(WaitForTestAgentState(root_process, kAgentStateAwaiting));

  user_process.process.Terminate(0, true);
  ASSERT_TRUE(WaitForTestAgentState(root_process, kAgentStateResumed));
}

TEST_F(AgentProcessBrokerTest, DestroyServer_TerminatesClientProcesses) {
  auto user_process = LaunchTestAgentProcess(/* is_root= */ false);
  ASSERT_TRUE(WaitForTestAgentState(user_process, kAgentStateResumed));

  auto root_process = LaunchTestAgentProcess(/* is_root= */ true);
  ASSERT_TRUE(WaitForTestAgentState(root_process, kAgentStateAwaiting));

  agent_process_broker_.reset();

  ASSERT_TRUE(user_process.process.WaitForExit(nullptr));
  ASSERT_TRUE(root_process.process.WaitForExit(nullptr));
}

MULTIPROCESS_TEST_MAIN(RemotingTestAgentProcess) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::IO};
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  mojo::NamedPlatformChannel::ServerName server_name =
      cmd_line->GetSwitchValueNative(kServerNameSwitch);
  base::RunLoop run_loop;
  AgentProcessBrokerClient broker_client(run_loop.QuitClosure());
  EXPECT_TRUE(broker_client.ConnectToServer(server_name));
  base::FilePath state_file_path =
      cmd_line->GetSwitchValuePath(kAgentStateFilePathSwitch);
  TestAgentProcess test_process(state_file_path);
  broker_client.OnAgentProcessLaunched(&test_process);
  run_loop.Run();
  return 0;
}

}  // namespace remoting

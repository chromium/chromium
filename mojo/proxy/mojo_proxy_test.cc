// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/base_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/numerics/safe_math.h"
#include "base/process/launch.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/core/test/test_switches.h"
#include "mojo/proxy/mojo_proxy_test.test-mojom-test-utils.h"
#include "mojo/proxy/mojo_proxy_test.test-mojom.h"
#include "mojo/proxy/switches.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/wait.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo_proxy::test {
namespace {

const base::FilePath::CharType kProxyExecutableName[] =
    FILE_PATH_LITERAL("mojo_proxy");

// Returns a full FilePath for the `mojo_proxy` executable, assuming it lives
// in the same directory as the currently running process's executable.
base::FilePath GetProxyExecutablePath() {
  const base::FilePath test_path = base::MakeAbsoluteFilePath(
      base::CommandLine::ForCurrentProcess()->GetProgram());
  return test_path.DirName().Append(kProxyExecutableName);
}

// Returns a file's entire contents as a string.
std::string ReadWholeFile(base::File& file) {
  auto size = base::checked_cast<size_t>(file.GetLength());
  std::vector<char> contents(size);
  std::optional<size_t> num_bytes_read =
      file.Read(0, base::as_writable_byte_span(contents));
  CHECK_EQ(size, num_bytes_read.value());
  return std::string{contents.begin(), contents.end()};
}

// Creates a new read-only shared memory region populated with given contents.
base::ReadOnlySharedMemoryRegion CreateMemory(std::string_view contents) {
  auto region = base::WritableSharedMemoryRegion::Create(contents.size());
  auto mapping = region.Map();
  memcpy(mapping.memory(), contents.data(), contents.size());
  return base::WritableSharedMemoryRegion::ConvertToReadOnly(std::move(region));
}

// Returns the contents of a shared memory region as a string.
std::string ReadMemory(base::ReadOnlySharedMemoryRegion region) {
  auto mapping = region.Map();
  const char* data = static_cast<const char*>(mapping.memory());
  return std::string{data, region.GetSize()};
}

// Returns the next message on a message pipe as a string.
std::string ReadMessagePipe(mojo::MessagePipeHandle pipe) {
  mojo::Wait(pipe, MOJO_HANDLE_SIGNAL_READABLE);
  std::vector<uint8_t> payload;
  CHECK_EQ(MOJO_RESULT_OK, mojo::ReadMessageRaw(pipe, &payload, nullptr,
                                                MOJO_READ_MESSAGE_FLAG_NONE));
  return std::string{payload.begin(), payload.end()};
}

void WriteMessagePipe(mojo::MessagePipeHandle pipe, std::string_view message) {
  CHECK_EQ(MOJO_RESULT_OK,
           mojo::WriteMessageRaw(pipe, message.data(), message.size(), nullptr,
                                 0, MOJO_WRITE_MESSAGE_FLAG_NONE));
}

class TempFileFactory {
 public:
  TempFileFactory() { CHECK(temp_dir_.CreateUniqueTempDir()); }

  ~TempFileFactory() { CHECK(temp_dir_.Delete()); }

  base::File CreateFileWithContents(std::string_view name,
                                    std::string_view contents) const {
    uint32_t flags = base::File::FLAG_CREATE | base::File::FLAG_READ |
                     base::File::FLAG_WRITE;
    base::File new_file{temp_dir_.GetPath().AppendASCII(name), flags};
    new_file.Write(0, base::as_byte_span(contents));
    return new_file;
  }

 private:
  base::ScopedTempDir temp_dir_;
};

class TestServiceImpl : public mojom::TestService {
 public:
  explicit TestServiceImpl(mojo::PendingReceiver<mojom::TestService> receiver) {
    receivers_.Add(this, std::move(receiver));
    receivers_.set_disconnect_handler(base::BindRepeating(
        &TestServiceImpl::OnDisconnect, base::Unretained(this)));
  }

  ~TestServiceImpl() override = default;

  void set_multiplier(int multiplier) { multiplier_ = multiplier; }

  void set_dead_callback(base::OnceClosure callback) {
    dead_callback_ = std::move(callback);
  }

  // mojom::TestService:
  void Echo(int32_t x, EchoCallback callback) override {
    std::move(callback).Run(x);
  }

  void Scale(int32_t x, ScaleCallback callback) override {
    std::move(callback).Run(x * multiplier_);
  }

  void FlipFile(base::File file, FlipFileCallback callback) override {
    std::string contents = ReadWholeFile(file);
    base::ranges::reverse(contents);
    std::move(callback).Run(
        file_factory_.CreateFileWithContents("flipped", contents));
  }

  void FlipMemory(base::ReadOnlySharedMemoryRegion region,
                  FlipMemoryCallback callback) override {
    std::string contents = ReadMemory(std::move(region));
    base::ranges::reverse(contents);
    std::move(callback).Run(CreateMemory(contents));
  }

  void BindReceiver(
      mojo::PendingReceiver<mojom::TestService> receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

  void BindNewRemote(BindNewRemoteCallback callback) override {
    mojo::PendingRemote<mojom::TestService> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    std::move(callback).Run(std::move(remote));
  }

 private:
  void OnDisconnect() {
    if (receivers_.empty()) {
      std::move(dead_callback_).Run();
    }
  }

  const TempFileFactory file_factory_;
  int multiplier_ = 1;
  mojo::ReceiverSet<mojom::TestService> receivers_;
  base::OnceClosure dead_callback_;
};

class MojoProxyTest : public testing::Test {
 public:
  TempFileFactory& file_factory() { return file_factory_; }

 private:
  base::test::TaskEnvironment task_environment_;
  TempFileFactory file_factory_;
};

// Used to launch a test child process which will run a legacy Mojo app.
class LegacyAppLauncher {
 public:
  LegacyAppLauncher() {
    // Make sure we're always launching from a process with ipcz enabled.
    CHECK(mojo::core::IsMojoIpczEnabled());
  }

  ~LegacyAppLauncher() {
    initial_remotes_.clear();
    if (app_process_.IsValid()) {
      WaitForExit();
    }
  }

  mojom::TestService* AddInitialRemote() {
    CHECK(!app_process_.IsValid());
    CHECK(!exit_code_);
    CHECK(absl::holds_alternative<size_t>(attachment_info_));
    const uint64_t index = absl::get<size_t>(attachment_info_)++;
    auto remote = std::make_unique<mojo::Remote<mojom::TestService>>(
        mojo::PendingRemote<mojom::TestService>{
            invitation_.AttachMessagePipe(index), 0});
    return initial_remotes_.emplace_back(std::move(remote))->get();
  }

  mojom::TestService* CreateNamedRemote(std::string_view name) {
    CHECK(!app_process_.IsValid());
    CHECK(!exit_code_);
    CHECK(absl::holds_alternative<size_t>(attachment_info_));
    CHECK_EQ(absl::get<size_t>(attachment_info_), 0u);
    attachment_info_.emplace<std::string>(name);
    auto remote = std::make_unique<mojo::Remote<mojom::TestService>>(
        mojo::PendingRemote<mojom::TestService>{
            invitation_.AttachMessagePipe(name), 0});
    return initial_remotes_.emplace_back(std::move(remote))->get();
  }

  // Launches a process to run the test child with the given name. If
  // `test_child_name` is "Foo", this launches a child process to run the
  // entry point defined by MULTIPROCESS_TEST_MAIN(Foo).
  //
  // This also launches a separate mojo_proxy process and connects it between
  // this launcher and the launched legacy app process.
  void LaunchWithProxy(std::string_view test_child_name) {
    base::CommandLine app_command_line =
        base::GetMultiProcessTestChildBaseCommandLine();

    // Ensure there aren't stale switches interfering with ones we'll set.
    app_command_line.RemoveSwitch(switches::kTestChildProcess);
    app_command_line.RemoveSwitch(mojo::PlatformChannel::kHandleSwitch);

    base::LaunchOptions app_launch_options;
    mojo::PlatformChannel app_channel;
    auto remote_app_endpoint = app_channel.TakeRemoteEndpoint();
    remote_app_endpoint.PrepareToPass(app_launch_options, app_command_line);

    // Disable Mojo initialization: the generic initialization path for
    // mojo_unittests child processes will use default Mojo settings, but we
    // want to forcibly disable ipcz in this process. The child can initialize
    // Mojo instead by constructing a LegacyAppEnvironment (see below).
    app_command_line.AppendSwitch(test_switches::kNoMojo);
    app_process_ = base::SpawnMultiProcessTestChild(
        std::string{test_child_name}, app_command_line, app_launch_options);
    remote_app_endpoint.ProcessLaunchAttempted();

    // Now launch a mojo_proxy instance for this app instance.
    base::CommandLine proxy_command_line{GetProxyExecutablePath()};
    mojo::PlatformChannel proxy_channel;
    constexpr int kLegacyClientFdValue = STDERR_FILENO + 1;
    constexpr int kHostIpczTransportFdValue = kLegacyClientFdValue + 1;
    auto local_app_endpoint = app_channel.TakeLocalEndpoint();
    base::LaunchOptions proxy_launch_options;
    proxy_launch_options.fds_to_remap.emplace_back(
        local_app_endpoint.platform_handle().GetFD().get(),
        kLegacyClientFdValue);
    proxy_launch_options.fds_to_remap.emplace_back(
        proxy_channel.remote_endpoint().platform_handle().GetFD().get(),
        kHostIpczTransportFdValue);
    proxy_command_line.AppendSwitchASCII(
        switches::kLegacyClientFd, base::NumberToString(kLegacyClientFdValue));
    proxy_command_line.AppendSwitchASCII(
        switches::kHostIpczTransportFd,
        base::NumberToString(kHostIpczTransportFdValue));
    absl::visit(base::Overloaded{[&](size_t num_attachments) {
                                   proxy_command_line.AppendSwitchASCII(
                                       switches::kNumAttachments,
                                       base::NumberToString(num_attachments));
                                 },
                                 [&](const std::string& name) {
                                   proxy_command_line.AppendSwitchASCII(
                                       switches::kAttachmentName, name);
                                 }},
                attachment_info_);
    if (!mojo::core::GetIpczNodeOptions().is_broker) {
      // When connecting though the proxy from a non-broker subprocess, we need
      // to inform the proxy that it must inherit our broker.
      proxy_command_line.AppendSwitch(switches::kInheritIpczBroker);
      invitation_.set_extra_flags(MOJO_SEND_INVITATION_FLAG_SHARE_BROKER);
    }
    proxy_process_ =
        base::LaunchProcess(proxy_command_line, proxy_launch_options);
    local_app_endpoint.ProcessLaunchAttempted();
    proxy_channel.TakeRemoteEndpoint().ProcessLaunchAttempted();

    // Finally, send our invitation to the proxy rather than sending directly to
    // the legacy app.
    mojo::OutgoingInvitation::Send(std::move(invitation_),
                                   proxy_process_.Handle(),
                                   proxy_channel.TakeLocalEndpoint());
  }

  int WaitForExit() {
    CHECK(app_process_.IsValid());
    if (!exit_code_) {
      int rv = -1;
      CHECK(base::WaitForMultiprocessTestChildExit(
          app_process_, TestTimeouts::action_timeout(), &rv));
      exit_code_ = rv;

      CHECK(proxy_process_.IsValid());
      CHECK(proxy_process_.WaitForExitWithTimeout(
          TestTimeouts::action_timeout(), &rv));
      CHECK_EQ(rv, 0);
    }
    return *exit_code_;
  }

 private:
  mojo::OutgoingInvitation invitation_;
  absl::variant<size_t, std::string> attachment_info_{0};
  base::Process app_process_;
  base::Process proxy_process_;
  std::optional<int> exit_code_;
  std::vector<std::unique_ptr<mojo::Remote<mojom::TestService>>>
      initial_remotes_;
};

// Sets up a legacy Mojo Core runtime environment on construction. Used in test
// test child processes which act as a mojo_proxy target.
class LegacyAppEnvironment {
 public:
  LegacyAppEnvironment() {
    io_thread_.StartWithOptions(
        base::Thread::Options{base::MessagePumpType::IO, 0});
    mojo::core::Init({
        .is_broker_process = false,
        .disable_ipcz = true,
    });
    ipc_support_.emplace(io_thread_.task_runner(),
                         mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

    auto endpoint = mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
        *base::CommandLine::ForCurrentProcess());
    invitation_ = mojo::IncomingInvitation::Accept(std::move(endpoint));
  }

  ~LegacyAppEnvironment() {
    invitation_.reset();
    ipc_support_.reset();
    io_thread_.Stop();
    mojo::core::ShutDown();
  }

  mojo::PendingReceiver<mojom::TestService> GetInitialReceiver(uint64_t n) {
    return mojo::PendingReceiver<mojom::TestService>(
        invitation_->ExtractMessagePipe(n));
  }

  mojo::PendingReceiver<mojom::TestService> GetInitialReceiver(
      std::string_view name) {
    return mojo::PendingReceiver<mojom::TestService>(
        invitation_->ExtractMessagePipe(name));
  }

  TestServiceImpl& AddServiceImpl(
      mojo::PendingReceiver<mojom::TestService> receiver) {
    return *service_impls_.emplace_back(
        std::make_unique<TestServiceImpl>(std::move(receiver)));
  }

  int Run() {
    base::RunLoop loop;
    auto callback =
        base::BarrierClosure(service_impls_.size(), loop.QuitClosure());
    for (auto& impl : service_impls_) {
      impl->set_dead_callback(callback);
    }
    loop.Run();
    return 0;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::Thread io_thread_{"IO thread"};
  std::optional<mojo::core::ScopedIPCSupport> ipc_support_;
  std::vector<mojo::ScopedMessagePipeHandle> initial_pipes_;
  std::optional<mojo::IncomingInvitation> invitation_;
  std::vector<std::unique_ptr<TestServiceImpl>> service_impls_;
};

TEST_F(MojoProxyTest, SingleAttachment) {
  LegacyAppLauncher launcher;
  mojom::TestServiceAsyncWaiter service{launcher.AddInitialRemote()};
  launcher.LaunchWithProxy("SingleServiceInstanceApp");
  EXPECT_EQ(1, service.Echo(1));
}

MULTIPROCESS_TEST_MAIN(SingleServiceInstanceApp) {
  LegacyAppEnvironment env;
  env.AddServiceImpl(env.GetInitialReceiver(0));
  return env.Run();
}

TEST_F(MojoProxyTest, MultipleAttachments) {
  LegacyAppLauncher launcher;
  mojom::TestServiceAsyncWaiter service1{launcher.AddInitialRemote()};
  mojom::TestServiceAsyncWaiter service2{launcher.AddInitialRemote()};
  mojom::TestServiceAsyncWaiter service3{launcher.AddInitialRemote()};
  launcher.LaunchWithProxy("MultipleServiceInstanceApp");
  EXPECT_EQ(10, service1.Scale(2));
  EXPECT_EQ(21, service2.Scale(3));
  EXPECT_EQ(55, service3.Scale(5));
}

MULTIPROCESS_TEST_MAIN(MultipleServiceInstanceApp) {
  LegacyAppEnvironment env;
  TestServiceImpl& impl1 = env.AddServiceImpl(env.GetInitialReceiver(0));
  TestServiceImpl& impl2 = env.AddServiceImpl(env.GetInitialReceiver(1));
  TestServiceImpl& impl3 = env.AddServiceImpl(env.GetInitialReceiver(2));

  // We use unique multipliers for each so that the test can easily verify that
  // each indexed initial receiver is connected to the correct endpoint in the
  // test app.
  impl1.set_multiplier(5);
  impl2.set_multiplier(7);
  impl3.set_multiplier(11);
  return env.Run();
}

constexpr std::string_view kPipeName = "baguette";

TEST_F(MojoProxyTest, SingleNamedAttachment) {
  LegacyAppLauncher launcher;
  mojom::TestServiceAsyncWaiter service{launcher.CreateNamedRemote(kPipeName)};
  launcher.LaunchWithProxy("SingleInstanceWithNamedAttachmentApp");
  EXPECT_EQ(1, service.Echo(1));
}

MULTIPROCESS_TEST_MAIN(SingleInstanceWithNamedAttachmentApp) {
  LegacyAppEnvironment env;
  env.AddServiceImpl(env.GetInitialReceiver(kPipeName));
  return env.Run();
}

TEST_F(MojoProxyTest, PassPipeToTarget) {
  LegacyAppLauncher launcher;
  mojom::TestService* remote1 = launcher.AddInitialRemote();
  launcher.LaunchWithProxy("SingleServiceInstanceApp");

  mojo::Remote<mojom::TestService> remote2;
  remote1->BindReceiver(remote2.BindNewPipeAndPassReceiver());

  mojom::TestServiceAsyncWaiter service2{remote2.get()};
  EXPECT_EQ(5, service2.Echo(5));
}

TEST_F(MojoProxyTest, PassPipeToHost) {
  LegacyAppLauncher launcher;
  mojom::TestServiceAsyncWaiter service1{launcher.AddInitialRemote()};
  launcher.LaunchWithProxy("SingleServiceInstanceApp");
  EXPECT_EQ(3, service1.Echo(3));

  mojo::Remote<mojom::TestService> remote2{service1.BindNewRemote()};
  mojom::TestServiceAsyncWaiter service2{remote2.get()};
  EXPECT_EQ(5, service2.Echo(5));
}

TEST_F(MojoProxyTest, PassPlatformHandle) {
  LegacyAppLauncher launcher;
  mojom::TestServiceAsyncWaiter service{launcher.AddInitialRemote()};
  launcher.LaunchWithProxy("SingleServiceInstanceApp");

  base::File flipped = service.FlipFile(
      file_factory().CreateFileWithContents("passwords.txt", "hello, world!"));
  EXPECT_EQ("!dlrow ,olleh", ReadWholeFile(flipped));
}

TEST_F(MojoProxyTest, PassSharedBuffer) {
  LegacyAppLauncher launcher;
  mojom::TestServiceAsyncWaiter service{launcher.AddInitialRemote()};
  launcher.LaunchWithProxy("SingleServiceInstanceApp");

  base::ReadOnlySharedMemoryRegion region = CreateMemory("hello, world!");
  EXPECT_EQ("!dlrow ,olleh", ReadMemory(service.FlipMemory(std::move(region))));
}

TEST_F(MojoProxyTest, ConnectFromNonBroker) {
  // Launch a subprocess to serve as the (non-broker) host.
  base::CommandLine host_command_line =
      base::GetMultiProcessTestChildBaseCommandLine();
  base::LaunchOptions host_launch_options;
  mojo::PlatformChannel host_channel;
  auto host_endpoint = host_channel.TakeRemoteEndpoint();
  host_endpoint.PrepareToPass(host_launch_options, host_command_line);
  mojo::OutgoingInvitation invitation;
  mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe(0);
  base::Process process = base::SpawnMultiProcessTestChild(
      "NonBrokerHostApp", host_command_line, host_launch_options);
  host_endpoint.ProcessLaunchAttempted();
  mojo::OutgoingInvitation::Send(std::move(invitation), process.Handle(),
                                 host_channel.TakeLocalEndpoint());

  EXPECT_EQ("done", ReadMessagePipe(pipe.get()));
  WriteMessagePipe(pipe.get(), "bye");
  int rv = -1;
  process.WaitForExitWithTimeout(TestTimeouts::action_timeout(), &rv);
  EXPECT_EQ(0, rv);
}

MULTIPROCESS_TEST_MAIN(NonBrokerHostApp) {
  // Accept a connection from the test process, which is also our broker.
  auto endpoint = mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
      *base::CommandLine::ForCurrentProcess());
  auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));
  mojo::ScopedMessagePipeHandle pipe = invitation.ExtractMessagePipe(0);

  base::test::TaskEnvironment task_environment;
  LegacyAppLauncher launcher;
  mojom::TestServiceAsyncWaiter service{launcher.AddInitialRemote()};
  launcher.LaunchWithProxy("SingleServiceInstanceApp");
  EXPECT_EQ(42, service.Echo(42));

  WriteMessagePipe(pipe.get(), "done");
  EXPECT_EQ("bye", ReadMessagePipe(pipe.get()));
  return 0;
}

}  // namespace
}  // namespace mojo_proxy::test

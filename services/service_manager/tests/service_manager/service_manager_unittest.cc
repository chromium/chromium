// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/token.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_receiver.h"
#include "services/service_manager/public/cpp/test/test_service_manager.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/service_manager/service_process_launcher.h"
#include "services/service_manager/tests/service_manager/service_manager.test-mojom.h"
#include "services/service_manager/tests/service_manager/test_manifests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace service_manager {

namespace {

void OnServiceStartedCallback(int* start_count,
                              std::string* service_name,
                              base::OnceClosure continuation,
                              const service_manager::Identity& identity) {
  (*start_count)++;
  *service_name = identity.name();
  std::move(continuation).Run();
}

void OnServiceFailedToStartCallback(bool* run,
                                    base::OnceClosure continuation,
                                    const service_manager::Identity& identity) {
  *run = true;
  std::move(continuation).Run();
}

void OnServicePIDReceivedCallback(std::string* service_name,
                                  uint32_t* service_pid,
                                  base::OnceClosure continuation,
                                  const service_manager::Identity& identity,
                                  uint32_t pid) {
  *service_name = identity.name();
  *service_pid = pid;
  std::move(continuation).Run();
}

class TestService : public Service, public test::mojom::CreateInstanceTest {
 public:
  explicit TestService(mojo::PendingReceiver<mojom::Service> receiver)
      : service_receiver_(this, std::move(receiver)) {
    registry_.AddInterface<test::mojom::CreateInstanceTest>(
        base::BindRepeating(&TestService::Create, base::Unretained(this)));
  }

  TestService(const TestService&) = delete;
  TestService& operator=(const TestService&) = delete;

  ~TestService() override = default;

  const Identity& target_identity() const { return target_identity_; }

  void WaitForTargetIdentityCall() {
    wait_for_target_identity_loop_ = std::make_unique<base::RunLoop>();
    wait_for_target_identity_loop_->Run();
  }

  Connector* connector() { return service_receiver_.GetConnector(); }

 private:
  // Service:
  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  void Create(mojo::PendingReceiver<test::mojom::CreateInstanceTest> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  // test::mojom::CreateInstanceTest:
  void SetTargetIdentity(const service_manager::Identity& identity) override {
    target_identity_ = identity;
    if (!wait_for_target_identity_loop_)
      LOG(ERROR) << "SetTargetIdentity call received when not waiting for it.";
    else
      wait_for_target_identity_loop_->Quit();
  }

  ServiceReceiver service_receiver_;
  Identity target_identity_;
  std::unique_ptr<base::RunLoop> wait_for_target_identity_loop_;

  BinderRegistry registry_;
  mojo::Receiver<test::mojom::CreateInstanceTest> receiver_{this};
};

class SimpleService : public Service {
 public:
  explicit SimpleService(mojo::PendingReceiver<mojom::Service> receiver)
      : receiver_(this, std::move(receiver)) {}

  SimpleService(const SimpleService&) = delete;
  SimpleService& operator=(const SimpleService&) = delete;

  ~SimpleService() override = default;

  Connector* connector() { return receiver_.GetConnector(); }

  void WaitForDisconnect() {
    base::RunLoop loop;
    connection_lost_closure_ = loop.QuitClosure();
    loop.Run();
  }

 private:
  // Service:
  void OnDisconnected() override {
    if (connection_lost_closure_)
      std::move(connection_lost_closure_).Run();
    Terminate();
  }

  ServiceReceiver receiver_;
  base::OnceClosure connection_lost_closure_;
};

}  // namespace

class ServiceManagerTest : public testing::Test,
                           public mojom::ServiceManagerListener {
 public:
  ServiceManagerTest()
      : test_service_manager_(GetTestManifests()),
        test_service_(
            test_service_manager_.RegisterTestInstance(kTestServiceName)) {}

  ServiceManagerTest(const ServiceManagerTest&) = delete;
  ServiceManagerTest& operator=(const ServiceManagerTest&) = delete;

  ~ServiceManagerTest() override = default;

 protected:
  struct InstanceInfo {
    explicit InstanceInfo(const Identity& identity)
        : identity(identity), pid(base::kNullProcessId) {}

    Identity identity;
    base::ProcessId pid;
  };

  Connector* connector() { return test_service_.connector(); }

  void AddListenerAndWaitForApplications() {
    mojo::Remote<mojom::ServiceManager> service_manager;
    connector()->Connect(service_manager::mojom::kServiceName,
                         service_manager.BindNewPipeAndPassReceiver());

    mojo::PendingRemote<mojom::ServiceManagerListener> listener;
    receiver_.Bind(listener.InitWithNewPipeAndPassReceiver());
    service_manager->AddListener(std::move(listener));

    wait_for_instances_loop_ = std::make_unique<base::RunLoop>();
    wait_for_instances_loop_->Run();
  }

  bool ContainsInstanceWithName(const std::string& name) const {
    for (const auto& instance : initial_instances_) {
      if (instance.identity.name() == name)
        return true;
    }
    for (const auto& instance : instances_) {
      if (instance.identity.name() == name)
        return true;
    }
    return false;
  }

  void WaitForTargetIdentityCall() {
    test_service_.WaitForTargetIdentityCall();
  }

  const Identity& target_identity() const {
    return test_service_.target_identity();
  }

  const std::vector<InstanceInfo>& instances() const { return instances_; }

  using ServiceStartedCallback =
      base::RepeatingCallback<void(const service_manager::Identity&)>;
  void set_service_started_callback(const ServiceStartedCallback& callback) {
    service_started_callback_ = callback;
  }

  using ServiceFailedToStartCallback =
      base::RepeatingCallback<void(const service_manager::Identity&)>;
  void set_service_failed_to_start_callback(
      const ServiceFailedToStartCallback& callback) {
    service_failed_to_start_callback_ = callback;
  }

  using ServicePIDReceivedCallback =
      base::RepeatingCallback<void(const service_manager::Identity&,
                                   uint32_t pid)>;
  void set_service_pid_received_callback(
      const ServicePIDReceivedCallback& callback) {
    service_pid_received_callback_ = callback;
  }

  void WaitForInstanceToStart(const Identity& identity) {
    base::RunLoop loop;
    set_service_started_callback(base::BindRepeating(
        [](base::RunLoop* loop, const Identity* expected_identity,
           const Identity& identity) {
          EXPECT_EQ(expected_identity->name(), identity.name());
          EXPECT_EQ(expected_identity->instance_group(),
                    identity.instance_group());
          EXPECT_EQ(expected_identity->instance_id(), identity.instance_id());
          loop->Quit();
        },
        &loop, &identity));
    loop.Run();
    set_service_started_callback(ServiceStartedCallback());
  }

  void StartTarget() {
    // The test executable is a data_deps and thus generated test data.
    base::FilePath target_path;
    CHECK(base::PathService::Get(base::DIR_OUT_TEST_DATA_ROOT, &target_path));

    target_path = target_path.AppendASCII(kTestTargetName);
#if BUILDFLAG(IS_WIN)
    target_path = target_path.AddExtensionASCII("exe");
#endif

    base::CommandLine child_command_line(target_path);
    // Forward the wait-for-debugger flag but nothing else - we don't want to
    // stamp on the platform-channel flag.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kWaitForDebugger)) {
      child_command_line.AppendSwitch(switches::kWaitForDebugger);
    }

    // Create the channel to be shared with the target process. Pass one end
    // on the command line.
    mojo::PlatformChannel channel;
    base::LaunchOptions options;
    channel.PrepareToPassRemoteEndpoint(&options, &child_command_line);

    mojo::OutgoingInvitation invitation;
    auto client = ServiceProcessLauncher::PassServiceRequestOnCommandLine(
        &invitation, &child_command_line);
    mojo::Remote<service_manager::mojom::ProcessMetadata> metadata;
    connector()->RegisterServiceInstance(
        service_manager::Identity(kTestTargetName, kSystemInstanceGroup,
                                  base::Token{}, base::Token::CreateRandom()),
        std::move(client), metadata.BindNewPipeAndPassReceiver());

    target_ = base::LaunchProcess(child_command_line, options);
    DCHECK(target_.IsValid());
    channel.RemoteProcessLaunchAttempted();
    metadata->SetPID(target_.Pid());
    mojo::OutgoingInvitation::Send(std::move(invitation), target_.Handle(),
                                   channel.TakeLocalEndpoint());
  }

  void StartEmbedderService() {
    base::RunLoop loop;
    int start_count = 0;
    std::string service_name;
    set_service_started_callback(
        base::BindRepeating(&OnServiceStartedCallback, &start_count,
                            &service_name, loop.QuitClosure()));
    bool failed_to_start = false;
    set_service_failed_to_start_callback(base::BindRepeating(
        &OnServiceFailedToStartCallback, &failed_to_start, loop.QuitClosure()));

    connector()->WarmService(
        service_manager::ServiceFilter::ByName(kTestEmbedderName));
    loop.Run();
    EXPECT_FALSE(failed_to_start);
    EXPECT_EQ(1, start_count);
    EXPECT_EQ(kTestEmbedderName, service_name);
  }

  void StartService(const ServiceFilter& filter, bool expect_service_started) {
    int start_count = 0;
    base::RunLoop loop;
    std::string service_name;
    set_service_started_callback(
        base::BindRepeating(&OnServiceStartedCallback, &start_count,
                            &service_name, loop.QuitClosure()));
    bool failed_to_start = false;
    set_service_failed_to_start_callback(base::BindRepeating(
        &OnServiceFailedToStartCallback, &failed_to_start, loop.QuitClosure()));

    connector()->WarmService(filter);
    if (!expect_service_started) {
      // Wait briefly and test no new service was created.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, loop.QuitClosure(), base::Seconds(1));
    }

    loop.Run();
    EXPECT_FALSE(failed_to_start);
    if (expect_service_started) {
      EXPECT_EQ(1, start_count);
      EXPECT_EQ(filter.service_name(), service_name);
    } else {
      // The callback was not invoked, nothing should have been set.
      EXPECT_EQ(0, start_count);
      EXPECT_TRUE(service_name.empty());
    }
  }

  void KillTarget() {
    target_.Terminate(0, false);
  }

 private:
  // Service:

  // mojom::ServiceManagerListener:
  void OnInit(std::vector<mojom::RunningServiceInfoPtr> instances) override {
    for (size_t i = 0; i < instances.size(); ++i)
      initial_instances_.push_back(InstanceInfo(instances[i]->identity));

    DCHECK(wait_for_instances_loop_);
    wait_for_instances_loop_->Quit();
  }
  void OnServiceCreated(mojom::RunningServiceInfoPtr instance) override {
    instances_.push_back(InstanceInfo(instance->identity));
  }
  void OnServiceStarted(const service_manager::Identity& identity,
                        uint32_t pid) override {
    for (auto& instance : instances_) {
      if (instance.identity == identity) {
        instance.pid = pid;
        break;
      }
    }
    if (!service_started_callback_.is_null())
        service_started_callback_.Run(identity);
  }
  void OnServiceFailedToStart(
      const service_manager::Identity& identity) override {
    if (!service_failed_to_start_callback_.is_null())
        service_failed_to_start_callback_.Run(identity);
  }
  void OnServiceStopped(const service_manager::Identity& identity) override {
    for (auto it = instances_.begin(); it != instances_.end(); ++it) {
      auto& instance = *it;
      if (instance.identity == identity) {
        instances_.erase(it);
        break;
      }
    }
  }
  void OnServicePIDReceived(const service_manager::Identity& identity,
                            uint32_t pid) override {
    if (!service_pid_received_callback_.is_null())
      service_pid_received_callback_.Run(identity, pid);
  }

  base::test::TaskEnvironment task_environment_;
  TestServiceManager test_service_manager_;
  TestService test_service_;

  mojo::Receiver<mojom::ServiceManagerListener> receiver_{this};
  std::vector<InstanceInfo> instances_;
  std::vector<InstanceInfo> initial_instances_;
  std::unique_ptr<base::RunLoop> wait_for_instances_loop_;
  ServiceStartedCallback service_started_callback_;
  ServiceFailedToStartCallback service_failed_to_start_callback_;
  ServicePIDReceivedCallback service_pid_received_callback_;
  base::Process target_;
};

TEST_F(ServiceManagerTest, CreateInstance) {
  AddListenerAndWaitForApplications();

  // 1. Launch a process.
  StartTarget();

  // 2. Wait for the target to connect to us. (via
  //    service:service_manager_unittest)
  WaitForTargetIdentityCall();

  // 3. Validate that this test suite's name was received from the application
  //    manager.
  EXPECT_TRUE(ContainsInstanceWithName(kTestServiceName));

  // 4. Validate that the right applications/processes were created.
  //    Note that the target process will be created even if the tests are
  //    run with --single-process.
  EXPECT_EQ(1u, instances().size());
  {
    auto& instance = instances().back();
    // We learn about the target process id via a ping from it.
    EXPECT_EQ(target_identity(), instance.identity);
    EXPECT_EQ(kTestTargetName, instance.identity.name());
    EXPECT_NE(base::kNullProcessId, instance.pid);
  }

  KillTarget();
}

// Tests that starting a regular packaged service works, and that when starting
// the service again, a new service is created unless the same user ID and
// instance names are used.
TEST_F(ServiceManagerTest, CreatePackagedRegularInstances) {
  AddListenerAndWaitForApplications();

  // Connect to the embedder service first.
  StartEmbedderService();

  auto filter = ServiceFilter::ByName(kTestRegularServiceName);
  StartService(filter, /*expect_service_started=*/true);

  // Retstarting with the same identity reuses the existing service.
  StartService(filter, /*expect_service_started=*/false);

  // Starting with a different instance group creates a new service.
  auto other_group_filter = ServiceFilter::ByNameInGroup(
      kTestRegularServiceName, base::Token::CreateRandom());
  StartService(other_group_filter, /*expect_service_started=*/true);

  // Starting with a different instance ID creates a new service as well.
  auto other_id_filter =
      ServiceFilter::ByNameWithId(kTestRegularServiceName, base::Token{1, 2});
  StartService(other_id_filter, /*expect_service_started=*/true);
}

// Tests that starting a shared instance packaged service works, and that when
// starting that service again, a new service is created only when a different
// instance name is specified.
TEST_F(ServiceManagerTest, CreatePackagedSharedAcrossGroupsInstances) {
  AddListenerAndWaitForApplications();

  // Connect to the embedder service first.
  StartEmbedderService();

  auto filter = ServiceFilter::ByName(kTestSharedServiceName);
  StartService(filter, /*expect_service_started=*/true);

  // Start again with a different instance group. The existing service should be
  // reused.
  auto other_group_filter = ServiceFilter::ByNameInGroup(
      kTestSharedServiceName, base::Token::CreateRandom());
  StartService(other_group_filter, /*expect_service_started=*/false);

  // Start again with a difference instance ID. In that case a new service
  // should get created.
  auto other_id_filter = ServiceFilter::ByNameWithIdInGroup(
      kTestSharedServiceName, base::Token{1, 2}, base::Token::CreateRandom());
  StartService(other_id_filter, /*expect_service_started=*/true);
}

// Tests that creating a singleton packaged service works, and that when
// starting that service again a new service is never created.
TEST_F(ServiceManagerTest, CreatePackagedSingletonInstances) {
  AddListenerAndWaitForApplications();

  // Connect to the embedder service first.
  StartEmbedderService();

  auto filter = ServiceFilter::ByName(kTestSingletonServiceName);
  StartService(filter, /*expect_service_started=*/true);

  // Start again with a different instance group. The existing service should be
  // reused.
  auto other_group_filter = ServiceFilter::ByNameInGroup(
      kTestSingletonServiceName, base::Token::CreateRandom());
  StartService(other_group_filter, /*expect_service_started=*/false);

  // Start again with the same instance group but a difference instance ID. The
  // existing service should still be reused.
  auto other_id_filter =
      ServiceFilter::ByNameWithId(kTestSingletonServiceName, base::Token{3, 4});
  StartService(other_id_filter, /*expect_service_started=*/false);
}

TEST_F(ServiceManagerTest, PIDReceivedCallback) {
  AddListenerAndWaitForApplications();

  {
    base::RunLoop loop;
    std::string service_name;
    uint32_t pid = 0u;
    set_service_pid_received_callback(
        base::BindRepeating(&OnServicePIDReceivedCallback, &service_name, &pid,
                            loop.QuitClosure()));
    bool failed_to_start = false;
    set_service_failed_to_start_callback(base::BindRepeating(
        &OnServiceFailedToStartCallback, &failed_to_start, loop.QuitClosure()));

    connector()->WarmService(ServiceFilter::ByName(kTestEmbedderName));
    loop.Run();
    EXPECT_FALSE(failed_to_start);
    EXPECT_EQ(kTestEmbedderName, service_name);
    EXPECT_NE(pid, 0u);
  }
}

TEST_F(ServiceManagerTest, ClientProcessCapabilityEnforced) {
  AddListenerAndWaitForApplications();

  const std::string kTestService = kTestTargetName;
  const Identity kInstance1Id(kTestService, kSystemInstanceGroup,
                              base::Token{1, 2}, base::Token::CreateRandom());
  const Identity kInstance2Id(kTestService, kSystemInstanceGroup,
                              base::Token{3, 4}, base::Token::CreateRandom());

  // Introduce a new service instance for service_manager_unittest_target,
  // which should be allowed because the test service has
  // |can_create_other_service_instances| set to |true| in its manifest.
  mojo::PendingRemote<mojom::Service> test_service_remote1;
  SimpleService test_service1(
      test_service_remote1.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::ProcessMetadata> metadata1;
  connector()->RegisterServiceInstance(kInstance1Id,
                                       std::move(test_service_remote1),
                                       metadata1.BindNewPipeAndPassReceiver());
  metadata1->SetPID(42);
  WaitForInstanceToStart(kInstance1Id);
  EXPECT_EQ(1u, instances().size());
  EXPECT_TRUE(ContainsInstanceWithName(kTestTargetName));

  // Now use the new instance (which does not have client_process capability)
  // to attempt introduction of yet another instance. This should fail.
  mojo::PendingRemote<mojom::Service> test_service_remote2;
  SimpleService test_service2(
      test_service_remote2.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::ProcessMetadata> metadata2;
  test_service1.connector()->RegisterServiceInstance(
      kInstance2Id, std::move(test_service_remote2),
      metadata2.BindNewPipeAndPassReceiver());
  metadata2->SetPID(43);

  // The new service should be disconnected immediately.
  test_service2.WaitForDisconnect();

  // And still only one service instance around.
  EXPECT_EQ(1u, instances().size());
}

TEST_F(ServiceManagerTest, ClonesDisconnectedConnectors) {
  Connector connector((mojo::PendingRemote<mojom::Connector>()));
  EXPECT_TRUE(connector.Clone());
}

}  // namespace service_manager

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop_current.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/service_manager/public/mojom/constants.mojom.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/service_manager/tests/service_manager/service_manager_unittest.mojom.h"

namespace service_manager {

namespace {

void OnServiceStartedCallback(int* start_count,
                              std::string* service_name,
                              const base::Closure& continuation,
                              const service_manager::Identity& identity) {
  (*start_count)++;
  *service_name = identity.name();
  continuation.Run();
}

void OnServiceFailedToStartCallback(bool* run,
                                    const base::Closure& continuation,
                                    const service_manager::Identity& identity) {
  *run = true;
  continuation.Run();
}

void OnServicePIDReceivedCallback(std::string* service_name,
                                  uint32_t* serivce_pid,
                                  const base::Closure& continuation,
                                  const service_manager::Identity& identity,
                                  uint32_t pid) {
  *service_name = identity.name();
  *serivce_pid = pid;
  continuation.Run();
}

class ServiceManagerTestClient : public test::ServiceTestClient,
                                 public test::mojom::CreateInstanceTest {
 public:
  explicit ServiceManagerTestClient(test::ServiceTest* test)
      : test::ServiceTestClient(test), binding_(this) {
    registry_.AddInterface<test::mojom::CreateInstanceTest>(
        base::Bind(&ServiceManagerTestClient::Create, base::Unretained(this)));
  }
  ~ServiceManagerTestClient() override {}

  const Identity& target_identity() const { return target_identity_; }

  void WaitForTargetIdentityCall() {
    wait_for_target_identity_loop_ = std::make_unique<base::RunLoop>();
    wait_for_target_identity_loop_->Run();
  }

 private:
  // test::ServiceTestClient:
  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  void Create(test::mojom::CreateInstanceTestRequest request) {
    binding_.Bind(std::move(request));
  }

  // test::mojom::CreateInstanceTest:
  void SetTargetIdentity(const service_manager::Identity& identity) override {
    target_identity_ = identity;
    if (!wait_for_target_identity_loop_)
      LOG(ERROR) << "SetTargetIdentity call received when not waiting for it.";
    else
      wait_for_target_identity_loop_->Quit();
  }

  service_manager::Identity target_identity_;
  std::unique_ptr<base::RunLoop> wait_for_target_identity_loop_;

  BinderRegistry registry_;
  mojo::Binding<test::mojom::CreateInstanceTest> binding_;

  DISALLOW_COPY_AND_ASSIGN(ServiceManagerTestClient);
};

class SimpleService {
 public:
  explicit SimpleService(mojom::ServiceRequest request)
      : context_(std::make_unique<ServiceImpl>(this), std::move(request)) {}
  ~SimpleService() {}

  Connector* connector() { return context_.connector(); }

  void WaitForDisconnect() {
    base::RunLoop loop;
    connection_lost_closure_ = loop.QuitClosure();
    loop.Run();
  }

 private:
  class ServiceImpl : public Service {
   public:
    explicit ServiceImpl(SimpleService* service) : service_(service) {}
    ~ServiceImpl() override {}

    bool OnServiceManagerConnectionLost() override {
      if (service_->connection_lost_closure_)
        std::move(service_->connection_lost_closure_).Run();
      return true;
    }

   private:
    SimpleService* service_;

    DISALLOW_COPY_AND_ASSIGN(ServiceImpl);
  };

  ServiceContext context_;
  base::OnceClosure connection_lost_closure_;

  DISALLOW_COPY_AND_ASSIGN(SimpleService);
};

}  // namespace

class ServiceManagerTest : public test::ServiceTest,
                           public mojom::ServiceManagerListener {
 public:
  ServiceManagerTest()
      : test::ServiceTest("service_manager_unittest"),
        service_(nullptr),
        binding_(this) {}
  ~ServiceManagerTest() override {}

 protected:
  struct InstanceInfo {
    explicit InstanceInfo(const Identity& identity)
        : identity(identity), pid(base::kNullProcessId) {}

    Identity identity;
    base::ProcessId pid;
  };

  void AddListenerAndWaitForApplications() {
    mojom::ServiceManagerPtr service_manager;
    connector()->BindInterface(service_manager::mojom::kServiceName,
                               &service_manager);

    mojom::ServiceManagerListenerPtr listener;
    binding_.Bind(mojo::MakeRequest(&listener));
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
    service_->WaitForTargetIdentityCall();
  }

  const Identity& target_identity() const {
    DCHECK(service_);
    return service_->target_identity();
  }

  const std::vector<InstanceInfo>& instances() const { return instances_; }

  using ServiceStartedCallback =
      base::Callback<void(const service_manager::Identity&)>;
  void set_service_started_callback(const ServiceStartedCallback& callback) {
    service_started_callback_ = callback;
  }

  using ServiceFailedToStartCallback =
      base::Callback<void(const service_manager::Identity&)>;
  void set_service_failed_to_start_callback(
      const ServiceFailedToStartCallback& callback) {
    service_failed_to_start_callback_ = callback;
  }

  using ServicePIDReceivedCallback =
      base::Callback<void(const service_manager::Identity&, uint32_t pid)>;
  void set_service_pid_received_callback(
      const ServicePIDReceivedCallback& callback) {
    service_pid_received_callback_ = callback;
  }

  void WaitForInstanceToStart(const Identity& identity) {
    base::RunLoop loop;
    set_service_started_callback(base::Bind(
        [](base::RunLoop* loop, const Identity* expected_identity,
           const Identity& identity) {
          EXPECT_EQ(expected_identity->name(), identity.name());
          EXPECT_EQ(expected_identity->user_id(), identity.user_id());
          EXPECT_EQ(expected_identity->instance(), identity.instance());
          loop->Quit();
        },
        &loop, &identity));
    loop.Run();
    set_service_started_callback(ServiceStartedCallback());
  }

  void StartTarget() {
    base::FilePath target_path;
    CHECK(base::PathService::Get(base::DIR_ASSETS, &target_path));

#if defined(OS_WIN)
    target_path = target_path.Append(
        FILE_PATH_LITERAL("service_manager_unittest_target.exe"));
#else
    target_path = target_path.Append(
        FILE_PATH_LITERAL("service_manager_unittest_target"));
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
    service_manager::mojom::ServicePtr client =
        service_manager::PassServiceRequestOnCommandLine(&invitation,
                                                         &child_command_line);
    service_manager::mojom::PIDReceiverPtr receiver;

    service_manager::Identity target("service_manager_unittest_target",
                                     service_manager::mojom::kInheritUserID);
    connector()->StartService(target, std::move(client),
                              MakeRequest(&receiver));
    Connector::TestApi test_api(connector());
    test_api.SetStartServiceCallback(base::Bind(
        &ServiceManagerTest::OnConnectionCompleted, base::Unretained(this)));

    target_ = base::LaunchProcess(child_command_line, options);
    DCHECK(target_.IsValid());
    channel.RemoteProcessLaunchAttempted();
    receiver->SetPID(target_.Pid());
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

    connector()->StartService("service_manager_unittest_embedder");
    loop.Run();
    EXPECT_FALSE(failed_to_start);
    EXPECT_EQ(1, start_count);
    EXPECT_EQ("service_manager_unittest_embedder", service_name);
  }

  void StartService(const Identity& identity, bool expect_service_started) {
    int start_count = 0;
    base::RunLoop loop;
    std::string service_name;
    set_service_started_callback(
        base::BindRepeating(&OnServiceStartedCallback, &start_count,
                            &service_name, loop.QuitClosure()));
    bool failed_to_start = false;
    set_service_failed_to_start_callback(base::BindRepeating(
        &OnServiceFailedToStartCallback, &failed_to_start, loop.QuitClosure()));

    connector()->StartService(identity);
    if (!expect_service_started) {
      // Wait briefly and test no new service was created.
      base::MessageLoopCurrent::Get()->task_runner()->PostDelayedTask(
          FROM_HERE, loop.QuitClosure(), base::TimeDelta::FromSeconds(1));
    }

    loop.Run();
    EXPECT_FALSE(failed_to_start);
    if (expect_service_started) {
      EXPECT_EQ(1, start_count);
      EXPECT_EQ(identity.name(), service_name);
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
  // test::ServiceTest:
  std::unique_ptr<Service> CreateService() override {
    service_ = new ServiceManagerTestClient(this);
    return base::WrapUnique(service_);
  }

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

  void OnConnectionCompleted(mojom::ConnectResult, const Identity&) {}

  ServiceManagerTestClient* service_;
  mojo::Binding<mojom::ServiceManagerListener> binding_;
  std::vector<InstanceInfo> instances_;
  std::vector<InstanceInfo> initial_instances_;
  std::unique_ptr<base::RunLoop> wait_for_instances_loop_;
  ServiceStartedCallback service_started_callback_;
  ServiceFailedToStartCallback service_failed_to_start_callback_;
  ServicePIDReceivedCallback service_pid_received_callback_;
  base::Process target_;

  DISALLOW_COPY_AND_ASSIGN(ServiceManagerTest);
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
  EXPECT_TRUE(ContainsInstanceWithName("service_manager_unittest"));

  // 4. Validate that the right applications/processes were created.
  //    Note that the target process will be created even if the tests are
  //    run with --single-process.
  EXPECT_EQ(1u, instances().size());
  {
    auto& instance = instances().back();
    // We learn about the target process id via a ping from it.
    EXPECT_EQ(target_identity(), instance.identity);
    EXPECT_EQ("service_manager_unittest_target",
              instance.identity.name());
    EXPECT_NE(base::kNullProcessId, instance.pid);
  }

  KillTarget();
}

// Tests that starting a regular packaged service works, and that when starting
// the service again, a new service is created unless the same user ID and
// instance names are used.
TEST_F(ServiceManagerTest, CreatePackagedRegularInstances) {
  constexpr char kRegularServiceName[] = "service_manager_unittest_regular";

  AddListenerAndWaitForApplications();

  // Connect to the embedder service first.
  StartEmbedderService();

  Identity identity(kRegularServiceName);
  StartService(identity, /*expect_service_started=*/true);

  // Retstarting with the same identity reuses the existing service.
  StartService(identity, /*expect_service_started=*/false);

  // Starting with a different user ID creates a new service.
  Identity other_user_identity(kRegularServiceName, base::GenerateGUID());
  StartService(other_user_identity, /*expect_service_started=*/true);

  // Starting with a different instance name creates a new service as well.
  Identity instance_identity(kRegularServiceName, mojom::kInheritUserID,
                             "my_instance");
  StartService(instance_identity, /*expect_service_started=*/true);
}

// Tests that starting a shared instance packaged service works, and that when
// starting that service again, a new service is created only when a different
// instance name is specified.
TEST_F(ServiceManagerTest, CreatePackagedAllUsersInstances) {
  constexpr char kAllUsersServiceName[] =
      "service_manager_unittest_shared_instance_across_users";

  AddListenerAndWaitForApplications();

  // Connect to the embedder service first.
  StartEmbedderService();

  Identity identity(kAllUsersServiceName);
  StartService(identity, /*expect_service_started=*/true);

  // Start again with a different user-id, the existing service should be
  // reused.
  Identity other_user_identity(kAllUsersServiceName, base::GenerateGUID());
  StartService(other_user_identity, /*expect_service_started=*/false);

  // Start again with a difference instance name, in that case a new service
  // should get created.
  Identity instance_identity(kAllUsersServiceName, base::GenerateGUID(),
                             "my_instance");
  StartService(instance_identity, /*expect_service_started=*/true);
}

// Tests that creating a singleton packaged service works, and that when
// starting that service again a new service is never created.
TEST_F(ServiceManagerTest, CreatePackagedSingletonInstances) {
  constexpr char kSingletonServiceName[] = "service_manager_unittest_singleton";
  AddListenerAndWaitForApplications();

  // Connect to the embedder service first.
  StartEmbedderService();

  Identity identity(kSingletonServiceName);
  StartService(identity, /*expect_service_started=*/true);

  // Start again with a different user-id, the existing service should be
  // reused.
  Identity other_user_identity(kSingletonServiceName, base::GenerateGUID());
  StartService(other_user_identity, /*expect_service_started=*/false);

  // Start again with the same user-ID but a difference instance name, the
  // existing service should still be reused.
  // should get created.
  Identity instance_identity(kSingletonServiceName, mojom::kInheritUserID,
                             "my_instance");
  StartService(instance_identity, /*expect_service_started=*/false);
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

    connector()->StartService("service_manager_unittest_embedder");
    loop.Run();
    EXPECT_FALSE(failed_to_start);
    EXPECT_EQ("service_manager_unittest_embedder", service_name);
    EXPECT_NE(pid, 0u);
  }
}

TEST_F(ServiceManagerTest, ClientProcessCapabilityEnforced) {
  AddListenerAndWaitForApplications();

  const std::string kTestService = "service_manager_unittest_target";
  const Identity kInstance1Id(kTestService, mojom::kRootUserID, "1");
  const Identity kInstance2Id(kTestService, mojom::kRootUserID);

  // Introduce a new service instance for service_manager_unittest_target,
  // using the client_process capability.
  mojom::ServicePtr test_service_proxy1;
  SimpleService test_service1(mojo::MakeRequest(&test_service_proxy1));
  mojom::PIDReceiverPtr pid_receiver1;
  connector()->StartService(kInstance1Id, std::move(test_service_proxy1),
                            mojo::MakeRequest(&pid_receiver1));
  pid_receiver1->SetPID(42);
  WaitForInstanceToStart(kInstance1Id);
  EXPECT_EQ(1u, instances().size());
  EXPECT_TRUE(ContainsInstanceWithName("service_manager_unittest_target"));

  // Now use the new instance (which does not have client_process capability)
  // to attempt introduction of yet another instance. This should fail.
  mojom::ServicePtr test_service_proxy2;
  SimpleService test_service2(mojo::MakeRequest(&test_service_proxy2));
  mojom::PIDReceiverPtr pid_receiver2;
  test_service1.connector()->StartService(kInstance2Id,
                                          std::move(test_service_proxy2),
                                          mojo::MakeRequest(&pid_receiver2));
  pid_receiver2->SetPID(43);

  // The new service should be disconnected immediately.
  test_service2.WaitForDisconnect();

  // And still only one service instance around.
  EXPECT_EQ(1u, instances().size());
}

TEST_F(ServiceManagerTest, ClonesDisconnectedConnectors) {
  Connector connector((mojom::ConnectorPtrInfo()));
  EXPECT_TRUE(connector.Clone());
}

}  // namespace service_manager

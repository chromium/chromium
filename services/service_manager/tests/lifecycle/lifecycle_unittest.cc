// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/test/test_service_manager.h"
#include "services/service_manager/public/mojom/constants.mojom.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/service_manager/tests/lifecycle/lifecycle.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace service_manager {

namespace {

const char kTestName[] = "lifecycle_unittest";
const char kTestAppName[] = "lifecycle_unittest_app";
const char kTestParentName[] = "lifecycle_unittest_parent";
const char kTestPackageName[] = "lifecycle_unittest_package";
const char kTestPackageAppNameA[] = "lifecycle_unittest_package_app_a";
const char kTestPackageAppNameB[] = "lifecycle_unittest_package_app_b";

const char kTestLifecycleControlCapability[] = "lifecycle_control";
const char kTestParentCapability[] = "lifecycle_unittest:parent";

const std::vector<Manifest>& GetTestManifests() {
  static base::NoDestructor<std::vector<Manifest>> manifests{
      {ManifestBuilder()
           .WithServiceName(kTestName)
           .WithOptions(ManifestOptionsBuilder()
                            .CanRegisterOtherServiceInstances(true)
                            .Build())
           .RequireCapability(kTestParentName, kTestParentCapability)
           .RequireCapability("*", kTestLifecycleControlCapability)
           .RequireCapability(mojom::kServiceName,
                              "service_manager:service_manager")
           .Build(),
       ManifestBuilder()
           .WithServiceName(kTestAppName)
           .WithOptions(ManifestOptionsBuilder()
                            .WithExecutionMode(
                                Manifest::ExecutionMode::kStandaloneExecutable)
                            .WithSandboxType("none")
                            .Build())
           .ExposeCapability(
               kTestLifecycleControlCapability,
               Manifest::InterfaceList<test::mojom::LifecycleControl>())
           .Build(),
       ManifestBuilder()
           .WithServiceName(kTestParentName)
           .WithOptions(ManifestOptionsBuilder()
                            .WithExecutionMode(
                                Manifest::ExecutionMode::kStandaloneExecutable)
                            .WithSandboxType("none")
                            .Build())
           .ExposeCapability(kTestParentCapability,
                             Manifest::InterfaceList<test::mojom::Parent>())
           .RequireCapability(kTestAppName, kTestLifecycleControlCapability)
           .Build(),
       ManifestBuilder()
           .WithServiceName(kTestPackageName)
           .WithOptions(ManifestOptionsBuilder()
                            .WithExecutionMode(
                                Manifest::ExecutionMode::kStandaloneExecutable)
                            .WithSandboxType("none")
                            .Build())
           .ExposeCapability(
               kTestLifecycleControlCapability,
               Manifest::InterfaceList<test::mojom::LifecycleControl>())
           .PackageService(
               ManifestBuilder()
                   .WithServiceName(kTestPackageAppNameA)
                   .ExposeCapability(
                       kTestLifecycleControlCapability,
                       Manifest::InterfaceList<test::mojom::LifecycleControl>())
                   .Build())
           .PackageService(
               ManifestBuilder()
                   .WithServiceName(kTestPackageAppNameB)
                   .ExposeCapability(
                       kTestLifecycleControlCapability,
                       Manifest::InterfaceList<test::mojom::LifecycleControl>())
                   .Build())
           .Build()}};
  return *manifests;
}

struct Instance {
  Instance() : pid(0) {}
  Instance(const Identity& identity, uint32_t pid)
      : identity(identity), pid(pid) {}

  Identity identity;
  uint32_t pid;
};

class InstanceState : public mojom::ServiceManagerListener {
 public:
  InstanceState(mojom::ServiceManagerListenerRequest request,
                base::OnceClosure on_init_complete)
      : binding_(this, std::move(request)),
        on_init_complete_(std::move(on_init_complete)),
        on_destruction_(destruction_loop_.QuitClosure()) {}
  ~InstanceState() override {}

  bool HasInstanceForName(const std::string& name) const {
    return instances_.find(name) != instances_.end();
  }
  size_t GetNewInstanceCount() const {
    return instances_.size() - initial_instances_.size();
  }
  void WaitForInstanceDestruction() {
    // If the instances have already stopped then |destruction_loop_.Run()|
    // will immediately return.
    destruction_loop_.Run();
  }

 private:
  // mojom::ServiceManagerListener:
  void OnInit(std::vector<mojom::RunningServiceInfoPtr> instances) override {
    for (const auto& instance : instances) {
      Instance i(instance->identity, instance->pid);
      initial_instances_[i.identity.name()] = i;
      instances_[i.identity.name()] = i;
    }
    std::move(on_init_complete_).Run();
  }
  void OnServiceCreated(mojom::RunningServiceInfoPtr instance) override {
    instances_[instance->identity.name()] =
        Instance(instance->identity, instance->pid);
  }
  void OnServiceStarted(const service_manager::Identity& identity,
                        uint32_t pid) override {
    for (auto& instance : instances_) {
      if (instance.second.identity == identity) {
        instance.second.pid = pid;
        break;
      }
    }
  }
  void OnServiceFailedToStart(
      const service_manager::Identity& identity) override {
  }
  void OnServiceStopped(const service_manager::Identity& identity) override {
    for (auto it = instances_.begin(); it != instances_.end(); ++it) {
      if (it->second.identity == identity) {
        instances_.erase(it);
        break;
      }
    }
    if (GetNewInstanceCount() == 0)
      std::move(on_destruction_).Run();
  }
  void OnServicePIDReceived(const service_manager::Identity& identity,
                            uint32_t pid) override {
    for (auto& instance : instances_) {
      if (instance.second.identity == identity) {
        instance.second.pid = pid;
        break;
      }
    }
  }

  // All currently running instances.
  std::map<std::string, Instance> instances_;
  // The initial set of instances.
  std::map<std::string, Instance> initial_instances_;

  mojo::Binding<mojom::ServiceManagerListener> binding_;
  base::OnceClosure on_init_complete_;

  // Set when the client wants to wait for this object to track the destruction
  // of an instance before proceeding.
  base::RunLoop destruction_loop_;
  base::OnceClosure on_destruction_;

  DISALLOW_COPY_AND_ASSIGN(InstanceState);
};

}  // namespace

class LifecycleTest : public testing::Test {
 public:
  LifecycleTest()
      : test_service_manager_(GetTestManifests()),
        test_service_binding_(
            &test_service_,
            test_service_manager_.RegisterInstance(
                Identity{kTestName, kSystemInstanceGroup, base::Token{},
                         base::Token::CreateRandom()})) {}

  ~LifecycleTest() override {}

  Connector* connector() { return test_service_binding_.GetConnector(); }

 protected:
  void SetUp() override {
    instances_ = TrackInstances();
  }

  void TearDown() override {
    instances_.reset();
  }

  bool CanRunCrashTest() {
    return !base::CommandLine::ForCurrentProcess()->HasSwitch("single-process");
  }

  test::mojom::LifecycleControlPtr ConnectTo(const std::string& name) {
    test::mojom::LifecycleControlPtr lifecycle;
    connector()->BindInterface(name, &lifecycle);
    PingPong(lifecycle.get());
    return lifecycle;
  }

  void PingPong(test::mojom::LifecycleControl* lifecycle) {
    base::RunLoop loop;
    lifecycle->Ping(loop.QuitClosure());
    loop.Run();
  }

  InstanceState* instances() { return instances_.get(); }

 private:
  std::unique_ptr<InstanceState> TrackInstances() {
    mojo::Remote<mojom::ServiceManager> service_manager;
    connector()->Connect(service_manager::mojom::kServiceName,
                         service_manager.BindNewPipeAndPassReceiver());
    mojo::PendingRemote<mojom::ServiceManagerListener> listener;
    base::RunLoop loop;
    InstanceState* state = new InstanceState(
        listener.InitWithNewPipeAndPassReceiver(), loop.QuitClosure());
    service_manager->AddListener(std::move(listener));
    loop.Run();
    return base::WrapUnique(state);
  }

  base::test::TaskEnvironment task_environment_;
  TestServiceManager test_service_manager_;
  Service test_service_;
  ServiceBinding test_service_binding_;
  std::unique_ptr<InstanceState> instances_;

  DISALLOW_COPY_AND_ASSIGN(LifecycleTest);
};

TEST_F(LifecycleTest, Standalone_GracefulQuit) {
  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestAppName);

  EXPECT_TRUE(instances()->HasInstanceForName(kTestAppName));
  EXPECT_EQ(1u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(loop.QuitClosure());
  lifecycle->GracefulQuit();
  loop.Run();

  instances()->WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestAppName));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}

TEST_F(LifecycleTest, Standalone_Crash) {
  if (!CanRunCrashTest()) {
    LOG(INFO) << "Skipping Standalone_Crash test in --single-process mode.";
    return;
  }

  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestAppName);

  EXPECT_TRUE(instances()->HasInstanceForName(kTestAppName));
  EXPECT_EQ(1u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(loop.QuitClosure());
  lifecycle->Crash();
  loop.Run();

  instances()->WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestAppName));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}

TEST_F(LifecycleTest, Standalone_CloseServiceManagerConnection) {
  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestAppName);

  EXPECT_TRUE(instances()->HasInstanceForName(kTestAppName));
  EXPECT_EQ(1u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(loop.QuitClosure());
  lifecycle->CloseServiceManagerConnection();

  instances()->WaitForInstanceDestruction();

  // |lifecycle| pipe should still be valid.
  PingPong(lifecycle.get());
}

TEST_F(LifecycleTest, PackagedApp_GracefulQuit) {
  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestPackageAppNameA);

  // There should be two new instances - one for the app and one for the package
  // that vended it.
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageName));
  EXPECT_EQ(2u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(loop.QuitClosure());
  lifecycle->GracefulQuit();
  loop.Run();

  instances()->WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageName));
  EXPECT_FALSE(instances()->HasInstanceForName(kTestAppName));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}

TEST_F(LifecycleTest, PackagedApp_Crash) {
  if (!CanRunCrashTest()) {
    LOG(INFO) << "Skipping Standalone_Crash test in --single-process mode.";
    return;
  }

  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestPackageAppNameA);

  // There should be two new instances - one for the app and one for the package
  // that vended it.
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageName));
  EXPECT_EQ(2u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(loop.QuitClosure());
  lifecycle->Crash();
  loop.Run();

  instances()->WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageName));
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}

// When a single package provides multiple apps out of one process, crashing one
// app crashes all.
TEST_F(LifecycleTest, PackagedApp_CrashCrashesOtherProvidedApp) {
  if (!CanRunCrashTest()) {
    LOG(INFO) << "Skipping Standalone_Crash test in --single-process mode.";
    return;
  }

  test::mojom::LifecycleControlPtr lifecycle_a =
      ConnectTo(kTestPackageAppNameA);
  test::mojom::LifecycleControlPtr lifecycle_b =
      ConnectTo(kTestPackageAppNameB);
  test::mojom::LifecycleControlPtr lifecycle_package =
      ConnectTo(kTestPackageName);

  // There should be three instances, one for each packaged app and the package
  // itself.
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageAppNameB));
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageName));
  size_t instance_count = instances()->GetNewInstanceCount();
  ASSERT_EQ(3u, instance_count);

  base::RunLoop loop;
  base::RepeatingClosure quit_on_last = base::BarrierClosure(
      static_cast<int>(instance_count), loop.QuitClosure());
  lifecycle_a.set_connection_error_handler(quit_on_last);
  lifecycle_b.set_connection_error_handler(quit_on_last);
  lifecycle_package.set_connection_error_handler(quit_on_last);

  // Now crash one of the packaged apps.
  lifecycle_a->Crash();
  loop.Run();

  instances()->WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageName));
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageAppNameB));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}

// When a single package provides multiple apps out of one process, crashing one
// app crashes all.
TEST_F(LifecycleTest, PackagedApp_GracefulQuitPackageQuitsAll) {
  test::mojom::LifecycleControlPtr lifecycle_a =
      ConnectTo(kTestPackageAppNameA);
  test::mojom::LifecycleControlPtr lifecycle_b =
      ConnectTo(kTestPackageAppNameB);
  test::mojom::LifecycleControlPtr lifecycle_package =
      ConnectTo(kTestPackageName);

  // There should be three instances, one for each packaged app and the package
  // itself.
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageAppNameB));
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageName));
  size_t instance_count = instances()->GetNewInstanceCount();
  ASSERT_EQ(3u, instance_count);

  base::RunLoop loop;
  base::RepeatingClosure quit_on_last = base::BarrierClosure(
      static_cast<int>(instance_count), loop.QuitClosure());
  lifecycle_a.set_connection_error_handler(quit_on_last);
  lifecycle_b.set_connection_error_handler(quit_on_last);
  lifecycle_package.set_connection_error_handler(quit_on_last);

  // Now quit the package. All the packaged apps should close.
  lifecycle_package->GracefulQuit();
  loop.Run();

  instances()->WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageName));
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageAppNameB));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}

}  // namespace service_manager

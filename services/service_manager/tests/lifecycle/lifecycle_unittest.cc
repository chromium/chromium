// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/service_manager/public/mojom/constants.mojom.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/service_manager/tests/lifecycle/lifecycle_unittest.mojom.h"
#include "services/service_manager/tests/util.h"

namespace service_manager {

namespace {

const char kTestAppName[] = "lifecycle_unittest_app";
const char kTestExeName[] = "lifecycle_unittest_exe";
const char kTestPackageName[] = "lifecycle_unittest_package";
const char kTestPackageAppNameA[] = "lifecycle_unittest_package_app_a";
const char kTestPackageAppNameB[] = "lifecycle_unittest_package_app_b";
const char kTestName[] = "lifecycle_unittest";

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

class LifecycleTest : public test::ServiceTest {
 public:
  LifecycleTest() : ServiceTest(kTestName) {}
  ~LifecycleTest() override {}

 protected:
  // test::ServiceTest:
  void SetUp() override {
    test::ServiceTest::SetUp();
    instances_ = TrackInstances();
  }
  void TearDown() override {
    instances_.reset();
    test::ServiceTest::TearDown();
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

  base::Process LaunchProcess() {
    base::Process process;
    test::LaunchAndConnectToProcess(
#if defined(OS_WIN)
        "lifecycle_unittest_exe.exe",
#else
        "lifecycle_unittest_exe",
#endif
        Identity(kTestExeName, mojom::kInheritUserID),
        connector(),
        &process);
    return process;
  }

  void PingPong(test::mojom::LifecycleControl* lifecycle) {
    base::RunLoop loop;
    lifecycle->Ping(loop.QuitClosure());
    loop.Run();
  }

  InstanceState* instances() { return instances_.get(); }

 private:
  std::unique_ptr<InstanceState> TrackInstances() {
    mojom::ServiceManagerPtr service_manager;
    connector()->BindInterface(service_manager::mojom::kServiceName,
                               &service_manager);
    mojom::ServiceManagerListenerPtr listener;
    base::RunLoop loop;
    InstanceState* state =
        new InstanceState(MakeRequest(&listener), loop.QuitClosure());
    service_manager->AddListener(std::move(listener));
    loop.Run();
    return base::WrapUnique(state);
  }

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

TEST_F(LifecycleTest, Exe_GracefulQuit) {
  base::Process process = LaunchProcess();

  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestExeName);

  EXPECT_TRUE(instances()->HasInstanceForName(kTestExeName));
  EXPECT_EQ(1u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(loop.QuitClosure());
  lifecycle->GracefulQuit();
  loop.Run();

  instances()->WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestExeName));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());

  process.Terminate(9, true);
}

#if defined(OS_FUCHSIA)
#define MAYBE_Exe_TerminateProcess DISABLED_Exe_TerminateProcess
#else
#define MAYBE_Exe_TerminateProcess Exe_TerminateProcess
#endif
TEST_F(LifecycleTest, MAYBE_Exe_TerminateProcess) {
  base::Process process = LaunchProcess();

  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestExeName);

  EXPECT_TRUE(instances()->HasInstanceForName(kTestExeName));
  EXPECT_EQ(1u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(loop.QuitClosure());
  process.Terminate(9, true);
  loop.Run();

  instances()->WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestExeName));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}

}  // namespace service_manager

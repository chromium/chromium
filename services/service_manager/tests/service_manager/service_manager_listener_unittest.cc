// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_receiver.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/service_manager/service_manager.h"
#include "services/service_manager/tests/service_manager/test_manifests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace service_manager {
namespace {

constexpr uint32_t kTestSelfPid = 1234;
constexpr uint32_t kTestTargetPid1 = 4567;
constexpr uint32_t kTestTargetPid2 = 8910;

class TestListener : public mojom::ServiceManagerListener {
 public:
  explicit TestListener(
      mojo::PendingReceiver<mojom::ServiceManagerListener> receiver)
      : receiver_(this, std::move(receiver)) {}

  TestListener(const TestListener&) = delete;
  TestListener& operator=(const TestListener&) = delete;

  ~TestListener() override = default;

  void WaitForInit() { wait_for_init_loop_.Run(); }

  void WaitForServiceStarted(Identity* out_identity, uint32_t* out_pid) {
    wait_for_start_identity_ = out_identity;
    wait_for_start_pid_ = out_pid;
    wait_for_start_loop_.emplace();
    wait_for_start_loop_->Run();
  }

  // mojom::ServiceManagerListener:
  void OnInit(std::vector<mojom::RunningServiceInfoPtr> instances) override {
    wait_for_init_loop_.Quit();
  }
  void OnServiceCreated(mojom::RunningServiceInfoPtr instance) override {}
  void OnServiceStarted(const Identity& identity, uint32_t pid) override {
    if (wait_for_start_loop_)
      wait_for_start_loop_->Quit();
    if (wait_for_start_identity_)
      *wait_for_start_identity_ = identity;
    if (wait_for_start_pid_)
      *wait_for_start_pid_ = pid;
    wait_for_start_identity_ = nullptr;
    wait_for_start_pid_ = nullptr;
  }
  void OnServiceFailedToStart(const Identity& identity) override {}
  void OnServiceStopped(const Identity& identity) override {}
  void OnServicePIDReceived(const Identity& identity, uint32_t pid) override {}

 private:
  mojo::Receiver<mojom::ServiceManagerListener> receiver_;
  base::RunLoop wait_for_init_loop_;

  std::optional<base::RunLoop> wait_for_start_loop_;
  raw_ptr<Identity> wait_for_start_identity_ = nullptr;
  raw_ptr<uint32_t> wait_for_start_pid_ = nullptr;
};

class TestTargetService : public Service {
 public:
  explicit TestTargetService(mojo::PendingReceiver<mojom::Service> receiver)
      : receiver_(this, std::move(receiver)) {}

  TestTargetService(const TestTargetService&) = delete;
  TestTargetService& operator=(const TestTargetService&) = delete;

  ~TestTargetService() override = default;

  Connector* connector() { return receiver_.GetConnector(); }

  // Tells the Service Manager this instance wants to die, and waits for ack.
  // When this returns, we can be sure the Service Manager is no longer keeping
  // this instance's Identity reserved and we may reuse it (modulo a new
  // globally unique ID) for another instance.
  void QuitGracefullyAndWait() {
    receiver_.RequestClose();
    wait_for_disconnect_loop_.Run();
  }

 private:
  // Service:
  void OnDisconnected() override { wait_for_disconnect_loop_.Quit(); }

  ServiceReceiver receiver_;
  base::RunLoop wait_for_disconnect_loop_;
};

class ServiceManagerListenerTest : public testing::Test, public Service {
 public:
  ServiceManagerListenerTest()
      : service_manager_(GetTestManifests(),
                         ServiceManager::ServiceExecutablePolicy::kSupported) {}

  ServiceManagerListenerTest(const ServiceManagerListenerTest&) = delete;
  ServiceManagerListenerTest& operator=(const ServiceManagerListenerTest&) =
      delete;

  ~ServiceManagerListenerTest() override = default;

  Connector* connector() { return service_receiver_.GetConnector(); }

  void SetUp() override {
    service_receiver_.Bind(
        RegisterServiceInstance(kTestServiceName, kTestSelfPid));

    mojo::Remote<mojom::ServiceManager> service_manager;
    connector()->Connect(mojom::kServiceName,
                         service_manager.BindNewPipeAndPassReceiver());

    mojo::PendingRemote<mojom::ServiceManagerListener> listener_proxy;
    listener_ = std::make_unique<TestListener>(
        listener_proxy.InitWithNewPipeAndPassReceiver());
    service_manager->AddListener(std::move(listener_proxy));
    listener_->WaitForInit();
  }

  mojo::PendingReceiver<mojom::Service> RegisterServiceInstance(
      const std::string& service_name,
      uint32_t fake_pid) {
    mojo::PendingRemote<mojom::Service> service;
    auto receiver = service.InitWithNewPipeAndPassReceiver();
    mojo::Remote<mojom::ProcessMetadata> metadata;
    service_manager_.RegisterService(
        Identity(service_name, kSystemInstanceGroup, base::Token{},
                 base::Token::CreateRandom()),
        std::move(service), metadata.BindNewPipeAndPassReceiver());
    metadata->SetPID(fake_pid);
    return receiver;
  }

  void WaitForServiceStarted(Identity* out_identity, uint32_t* out_pid) {
    listener_->WaitForServiceStarted(out_identity, out_pid);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  ServiceManager service_manager_;
  ServiceReceiver service_receiver_{this};
  std::unique_ptr<TestListener> listener_;
};

TEST_F(ServiceManagerListenerTest, InstancesHaveUniqueIdentity) {
  TestTargetService target1(
      RegisterServiceInstance(kTestTargetName, kTestTargetPid1));

  Identity identity1;
  uint32_t pid1;
  WaitForServiceStarted(&identity1, &pid1);
  EXPECT_EQ(kTestTargetName, identity1.name());
  EXPECT_FALSE(identity1.globally_unique_id().is_zero());
  EXPECT_EQ(kTestTargetPid1, pid1);

  // We retain a Connector from the first instance before disconnecting it. This
  // keeps some state for the instance alive in the Service Manager and blocks
  // OnInstanceStopped from being broadcast to ServiceManagerListeners, but the
  // instance is no longer reachable and its basic identity can be reused by a
  // new instance.
  std::unique_ptr<Connector> connector = target1.connector()->Clone();
  target1.QuitGracefullyAndWait();

  TestTargetService target2(
      RegisterServiceInstance(kTestTargetName, kTestTargetPid2));

  Identity identity2;
  uint32_t pid2;
  WaitForServiceStarted(&identity2, &pid2);
  EXPECT_EQ(kTestTargetName, identity2.name());
  EXPECT_FALSE(identity2.globally_unique_id().is_zero());
  EXPECT_EQ(kTestTargetPid2, pid2);

  // This is the important part of the test. The globally unique IDs of both
  // instances must differ, even though all other fields may be (and in this
  // case, will be) the same.
  EXPECT_EQ(identity1.name(), identity2.name());
  EXPECT_EQ(identity1.instance_group(), identity2.instance_group());
  EXPECT_EQ(identity1.instance_id(), identity2.instance_id());
  EXPECT_NE(identity1.globally_unique_id(), identity2.globally_unique_id());
}

}  // namespace
}  // namespace service_manager

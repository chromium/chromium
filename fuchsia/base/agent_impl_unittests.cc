// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/agent_impl.h"

#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/testfidl/cpp/fidl.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/result_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cr_fuchsia {

namespace {

const char kNoServicesComponentId[] = "no-services";
const char kAccumulatorComponentId1[] = "accumulator1";
const char kAccumulatorComponentId2[] = "accumulator2";
const char kKeepAliveComponentId[] = "keep-alive";

class EmptyComponentState : public AgentImpl::ComponentStateBase {
 public:
  EmptyComponentState(base::StringPiece component)
      : ComponentStateBase(component) {}
};

class AccumulatingTestInterfaceImpl
    : public base::fuchsia::testfidl::TestInterface {
 public:
  AccumulatingTestInterfaceImpl() = default;

  // TestInterface implementation:
  void Add(int32_t a, int32_t b, AddCallback callback) override {
    accumulated_ += a + b;
    callback(accumulated_);
  }

 private:
  int32_t accumulated_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AccumulatingTestInterfaceImpl);
};

class AccumulatorComponentState : public AgentImpl::ComponentStateBase {
 public:
  AccumulatorComponentState(base::StringPiece component)
      : ComponentStateBase(component),
        service_binding_(outgoing_directory(), &service_) {}

 protected:
  AccumulatingTestInterfaceImpl service_;
  base::fuchsia::ScopedServiceBinding<base::fuchsia::testfidl::TestInterface>
      service_binding_;
};

class KeepAliveComponentState : public AccumulatorComponentState {
 public:
  KeepAliveComponentState(base::StringPiece component)
      : AccumulatorComponentState(component) {
    AddKeepAliveBinding(&service_binding_);
  }

  void DisconnectClientsAndTeardown() {
    AgentImpl::ComponentStateBase::DisconnectClientsAndTeardown();
  }
};

class AgentImplTest : public ::testing::Test {
 public:
  AgentImplTest() {
    fidl::InterfaceHandle<::fuchsia::io::Directory> directory;
    services_.GetOrCreateDirectory("svc")->Serve(
        fuchsia::io::OPEN_RIGHT_READABLE | fuchsia::io::OPEN_RIGHT_WRITABLE,
        directory.NewRequest().TakeChannel());

    services_client_ =
        std::make_unique<sys::ServiceDirectory>(std::move(directory));
  }

  fuchsia::modular::AgentPtr CreateAgentAndConnect() {
    DCHECK(!agent_impl_);
    agent_impl_ = std::make_unique<AgentImpl>(
        &services_, base::BindRepeating(&AgentImplTest::OnComponentConnect,
                                        base::Unretained(this)));
    fuchsia::modular::AgentPtr agent;
    services_client_->Connect(agent.NewRequest());
    return agent;
  }

  void TeardownKeepAliveComponentState() {
    std::move(disconnect_clients_and_teardown_).Run();
  }

 protected:
  std::unique_ptr<AgentImpl::ComponentStateBase> OnComponentConnect(
      base::StringPiece component_id) {
    if (component_id == kNoServicesComponentId) {
      return std::make_unique<EmptyComponentState>(component_id);
    } else if (component_id == kAccumulatorComponentId1 ||
               component_id == kAccumulatorComponentId2) {
      return std::make_unique<AccumulatorComponentState>(component_id);
    } else if (component_id == kKeepAliveComponentId) {
      auto component_state =
          std::make_unique<KeepAliveComponentState>(component_id);
      disconnect_clients_and_teardown_ =
          base::BindOnce(&KeepAliveComponentState::DisconnectClientsAndTeardown,
                         base::Unretained(component_state.get()));
      return component_state;
    }
    return nullptr;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  sys::OutgoingDirectory services_;
  std::unique_ptr<sys::ServiceDirectory> services_client_;

  std::unique_ptr<AgentImpl> agent_impl_;

  // Set only if a keep-alive component was connected, to allow the test to
  // forcibly teardown the ComponentState for it.
  base::OnceClosure disconnect_clients_and_teardown_;

  DISALLOW_COPY_AND_ASSIGN(AgentImplTest);
};

}  // namespace

// Verify that the Agent can publish and unpublish itself.
TEST_F(AgentImplTest, PublishAndUnpublish) {
  cr_fuchsia::ResultReceiver<zx_status_t> client_disconnect_status1;
  fuchsia::modular::AgentPtr agent = CreateAgentAndConnect();
  agent.set_error_handler(cr_fuchsia::CallbackToFitFunction(
      client_disconnect_status1.GetReceiveCallback()));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(client_disconnect_status1.has_value());

  // Teardown the Agent.
  agent_impl_.reset();

  // Verify that the client got disconnected on teardown.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(*client_disconnect_status1, ZX_ERR_PEER_CLOSED);

  // Verify that the Agent service is no longer available.
  cr_fuchsia::ResultReceiver<zx_status_t> client_disconnect_status2;
  services_client_->Connect(agent.NewRequest());
  agent.set_error_handler(cr_fuchsia::CallbackToFitFunction(
      client_disconnect_status2.GetReceiveCallback()));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(*client_disconnect_status2, ZX_ERR_PEER_CLOSED);
}

// Verify that multiple connection attempts with the different component Ids
// to the same service get different instances.
TEST_F(AgentImplTest, DifferentComponentIdSameService) {
  fuchsia::modular::AgentPtr agent = CreateAgentAndConnect();

  // Connect to the Agent twice using the same component Id.
  fuchsia::sys::ServiceProviderPtr component_services1;
  agent->Connect(kAccumulatorComponentId1, component_services1.NewRequest());
  fuchsia::sys::ServiceProviderPtr component_services2;
  agent->Connect(kAccumulatorComponentId2, component_services2.NewRequest());

  // Request the TestInterface from each of the service directories.
  base::fuchsia::testfidl::TestInterfacePtr test_interface1;
  component_services1->ConnectToService(
      base::fuchsia::testfidl::TestInterface::Name_,
      test_interface1.NewRequest().TakeChannel());
  base::fuchsia::testfidl::TestInterfacePtr test_interface2;
  component_services2->ConnectToService(
      base::fuchsia::testfidl::TestInterface::Name_,
      test_interface1.NewRequest().TakeChannel());

  // Both TestInterface pointers should remain valid until we are done.
  test_interface1.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status);
    ADD_FAILURE();
  });
  test_interface2.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status);
    ADD_FAILURE();
  });

  // Call Add() via one TestInterface and verify accumulator had been at zero.
  {
    base::RunLoop loop;
    test_interface1->Add(1, 2,
                         [quit_loop = loop.QuitClosure()](int32_t result) {
                           EXPECT_EQ(result, 3);
                           quit_loop.Run();
                         });
  }

  // Call Add() via the second TestInterface, and verify that first Add() call's
  // effects aren't visible.
  {
    base::RunLoop loop;
    test_interface2->Add(3, 4,
                         [quit_loop = loop.QuitClosure()](int32_t result) {
                           EXPECT_EQ(result, 7);
                           quit_loop.Run();
                         });
  }

  test_interface1.set_error_handler(nullptr);
  test_interface2.set_error_handler(nullptr);
}

// Verify that multiple connection attempts with the same component Id connect
// it to the same service instances.
TEST_F(AgentImplTest, SameComponentIdSameService) {
  fuchsia::modular::AgentPtr agent = CreateAgentAndConnect();

  // Connect to the Agent twice using the same component Id.
  fuchsia::sys::ServiceProviderPtr component_services1;
  agent->Connect(kAccumulatorComponentId1, component_services1.NewRequest());
  fuchsia::sys::ServiceProviderPtr component_services2;
  agent->Connect(kAccumulatorComponentId1, component_services2.NewRequest());

  // Request the TestInterface from each of the service directories.
  base::fuchsia::testfidl::TestInterfacePtr test_interface1;
  component_services1->ConnectToService(
      base::fuchsia::testfidl::TestInterface::Name_,
      test_interface1.NewRequest().TakeChannel());
  base::fuchsia::testfidl::TestInterfacePtr test_interface2;
  component_services2->ConnectToService(
      base::fuchsia::testfidl::TestInterface::Name_,
      test_interface1.NewRequest().TakeChannel());

  // Both TestInterface pointers should remain valid until we are done.
  test_interface1.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status);
    ADD_FAILURE();
  });
  test_interface2.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status);
    ADD_FAILURE();
  });

  // Call Add() via one TestInterface and verify accumulator had been at zero.
  {
    base::RunLoop loop;
    test_interface1->Add(1, 2,
                         [quit_loop = loop.QuitClosure()](int32_t result) {
                           EXPECT_EQ(result, 3);
                           quit_loop.Run();
                         });
  }

  // Call Add() via the other TestInterface, and verify that the result of the
  // previous Add() was already in the accumulator.
  {
    base::RunLoop loop;
    test_interface2->Add(3, 4,
                         [quit_loop = loop.QuitClosure()](int32_t result) {
                           EXPECT_EQ(result, 10);
                           quit_loop.Run();
                         });
  }

  test_interface1.set_error_handler(nullptr);
  test_interface2.set_error_handler(nullptr);
}

// Verify that connections to a service registered to keep-alive the
// ComponentStateBase keeps it alive after the ServiceProvider is dropped.
TEST_F(AgentImplTest, KeepAliveBinding) {
  fuchsia::modular::AgentPtr agent = CreateAgentAndConnect();

  {
    // Connect to the Agent and request the TestInterface.
    fuchsia::sys::ServiceProviderPtr component_services;
    agent->Connect(kAccumulatorComponentId1, component_services.NewRequest());
    base::fuchsia::testfidl::TestInterfacePtr test_interface;
    component_services->ConnectToService(
        base::fuchsia::testfidl::TestInterface::Name_,
        test_interface.NewRequest().TakeChannel());

    // The TestInterface pointer should be closed as soon as we Unbind() the
    // ServiceProvider.
    test_interface.set_error_handler(
        [](zx_status_t status) { EXPECT_EQ(status, ZX_ERR_PEER_CLOSED); });

    // After disconnecting ServiceProvider, TestInterface should remain valid.
    component_services.Unbind();
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(test_interface);
  }

  {
    // Connect to the Agent and request the TestInterface.
    fuchsia::sys::ServiceProviderPtr component_services;
    agent->Connect(kKeepAliveComponentId, component_services.NewRequest());
    base::fuchsia::testfidl::TestInterfacePtr test_interface;
    component_services->ConnectToService(
        base::fuchsia::testfidl::TestInterface::Name_,
        test_interface.NewRequest().TakeChannel());

    // The TestInterface pointer should remain valid until we are done.
    test_interface.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status);
      ADD_FAILURE();
    });

    // After disconnecting ServiceProvider, TestInterface should remain valid.
    component_services.Unbind();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(test_interface);
  }

  // Spin the MessageLoop to let the AgentImpl see that TestInterface is gone.
  base::RunLoop().RunUntilIdle();
}

// Verify that connections to a service registered to keep-alive the
// ComponentStateBase is disconnected by DisconnectClientsAndTeardown.
TEST_F(AgentImplTest, DisconnectClientsAndTeardown) {
  fuchsia::modular::AgentPtr agent = CreateAgentAndConnect();

  {
    // Connect to the Agent and request the TestInterface.
    fuchsia::sys::ServiceProviderPtr component_services;
    agent->Connect(kKeepAliveComponentId, component_services.NewRequest());
    base::fuchsia::testfidl::TestInterfacePtr test_interface;
    component_services->ConnectToService(
        base::fuchsia::testfidl::TestInterface::Name_,
        test_interface.NewRequest().TakeChannel());

    // The TestInterface pointer should remain valid until we call
    // DisconnectClientsAndTeardown().
    test_interface.set_error_handler(
        [](zx_status_t status) { ZX_LOG_IF(ERROR, status != ZX_OK, status); });

    // After disconnecting ServiceProvider, TestInterface should remain valid.
    component_services.Unbind();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(test_interface);

    // After invoking DisconnectClientsAndTeardown(), TestInterface should
    // be disconnected.
    TeardownKeepAliveComponentState();
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(test_interface);
  }
}

}  // namespace cr_fuchsia

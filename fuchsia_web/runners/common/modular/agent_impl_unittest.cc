// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/common/modular/agent_impl.h"

#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/testfidl/cpp/fidl.h"
#include "fuchsia_web/common/test/fit_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cr_fuchsia {

namespace {

const char kNoServicesComponentId[] = "no-services";
const char kAccumulatorComponentId1[] = "accumulator1";
const char kAccumulatorComponentId2[] = "accumulator2";
const char kKeepAliveComponentId[] = "keep-alive";

class EmptyComponentState : public AgentImpl::ComponentStateBase {
 public:
  explicit EmptyComponentState(base::StringPiece component)
      : ComponentStateBase(component) {}
};

class AccumulatingTestInterfaceImpl : public base::testfidl::TestInterface {
 public:
  AccumulatingTestInterfaceImpl() = default;

  AccumulatingTestInterfaceImpl(const AccumulatingTestInterfaceImpl&) = delete;
  AccumulatingTestInterfaceImpl& operator=(
      const AccumulatingTestInterfaceImpl&) = delete;

  // TestInterface implementation:
  void Add(int32_t a, int32_t b, AddCallback callback) override {
    accumulated_ += a + b;
    callback(accumulated_);
  }

 private:
  int32_t accumulated_ = 0;
};

class AccumulatorComponentState : public AgentImpl::ComponentStateBase {
 public:
  explicit AccumulatorComponentState(base::StringPiece component)
      : ComponentStateBase(component),
        service_binding_(outgoing_directory(), &service_) {}

 protected:
  AccumulatingTestInterfaceImpl service_;
  base::ScopedServiceBinding<base::testfidl::TestInterface> service_binding_;
};

class KeepAliveComponentState : public AccumulatorComponentState {
 public:
  explicit KeepAliveComponentState(base::StringPiece component)
      : AccumulatorComponentState(component) {
    AddKeepAliveBinding(&service_binding_);
  }

  void DisconnectClientsAndTeardown() {
    AgentImpl::ComponentStateBase::DisconnectClientsAndTeardown();
  }
};

class AgentImplTest : public ::testing::Test {
 protected:
  AgentImplTest() = default;

  AgentImplTest(const AgentImplTest&) = delete;
  AgentImplTest& operator=(const AgentImplTest&) = delete;

  fuchsia::modular::AgentPtr CreateAgentAndConnect() {
    EXPECT_FALSE(agent_impl_);
    if (public_services_.empty()) {
      agent_impl_ = std::make_unique<AgentImpl>(
          base::ComponentContextForProcess()->outgoing().get(),
          base::BindRepeating(&AgentImplTest::OnComponentConnect,
                              base::Unretained(this)));
    } else {
      agent_impl_ = std::make_unique<AgentImpl>(
          base::ComponentContextForProcess()->outgoing().get(),
          base::BindRepeating(&AgentImplTest::OnComponentConnect,
                              base::Unretained(this)),
          public_services_);
    }
    fuchsia::modular::AgentPtr agent;
    test_context_.published_services()->Connect(agent.NewRequest());
    return agent;
  }

  void TeardownKeepAliveComponentState() {
    std::move(disconnect_clients_and_teardown_).Run();
  }

  void set_public_services(std::vector<std::string> services) {
    public_services_ = std::move(services);
  }

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
  const base::TestComponentContextForProcess test_context_;

  std::unique_ptr<AgentImpl> agent_impl_;

  // Set only if a keep-alive component was connected, to allow the test to
  // forcibly teardown the ComponentState for it.
  base::OnceClosure disconnect_clients_and_teardown_;

  // Service names passed to the AgentImpl constructor to publish from
  // the process' outgoing directory.
  std::vector<std::string> public_services_;
};

}  // namespace

// Verify that the Agent can publish and unpublish itself.
TEST_F(AgentImplTest, PublishAndUnpublish) {
  base::test::TestFuture<zx_status_t> client_disconnect_status1;
  fuchsia::modular::AgentPtr agent = CreateAgentAndConnect();
  agent.set_error_handler(
      CallbackToFitFunction(client_disconnect_status1.GetCallback()));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(client_disconnect_status1.IsReady());

  // Teardown the Agent.
  agent_impl_.reset();

  // Verify that the client got disconnected on teardown.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(client_disconnect_status1.Get(), ZX_ERR_PEER_CLOSED);

  // Verify that the Agent service is no longer available.
  base::test::TestFuture<zx_status_t> client_disconnect_status2;
  test_context_.published_services()->Connect(agent.NewRequest());
  agent.set_error_handler(
      CallbackToFitFunction(client_disconnect_status2.GetCallback()));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(client_disconnect_status2.Get(), ZX_ERR_PEER_CLOSED);
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
  base::testfidl::TestInterfacePtr test_interface1;
  component_services1->ConnectToService(
      base::testfidl::TestInterface::Name_,
      test_interface1.NewRequest().TakeChannel());
  base::testfidl::TestInterfacePtr test_interface2;
  component_services2->ConnectToService(
      base::testfidl::TestInterface::Name_,
      test_interface2.NewRequest().TakeChannel());

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
    loop.RunUntilIdle();
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
    loop.RunUntilIdle();
  }

  // Cleanly unbind the test interfaces now that we're done with them.
  test_interface1 = nullptr;
  test_interface2 = nullptr;

  // Tear down connections to the agent and let the error handlers unwind.
  {
    base::RunLoop loop;
    component_services1.Unbind();
    component_services2.Unbind();
    loop.RunUntilIdle();
  }
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
  base::testfidl::TestInterfacePtr test_interface1;
  component_services1->ConnectToService(
      base::testfidl::TestInterface::Name_,
      test_interface1.NewRequest().TakeChannel());
  base::testfidl::TestInterfacePtr test_interface2;
  component_services2->ConnectToService(
      base::testfidl::TestInterface::Name_,
      test_interface2.NewRequest().TakeChannel());

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
    loop.RunUntilIdle();
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
    loop.RunUntilIdle();
  }

  // Cleanly unbind the test interfaces now that we're done with them.
  test_interface1 = nullptr;
  test_interface2 = nullptr;

  // Tear down connections to the agent and let the error handlers unwind.
  {
    base::RunLoop loop;
    component_services1.Unbind();
    component_services2.Unbind();
    loop.RunUntilIdle();
  }
}

// Verify that connections to a service registered to keep-alive the
// ComponentStateBase keeps it alive after the ServiceProvider is dropped.
TEST_F(AgentImplTest, KeepAliveBinding) {
  fuchsia::modular::AgentPtr agent = CreateAgentAndConnect();

  {
    // Connect to the Agent and request the TestInterface.
    fuchsia::sys::ServiceProviderPtr component_services;
    agent->Connect(kAccumulatorComponentId1, component_services.NewRequest());
    base::testfidl::TestInterfacePtr test_interface;
    component_services->ConnectToService(
        base::testfidl::TestInterface::Name_,
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
    base::testfidl::TestInterfacePtr test_interface;
    component_services->ConnectToService(
        base::testfidl::TestInterface::Name_,
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
    base::testfidl::TestInterfacePtr test_interface;
    component_services->ConnectToService(
        base::testfidl::TestInterface::Name_,
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

class AgentImplTestWithPublicService : public AgentImplTest {
 protected:
  static constexpr char kServiceName[] = "base.testfidl.TestInterface-public";

  AgentImplTestWithPublicService() : AgentImplTest() {
    // Publish kServiceName to the outgoing services directory for the current
    // process (scoped to this test by the TestComponentContextForProcess).
    // AgentImpl should route requests for this "public" service via the
    // outgoing services directory.
    base::ComponentContextForProcess()
        ->outgoing()
        ->AddPublicService<base::testfidl::TestInterface>(
            [](fidl::InterfaceRequest<base::testfidl::TestInterface> request) {
              request.Close(ZX_OK);
            },
            kServiceName);
  }
};

// Verify that the DefaultComponentState publishes the process' outgoing
// service directory.
TEST_F(AgentImplTestWithPublicService, PublicService) {
  // Configure the AgentImpl to provide the "public" service.
  set_public_services({kServiceName});
  fuchsia::modular::AgentPtr agent = CreateAgentAndConnect();

  // Connect to the ServiceProvider for the a dummy component.
  fuchsia::sys::ServiceProviderPtr component_services;
  agent->Connect(kAccumulatorComponentId1, component_services.NewRequest());

  // Connect to the public service.
  base::testfidl::TestInterfacePtr test_interface;
  component_services->ConnectToService(
      kServiceName, test_interface.NewRequest().TakeChannel());

  // If we successfully connect then the service connection will be closed
  // with ZX_OK, by the connection-handler implementation in the test base.
  base::test::TestFuture<zx_status_t> service_disconnect_status;
  test_interface.set_error_handler(
      CallbackToFitFunction(service_disconnect_status.GetCallback()));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(service_disconnect_status.IsReady());
  EXPECT_EQ(service_disconnect_status.Get(), ZX_OK);

  // Close the ServiceProvider channel and spin the MessageLoop to let the
  // AgentImpl clean up the component state.
  component_services = nullptr;
  base::RunLoop().RunUntilIdle();
}

// Verify that services published via the outgoing directory, but not as
// public services, are not available.
TEST_F(AgentImplTestWithPublicService, PublicServiceNotProvided) {
  // Configure the AgentImpl to provide the "public" service.
  set_public_services({});
  fuchsia::modular::AgentPtr agent = CreateAgentAndConnect();

  // Connect to the ServiceProvider for the a dummy component.
  fuchsia::sys::ServiceProviderPtr component_services;
  agent->Connect(kAccumulatorComponentId1, component_services.NewRequest());

  // Connect to the public service.
  base::testfidl::TestInterfacePtr test_interface;
  component_services->ConnectToService(
      kServiceName, test_interface.NewRequest().TakeChannel());

  // If the service is not routed as "public" by the AgentImpl then the
  // request should simply be dropped.
  base::test::TestFuture<zx_status_t> service_disconnect_status;
  test_interface.set_error_handler(
      CallbackToFitFunction(service_disconnect_status.GetCallback()));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(service_disconnect_status.IsReady());
  EXPECT_EQ(service_disconnect_status.Get(), ZX_ERR_PEER_CLOSED);

  // Close the ServiceProvider channel and spin the MessageLoop to let the
  // AgentImpl clean up the component state.
  component_services = nullptr;
  base::RunLoop().RunUntilIdle();
}

}  // namespace cr_fuchsia

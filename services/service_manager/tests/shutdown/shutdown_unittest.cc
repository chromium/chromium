// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_receiver.h"
#include "services/service_manager/public/cpp/test/test_service_manager.h"
#include "services/service_manager/public/mojom/constants.mojom.h"
#include "services/service_manager/tests/shutdown/shutdown.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace service_manager {
namespace {

const char kTestServiceName[] = "shutdown_unittest";
const char kShutdownServiceName[] = "shutdown_service";
const char kShutdownClientName[] = "shutdown_client";

const char kClientControllerCapability[] = "client_controller";
const char kShutdownServiceCapability[] = "shutdown_service";

const std::vector<Manifest>& GetTestManifests() {
  static base::NoDestructor<std::vector<Manifest>> manifests{
      {ManifestBuilder()
           .WithServiceName(kShutdownClientName)
           .WithOptions(ManifestOptionsBuilder()
                            .WithExecutionMode(
                                Manifest::ExecutionMode::kStandaloneExecutable)
                            .WithSandboxType("none")
                            .Build())
           .ExposeCapability(
               kClientControllerCapability,
               Manifest::InterfaceList<mojom::ShutdownTestClientController>())
           .RequireCapability(kShutdownServiceName, kShutdownServiceCapability)
           .RequireCapability(mojom::kServiceName,
                              "service_manager:service_manager")
           .Build(),
       ManifestBuilder()
           .WithServiceName(kShutdownServiceName)
           .WithOptions(ManifestOptionsBuilder()
                            .WithExecutionMode(
                                Manifest::ExecutionMode::kStandaloneExecutable)
                            .WithSandboxType("none")
                            .Build())
           .ExposeCapability(
               kShutdownServiceCapability,
               Manifest::InterfaceList<mojom::ShutdownTestService>())
           .RequireCapability(mojom::kServiceName,
                              "service_manager:service_manager")
           .Build(),
       ManifestBuilder()
           .WithServiceName(kTestServiceName)
           .RequireCapability(kShutdownServiceName, kShutdownServiceCapability)
           .RequireCapability(mojom::kServiceName,
                              "service_manager:service_manager")
           .RequireCapability(kShutdownClientName, kClientControllerCapability)
           .Build()}};
  return *manifests;
}

class ShutdownTest : public testing::Test {
 public:
  ShutdownTest()
      : test_service_manager_(GetTestManifests()),
        test_service_receiver_(
            &test_service_,
            test_service_manager_.RegisterTestInstance(kTestServiceName)) {}

  ShutdownTest(const ShutdownTest&) = delete;
  ShutdownTest& operator=(const ShutdownTest&) = delete;

  ~ShutdownTest() override = default;

  Connector* connector() { return test_service_receiver_.GetConnector(); }

 private:
  base::test::TaskEnvironment task_environment_;
  TestServiceManager test_service_manager_;
  Service test_service_;
  ServiceReceiver test_service_receiver_;
};

TEST_F(ShutdownTest, ConnectRace) {
  // This test exercises a number of potential shutdown races that can lead to
  // client deadlock if any of various parts of the EDK or service manager are
  // not
  // working as intended.

  mojo::Remote<mojom::ShutdownTestClientController> control;
  connector()->Connect(kShutdownClientName,
                       control.BindNewPipeAndPassReceiver());

  // Connect to shutdown_service and immediately request that it shut down.
  mojo::Remote<mojom::ShutdownTestService> service;
  connector()->Connect(kShutdownServiceName,
                       service.BindNewPipeAndPassReceiver());
  service->ShutDown();

  // Tell shutdown_client to connect to an interface on shutdown_service and
  // then block waiting for the interface pipe to signal something. If anything
  // goes wrong, its pipe won't signal and the client process will hang without
  // responding to this request.
  base::RunLoop loop;
  control->ConnectAndWait(loop.QuitClosure());
  loop.Run();
}

}  // namespace
}  // namespace service_manager

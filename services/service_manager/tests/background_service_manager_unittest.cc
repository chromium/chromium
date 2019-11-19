// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/background_service_manager.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/tests/background.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace service_manager {
namespace {

const char kTestName[] = "background_service_manager_unittest";
const char kAppName[] = "background_service_manager_test_service";

const char kTestServiceCapability[] = "test_service";

const std::vector<Manifest>& GetTestManifests() {
  static base::NoDestructor<std::vector<Manifest>> manifests{
      {ManifestBuilder()
           .WithServiceName(kTestName)
           .RequireCapability(kAppName, kTestServiceCapability)
           .Build(),
       service_manager::ManifestBuilder()
           .WithServiceName(kAppName)
           .WithOptions(ManifestOptionsBuilder()
                            .WithExecutionMode(
                                Manifest::ExecutionMode::kStandaloneExecutable)
                            .WithSandboxType("none")
                            .Build())
           .ExposeCapability(kTestServiceCapability,
                             Manifest::InterfaceList<mojom::TestService>())
           .Build()}};
  return *manifests;
}

// The parent unit test suite service, not the underlying test service.
class ServiceImpl : public Service {
 public:
  explicit ServiceImpl(mojom::ServiceRequest request)
      : binding_(this, std::move(request)) {}
  ~ServiceImpl() override = default;

  Connector* connector() { return binding_.GetConnector(); }

 private:
  ServiceBinding binding_;

  DISALLOW_COPY_AND_ASSIGN(ServiceImpl);
};

void SetFlagAndRunClosure(bool* flag, base::OnceClosure closure) {
  *flag = true;
  std::move(closure).Run();
}

// Uses BackgroundServiceManager to start the service manager in the background
// and connects to background_service_manager_test_service, verifying we can
// send a message to the service.
#if defined(OS_ANDROID)
// TODO(crbug.com/589784): This test is disabled, as it fails
// on the Android GN bot.
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif
TEST(BackgroundServiceManagerTest, MAYBE_Basic) {
  base::test::TaskEnvironment task_environment;
  BackgroundServiceManager background_service_manager(GetTestManifests());
  mojom::ServicePtr service;
  ServiceImpl service_impl(mojo::MakeRequest(&service));
  background_service_manager.RegisterService(
      Identity(kTestName, kSystemInstanceGroup, base::Token{},
               base::Token::CreateRandom()),
      service.PassInterface(), mojo::NullReceiver() /* metadata_receiver */);

  mojo::Remote<mojom::TestService> test_service;
  service_impl.connector()->Connect(ServiceFilter::ByName(kAppName),
                                    test_service.BindNewPipeAndPassReceiver());
  base::RunLoop run_loop;
  bool got_result = false;
  test_service->Test(base::BindOnce(&SetFlagAndRunClosure, &got_result,
                                    run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(got_result);
}

}  // namespace
}  // namespace service_manager

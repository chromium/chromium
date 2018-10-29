// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/test_suite.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/service_manager/tests/connect/connect_test.mojom.h"
#include "services/service_manager/tests/util.h"

// Tests that multiple services can be packaged in a single service by
// implementing ServiceFactory; that these services can be specified by
// the package's manifest and are thus registered with the PackageManager.

namespace service_manager {

namespace {

const char kTestPackageName[] = "connect_test_package";
const char kTestAppName[] = "connect_test_app";
const char kTestAppAName[] = "connect_test_a";
const char kTestAppBName[] = "connect_test_b";
const char kTestNonexistentAppName[] = "connect_test_nonexistent_app";
const char kTestSandboxedAppName[] = "connect_test_sandboxed_app";
const char kTestClassAppName[] = "connect_test_class_app";
const char kTestSingletonAppName[] = "connect_test_singleton_app";

void ReceiveOneString(std::string* out_string,
                      base::RunLoop* loop,
                      const std::string& in_string) {
  *out_string = in_string;
  loop->Quit();
}

void ReceiveTwoStrings(std::string* out_string_1,
                       std::string* out_string_2,
                       base::RunLoop* loop,
                       const std::string& in_string_1,
                       const std::string& in_string_2) {
  *out_string_1 = in_string_1;
  *out_string_2 = in_string_2;
  loop->Quit();
}

void ReceiveQueryResult(mojom::ConnectResult* out_result,
                        std::string* out_string,
                        base::RunLoop* loop,
                        mojom::ConnectResult in_result,
                        const std::string& in_string) {
  *out_result = in_result;
  *out_string = in_string;
  loop->Quit();
}

void ReceiveConnectionResult(mojom::ConnectResult* out_result,
                             Identity* out_target,
                             base::RunLoop* loop,
                             int32_t in_result,
                             const service_manager::Identity& in_identity) {
  *out_result = static_cast<mojom::ConnectResult>(in_result);
  *out_target = in_identity;
  loop->Quit();
}

void StartServiceResponse(base::RunLoop* quit_loop,
                          mojom::ConnectResult* out_result,
                          Identity* out_resolved_identity,
                          mojom::ConnectResult result,
                          const Identity& resolved_identity) {
  if (quit_loop)
    quit_loop->Quit();
  if (out_result)
    *out_result = result;
  if (out_resolved_identity)
    *out_resolved_identity = resolved_identity;
}

void QuitLoop(base::RunLoop* loop) {
  loop->Quit();
}

}  // namespace

class ConnectTest : public test::ServiceTest,
                    public test::mojom::ExposedInterface {
 public:
  ConnectTest() : ServiceTest("connect_unittests") {}
  ~ConnectTest() override {}

 protected:
  void CompareConnectionState(
      const std::string& connection_local_name,
      const std::string& connection_remote_name,
      const std::string& connection_remote_userid,
      const std::string& initialize_local_name,
      const std::string& initialize_userid) {
    EXPECT_EQ(connection_remote_name,
              connection_state_->connection_remote_name);
    EXPECT_EQ(connection_remote_userid,
              connection_state_->connection_remote_userid);
    EXPECT_EQ(initialize_local_name, connection_state_->initialize_local_name);
    EXPECT_EQ(initialize_userid, connection_state_->initialize_userid);
  }

 private:
  class TestService : public test::ServiceTestClient {
   public:
    explicit TestService(ConnectTest* connect_test)
        : test::ServiceTestClient(connect_test), connect_test_(connect_test) {
      registry_.AddInterface<test::mojom::ExposedInterface>(
          base::Bind(&ConnectTest::Create, base::Unretained(connect_test_)));
    }
    ~TestService() override {}

   private:
    void OnBindInterface(
        const BindSourceInfo& source_info,
        const std::string& interface_name,
        mojo::ScopedMessagePipeHandle interface_pipe) override {
      registry_.BindInterface(interface_name, std::move(interface_pipe));
    }

    ConnectTest* connect_test_;
    BinderRegistry registry_;

    DISALLOW_COPY_AND_ASSIGN(TestService);
  };

  // test::ServiceTest:
  void SetUp() override {
    test::ServiceTest::SetUp();
    // We need to connect to the package first to force the service manager to
    // read the
    // package app's manifest and register aliases for the applications it
    // provides.
    test::mojom::ConnectTestServicePtr root_service;
    connector()->BindInterface(kTestPackageName, &root_service);
    base::RunLoop run_loop;
    std::string root_name;
    root_service->GetTitle(
        base::Bind(&ReceiveOneString, &root_name, &run_loop));
    run_loop.Run();
  }
  std::unique_ptr<Service> CreateService() override {
    return std::make_unique<TestService>(this);
  }

  void Create(test::mojom::ExposedInterfaceRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  void ConnectionAccepted(test::mojom::ConnectionStatePtr state) override {
    connection_state_ = std::move(state);
  }

  test::mojom::ConnectionStatePtr connection_state_;

  mojo::BindingSet<test::mojom::ExposedInterface> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ConnectTest);
};

// Ensure the connection was properly established and that a round trip
// method call/response is completed.
TEST_F(ConnectTest, BindInterface) {
  test::mojom::ConnectTestServicePtr service;
  connector()->BindInterface(kTestAppName, &service);
  base::RunLoop run_loop;
  std::string title;
  service->GetTitle(base::Bind(&ReceiveOneString, &title, &run_loop));
  run_loop.Run();
  EXPECT_EQ("APP", title);
}

TEST_F(ConnectTest, Instances) {
  Identity identity_a(kTestAppName, mojom::kInheritUserID, "A");
  std::string instance_a1, instance_a2;
  test::mojom::ConnectTestServicePtr service_a1;
  {
    connector()->BindInterface(identity_a, &service_a1);
    base::RunLoop loop;
    service_a1->GetInstance(base::Bind(&ReceiveOneString, &instance_a1, &loop));
    loop.Run();
  }
  test::mojom::ConnectTestServicePtr service_a2;
  {
    connector()->BindInterface(identity_a, &service_a2);
    base::RunLoop loop;
    service_a2->GetInstance(base::Bind(&ReceiveOneString, &instance_a2, &loop));
    loop.Run();
  }
  EXPECT_EQ(instance_a1, instance_a2);

  Identity identity_b(kTestAppName, mojom::kInheritUserID, "B");
  std::string instance_b;
  test::mojom::ConnectTestServicePtr service_b;
  {
    connector()->BindInterface(identity_b, &service_b);
    base::RunLoop loop;
    service_b->GetInstance(base::Bind(&ReceiveOneString, &instance_b, &loop));
    loop.Run();
  }

  EXPECT_NE(instance_a1, instance_b);
}

TEST_F(ConnectTest, QueryService) {
  mojom::ConnectResult result;
  std::string sandbox_type;
  base::RunLoop run_loop;
  connector()->QueryService(
      Identity(kTestSandboxedAppName, mojom::kInheritUserID, "A"),
      base::BindOnce(&ReceiveQueryResult, &result, &sandbox_type, &run_loop));
  run_loop.Run();
  EXPECT_EQ(mojom::ConnectResult::SUCCEEDED, result);
  EXPECT_EQ("superduper", sandbox_type);
}

TEST_F(ConnectTest, QueryNonexistentService) {
  mojom::ConnectResult result;
  std::string sandbox_type;
  base::RunLoop run_loop;
  connector()->QueryService(
      Identity(kTestNonexistentAppName, mojom::kInheritUserID, "A"),
      base::BindOnce(&ReceiveQueryResult, &result, &sandbox_type, &run_loop));
  run_loop.Run();
  EXPECT_EQ(mojom::ConnectResult::INVALID_ARGUMENT, result);
  EXPECT_EQ("", sandbox_type);
}

#if DCHECK_IS_ON()
// This test triggers intentional DCHECKs but is not suitable for death testing.
#define MAYBE_BlockedInterface DISABLED_BlockedInterface
#else
#define MAYBE_BlockedInterface BlockedInterface
#endif

// BlockedInterface should not be exposed to this application because it is not
// in our CapabilityFilter whitelist.
TEST_F(ConnectTest, MAYBE_BlockedInterface) {
  base::RunLoop run_loop;
  test::mojom::BlockedInterfacePtr blocked;
  connector()->BindInterface(kTestAppName, &blocked);
  blocked.set_connection_error_handler(base::Bind(&QuitLoop, &run_loop));
  std::string title = "unchanged";
  blocked->GetTitleBlocked(base::Bind(&ReceiveOneString, &title, &run_loop));
  run_loop.Run();
  EXPECT_EQ("unchanged", title);
}

// Connects to an app provided by a package.
TEST_F(ConnectTest, PackagedApp) {
  test::mojom::ConnectTestServicePtr service_a;
  connector()->BindInterface(kTestAppAName, &service_a);
  Connector::TestApi test_api(connector());
  Identity resolved_identity;
  test_api.SetStartServiceCallback(
      base::Bind(&StartServiceResponse, nullptr, nullptr, &resolved_identity));
  base::RunLoop run_loop;
  std::string a_name;
  service_a->GetTitle(base::Bind(&ReceiveOneString, &a_name, &run_loop));
  run_loop.Run();
  EXPECT_EQ("A", a_name);
  EXPECT_EQ(resolved_identity.name(), kTestAppAName);
}

#if DCHECK_IS_ON()
// This test triggers intentional DCHECKs but is not suitable for death testing.
#define MAYBE_BlockedPackage DISABLED_BlockedPackage
#else
#define MAYBE_BlockedPackage BlockedPackage
#endif

// Ask the target application to attempt to connect to a third application
// provided by a package whose id is permitted by the primary target's
// CapabilityFilter but whose package is not. The connection should be
// allowed regardless of the target's CapabilityFilter with respect to the
// package.
TEST_F(ConnectTest, MAYBE_BlockedPackage) {
  test::mojom::StandaloneAppPtr standalone_app;
  connector()->BindInterface(kTestAppName, &standalone_app);
  base::RunLoop run_loop;
  std::string title;
  standalone_app->ConnectToAllowedAppInBlockedPackage(
      base::Bind(&ReceiveOneString, &title, &run_loop));
  run_loop.Run();
  EXPECT_EQ("A", title);
}

#if DCHECK_IS_ON()
// This test triggers intentional DCHECKs but is not suitable for death testing.
#define MAYBE_PackagedApp_BlockedInterface DISABLED_PackagedApp_BlockedInterface
#else
#define MAYBE_PackagedApp_BlockedInterface PackagedApp_BlockedInterface
#endif

// BlockedInterface should not be exposed to this application because it is not
// in our CapabilityFilter whitelist.
TEST_F(ConnectTest, MAYBE_PackagedApp_BlockedInterface) {
  base::RunLoop run_loop;
  test::mojom::BlockedInterfacePtr blocked;
  connector()->BindInterface(kTestAppAName, &blocked);
  blocked.set_connection_error_handler(base::Bind(&QuitLoop, &run_loop));
  run_loop.Run();
}

#if DCHECK_IS_ON()
// This test triggers intentional DCHECKs but is not suitable for death testing.
#define MAYBE_BlockedPackagedApplication DISABLED_BlockedPackagedApplication
#else
#define MAYBE_BlockedPackagedApplication BlockedPackagedApplication
#endif

// Connection to another application provided by the same package, blocked
// because it's not in the capability filter whitelist.
TEST_F(ConnectTest, MAYBE_BlockedPackagedApplication) {
  test::mojom::ConnectTestServicePtr service_b;
  connector()->BindInterface(kTestAppBName, &service_b);
  Connector::TestApi test_api(connector());
  mojom::ConnectResult result;
  test_api.SetStartServiceCallback(
      base::Bind(&StartServiceResponse, nullptr, &result, nullptr));
  base::RunLoop run_loop;
  service_b.set_connection_error_handler(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(mojom::ConnectResult::ACCESS_DENIED, result);
}

TEST_F(ConnectTest, CapabilityClasses) {
  test::mojom::StandaloneAppPtr standalone_app;
  connector()->BindInterface(kTestAppName, &standalone_app);
  std::string string1, string2;
  base::RunLoop loop;
  standalone_app->ConnectToClassInterface(
      base::Bind(&ReceiveTwoStrings, &string1, &string2, &loop));
  loop.Run();
  EXPECT_EQ("PONG", string1);
  EXPECT_EQ("CLASS APP", string2);
}

#if DCHECK_IS_ON()
// This test triggers intentional DCHECKs but is not suitable for death testing.
#define MAYBE_ConnectWithoutExplicitClassBlocked \
  DISABLED_ConnectWithoutExplicitClassBlocked
#else
#define MAYBE_ConnectWithoutExplicitClassBlocked \
  ConnectWithoutExplicitClassBlocked
#endif

TEST_F(ConnectTest, MAYBE_ConnectWithoutExplicitClassBlocked) {
  // We not be able to bind a ClassInterfacePtr since the connect_unittest app
  // does not explicitly request the "class" capability from
  // connect_test_class_app. This test will hang if it is bound.
  test::mojom::ClassInterfacePtr class_interface;
  connector()->BindInterface(kTestClassAppName, &class_interface);
  base::RunLoop loop;
  class_interface.set_connection_error_handler(base::Bind(&QuitLoop, &loop));
  loop.Run();
}

TEST_F(ConnectTest, ConnectAsDifferentUser_Allowed) {
  test::mojom::IdentityTestPtr identity_test;
  connector()->BindInterface(kTestAppName, &identity_test);
  mojom::ConnectResult result;
  Identity target(kTestClassAppName, base::GenerateGUID());
  Identity result_identity;
  {
    base::RunLoop loop;
    identity_test->ConnectToClassAppWithIdentity(
        target,
        base::Bind(&ReceiveConnectionResult, &result, &result_identity, &loop));
    loop.Run();
  }
  EXPECT_EQ(result, mojom::ConnectResult::SUCCEEDED);
  EXPECT_EQ(target, result_identity);
}

TEST_F(ConnectTest, ConnectAsDifferentUser_Blocked) {
  test::mojom::IdentityTestPtr identity_test;
  connector()->BindInterface(kTestAppAName, &identity_test);
  mojom::ConnectResult result;
  Identity target(kTestClassAppName, base::GenerateGUID());
  Identity result_identity;
  {
    base::RunLoop loop;
    identity_test->ConnectToClassAppWithIdentity(
        target,
        base::Bind(&ReceiveConnectionResult, &result, &result_identity, &loop));
    loop.Run();
  }
  EXPECT_EQ(mojom::ConnectResult::ACCESS_DENIED, result);
  EXPECT_FALSE(target == result_identity);
}

TEST_F(ConnectTest, ConnectWithDifferentInstanceName_Blocked) {
  test::mojom::IdentityTestPtr identity_test;
  connector()->BindInterface(kTestAppAName, &identity_test);

  mojom::ConnectResult result;
  Identity target(kTestClassAppName, mojom::kInheritUserID,
                  base::GenerateGUID());
  Identity result_identity;
  base::RunLoop loop;
  identity_test->ConnectToClassAppWithIdentity(
      target, base::BindRepeating(&ReceiveConnectionResult, &result,
                                  &result_identity, &loop));
  loop.Run();
  EXPECT_EQ(mojom::ConnectResult::ACCESS_DENIED, result);
  EXPECT_FALSE(target == result_identity);
}

// There are various other tests (service manager, lifecycle) that test valid
// client
// process specifications. This is the only one for blocking.
TEST_F(ConnectTest, ConnectToClientProcess_Blocked) {
  base::Process process;
  mojom::ConnectResult result =
      service_manager::test::LaunchAndConnectToProcess(
#if defined(OS_WIN)
          "connect_test_exe.exe",
#else
          "connect_test_exe",
#endif
          service_manager::Identity("connect_test_exe",
                                    service_manager::mojom::kInheritUserID),
          connector(), &process);
  EXPECT_EQ(result, mojom::ConnectResult::ACCESS_DENIED);
}

// Verifies that a client with the "shared_instance_across_users" value of
// "instance_sharing" option can receive connections from clients run as other
// users.
TEST_F(ConnectTest, AllUsersSingleton) {
  // Connect to an instance with an explicitly different user_id. This supplied
  // user id should be ignored by the service manager (which will generate its
  // own synthetic user id for all-user singleton instances).
  const std::string singleton_userid = base::GenerateGUID();
  Identity singleton_id(kTestSingletonAppName, singleton_userid);
  connector()->StartService(singleton_id);
  Identity first_resolved_identity;
  {
    base::RunLoop loop;
    Connector::TestApi test_api(connector());
    test_api.SetStartServiceCallback(base::Bind(
        &StartServiceResponse, &loop, nullptr, &first_resolved_identity));
    loop.Run();
    EXPECT_NE(first_resolved_identity.user_id(), singleton_userid);
  }
  // This connects using the current client's user_id. It should be bound to the
  // same service started above, with the same service manager-generated user
  // id.
  connector()->StartService(kTestSingletonAppName);
  {
    base::RunLoop loop;
    Connector::TestApi test_api(connector());
    Identity resolved_identity;
    test_api.SetStartServiceCallback(
        base::Bind(&StartServiceResponse, &loop, nullptr, &resolved_identity));
    loop.Run();
    EXPECT_EQ(resolved_identity.user_id(), first_resolved_identity.user_id());
  }
}

}  // namespace service_manager

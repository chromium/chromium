// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_TEST_TEST_SERVICE_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_TEST_TEST_SERVICE_H_

#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_receiver.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace service_manager {

// A very simple test helper for unit tests which want to use a real Service
// Manager instance and have each test behave like a unique service instance
// that can connect to any of various services under test.
//
// Typical usage is paired with ServiceManager::RegisterTestInstance, for
// example:
//
//   class MyTest : public testing::Test {
//    public:
//     MyTest()
//         : test_service_(
//               test_service_manager_.RegisterTestInstance("foo_unittests")) {}
//
//     service_manager::Connector* connector() {
//       return test_service_.connector();
//     }
//
//    private:
//     base::test::TaskEnvironment task_environment_;
//     service_manager::TestServiceManager test_service_manager_;
//     service_manager::TestService test_service_;
//   };
//
//   TEST_F(MyTest, ConnectToFoo) {
//     mojo::Remote<foo::mojom::Foo> foo;
//     connector()->BindInterface("foo", foo.BindNewPipeAndPassReceiver());
//     foo->DoSomeStuff();
//     // etc...
//   }
class TestService : public Service {
 public:
  explicit TestService(mojo::PendingReceiver<mojom::Service> receiver);

  TestService(const TestService&) = delete;
  TestService& operator=(const TestService&) = delete;

  ~TestService() override;

  Connector* connector() { return receiver_.GetConnector(); }

 private:
  ServiceReceiver receiver_;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_TEST_TEST_SERVICE_H_

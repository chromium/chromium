// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_TEST_TEST_SERVICE_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_TEST_TEST_SERVICE_H_

#include "base/macros.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
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
//     foo::mojom::FooPtr foo;
//     connector()->BindInterface("foo", mojo::MakeRequest(&foo));
//     foo->DoSomeStuff();
//     // etc...
//   }
class TestService : public Service {
 public:
  explicit TestService(mojom::ServiceRequest request);
  ~TestService() override;

  Connector* connector() { return binding_.GetConnector(); }

 private:
  ServiceBinding binding_;

  DISALLOW_COPY_AND_ASSIGN(TestService);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_TEST_TEST_SERVICE_H_

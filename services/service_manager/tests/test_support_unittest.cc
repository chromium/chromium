// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "services/service_manager/tests/test_support.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace service_manager {

namespace {

// TestBImpl and TestCImpl are simple test interfaces whose methods invokes
// their callback when called without doing anything.
class TestBImpl : public mojom::TestB {
 public:
  TestBImpl() = default;
  ~TestBImpl() override = default;

 private:
  // TestB:
  void B(BCallback callback) override { std::move(callback).Run(); }
  void CallC(CallCCallback callback) override { std::move(callback).Run(); }

  DISALLOW_COPY_AND_ASSIGN(TestBImpl);
};

class TestCImpl : public mojom::TestC {
 public:
  TestCImpl() = default;
  ~TestCImpl() override = default;

 private:
  // TestC:
  void C(CCallback callback) override { std::move(callback).Run(); }

  DISALLOW_COPY_AND_ASSIGN(TestCImpl);
};

void OnTestBRequest(mojom::TestBRequest request) {
  mojo::MakeStrongBinding(std::make_unique<TestBImpl>(), std::move(request));
}

void OnTestCRequest(mojom::TestCRequest request) {
  mojo::MakeStrongBinding(std::make_unique<TestCImpl>(), std::move(request));
}

class TestBServiceImpl : public Service {
 public:
  TestBServiceImpl(mojom::ServiceRequest request)
      : service_binding_(this, std::move(request)) {
    registry_.AddInterface(base::BindRepeating(&OnTestBRequest));
  }

  ~TestBServiceImpl() override = default;

 private:
  // Service:
  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  service_manager::ServiceBinding service_binding_;
  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(TestBServiceImpl);
};

class TestCServiceImpl : public Service {
 public:
  TestCServiceImpl(mojom::ServiceRequest request)
      : service_binding_(this, std::move(request)) {
    registry_.AddInterface(base::BindRepeating(&OnTestCRequest));
  }

  ~TestCServiceImpl() override = default;

 private:
  // Service:
  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  service_manager::ServiceBinding service_binding_;
  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(TestCServiceImpl);
};

constexpr char kServiceBName[] = "ServiceB";
constexpr char kServiceCName[] = "ServiceC";

}  // namespace

TEST(ServiceManagerTestSupport, TestConnectorFactoryUniqueService) {
  base::test::TaskEnvironment task_environment;

  TestConnectorFactory factory;
  TestCServiceImpl c_service(factory.RegisterInstance(kServiceCName));
  auto* connector = factory.GetDefaultConnector();

  mojom::TestCPtr c;
  connector->BindInterface(kServiceCName, &c);
  base::RunLoop loop;
  c->C(loop.QuitClosure());
  loop.Run();
}

TEST(ServiceManagerTestSupport, TestConnectorFactoryMultipleServices) {
  base::test::TaskEnvironment task_environment;

  TestConnectorFactory factory;
  TestBServiceImpl b_service(factory.RegisterInstance(kServiceBName));
  TestCServiceImpl c_service(factory.RegisterInstance(kServiceCName));
  auto* connector = factory.GetDefaultConnector();

  {
    mojom::TestBPtr b;
    connector->BindInterface(kServiceBName, &b);
    base::RunLoop loop;
    b->B(loop.QuitClosure());
    loop.Run();
  }

  {
    mojom::TestCPtr c;
    connector->BindInterface(kServiceCName, &c);
    base::RunLoop loop;
    c->C(loop.QuitClosure());
    loop.Run();
  }
}

}  // namespace service_manager

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/service_factory.h"

#include <optional>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/tests/service_factory_unittest.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace service_factory_unittest {

class ServiceFactoryTest : public testing::Test {
 public:
  ServiceFactoryTest() = default;

  ServiceFactoryTest(const ServiceFactoryTest&) = delete;
  ServiceFactoryTest& operator=(const ServiceFactoryTest&) = delete;

 private:
  base::test::TaskEnvironment task_environment_;
};

class TestService1Impl : public mojom::TestService1 {
 public:
  explicit TestService1Impl(PendingReceiver<mojom::TestService1> receiver)
      : receiver_(this, std::move(receiver)) {
    ++num_instances_;
  }

  TestService1Impl(const TestService1Impl&) = delete;
  TestService1Impl& operator=(const TestService1Impl&) = delete;

  ~TestService1Impl() override {
    --num_instances_;
  }

  static int num_instances() { return num_instances_; }

 private:
  // mojom::TestService1:
  void GetIdentity(GetIdentityCallback callback) override {
    std::move(callback).Run(1);
  }

  void Quit() override { receiver_.reset(); }

  Receiver<mojom::TestService1> receiver_;
  static int num_instances_;
};

class TestService2Impl : public mojom::TestService2 {
 public:
  explicit TestService2Impl(PendingReceiver<mojom::TestService2> receiver)
      : receiver_(this, std::move(receiver)) {}

  TestService2Impl(const TestService2Impl&) = delete;
  TestService2Impl& operator=(const TestService2Impl&) = delete;

  ~TestService2Impl() override = default;

 private:
  // mojom::TestService2:
  void GetIdentity(GetIdentityCallback callback) override {
    std::move(callback).Run(2);
  }

  Receiver<mojom::TestService2> receiver_;
};

int TestService1Impl::num_instances_ = 0;

auto RunTestService1(PendingReceiver<mojom::TestService1> receiver) {
  return std::make_unique<TestService1Impl>(std::move(receiver));
}

auto RunTestService2(PendingReceiver<mojom::TestService2> receiver) {
  return std::make_unique<TestService2Impl>(std::move(receiver));
}

TEST_F(ServiceFactoryTest, BasicMatching) {
  ServiceFactory factory;
  factory.Add(RunTestService1);
  factory.Add(RunTestService2);

  Remote<mojom::TestService1> remote1;
  GenericPendingReceiver receiver = remote1.BindNewPipeAndPassReceiver();
  EXPECT_TRUE(factory.RunService(std::move(receiver), base::NullCallback()));
  EXPECT_FALSE(receiver.is_valid());

  // Verify that we connected to an instance of TestService1.
  int32_t id = 0;
  EXPECT_TRUE(remote1->GetIdentity(&id));
  EXPECT_EQ(1, id);

  Remote<mojom::TestService2> remote2;
  receiver = remote2.BindNewPipeAndPassReceiver();
  EXPECT_TRUE(factory.RunService(std::move(receiver), base::NullCallback()));
  EXPECT_FALSE(receiver.is_valid());

  // Verify that we connected to an instance of TestService2.
  EXPECT_TRUE(remote2->GetIdentity(&id));
  EXPECT_EQ(2, id);

  Remote<mojom::TestService3> remote3;
  receiver = remote3.BindNewPipeAndPassReceiver();
  EXPECT_FALSE(factory.CanRunService(receiver));
  EXPECT_FALSE(factory.RunService(std::move(receiver), base::NullCallback()));
  EXPECT_FALSE(receiver.is_valid());
}

TEST_F(ServiceFactoryTest, DestroyInstanceOnClientDisconnect) {
  ServiceFactory factory;
  factory.Add(RunTestService1);

  base::RunLoop loop1;
  base::OnceClosure quit1 = loop1.QuitClosure();
  Remote<mojom::TestService1> remote1;
  GenericPendingReceiver receiver = remote1.BindNewPipeAndPassReceiver();
  EXPECT_TRUE(factory.RunService(std::move(receiver), std::move(quit1)));

  base::RunLoop loop2;
  base::OnceClosure quit2 = loop2.QuitClosure();
  Remote<mojom::TestService1> remote2;
  receiver = remote2.BindNewPipeAndPassReceiver();
  EXPECT_TRUE(factory.RunService(std::move(receiver), std::move(quit2)));

  remote1.FlushForTesting();
  remote2.FlushForTesting();
  EXPECT_EQ(2, TestService1Impl::num_instances());

  remote1.reset();
  loop1.Run();
  EXPECT_EQ(1, TestService1Impl::num_instances());

  remote2.FlushForTesting();
  EXPECT_EQ(1, TestService1Impl::num_instances());

  remote2.reset();
  loop2.Run();
  EXPECT_EQ(0, TestService1Impl::num_instances());
}

TEST_F(ServiceFactoryTest, DestroyInstanceOnServiceDisconnect) {
  ServiceFactory factory;
  factory.Add(RunTestService1);

  base::RunLoop loop;
  base::OnceClosure quit = loop.QuitClosure();
  Remote<mojom::TestService1> remote;
  GenericPendingReceiver receiver = remote.BindNewPipeAndPassReceiver();
  EXPECT_TRUE(factory.RunService(std::move(receiver), std::move(quit)));

  remote.FlushForTesting();
  EXPECT_EQ(1, TestService1Impl::num_instances());
  remote->Quit();
  loop.Run();
  EXPECT_EQ(0, TestService1Impl::num_instances());
}

TEST_F(ServiceFactoryTest, DestroyInstancesOnFactoryDestruction) {
  std::optional<ServiceFactory> factory{std::in_place};
  factory->Add(RunTestService1);

  Remote<mojom::TestService1> remote1;
  GenericPendingReceiver receiver = remote1.BindNewPipeAndPassReceiver();
  EXPECT_TRUE(factory->RunService(std::move(receiver), base::NullCallback()));
  remote1.FlushForTesting();
  EXPECT_EQ(1, TestService1Impl::num_instances());

  Remote<mojom::TestService1> remote2;
  receiver = remote2.BindNewPipeAndPassReceiver();
  EXPECT_TRUE(factory->RunService(std::move(receiver), base::NullCallback()));
  remote2.FlushForTesting();
  EXPECT_EQ(2, TestService1Impl::num_instances());

  factory.reset();
  EXPECT_EQ(0, TestService1Impl::num_instances());

  remote1.FlushForTesting();
  remote2.FlushForTesting();
  EXPECT_FALSE(remote1.is_connected());
  EXPECT_FALSE(remote2.is_connected());
}

}  // namespace service_factory_unittest
}  // namespace test
}  // namespace mojo

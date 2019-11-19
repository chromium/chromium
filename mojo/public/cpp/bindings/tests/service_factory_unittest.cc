// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/service_factory.h"

#include "base/callback.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
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

 private:
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ServiceFactoryTest);
};

class TestService1Impl : public mojom::TestService1 {
 public:
  explicit TestService1Impl(PendingReceiver<mojom::TestService1> receiver)
      : receiver_(this, std::move(receiver)) {
    ++num_instances_;
  }

  ~TestService1Impl() override {
    --num_instances_;
    if (destruction_wait_loop_)
      destruction_wait_loop_->Quit();
  }

  static int num_instances() { return num_instances_; }

  static void WaitForInstanceDestruction() {
    static base::NoDestructor<base::Optional<base::RunLoop>> loop;
    loop->emplace();
    destruction_wait_loop_ = &loop->value();
    (*loop)->Run();
    destruction_wait_loop_ = nullptr;
  }

 private:
  // mojom::TestService1:
  void GetIdentity(GetIdentityCallback callback) override {
    std::move(callback).Run(1);
  }

  void Quit() override { receiver_.reset(); }

  Receiver<mojom::TestService1> receiver_;
  static int num_instances_;
  static base::RunLoop* destruction_wait_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestService1Impl);
};

class TestService2Impl : public mojom::TestService2 {
 public:
  explicit TestService2Impl(PendingReceiver<mojom::TestService2> receiver)
      : receiver_(this, std::move(receiver)) {}

  ~TestService2Impl() override = default;

 private:
  // mojom::TestService2:
  void GetIdentity(GetIdentityCallback callback) override {
    std::move(callback).Run(2);
  }

  Receiver<mojom::TestService2> receiver_;

  DISALLOW_COPY_AND_ASSIGN(TestService2Impl);
};

int TestService1Impl::num_instances_ = 0;
base::RunLoop* TestService1Impl::destruction_wait_loop_ = nullptr;

auto RunTestService1(PendingReceiver<mojom::TestService1> receiver) {
  return std::make_unique<TestService1Impl>(std::move(receiver));
}

auto RunTestService2(PendingReceiver<mojom::TestService2> receiver) {
  return std::make_unique<TestService2Impl>(std::move(receiver));
}

TEST_F(ServiceFactoryTest, BasicMatching) {
  ServiceFactory factory{RunTestService1, RunTestService2};

  Remote<mojom::TestService1> remote1;
  GenericPendingReceiver receiver = remote1.BindNewPipeAndPassReceiver();
  EXPECT_TRUE(factory.MaybeRunService(&receiver));
  EXPECT_FALSE(receiver.is_valid());

  // Verify that we connected to an instance of TestService1.
  int32_t id = 0;
  EXPECT_TRUE(remote1->GetIdentity(&id));
  EXPECT_EQ(1, id);

  Remote<mojom::TestService2> remote2;
  receiver = remote2.BindNewPipeAndPassReceiver();
  EXPECT_TRUE(factory.MaybeRunService(&receiver));
  EXPECT_FALSE(receiver.is_valid());

  // Verify that we connected to an instance of TestService2.
  EXPECT_TRUE(remote2->GetIdentity(&id));
  EXPECT_EQ(2, id);

  Remote<mojom::TestService3> remote3;
  receiver = remote3.BindNewPipeAndPassReceiver();
  EXPECT_FALSE(factory.MaybeRunService(&receiver));
  EXPECT_TRUE(receiver.is_valid());
  EXPECT_TRUE(receiver.As<mojom::TestService3>());
}

TEST_F(ServiceFactoryTest, DestroyInstanceOnClientDisconnect) {
  ServiceFactory factory{RunTestService1};

  Remote<mojom::TestService1> remote1;
  GenericPendingReceiver receiver = remote1.BindNewPipeAndPassReceiver();
  EXPECT_TRUE(factory.MaybeRunService(&receiver));

  Remote<mojom::TestService1> remote2;
  receiver = remote2.BindNewPipeAndPassReceiver();
  EXPECT_TRUE(factory.MaybeRunService(&receiver));

  remote1.FlushForTesting();
  remote2.FlushForTesting();
  EXPECT_EQ(2, TestService1Impl::num_instances());

  remote1.reset();
  TestService1Impl::WaitForInstanceDestruction();
  EXPECT_EQ(1, TestService1Impl::num_instances());

  remote2.FlushForTesting();
  EXPECT_EQ(1, TestService1Impl::num_instances());

  remote2.reset();
  TestService1Impl::WaitForInstanceDestruction();
  EXPECT_EQ(0, TestService1Impl::num_instances());
}

TEST_F(ServiceFactoryTest, DestroyInstanceOnServiceDisconnect) {
  ServiceFactory factory{RunTestService1};

  Remote<mojom::TestService1> remote;
  GenericPendingReceiver receiver = remote.BindNewPipeAndPassReceiver();
  EXPECT_TRUE(factory.MaybeRunService(&receiver));

  remote.FlushForTesting();
  EXPECT_EQ(1, TestService1Impl::num_instances());
  remote->Quit();
  remote.FlushForTesting();
  EXPECT_EQ(0, TestService1Impl::num_instances());
}

TEST_F(ServiceFactoryTest, DestroyInstancesOnFactoryDestruction) {
  base::Optional<ServiceFactory> factory{base::in_place, RunTestService1};

  Remote<mojom::TestService1> remote1;
  GenericPendingReceiver receiver = remote1.BindNewPipeAndPassReceiver();
  EXPECT_TRUE(factory->MaybeRunService(&receiver));
  remote1.FlushForTesting();
  EXPECT_EQ(1, TestService1Impl::num_instances());

  Remote<mojom::TestService1> remote2;
  receiver = remote2.BindNewPipeAndPassReceiver();
  EXPECT_TRUE(factory->MaybeRunService(&receiver));
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

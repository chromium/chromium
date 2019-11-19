// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/mojo/services/deferred_destroy_strong_binding_set.h"
#include "mojo/public/interfaces/bindings/tests/ping_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

using PingService = mojo::test::PingService;

class DeferredDestroyPingImpl : public DeferredDestroy<PingService> {
 public:
  DeferredDestroyPingImpl() { ++instance_count; }
  ~DeferredDestroyPingImpl() override { --instance_count; }

  // DeferredDestroy<PingService> implementation.
  void Ping(PingCallback callback) override {}
  void OnDestroyPending(base::OnceClosure destroy_cb) override {
    destroy_cb_ = std::move(destroy_cb);
    if (can_destroy_)
      std::move(destroy_cb_).Run();
  }

  void set_can_destroy() {
    can_destroy_ = true;
    if (destroy_cb_)
      std::move(destroy_cb_).Run();
  }

  static int instance_count;

 private:
  bool can_destroy_ = false;
  base::OnceClosure destroy_cb_;
};
int DeferredDestroyPingImpl::instance_count = 0;

DeferredDestroyPingImpl* AddDeferredDestroyBinding(
    DeferredDestroyStrongBindingSet<PingService>* bindings,
    mojo::PendingRemote<PingService>* ptr) {
  auto impl = std::make_unique<DeferredDestroyPingImpl>();
  DeferredDestroyPingImpl* impl_ptr = impl.get();
  bindings->AddBinding(std::move(impl), ptr->InitWithNewPipeAndPassReceiver());
  return impl_ptr;
}

class DeferredDestroyStrongBindingSetTest : public testing::Test {
 public:
  DeferredDestroyStrongBindingSetTest() = default;
  ~DeferredDestroyStrongBindingSetTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(DeferredDestroyStrongBindingSetTest, Destructor) {
  mojo::PendingRemote<PingService> ping[2];
  auto bindings =
      std::make_unique<DeferredDestroyStrongBindingSet<PingService>>();

  for (int i = 0; i < 2; ++i)
    AddDeferredDestroyBinding(bindings.get(), ping + i);
  EXPECT_EQ(2, DeferredDestroyPingImpl::instance_count);

  bindings.reset();
  EXPECT_EQ(0, DeferredDestroyPingImpl::instance_count);
}

TEST_F(DeferredDestroyStrongBindingSetTest, ConnectionError) {
  mojo::PendingRemote<PingService> ping[4];
  DeferredDestroyPingImpl* impl[4];
  auto bindings =
      std::make_unique<DeferredDestroyStrongBindingSet<PingService>>();

  for (int i = 0; i < 4; ++i)
    impl[i] = AddDeferredDestroyBinding(bindings.get(), ping + i);
  EXPECT_EQ(4, DeferredDestroyPingImpl::instance_count);

  // Destroy deferred after connection error until set_can_destroy()..
  ping[0].reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, DeferredDestroyPingImpl::instance_count);
  impl[0]->set_can_destroy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, DeferredDestroyPingImpl::instance_count);

  // Destroyed immediately after connection error if set_can_destroy() in
  // advance.
  impl[1]->set_can_destroy();
  ping[1].reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, DeferredDestroyPingImpl::instance_count);

  // Deferred after connection until binding set destruction.
  ping[2].reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, DeferredDestroyPingImpl::instance_count);

  // Destructing the binding set will destruct all impls, including deferred
  // destroy impls.
  bindings.reset();
  EXPECT_EQ(0, DeferredDestroyPingImpl::instance_count);
}

TEST_F(DeferredDestroyStrongBindingSetTest, CloseAllBindings) {
  mojo::PendingRemote<PingService> ping[3];
  DeferredDestroyPingImpl* impl[3];
  DeferredDestroyStrongBindingSet<PingService> bindings;

  for (int i = 0; i < 2; ++i)
    impl[i] = AddDeferredDestroyBinding(&bindings, ping + i);
  EXPECT_EQ(2, DeferredDestroyPingImpl::instance_count);
  EXPECT_FALSE(bindings.empty());

  bindings.CloseAllBindings();
  EXPECT_EQ(0, DeferredDestroyPingImpl::instance_count);
  EXPECT_TRUE(bindings.empty());

  // After CloseAllBindings, new added bindings can still be deferred destroyed.
  impl[2] = AddDeferredDestroyBinding(&bindings, ping + 2);
  EXPECT_EQ(1, DeferredDestroyPingImpl::instance_count);

  ping[2].reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, DeferredDestroyPingImpl::instance_count);

  impl[2]->set_can_destroy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, DeferredDestroyPingImpl::instance_count);
}

}  // namespace
}  // namespace media

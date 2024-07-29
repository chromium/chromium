// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/mojo/services/deferred_destroy_unique_receiver_set.h"
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

DeferredDestroyPingImpl* AddDeferredDestroyReceiver(
    DeferredDestroyUniqueReceiverSet<PingService>* receivers,
    mojo::PendingRemote<PingService>* ptr) {
  auto impl = std::make_unique<DeferredDestroyPingImpl>();
  DeferredDestroyPingImpl* impl_ptr = impl.get();
  receivers->Add(std::move(impl), ptr->InitWithNewPipeAndPassReceiver());
  return impl_ptr;
}

class DeferredDestroyUniqueReceiverSetTest : public testing::Test {
 public:
  DeferredDestroyUniqueReceiverSetTest() = default;
  ~DeferredDestroyUniqueReceiverSetTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(DeferredDestroyUniqueReceiverSetTest, Destructor) {
  mojo::PendingRemote<PingService> ping[2];
  auto receivers =
      std::make_unique<DeferredDestroyUniqueReceiverSet<PingService>>();

  for (int i = 0; i < 2; ++i)
    AddDeferredDestroyReceiver(receivers.get(), ping + i);
  EXPECT_EQ(2, DeferredDestroyPingImpl::instance_count);

  receivers.reset();
  EXPECT_EQ(0, DeferredDestroyPingImpl::instance_count);
}

TEST_F(DeferredDestroyUniqueReceiverSetTest, ConnectionError) {
  mojo::PendingRemote<PingService> ping[4];
  DeferredDestroyPingImpl* impl[4];
  auto receivers =
      std::make_unique<DeferredDestroyUniqueReceiverSet<PingService>>();

  for (int i = 0; i < 4; ++i)
    impl[i] = AddDeferredDestroyReceiver(receivers.get(), ping + i);
  EXPECT_EQ(4, DeferredDestroyPingImpl::instance_count);

  // Destroy deferred after disconnection until set_can_destroy()..
  ping[0].reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, DeferredDestroyPingImpl::instance_count);
  impl[0]->set_can_destroy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, DeferredDestroyPingImpl::instance_count);

  // Destroyed immediately after disconnection if set_can_destroy() in
  // advance.
  impl[1]->set_can_destroy();
  ping[1].reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, DeferredDestroyPingImpl::instance_count);

  // Deferred after connection until receiver set destruction.
  ping[2].reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, DeferredDestroyPingImpl::instance_count);

  // Destructing the receiver set will destruct all impls, including deferred
  // destroy impls.
  receivers.reset();
  EXPECT_EQ(0, DeferredDestroyPingImpl::instance_count);
}

TEST_F(DeferredDestroyUniqueReceiverSetTest, CloseAllReceivers) {
  mojo::PendingRemote<PingService> ping[3];
  DeferredDestroyPingImpl* impl[3];
  DeferredDestroyUniqueReceiverSet<PingService> receivers;

  for (int i = 0; i < 2; ++i)
    impl[i] = AddDeferredDestroyReceiver(&receivers, ping + i);
  EXPECT_EQ(2, DeferredDestroyPingImpl::instance_count);
  EXPECT_FALSE(receivers.empty());

  receivers.CloseAllReceivers();
  EXPECT_EQ(0, DeferredDestroyPingImpl::instance_count);
  EXPECT_TRUE(receivers.empty());

  // After CloseAllReceivers, new added receivers can still be deferred
  // destroyed.
  impl[2] = AddDeferredDestroyReceiver(&receivers, ping + 2);
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

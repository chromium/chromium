// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/deferred_destroy_unique_receiver_set.h"

#include <array>
#include <cstdint>
#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/interfaces/bindings/tests/ping_service.test-mojom.h"
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
  std::array<mojo::PendingRemote<PingService>, 2> ping;
  auto receivers =
      std::make_unique<DeferredDestroyUniqueReceiverSet<PingService>>();

  for (int i = 0; i < 2; ++i)
    AddDeferredDestroyReceiver(
        receivers.get(), base::span<mojo::PendingRemote<PingService>>(ping)
                             .subspan(base::checked_cast<size_t>(i))
                             .data());
  EXPECT_EQ(2, DeferredDestroyPingImpl::instance_count);

  receivers.reset();
  EXPECT_EQ(0, DeferredDestroyPingImpl::instance_count);
}

TEST_F(DeferredDestroyUniqueReceiverSetTest, ConnectionError) {
  std::array<mojo::PendingRemote<PingService>, 4> ping;
  std::array<DeferredDestroyPingImpl*, 4> impl;
  auto receivers =
      std::make_unique<DeferredDestroyUniqueReceiverSet<PingService>>();

  for (int i = 0; i < 4; ++i)
    impl[i] = AddDeferredDestroyReceiver(
        receivers.get(), base::span<mojo::PendingRemote<PingService>>(ping)
                             .subspan(base::checked_cast<size_t>(i))
                             .data());
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
  std::array<mojo::PendingRemote<PingService>, 3> ping;
  std::array<DeferredDestroyPingImpl*, 3> impl;
  DeferredDestroyUniqueReceiverSet<PingService> receivers;

  for (int i = 0; i < 2; ++i)
    impl[i] = AddDeferredDestroyReceiver(
        &receivers, base::span<mojo::PendingRemote<PingService>>(ping)
                        .subspan(base::checked_cast<size_t>(i))
                        .data());
  EXPECT_EQ(2, DeferredDestroyPingImpl::instance_count);
  EXPECT_FALSE(receivers.empty());

  receivers.CloseAllReceivers();
  EXPECT_EQ(0, DeferredDestroyPingImpl::instance_count);
  EXPECT_TRUE(receivers.empty());

  // After CloseAllReceivers, new added receivers can still be deferred
  // destroyed.
  impl[2] = AddDeferredDestroyReceiver(
      &receivers,
      base::span<mojo::PendingRemote<PingService>>(ping).subspan(2u).data());
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

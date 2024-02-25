// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/cpp/bindings/tests/connection_group_unittest.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace connection_group_unittest {

class ConnectionGroupTest : public testing::Test {
 public:
  ConnectionGroupTest() = default;

  ConnectionGroupTest(const ConnectionGroupTest&) = delete;
  ConnectionGroupTest& operator=(const ConnectionGroupTest&) = delete;

 private:
  base::test::TaskEnvironment task_environment_;
};

using ConnectionGroupBindingsTest = BindingsTestBase;

class TestInterfaceImpl : public mojom::TestInterface {
 public:
  explicit TestInterfaceImpl(PendingReceiver<mojom::TestInterface> receiver) {
    receivers_.Add(this, std::move(receiver));
    receivers_.set_disconnect_handler(base::BindRepeating(
        &TestInterfaceImpl::OnDisconnect, base::Unretained(this)));
  }

  TestInterfaceImpl(const TestInterfaceImpl&) = delete;
  TestInterfaceImpl& operator=(const TestInterfaceImpl&) = delete;

  ~TestInterfaceImpl() override = default;

  void WaitForDisconnect() {
    base::RunLoop loop;
    wait_for_disconnect_closure_ = loop.QuitClosure();
    loop.Run();
  }

 private:
  void OnDisconnect() {
    if (wait_for_disconnect_closure_)
      std::move(wait_for_disconnect_closure_).Run();
  }

  // mojom::TestInterface:
  void BindReceiver(
      mojo::PendingReceiver<mojom::TestInterface> receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

  ReceiverSet<mojom::TestInterface> receivers_;
  base::OnceClosure wait_for_disconnect_closure_;
};

TEST_P(ConnectionGroupBindingsTest, RefCounting) {
  ConnectionGroup::Ref ref =
      ConnectionGroup::Create(base::DoNothing(), nullptr);
  auto group = ref.GetGroupForTesting();

  // The initial ref is valid but does not increase the ref-count.
  EXPECT_TRUE(ref);
  EXPECT_EQ(0u, group->GetNumRefsForTesting());

  // Moving the initial ref preserves its weak type.
  ConnectionGroup::Ref moved_ref = std::move(ref);
  EXPECT_FALSE(ref);
  EXPECT_TRUE(moved_ref);
  EXPECT_EQ(0u, group->GetNumRefsForTesting());
  ref = std::move(moved_ref);
  EXPECT_FALSE(moved_ref);
  EXPECT_TRUE(ref);
  EXPECT_EQ(0u, group->GetNumRefsForTesting());

  // Any copy of the initial ref does increase ref-count.
  ConnectionGroup::Ref copy = ref;
  EXPECT_TRUE(ref);
  EXPECT_TRUE(copy);
  EXPECT_EQ(1u, group->GetNumRefsForTesting());

  copy.reset();
  EXPECT_TRUE(ref);
  EXPECT_FALSE(copy);
  EXPECT_EQ(0u, group->GetNumRefsForTesting());
}

TEST_P(ConnectionGroupBindingsTest, PassedEndpointsInheritFromReceiver) {
  Remote<mojom::TestInterface> remote;
  auto pending_receiver = remote.BindNewPipeAndPassReceiver();

  ConnectionGroup::Ref ref =
      ConnectionGroup::Create(base::DoNothing(), nullptr);

  auto group = ref.GetGroupForTesting();
  pending_receiver.set_connection_group(std::move(ref));

  TestInterfaceImpl impl(std::move(pending_receiver));
  EXPECT_EQ(0u, group->GetNumRefsForTesting());

  // Verify that the connection group references spread to receivers passed over
  // the main interface.
  Remote<mojom::TestInterface> remote2;
  remote->BindReceiver(remote2.BindNewPipeAndPassReceiver());
  remote2.FlushForTesting();
  EXPECT_EQ(1u, group->GetNumRefsForTesting());

  Remote<mojom::TestInterface> remote3;
  remote->BindReceiver(remote3.BindNewPipeAndPassReceiver());
  remote3.FlushForTesting();
  EXPECT_EQ(2u, group->GetNumRefsForTesting());

  remote2.reset();
  impl.WaitForDisconnect();
  EXPECT_EQ(1u, group->GetNumRefsForTesting());

  remote3.reset();
  impl.WaitForDisconnect();
  EXPECT_EQ(0u, group->GetNumRefsForTesting());

  // Verify that group references continue to propagate through arbitrarily many
  // indirections (i.e. receivers passed over receivers passed over receivers,
  // etc.)
  remote->BindReceiver(remote2.BindNewPipeAndPassReceiver());
  remote2->BindReceiver(remote3.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::TestInterface> remote4;
  remote3->BindReceiver(remote4.BindNewPipeAndPassReceiver());
  remote4.FlushForTesting();
  EXPECT_EQ(3u, group->GetNumRefsForTesting());

  remote2.reset();
  impl.WaitForDisconnect();
  EXPECT_EQ(2u, group->GetNumRefsForTesting());

  remote3.reset();
  impl.WaitForDisconnect();
  EXPECT_EQ(1u, group->GetNumRefsForTesting());

  remote4.reset();
  impl.WaitForDisconnect();
  EXPECT_EQ(0u, group->GetNumRefsForTesting());
}

TEST_F(ConnectionGroupTest, NotifyOnDecrementToZero) {
  base::RunLoop loop;
  ConnectionGroup::Ref ref = ConnectionGroup::Create(
      loop.QuitClosure(), base::SequencedTaskRunner::GetCurrentDefault());
  auto group = ref.GetGroupForTesting();

  EXPECT_EQ(0u, group->GetNumRefsForTesting());
  ConnectionGroup::Ref copy = ref;
  EXPECT_EQ(1u, group->GetNumRefsForTesting());
  copy.reset();
  EXPECT_EQ(0u, group->GetNumRefsForTesting());

  loop.Run();
}

TEST_F(ConnectionGroupTest, NotifyOnDecrementToZeroMultipleTimes) {
  std::optional<base::RunLoop> loop;
  ConnectionGroup::Ref ref =
      ConnectionGroup::Create(base::BindLambdaForTesting([&] {
                                ASSERT_TRUE(loop.has_value());
                                loop->Quit();
                              }),
                              base::SequencedTaskRunner::GetCurrentDefault());

  auto group = ref.GetGroupForTesting();

  ConnectionGroup::Ref copy = ref;
  EXPECT_EQ(1u, group->GetNumRefsForTesting());
  copy.reset();
  EXPECT_EQ(0u, group->GetNumRefsForTesting());

  loop.emplace();
  loop->Run();

  EXPECT_EQ(0u, group->GetNumRefsForTesting());
  copy = ref;
  EXPECT_EQ(1u, group->GetNumRefsForTesting());
  copy.reset();
  EXPECT_EQ(0u, group->GetNumRefsForTesting());

  loop.emplace();
  loop->Run();
}

INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(ConnectionGroupBindingsTest);

}  // namespace connection_group_unittest
}  // namespace test
}  // namespace mojo

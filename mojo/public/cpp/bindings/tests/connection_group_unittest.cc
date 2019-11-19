// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/cpp/bindings/binding_set.h"
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

 private:
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionGroupTest);
};

using ConnectionGroupBindingsTest = BindingsTestBase;

class TestInterfaceImpl : public mojom::TestInterface {
 public:
  explicit TestInterfaceImpl(PendingReceiver<mojom::TestInterface> receiver) {
    receivers_.Add(this, std::move(receiver));
    receivers_.set_disconnect_handler(base::BindRepeating(
        &TestInterfaceImpl::OnDisconnect, base::Unretained(this)));
  }

  explicit TestInterfaceImpl(mojom::TestInterfaceRequest request) {
    bindings_.AddBinding(this, std::move(request));
    bindings_.set_connection_error_handler(base::BindRepeating(
        &TestInterfaceImpl::OnDisconnect, base::Unretained(this)));
  }

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
    DCHECK(bindings_.empty());
    receivers_.Add(this, std::move(receiver));
  }

  void BindRequest(mojom::TestInterfaceRequest request) override {
    DCHECK(receivers_.empty());
    bindings_.AddBinding(this, std::move(request));
  }

  ReceiverSet<mojom::TestInterface> receivers_;
  BindingSet<mojom::TestInterface> bindings_;
  base::OnceClosure wait_for_disconnect_closure_;

  DISALLOW_COPY_AND_ASSIGN(TestInterfaceImpl);
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
      loop.QuitClosure(), base::SequencedTaskRunnerHandle::Get());
  auto group = ref.GetGroupForTesting();

  EXPECT_EQ(0u, group->GetNumRefsForTesting());
  ConnectionGroup::Ref copy = ref;
  EXPECT_EQ(1u, group->GetNumRefsForTesting());
  copy.reset();
  EXPECT_EQ(0u, group->GetNumRefsForTesting());

  loop.Run();
}

TEST_F(ConnectionGroupTest, NotifyOnDecrementToZeroMultipleTimes) {
  base::Optional<base::RunLoop> loop;
  ConnectionGroup::Ref ref =
      ConnectionGroup::Create(base::BindLambdaForTesting([&] {
                                ASSERT_TRUE(loop.has_value());
                                loop->Quit();
                              }),
                              base::SequencedTaskRunnerHandle::Get());

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

TEST_P(ConnectionGroupBindingsTest, OldBindingsTypes) {
  // Just a basic smoke test to ensure that the ConnectionGroup mechanism also
  // works with old bindings types. The relevant implementation is largely
  // shared between the old and new types, so additional detailed coverage is
  // unnecessary.

  mojom::TestInterfacePtr ptr;
  auto request = MakeRequest(&ptr);

  ConnectionGroup::Ref ref =
      ConnectionGroup::Create(base::DoNothing(), nullptr);

  auto group = ref.GetGroupForTesting();
  request.set_connection_group(std::move(ref));

  TestInterfaceImpl impl(std::move(request));
  EXPECT_EQ(0u, group->GetNumRefsForTesting());

  // Verify that the connection group references spread to requests passed over
  // the main interface.
  mojom::TestInterfacePtr ptr2;
  ptr->BindRequest(MakeRequest(&ptr2));
  ptr2.FlushForTesting();
  EXPECT_EQ(1u, group->GetNumRefsForTesting());

  // Also verify that implicit conversion between PendingReceiver and
  // InterfaceRequest retains the connection group reference. First we set up
  // a new PendingReceiver holding a strong ConnectionGroup ref.
  ref = ConnectionGroup::Create(base::DoNothing(), nullptr);
  group = ref.GetGroupForTesting();
  mojo::Remote<mojom::TestInterface> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();
  receiver.set_connection_group(ref);
  EXPECT_EQ(1u, group->GetNumRefsForTesting());

  // Now verify implicit conversion both to and from the InterfaceRequest type.
  request = std::move(receiver);
  receiver.reset();
  EXPECT_EQ(1u, group->GetNumRefsForTesting());

  receiver = std::move(request);
  request = {};
  EXPECT_EQ(1u, group->GetNumRefsForTesting());
}

INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(ConnectionGroupBindingsTest);

}  // namespace connection_group_unittest
}  // namespace test
}  // namespace mojo

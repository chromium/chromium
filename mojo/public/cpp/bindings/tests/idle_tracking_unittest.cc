// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/cpp/bindings/tests/idle_tracking_unittest.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace idle_tracking_unittest {

using IdleTrackingTest = BindingsTestBase;

class TestServiceImpl : public mojom::TestService, public mojom::KeepAlive {
 public:
  explicit TestServiceImpl(PendingReceiver<mojom::TestService> receiver)
      : receiver_(this, std::move(receiver)) {}

  TestServiceImpl(const TestServiceImpl&) = delete;
  TestServiceImpl& operator=(const TestServiceImpl&) = delete;

  ~TestServiceImpl() override = default;

  void HoldNextPingPong() { hold_next_ping_pong_ = true; }
  void ReplyLastPingPong() {
    DCHECK(last_ping_pong_reply_);
    std::move(last_ping_pong_reply_).Run();
  }

 private:
  // mojom::TestService:
  void Ping() override {}

  void PingPong(PingPongCallback callback) override {
    if (hold_next_ping_pong_) {
      hold_next_ping_pong_ = false;
      last_ping_pong_reply_ = std::move(callback);
    } else {
      std::move(callback).Run();
    }
  }

  void BindKeepAlive(PendingReceiver<mojom::KeepAlive> receiver) override {
    keepalive_receivers_.Add(this, std::move(receiver));
  }

  Receiver<mojom::TestService> receiver_;
  ReceiverSet<mojom::KeepAlive> keepalive_receivers_;

  bool hold_next_ping_pong_ = false;
  base::OnceClosure last_ping_pong_reply_;
};

TEST_P(IdleTrackingTest, ControlMessagesDontExpectAck) {
  // Verifies that only mojom interface messages expect to be acknowledged and
  // control messages do not. This means that control messages cannot trigger
  // idle notifications.
  Remote<mojom::TestService> remote;
  TestServiceImpl impl(remote.BindNewPipeAndPassReceiver());

  base::RunLoop loop;
  remote.set_idle_handler(base::TimeDelta(),
                          base::BindRepeating([] { NOTREACHED(); }));
  remote.FlushAsyncForTesting(loop.QuitClosure());
  EXPECT_EQ(0u, remote.GetNumUnackedMessagesForTesting());
  loop.Run();
  EXPECT_EQ(0u, remote.GetNumUnackedMessagesForTesting());
}

TEST_P(IdleTrackingTest, BasicTracking) {
  // Verifies that basic idle tracking works when sending one-off messages with
  // reply and no attached PendingReceivers. Such messages can cause the
  // receiver to re-signal idle as soon as they're dispatched.

  Remote<mojom::TestService> remote;
  TestServiceImpl impl(remote.BindNewPipeAndPassReceiver());

  constexpr size_t kNumPings = 5;
  for (size_t i = 0; i < kNumPings; ++i) {
    base::RunLoop loop;
    remote.set_idle_handler(base::TimeDelta(), loop.QuitClosure());
    remote->Ping();
    EXPECT_EQ(1u, remote.GetNumUnackedMessagesForTesting());
    loop.Run();
    EXPECT_EQ(0u, remote.GetNumUnackedMessagesForTesting());
  }
}

TEST_P(IdleTrackingTest, PendingRepliesPreventIdling) {
  // Verifies that any reply-expecting messages sent to an idle-tracking
  // receiver will keep that receiver from idling until their reply is sent.

  Remote<mojom::TestService> remote;
  TestServiceImpl impl(remote.BindNewPipeAndPassReceiver());

  impl.HoldNextPingPong();
  remote.set_idle_handler(base::TimeDelta(),
                          base::BindRepeating([] { NOTREACHED(); }));

  bool idle = false;
  bool replied = false;
  {
    base::RunLoop loop;
    remote->PingPong(base::BindLambdaForTesting([&] {
      EXPECT_FALSE(idle);
      replied = true;
    }));
    remote.FlushAsyncForTesting(base::BindLambdaForTesting([&] {
      // The PingPong message should have been acked by now, but we still should
      // not have seen the idle handler invoked because the PingPong reply is
      // still pending.
      EXPECT_EQ(0u, remote.GetNumUnackedMessagesForTesting());
      EXPECT_FALSE(replied);
      loop.Quit();
    }));
    loop.Run();
  }

  // For good measure, also confirm that other one-off messages do not trigger
  // an idle notification while the above reply is still pending.
  remote->Ping();

  // We RunUntilIdle because we want to ensure that no asynchronous side-effects
  // of the above operations result in the idle handler being invoked.
  base::RunLoop().RunUntilIdle();

  {
    // Let the impl send its PingPong reply. This should allow the idle handler
    // to be invoked (*after* the reply is received).
    impl.ReplyLastPingPong();

    base::RunLoop loop;
    remote.set_idle_handler(base::TimeDelta(), base::BindLambdaForTesting([&] {
                              EXPECT_TRUE(replied);
                              EXPECT_FALSE(idle);
                              idle = true;
                              loop.Quit();
                            }));
    loop.Run();
  }
}

TEST_P(IdleTrackingTest, OtherBoundReceiversPreventIdling) {
  // Verifies that the existence of other receivers bound through an
  // idle-tracking receiver will keep that receiver from signaling idle as long
  // as they remain bound.

  Remote<mojom::TestService> remote;
  TestServiceImpl impl(remote.BindNewPipeAndPassReceiver());

  // First see that we can bind another receiver and that the Remote does not
  // invoke its idle handler even though its number of unacked messages goes to
  // zero.
  remote.set_idle_handler(base::TimeDelta(),
                          base::BindRepeating([] { NOTREACHED(); }));
  Remote<mojom::KeepAlive> keepalive;
  remote->BindKeepAlive(keepalive.BindNewPipeAndPassReceiver());
  EXPECT_EQ(1u, remote.GetNumUnackedMessagesForTesting());
  keepalive.FlushForTesting();
  remote.FlushForTesting();

  // We RunUntilIdle because we want to ensure that no asynchronous side-effects
  // of the above operations result in the idle handler being invoked.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, remote.GetNumUnackedMessagesForTesting());

  // Now we can reset the KeepAlive, and we expect the TestService Remote to
  // subsequently report that its receiver is idle.
  base::RunLoop loop;
  remote.set_idle_handler(base::TimeDelta(), loop.QuitClosure());
  keepalive.reset();
  loop.Run();
}

TEST_P(IdleTrackingTest, NonZeroTimeout) {
  // Verifies that a non-zero idle timeout will not signal idle for at least as
  // long as the specified duration.

  Remote<mojom::TestService> remote;
  TestServiceImpl impl(remote.BindNewPipeAndPassReceiver());

  constexpr auto kTimeout = base::Milliseconds(500);
  base::ElapsedTimer timer;
  base::RunLoop loop;
  remote.set_idle_handler(kTimeout, base::BindLambdaForTesting([&] {
                            EXPECT_GE(timer.Elapsed(), kTimeout);
                            loop.Quit();
                          }));
  remote->Ping();
  loop.Run();
}

TEST_P(IdleTrackingTest, SubInterfacesCanIdleSeparately) {
  // Verifies that hierarchical ConnectionGroup references work properly, such
  // that a main interface with an idle timeout can bind a subinterface with its
  // own idle timeout, and the subinterface (and all subinterfaces it binds
  // transitively) will still keep the main interface from timing out.

  Remote<mojom::TestService> remote;
  TestServiceImpl impl(remote.BindNewPipeAndPassReceiver());

  // First see that we can bind another receiver and that the Remote does not
  // invoke its idle handler even though its number of unacked messages goes to
  // zero.
  remote.set_idle_handler(base::TimeDelta(),
                          base::BindRepeating([] { NOTREACHED(); }));
  Remote<mojom::KeepAlive> keepalive;
  remote->BindKeepAlive(keepalive.BindNewPipeAndPassReceiver());
  EXPECT_EQ(1u, remote.GetNumUnackedMessagesForTesting());
  keepalive.FlushForTesting();
  remote.FlushForTesting();

  // We RunUntilIdle because we want to ensure that no asynchronous side-effects
  // of the above operations result in the idle handler being invoked.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, remote.GetNumUnackedMessagesForTesting());

  // Now we set an idle handler on the KeepAlive itself, and we expect the
  // TestService Remote to NOT invoke its idle handler.
  keepalive.set_idle_handler(base::TimeDelta(), base::DoNothing());

  // We use RunUntilIdle() again because we want to ensure there are no
  // asynchronous side-effects of the above operation that lead to idling on the
  // main interface. If the main interface idles, it will hit an assertion
  // before this call returns.
  base::RunLoop().RunUntilIdle();

  // Finally verify that the main interface does still go idle once we reset the
  // keepalive connection.
  base::RunLoop loop;
  remote.set_idle_handler(base::TimeDelta(), loop.QuitClosure());
  remote.FlushForTesting();
  keepalive.reset();
  loop.Run();
}

INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(IdleTrackingTest);

}  // namespace idle_tracking_unittest
}  // namespace test
}  // namespace mojo

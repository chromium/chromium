// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/direct_receiver.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/message_loop/message_pump_type.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/tests/direct_receiver_unittest.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/ipcz/src/test/test_base.h"

namespace mojo::test::direct_receiver_unittest {

namespace {

// Helper to run a closure on `thread`, blocking until it completes.
template <typename Fn>
void RunOn(base::Thread& thread, Fn closure) {
  base::WaitableEvent wait;
  thread.task_runner()->PostTask(FROM_HERE, base::BindLambdaForTesting([&] {
                                   closure();
                                   wait.Signal();
                                 }));
  wait.Wait();
}

}  // namespace

// We use an ipcz-internal test fixture as a base because it provides useful
// introspection into internal portal state. We also use MojoTestBase because it
// provides multiprocess test facilities.
class DirectReceiverTest : public ipcz::test::internal::TestBase,
                           public core::test::MojoTestBase {
  void SetUp() override {
    if (!core::IsMojoIpczEnabled()) {
      GTEST_SKIP() << "This test is only valid when MojoIpcz is enabled.";
    }
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

// From the time this object's constructor returns, and until the object is
// destroyed, the IO thread will not run any new tasks.
class ScopedPauseIOThread {
 public:
  ScopedPauseIOThread() {
    base::RunLoop loop;
    core::GetIOTaskRunner()->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([this, quit = loop.QuitClosure()] {
          // It's OK for the caller to continue before we start waiting. What's
          // important is that this is the last task the IO thread will run
          // until it's unpaused.
          quit.Run();
          unblock_event_.Wait();
        }));
    loop.Run();
  }

  ~ScopedPauseIOThread() {
    base::RunLoop loop;
    unblock_event_.Signal();
    core::GetIOTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

 private:
  base::WaitableEvent unblock_event_;
};

class ServiceImpl : public mojom::Service {
 public:
  explicit ServiceImpl(scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(std::move(task_runner)) {}
  ~ServiceImpl() override = default;

  DirectReceiver<mojom::Service>& receiver() { return receiver_; }

  // Binds our DirectReceiver to `receiver` and then uses a test-only API to
  // pause it internally, preventing Mojo bindings from processing incoming
  // messages. This is needed so we can use some ipcz test facilities to wait
  // for a direct internal link for the pipe, a process which uses non-Mojo
  // communication. Signals `bound_event` when finished. `ping_event` is
  // retained by the ServiceImpl and signaled when it receives its first Ping()
  // call.
  void BindAndPauseReceiver(PendingReceiver<mojom::Service> receiver,
                            base::WaitableEvent* bound_event,
                            base::WaitableEvent* ping_event) {
    ping_event_ = ping_event;
    receiver_.Bind(std::move(receiver));
    receiver_.receiver_for_testing().Pause();
    bound_event->Signal();
  }

  void UnpauseReceiver() { receiver_.receiver_for_testing().Resume(); }

  // Exposes the underlying portal used by the DirectReceiver on its own node.
  // The ServiceRunner below needs this to wait for the portal to establish a
  // direct link to the child.
  IpczHandle GetReceiverPortal() {
    return receiver_.receiver_for_testing().internal_state()->handle().value();
  }

  // mojom::Service:
  void Ping(PingCallback callback) override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    std::move(callback).Run();
    if (auto event = std::exchange(ping_event_, nullptr)) {
      event->Signal();
    }
  }

 private:
  DirectReceiver<mojom::Service> receiver_{DirectReceiverKey{}, this};
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  raw_ptr<base::WaitableEvent> ping_event_ = nullptr;
};

class ServiceRunner {
 public:
  explicit ServiceRunner(DirectReceiverTest& test) : test_(test) {}

  ~ServiceRunner() { service_.SynchronouslyResetForTest(); }

  // Spawns a background thread and runs a new ServiceImpl on it, where it binds
  // to `receiver` via a DirectReceiver. This call only returns once the
  // ServiceImpl is running, fully bound, and has a direct link to its child's
  // portal; thus ensuring that in a well-behaved system, all child IPC to the
  // service goes directly to the service thread without an intermediate IO
  // thread hop. `ping_event` is signaled when the service receives its first
  // Ping() call.
  void Start(PendingReceiver<mojom::Service> receiver,
             base::WaitableEvent& ping_event) {
    impl_thread_.StartWithOptions(
        base::Thread::Options{base::MessagePumpType::IO, 0});
    service_.emplace(impl_thread_.task_runner(), impl_thread_.task_runner());

    // First the service bound on its own thread.
    base::WaitableEvent bound_event;
    service_.AsyncCall(&ServiceImpl::BindAndPauseReceiver)
        .WithArgs(std::move(receiver), &bound_event, &ping_event);
    bound_event.Wait();

    // Now wait for its portal to have a direct link. This internally uses some
    // non-Mojo message passing to synchronize with the child, but it's safe
    // because the Receiver is paused and any received message from the child
    // will be removed from the pipe before unpausing below.
    base::RunLoop wait_loop;
    service_.AsyncCall(&ServiceImpl::GetReceiverPortal)
        .Then(base::BindLambdaForTesting([&](IpczHandle portal) {
          test_->WaitForDirectRemoteLink(portal);
          wait_loop.Quit();
        }));
    wait_loop.Run();

    // Now the receiver can resume listening for messages.
    base::RunLoop unpause_loop;
    service_.AsyncCall(&ServiceImpl::UnpauseReceiver)
        .Then(unpause_loop.QuitClosure());
    unpause_loop.Run();
  }

 private:
  const raw_ref<DirectReceiverTest> test_;
  base::Thread impl_thread_{"Impl Thread"};
  base::SequenceBound<ServiceImpl> service_;
};

TEST_F(DirectReceiverTest, NoIOThreadHopInBroker) {
  ServiceRunner runner{*this};
  RunTestClient("NoIOThreadHopInBroker_Child", [&](MojoHandle child) {
    PendingRemote<mojom::Service> remote;
    PendingReceiver<mojom::Service> receiver =
        remote.InitWithNewPipeAndPassReceiver();

    // Pass the child its pipe.
    MojoHandle remote_pipe = remote.PassPipe().release().value();
    WriteMessageWithHandles(child, "", &remote_pipe, 1);

    // Start the service. This blocks until it has a direct link to the child
    // portal (i.e. no proxies), so any message sent from the child after this
    // returns should not hop through our IO thread, but should instead go
    // directly to the ServiceImpl's thread.
    base::WaitableEvent ping_event;
    runner.Start(std::move(receiver), ping_event);

    // Pause the IO thread and tell the child to ping the service.
    {
      ScopedPauseIOThread pause_io;
      WriteMessage(child, "ok go");

      // Now wait for receipt of the Ping() before unblocking IO. The only way
      // this wait can terminate is if the service receives the child's Ping()
      // IPC directly.
      ping_event.Wait();
    }

    // Wait for the child to finish.
    EXPECT_EQ("done", ReadMessage(child));
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(NoIOThreadHopInBroker_Child,
                                  DirectReceiverTest,
                                  test_pipe_handle) {
  const ScopedMessagePipeHandle test_pipe{MessagePipeHandle{test_pipe_handle}};

  // Before binding to a Remote, wait for the pipe's portal to have a direct
  // link to the service.
  MojoHandle handle;
  ReadMessageWithHandles(test_pipe->value(), &handle, 1);
  WaitForDirectRemoteLink(handle);

  Remote<mojom::Service> service{PendingRemote<mojom::Service>{
      MakeScopedHandle(MessagePipeHandle{handle}), 0}};

  // Wait for the test to be ready for our Ping(), ensuring that its IO thread
  // is paused first.
  EXPECT_EQ("ok go", ReadMessage(test_pipe->value()));

  base::RunLoop loop;
  service->Ping(loop.QuitClosure());
  loop.Run();

  WriteMessage(test_pipe->value(), "done");
}

TEST_F(DirectReceiverTest, NoIOThreadHopInNonBrokerProcess) {
  PendingRemote<mojom::Service> remote;
  PendingReceiver<mojom::Service> receiver =
      remote.InitWithNewPipeAndPassReceiver();
  RunTestClient("NoIOThreadHopInNonBroker_Child", [&](MojoHandle child) {
    MojoHandle service_pipe = receiver.PassPipe().release().value();
    WriteMessageWithHandles(child, "", &service_pipe, 1);

    // Before binding it to a Remote, wait for the pipe's portal to have a
    // direct link to the service portal in the child process.
    WaitForDirectRemoteLink(remote.internal_state()->pipe->value());
    Remote<mojom::Service> service{std::move(remote)};

    // Wait for the child to be ready for our Ping(), ensuring that its IO
    // thread is paused first.
    EXPECT_EQ("ok go", ReadMessage(child));

    base::RunLoop loop;
    service->Ping(loop.QuitClosure());
    loop.Run();

    // Wait for the child to finish.
    EXPECT_EQ("done", ReadMessage(child));
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(NoIOThreadHopInNonBroker_Child,
                                  DirectReceiverTest,
                                  test_pipe_handle) {
  const ScopedMessagePipeHandle test_pipe{MessagePipeHandle{test_pipe_handle}};

  // First get the service pipe from the test process.
  MojoHandle service_pipe;
  ReadMessageWithHandles(test_pipe->value(), &service_pipe, 1);
  PendingReceiver<mojom::Service> receiver{
      ScopedMessagePipeHandle{MessagePipeHandle{service_pipe}}};

  // Start the service. This blocks until it has a direct link to the test
  // process's portal (i.e. no proxies), so any message sent from the test
  // process after this returns should not hop through our IO thread, but should
  // instead go directly to the ServiceImpl's thread.
  ServiceRunner runner{*this};
  base::WaitableEvent ping_event;
  runner.Start(std::move(receiver), ping_event);

  // Pause the IO thread and tell the test process to ping the service.
  {
    ScopedPauseIOThread pause_io;
    WriteMessage(test_pipe->value(), "ok go");

    // Now wait for receipt of the Ping() before unblocking IO. The only way
    // this wait can terminate is if the service receives the Ping() directly.
    ping_event.Wait();
  }

  WriteMessage(test_pipe->value(), "done");
}

TEST_F(DirectReceiverTest, ThreadLocalInstanceShared) {
  // Creates two DirectReceivers on the same thread and validates that they
  // share the same underlying ipcz node.

  base::Thread io_thread{"Test IO thread"};
  io_thread.StartWithOptions(
      base::Thread::Options{base::MessagePumpType::IO, 0});

  std::unique_ptr<ServiceImpl> impl1;
  std::unique_ptr<ServiceImpl> impl2;
  Remote<mojom::Service> remote1;
  Remote<mojom::Service> remote2;
  auto receiver1 = remote1.BindNewPipeAndPassReceiver();
  auto receiver2 = remote2.BindNewPipeAndPassReceiver();
  RunOn(io_thread, [&] {
    impl1 = std::make_unique<ServiceImpl>(io_thread.task_runner());
    impl1->receiver().Bind(std::move(receiver1));

    impl2 = std::make_unique<ServiceImpl>(io_thread.task_runner());
    impl2->receiver().Bind(std::move(receiver2));
  });

  // Both receivers should be using the same node to receive I/O.
  EXPECT_EQ(impl1->receiver().node_for_testing().node(),
            impl2->receiver().node_for_testing().node());

  // For good measure, verify that the receivers actually work too.
  base::RunLoop loop1;
  remote1->Ping(loop1.QuitClosure());
  loop1.Run();
  base::RunLoop loop2;
  remote1->Ping(loop2.QuitClosure());
  loop2.Run();

  RunOn(io_thread, [&] {
    // Also verify that when the last receiver goes away on this thread, the
    // thread's node also goes away.
    EXPECT_TRUE(internal::ThreadLocalNode::CurrentThreadHasInstance());
    impl1.reset();
    EXPECT_TRUE(internal::ThreadLocalNode::CurrentThreadHasInstance());
    impl2.reset();
    EXPECT_FALSE(internal::ThreadLocalNode::CurrentThreadHasInstance());
  });
}

TEST_F(DirectReceiverTest, UniqueNodePerThread) {
  // Creates a DirectReceiver on each of two distinct threads and validates that
  // they each have their own distinct ipcz node.

  base::Thread io_thread1{"Test IO thread 1"};
  base::Thread io_thread2{"Test IO thread 2"};
  io_thread1.StartWithOptions(
      base::Thread::Options{base::MessagePumpType::IO, 0});
  io_thread2.StartWithOptions(
      base::Thread::Options{base::MessagePumpType::IO, 0});

  std::unique_ptr<ServiceImpl> impl1;
  std::unique_ptr<ServiceImpl> impl2;
  Remote<mojom::Service> remote1;
  Remote<mojom::Service> remote2;
  auto receiver1 = remote1.BindNewPipeAndPassReceiver();
  auto receiver2 = remote2.BindNewPipeAndPassReceiver();
  RunOn(io_thread1, [&] {
    impl1 = std::make_unique<ServiceImpl>(io_thread1.task_runner());
    impl1->receiver().Bind(std::move(receiver1));
  });
  RunOn(io_thread2, [&] {
    impl2 = std::make_unique<ServiceImpl>(io_thread2.task_runner());
    impl2->receiver().Bind(std::move(receiver2));
  });

  // Both receivers should be using different nodes to receive I/O.
  EXPECT_NE(impl1->receiver().node_for_testing().node(),
            impl2->receiver().node_for_testing().node());

  // For good measure, verify that the receivers actually work too.
  base::RunLoop loop1;
  remote1->Ping(loop1.QuitClosure());
  loop1.Run();
  base::RunLoop loop2;
  remote1->Ping(loop2.QuitClosure());
  loop2.Run();

  RunOn(io_thread1, [&] {
    EXPECT_TRUE(internal::ThreadLocalNode::CurrentThreadHasInstance());
    impl1.reset();
    EXPECT_FALSE(internal::ThreadLocalNode::CurrentThreadHasInstance());
  });

  RunOn(io_thread2, [&] {
    EXPECT_TRUE(internal::ThreadLocalNode::CurrentThreadHasInstance());
    impl2.reset();
    EXPECT_FALSE(internal::ThreadLocalNode::CurrentThreadHasInstance());
  });
}

}  // namespace mojo::test::direct_receiver_unittest

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_associated_remote.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "mojo/public/interfaces/bindings/tests/ping_service.mojom.h"
#include "mojo/public/interfaces/bindings/tests/test_associated_interfaces.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

using mojo::internal::MultiplexRouter;

class IntegerSenderImpl : public IntegerSender {
 public:
  IntegerSenderImpl() = default;
  explicit IntegerSenderImpl(PendingAssociatedReceiver<IntegerSender> receiver)
      : receiver_(this, std::move(receiver)) {}

  ~IntegerSenderImpl() override = default;

  void set_notify_send_method_called(
      base::RepeatingCallback<void(int32_t)> callback) {
    notify_send_method_called_ = std::move(callback);
  }

  void Echo(int32_t value, EchoCallback callback) override {
    std::move(callback).Run(value);
  }
  void Send(int32_t value) override { notify_send_method_called_.Run(value); }

  AssociatedReceiver<IntegerSender>* receiver() { return &receiver_; }

 private:
  AssociatedReceiver<IntegerSender> receiver_{this};
  base::RepeatingCallback<void(int32_t)> notify_send_method_called_;
};

class IntegerSenderConnectionImpl : public IntegerSenderConnection {
 public:
  explicit IntegerSenderConnectionImpl(
      PendingReceiver<IntegerSenderConnection> receiver)
      : receiver_(this, std::move(receiver)) {}

  ~IntegerSenderConnectionImpl() override = default;

  void GetSender(PendingAssociatedReceiver<IntegerSender> receiver) override {
    DCHECK(receiver.is_valid());
    senders_.Add(std::make_unique<IntegerSenderImpl>(), std::move(receiver));
  }

  void AsyncGetSender(AsyncGetSenderCallback callback) override {
    PendingAssociatedRemote<IntegerSender> remote;
    GetSender(remote.InitWithNewEndpointAndPassReceiver());
    std::move(callback).Run(std::move(remote));
  }

  Receiver<IntegerSenderConnection>* receiver() { return &receiver_; }

 private:
  Receiver<IntegerSenderConnection> receiver_;
  UniqueAssociatedReceiverSet<IntegerSender> senders_;
};

class AssociatedInterfaceTest : public testing::Test {
 public:
  AssociatedInterfaceTest()
      : main_runner_(base::ThreadTaskRunnerHandle::Get()) {}
  ~AssociatedInterfaceTest() override = default;

  template <typename T>
  PendingAssociatedRemote<T> EmulatePassingAssociatedRemote(
      PendingAssociatedRemote<T> remote,
      scoped_refptr<MultiplexRouter> source,
      scoped_refptr<MultiplexRouter> target) {
    ScopedInterfaceEndpointHandle handle = remote.PassHandle();
    CHECK(handle.pending_association());
    auto id = source->AssociateInterface(std::move(handle));
    return PendingAssociatedRemote<T>(target->CreateLocalEndpointHandle(id),
                                      remote.version());
  }

  void CreateRouterPair(scoped_refptr<MultiplexRouter>* router0,
                        scoped_refptr<MultiplexRouter>* router1) {
    MessagePipe pipe;
    *router0 = new MultiplexRouter(std::move(pipe.handle0),
                                   MultiplexRouter::MULTI_INTERFACE, true,
                                   main_runner_);
    *router1 = new MultiplexRouter(std::move(pipe.handle1),
                                   MultiplexRouter::MULTI_INTERFACE, false,
                                   main_runner_);
  }

  void CreateIntegerSenderWithExistingRouters(
      scoped_refptr<MultiplexRouter> router0,
      PendingAssociatedRemote<IntegerSender>* remote0,
      scoped_refptr<MultiplexRouter> router1,
      PendingAssociatedReceiver<IntegerSender>* receiver1) {
    *receiver1 = remote0->InitWithNewEndpointAndPassReceiver();
    *remote0 =
        EmulatePassingAssociatedRemote(std::move(*remote0), router1, router0);
  }

  void CreateIntegerSender(PendingAssociatedRemote<IntegerSender>* remote,
                           PendingAssociatedReceiver<IntegerSender>* receiver) {
    scoped_refptr<MultiplexRouter> router0;
    scoped_refptr<MultiplexRouter> router1;
    CreateRouterPair(&router0, &router1);
    CreateIntegerSenderWithExistingRouters(router1, remote, router0, receiver);
  }

 private:
  base::test::TaskEnvironment task_environment;
  scoped_refptr<base::SequencedTaskRunner> main_runner_;
};

void Fail() {
  FAIL() << "Unexpected connection error";
}

TEST_F(AssociatedInterfaceTest, InterfacesAtBothEnds) {
  // Bind to the same pipe two associated interfaces, whose implementation lives
  // at different ends. Test that the two don't interfere with each other.

  scoped_refptr<MultiplexRouter> router0;
  scoped_refptr<MultiplexRouter> router1;
  CreateRouterPair(&router0, &router1);

  PendingAssociatedReceiver<IntegerSender> receiver;
  PendingAssociatedRemote<IntegerSender> remote;

  CreateIntegerSenderWithExistingRouters(router1, &remote, router0, &receiver);
  IntegerSenderImpl impl0(std::move(receiver));
  AssociatedRemote<IntegerSender> remote0(std::move(remote));

  CreateIntegerSenderWithExistingRouters(router0, &remote, router1, &receiver);
  IntegerSenderImpl impl1(std::move(receiver));
  AssociatedRemote<IntegerSender> remote1(std::move(remote));

  base::RunLoop run_loop, run_loop2;
  bool remote0_callback_run = false;
  remote0->Echo(123, base::BindLambdaForTesting([&](int32_t value) {
                  EXPECT_EQ(123, value);
                  remote0_callback_run = true;
                  run_loop.Quit();
                }));

  bool remote1_callback_run = false;
  remote1->Echo(456, base::BindLambdaForTesting([&](int32_t value) {
                  EXPECT_EQ(456, value);
                  remote1_callback_run = true;
                  run_loop2.Quit();
                }));

  run_loop.Run();
  run_loop2.Run();
  EXPECT_TRUE(remote0_callback_run);
  EXPECT_TRUE(remote1_callback_run);

  bool remote0_disconnect_handler_run = false;
  base::RunLoop run_loop3;
  remote0.set_disconnect_handler(base::BindLambdaForTesting([&] {
    remote0_disconnect_handler_run = true;
    run_loop3.Quit();
  }));

  impl0.receiver()->reset();
  run_loop3.Run();
  EXPECT_TRUE(remote0_disconnect_handler_run);

  bool remote1_disconnect_handler_run = false;
  base::RunLoop run_loop4;
  impl1.receiver()->set_disconnect_handler(base::BindLambdaForTesting([&] {
    remote1_disconnect_handler_run = true;
    run_loop4.Quit();
  }));

  remote1.reset();
  run_loop4.Run();
  EXPECT_TRUE(remote1_disconnect_handler_run);
}

class TestSender {
 public:
  TestSender()
      : task_runner_(base::CreateSequencedTaskRunner({base::ThreadPool()})),
        next_sender_(nullptr),
        max_value_to_send_(-1) {}

  // The following three methods are called on the corresponding sender thread.
  void SetUp(PendingAssociatedRemote<IntegerSender> remote,
             TestSender* next_sender,
             int32_t max_value_to_send) {
    CHECK(task_runner()->RunsTasksInCurrentSequence());

    remote_.Bind(std::move(remote));
    next_sender_ = next_sender ? next_sender : this;
    max_value_to_send_ = max_value_to_send;
  }

  void Send(int32_t value) {
    CHECK(task_runner()->RunsTasksInCurrentSequence());

    if (value > max_value_to_send_)
      return;

    remote_->Send(value);

    next_sender_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&TestSender::Send,
                                  base::Unretained(next_sender_), ++value));
  }

  void TearDown() {
    CHECK(task_runner()->RunsTasksInCurrentSequence());

    remote_.reset();
  }

  base::SequencedTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  TestSender* next_sender_;
  int32_t max_value_to_send_;

  AssociatedRemote<IntegerSender> remote_;
};

class TestReceiver {
 public:
  TestReceiver()
      : task_runner_(base::CreateSequencedTaskRunner({base::ThreadPool()})),
        expected_calls_(0) {}

  void SetUp(PendingAssociatedReceiver<IntegerSender> receiver0,
             PendingAssociatedReceiver<IntegerSender> receiver1,
             size_t expected_calls,
             base::OnceClosure notify_finish) {
    CHECK(task_runner()->RunsTasksInCurrentSequence());

    impl0_.reset(new IntegerSenderImpl(std::move(receiver0)));
    impl0_->set_notify_send_method_called(base::BindRepeating(
        &TestReceiver::SendMethodCalled, base::Unretained(this)));
    impl1_.reset(new IntegerSenderImpl(std::move(receiver1)));
    impl1_->set_notify_send_method_called(base::BindRepeating(
        &TestReceiver::SendMethodCalled, base::Unretained(this)));

    expected_calls_ = expected_calls;
    notify_finish_ = std::move(notify_finish);
  }

  void TearDown() {
    CHECK(task_runner()->RunsTasksInCurrentSequence());

    impl0_.reset();
    impl1_.reset();
  }

  base::SequencedTaskRunner* task_runner() { return task_runner_.get(); }
  const std::vector<int32_t>& values() const { return values_; }

 private:
  void SendMethodCalled(int32_t value) {
    values_.push_back(value);

    if (values_.size() >= expected_calls_)
      std::move(notify_finish_).Run();
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  size_t expected_calls_;

  std::unique_ptr<IntegerSenderImpl> impl0_;
  std::unique_ptr<IntegerSenderImpl> impl1_;

  std::vector<int32_t> values_;

  base::OnceClosure notify_finish_;
};

class NotificationCounter {
 public:
  NotificationCounter(size_t total_count, base::OnceClosure notify_finish)
      : total_count_(total_count),
        current_count_(0),
        notify_finish_(std::move(notify_finish)) {}

  ~NotificationCounter() = default;

  // Okay to call from any thread.
  void OnGotNotification() {
    bool finshed = false;
    {
      base::AutoLock locker(lock_);
      CHECK_LT(current_count_, total_count_);
      current_count_++;
      finshed = current_count_ == total_count_;
    }

    if (finshed)
      std::move(notify_finish_).Run();
  }

 private:
  base::Lock lock_;
  const size_t total_count_;
  size_t current_count_;
  base::OnceClosure notify_finish_;
};

TEST_F(AssociatedInterfaceTest, MultiThreadAccess) {
  // Set up four associated interfaces on a message pipe. Use the inteface
  // pointers on four threads in parallel; run the interface implementations on
  // two threads. Test that multi-threaded access works.

  const int32_t kMaxValue = 1000;
  MessagePipe pipe;
  scoped_refptr<MultiplexRouter> router0;
  scoped_refptr<MultiplexRouter> router1;
  CreateRouterPair(&router0, &router1);

  PendingAssociatedReceiver<IntegerSender> pending_receivers[4];
  PendingAssociatedRemote<IntegerSender> pending_remotes[4];
  for (size_t i = 0; i < 4; ++i) {
    CreateIntegerSenderWithExistingRouters(router1, &pending_remotes[i],
                                           router0, &pending_receivers[i]);
  }

  TestSender senders[4];
  for (size_t i = 0; i < 4; ++i) {
    senders[i].task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&TestSender::SetUp, base::Unretained(&senders[i]),
                       std::move(pending_remotes[i]), nullptr,
                       kMaxValue * (i + 1) / 4));
  }

  base::RunLoop run_loop;
  TestReceiver receivers[2];
  NotificationCounter counter(2, run_loop.QuitClosure());
  for (size_t i = 0; i < 2; ++i) {
    receivers[i].task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&TestReceiver::SetUp, base::Unretained(&receivers[i]),
                       std::move(pending_receivers[2 * i]),
                       std::move(pending_receivers[2 * i + 1]),
                       static_cast<size_t>(kMaxValue / 2),
                       base::BindOnce(&NotificationCounter::OnGotNotification,
                                      base::Unretained(&counter))));
  }

  for (size_t i = 0; i < 4; ++i) {
    senders[i].task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&TestSender::Send, base::Unretained(&senders[i]),
                       kMaxValue * i / 4 + 1));
  }

  run_loop.Run();

  for (size_t i = 0; i < 4; ++i) {
    base::RunLoop run_loop;
    senders[i].task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&TestSender::TearDown, base::Unretained(&senders[i])),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  for (size_t i = 0; i < 2; ++i) {
    base::RunLoop run_loop;
    receivers[i].task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&TestReceiver::TearDown,
                       base::Unretained(&receivers[i])),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  EXPECT_EQ(static_cast<size_t>(kMaxValue / 2), receivers[0].values().size());
  EXPECT_EQ(static_cast<size_t>(kMaxValue / 2), receivers[1].values().size());

  std::vector<int32_t> all_values;
  all_values.insert(all_values.end(), receivers[0].values().begin(),
                    receivers[0].values().end());
  all_values.insert(all_values.end(), receivers[1].values().begin(),
                    receivers[1].values().end());

  std::sort(all_values.begin(), all_values.end());
  for (size_t i = 0; i < all_values.size(); ++i)
    ASSERT_EQ(static_cast<int32_t>(i + 1), all_values[i]);
}

TEST_F(AssociatedInterfaceTest, FIFO) {
  // Set up four associated interfaces on a message pipe. Use the inteface
  // pointers on four threads; run the interface implementations on two threads.
  // Take turns to make calls using the four pointers. Test that FIFO-ness is
  // preserved.

  const int32_t kMaxValue = 100;
  MessagePipe pipe;
  scoped_refptr<MultiplexRouter> router0;
  scoped_refptr<MultiplexRouter> router1;
  CreateRouterPair(&router0, &router1);

  PendingAssociatedReceiver<IntegerSender> pending_receivers[4];
  PendingAssociatedRemote<IntegerSender> pending_remotes[4];
  for (size_t i = 0; i < 4; ++i) {
    CreateIntegerSenderWithExistingRouters(router1, &pending_remotes[i],
                                           router0, &pending_receivers[i]);
  }

  TestSender senders[4];
  for (size_t i = 0; i < 4; ++i) {
    senders[i].task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&TestSender::SetUp, base::Unretained(&senders[i]),
                       std::move(pending_remotes[i]),
                       base::Unretained(&senders[(i + 1) % 4]), kMaxValue));
  }

  base::RunLoop run_loop;
  TestReceiver receivers[2];
  NotificationCounter counter(2, run_loop.QuitClosure());
  for (size_t i = 0; i < 2; ++i) {
    receivers[i].task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&TestReceiver::SetUp, base::Unretained(&receivers[i]),
                       std::move(pending_receivers[2 * i]),
                       std::move(pending_receivers[2 * i + 1]),
                       static_cast<size_t>(kMaxValue / 2),
                       base::BindOnce(&NotificationCounter::OnGotNotification,
                                      base::Unretained(&counter))));
  }

  senders[0].task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TestSender::Send, base::Unretained(&senders[0]), 1));

  run_loop.Run();

  for (size_t i = 0; i < 4; ++i) {
    base::RunLoop run_loop;
    senders[i].task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&TestSender::TearDown, base::Unretained(&senders[i])),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  for (size_t i = 0; i < 2; ++i) {
    base::RunLoop run_loop;
    receivers[i].task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&TestReceiver::TearDown,
                       base::Unretained(&receivers[i])),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  EXPECT_EQ(static_cast<size_t>(kMaxValue / 2), receivers[0].values().size());
  EXPECT_EQ(static_cast<size_t>(kMaxValue / 2), receivers[1].values().size());

  for (size_t i = 0; i < 2; ++i) {
    for (size_t j = 1; j < receivers[i].values().size(); ++j)
      EXPECT_LT(receivers[i].values()[j - 1], receivers[i].values()[j]);
  }
}

TEST_F(AssociatedInterfaceTest, PassAssociatedInterfaces) {
  Remote<IntegerSenderConnection> connection_remote;
  IntegerSenderConnectionImpl connection(
      connection_remote.BindNewPipeAndPassReceiver());

  AssociatedRemote<IntegerSender> sender0;
  connection_remote->GetSender(sender0.BindNewEndpointAndPassReceiver());

  base::RunLoop run_loop;
  sender0->Echo(123, base::BindLambdaForTesting([&](int32_t value) {
                  EXPECT_EQ(123, value);
                  run_loop.Quit();
                }));
  run_loop.Run();

  AssociatedRemote<IntegerSender> sender1;
  base::RunLoop run_loop2;
  connection_remote->AsyncGetSender(base::BindLambdaForTesting(
      [&](PendingAssociatedRemote<IntegerSender> sender) {
        sender1.Bind(std::move(sender));
        run_loop2.Quit();
      }));
  run_loop2.Run();
  EXPECT_TRUE(sender1);

  base::RunLoop run_loop3;
  sender1->Echo(456, base::BindLambdaForTesting([&](int32_t value) {
                  EXPECT_EQ(456, value);
                  run_loop3.Quit();
                }));
  run_loop3.Run();
}

TEST_F(AssociatedInterfaceTest,
       ReceiverWaitAndPauseWhenNoAssociatedInterfaces) {
  Remote<IntegerSenderConnection> connection_remote;
  IntegerSenderConnectionImpl connection(
      connection_remote.BindNewPipeAndPassReceiver());

  AssociatedRemote<IntegerSender> sender0;
  connection_remote->GetSender(sender0.BindNewEndpointAndPassReceiver());

  EXPECT_FALSE(
      connection.receiver()->internal_state()->HasAssociatedInterfaces());

  // There are no associated interfaces running on the pipe yet. It is okay to
  // pause.
  connection.receiver()->Pause();
  connection.receiver()->Resume();

  // There are no associated interfaces running on the pipe yet. It is okay to
  // wait.
  EXPECT_TRUE(connection.receiver()->WaitForIncomingCall());

  // The previous wait has dispatched the GetSender request message, therefore
  // an associated interface has been set up on the pipe. It is not allowed to
  // wait or pause.
  EXPECT_TRUE(
      connection.receiver()->internal_state()->HasAssociatedInterfaces());
}

class PingServiceImpl : public PingService {
 public:
  explicit PingServiceImpl(PendingAssociatedReceiver<PingService> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~PingServiceImpl() override = default;

  AssociatedReceiver<PingService>& receiver() { return receiver_; }

  void set_ping_handler(base::RepeatingClosure handler) {
    ping_handler_ = std::move(handler);
  }

  // PingService:
  void Ping(PingCallback callback) override {
    if (ping_handler_)
      ping_handler_.Run();
    std::move(callback).Run();
  }

 private:
  AssociatedReceiver<PingService> receiver_;
  base::RepeatingClosure ping_handler_;
};

class PingProviderImpl : public AssociatedPingProvider {
 public:
  explicit PingProviderImpl(PendingReceiver<AssociatedPingProvider> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~PingProviderImpl() override = default;

  // AssociatedPingProvider:
  void GetPing(PendingAssociatedReceiver<PingService> receiver) override {
    ping_services_.emplace_back(new PingServiceImpl(std::move(receiver)));

    if (expected_receivers_count_ > 0 &&
        ping_services_.size() == expected_receivers_count_ && quit_waiting_) {
      expected_receivers_count_ = 0;
      std::move(quit_waiting_).Run();
    }
  }

  std::vector<std::unique_ptr<PingServiceImpl>>& ping_services() {
    return ping_services_;
  }

  void WaitForReceivers(size_t count) {
    DCHECK(!quit_waiting_);
    expected_receivers_count_ = count;
    base::RunLoop loop;
    quit_waiting_ = loop.QuitClosure();
    loop.Run();
  }

 private:
  Receiver<AssociatedPingProvider> receiver_;
  std::vector<std::unique_ptr<PingServiceImpl>> ping_services_;
  size_t expected_receivers_count_ = 0;
  base::OnceClosure quit_waiting_;
};

class CallbackFilter : public MessageFilter {
 public:
  explicit CallbackFilter(base::RepeatingClosure callback)
      : callback_(std::move(callback)) {}
  ~CallbackFilter() override = default;

  static std::unique_ptr<CallbackFilter> Wrap(base::RepeatingClosure callback) {
    return std::make_unique<CallbackFilter>(std::move(callback));
  }

  // MessageFilter:
  bool WillDispatch(Message* message) override {
    callback_.Run();
    return true;
  }

  void DidDispatchOrReject(Message* message, bool accepted) override {}

 private:
  base::RepeatingClosure callback_;
};

// Verifies that filters work as expected on associated receivers, i.e. that
// they're notified in order, before dispatch; and that each associated
// receiver in a group operates with its own set of filters.
TEST_F(AssociatedInterfaceTest, ReceiverWithFilters) {
  Remote<AssociatedPingProvider> provider;
  PingProviderImpl provider_impl(provider.BindNewPipeAndPassReceiver());

  AssociatedRemote<PingService> ping_a, ping_b;
  provider->GetPing(ping_a.BindNewEndpointAndPassReceiver());
  provider->GetPing(ping_b.BindNewEndpointAndPassReceiver());
  provider_impl.WaitForReceivers(2);

  ASSERT_EQ(2u, provider_impl.ping_services().size());
  PingServiceImpl& ping_a_impl = *provider_impl.ping_services()[0];
  PingServiceImpl& ping_b_impl = *provider_impl.ping_services()[1];

  int a_status = 0;
  int b_status = 0;

  ping_a_impl.receiver().SetFilter(
      CallbackFilter::Wrap(base::BindLambdaForTesting([&] {
        EXPECT_EQ(0, a_status);
        EXPECT_EQ(0, b_status);
        a_status = 1;
      })));

  ping_b_impl.receiver().SetFilter(
      CallbackFilter::Wrap(base::BindLambdaForTesting([&] {
        EXPECT_EQ(1, a_status);
        EXPECT_EQ(0, b_status);
        b_status = 1;
      })));

  for (int i = 0; i < 10; ++i) {
    a_status = 0;
    b_status = 0;

    {
      base::RunLoop loop;
      ping_a->Ping(loop.QuitClosure());
      loop.Run();
    }

    EXPECT_EQ(1, a_status);
    EXPECT_EQ(0, b_status);

    {
      base::RunLoop loop;
      ping_b->Ping(loop.QuitClosure());
      loop.Run();
    }

    EXPECT_EQ(1, a_status);
    EXPECT_EQ(1, b_status);
  }
}

TEST_F(AssociatedInterfaceTest, AssociatedRemoteFlushForTesting) {
  PendingAssociatedReceiver<IntegerSender> receiver;
  PendingAssociatedRemote<IntegerSender> remote;
  CreateIntegerSender(&remote, &receiver);

  IntegerSenderImpl impl0(std::move(receiver));
  AssociatedRemote<IntegerSender> remote0(std::move(remote));
  remote0.set_disconnect_handler(base::BindOnce(&Fail));

  bool remote0_callback_run = false;
  remote0->Echo(123, base::BindLambdaForTesting([&](int32_t value) {
                  EXPECT_EQ(123, value);
                  remote0_callback_run = true;
                }));
  remote0.FlushForTesting();
  EXPECT_TRUE(remote0_callback_run);
}

TEST_F(AssociatedInterfaceTest, AssociatedRemoteFlushForTestingWithClosedPeer) {
  PendingAssociatedReceiver<IntegerSender> receiver;
  PendingAssociatedRemote<IntegerSender> remote;
  CreateIntegerSender(&remote, &receiver);

  AssociatedRemote<IntegerSender> remote0(std::move(remote));
  bool called = false;
  remote0.set_disconnect_handler(
      base::BindLambdaForTesting([&] { called = true; }));
  receiver.reset();

  remote0.FlushForTesting();
  EXPECT_TRUE(called);
  remote0.FlushForTesting();
}

TEST_F(AssociatedInterfaceTest, AssociatedBindingFlushForTesting) {
  PendingAssociatedReceiver<IntegerSender> receiver;
  PendingAssociatedRemote<IntegerSender> remote;
  CreateIntegerSender(&remote, &receiver);

  IntegerSenderImpl impl0(std::move(receiver));
  impl0.receiver()->set_disconnect_handler(base::BindOnce(&Fail));
  AssociatedRemote<IntegerSender> remote0(std::move(remote));

  bool remote0_callback_run = false;
  remote0->Echo(123, base::BindLambdaForTesting([&](int32_t value) {
                  EXPECT_EQ(123, value);
                  remote0_callback_run = true;
                }));

  // Because the flush is sent from the receiver, it only guarantees that the
  // request has been received, not the response. The second flush waits for the
  // response to be received.
  impl0.receiver()->FlushForTesting();
  impl0.receiver()->FlushForTesting();
  EXPECT_TRUE(remote0_callback_run);
}

TEST_F(AssociatedInterfaceTest,
       AssociatedReceiverFlushForTestingWithClosedPeer) {
  scoped_refptr<MultiplexRouter> router0;
  scoped_refptr<MultiplexRouter> router1;
  CreateRouterPair(&router0, &router1);

  PendingAssociatedReceiver<IntegerSender> receiver;
  {
    PendingAssociatedRemote<IntegerSender> remote;
    CreateIntegerSenderWithExistingRouters(router1, &remote, router0,
                                           &receiver);
  }

  IntegerSenderImpl impl(std::move(receiver));
  bool called = false;
  impl.receiver()->set_disconnect_handler(
      base::BindLambdaForTesting([&] { called = true; }));
  impl.receiver()->FlushForTesting();
  EXPECT_TRUE(called);
  impl.receiver()->FlushForTesting();
}

TEST_F(AssociatedInterfaceTest, ReceiverFlushForTesting) {
  Remote<IntegerSenderConnection> remote;
  IntegerSenderConnectionImpl impl(remote.BindNewPipeAndPassReceiver());
  bool called = false;
  remote->AsyncGetSender(base::BindLambdaForTesting(
      [&](PendingAssociatedRemote<IntegerSender> remote) { called = true; }));
  EXPECT_FALSE(called);
  impl.receiver()->set_disconnect_handler(base::BindOnce(&Fail));

  // Because the flush is sent from the receiver, it only guarantees that the
  // request has been received, not the response. The second flush waits for the
  // response to be received.
  impl.receiver()->FlushForTesting();
  impl.receiver()->FlushForTesting();

  EXPECT_TRUE(called);
}

TEST_F(AssociatedInterfaceTest, ReceiverFlushForTestingWithClosedPeer) {
  Remote<IntegerSenderConnection> remote;
  IntegerSenderConnectionImpl impl(remote.BindNewPipeAndPassReceiver());
  bool called = false;
  impl.receiver()->set_disconnect_handler(
      base::BindLambdaForTesting([&] { called = true; }));
  remote.reset();
  EXPECT_FALSE(called);
  impl.receiver()->FlushForTesting();
  EXPECT_TRUE(called);
  impl.receiver()->FlushForTesting();
}

TEST_F(AssociatedInterfaceTest, RemoteFlushForTesting) {
  Remote<IntegerSenderConnection> remote;
  IntegerSenderConnectionImpl impl(remote.BindNewPipeAndPassReceiver());
  bool called = false;
  remote.set_disconnect_handler(base::BindOnce(&Fail));
  remote->AsyncGetSender(base::BindLambdaForTesting(
      [&](PendingAssociatedRemote<IntegerSender> remote) { called = true; }));
  EXPECT_FALSE(called);
  remote.FlushForTesting();
  EXPECT_TRUE(called);
}

TEST_F(AssociatedInterfaceTest, RemoteFlushForTestingWithClosedPeer) {
  Remote<IntegerSenderConnection> remote;
  ignore_result(remote.BindNewPipeAndPassReceiver());
  bool called = false;
  remote.set_disconnect_handler(
      base::BindLambdaForTesting([&] { called = true; }));
  EXPECT_FALSE(called);
  remote.FlushForTesting();
  EXPECT_TRUE(called);
  remote.FlushForTesting();
}

TEST_F(AssociatedInterfaceTest, AssociatedReceiverConnectionErrorWithReason) {
  PendingAssociatedReceiver<IntegerSender> pending_receiver;
  PendingAssociatedRemote<IntegerSender> pending_remote;
  CreateIntegerSender(&pending_remote, &pending_receiver);

  IntegerSenderImpl impl(std::move(pending_receiver));
  AssociatedRemote<IntegerSender> remote(std::move(pending_remote));

  base::RunLoop run_loop;
  impl.receiver()->set_disconnect_with_reason_handler(
      base::BindLambdaForTesting(
          [&](uint32_t custom_reason, const std::string& description) {
            EXPECT_EQ(123u, custom_reason);
            EXPECT_EQ("farewell", description);
            run_loop.Quit();
          }));

  remote.ResetWithReason(123u, "farewell");

  run_loop.Run();
}

TEST_F(AssociatedInterfaceTest,
       PendingAssociatedReceiverConnectionErrorWithReason) {
  // Test that AssociatedReceiver is notified with connection error when the
  // interface hasn't associated with a message pipe and the peer is closed.

  AssociatedRemote<IntegerSender> remote;
  IntegerSenderImpl impl(remote.BindNewEndpointAndPassReceiver());

  base::RunLoop run_loop;
  impl.receiver()->set_disconnect_with_reason_handler(
      base::BindLambdaForTesting(
          [&](uint32_t custom_reason, const std::string& description) {
            EXPECT_EQ(123u, custom_reason);
            EXPECT_EQ("farewell", description);
            run_loop.Quit();
          }));

  remote.ResetWithReason(123u, "farewell");
  run_loop.Run();
}

TEST_F(AssociatedInterfaceTest, AssociatedRemoteConnectionErrorWithReason) {
  PendingAssociatedReceiver<IntegerSender> pending_receiver;
  PendingAssociatedRemote<IntegerSender> pending_remote;
  CreateIntegerSender(&pending_remote, &pending_receiver);

  IntegerSenderImpl impl(std::move(pending_receiver));
  AssociatedRemote<IntegerSender> remote(std::move(pending_remote));

  base::RunLoop run_loop;
  remote.set_disconnect_with_reason_handler(base::BindLambdaForTesting(
      [&](uint32_t custom_reason, const std::string& description) {
        EXPECT_EQ(456u, custom_reason);
        EXPECT_EQ("farewell", description);
        run_loop.Quit();
      }));

  impl.receiver()->ResetWithReason(456u, "farewell");
  run_loop.Run();
}

TEST_F(AssociatedInterfaceTest,
       PendingAssociatedRemoteConnectionErrorWithReason) {
  // Test that AssociatedInterfacePtr is notified with connection error when the
  // interface hasn't associated with a message pipe and the peer is closed.

  AssociatedRemote<IntegerSender> remote;
  auto pending_receiver = remote.BindNewEndpointAndPassReceiver();

  base::RunLoop run_loop;
  remote.set_disconnect_with_reason_handler(base::BindLambdaForTesting(
      [&](uint32_t custom_reason, const std::string& description) {
        EXPECT_EQ(456u, custom_reason);
        EXPECT_EQ("farewell", description);
        run_loop.Quit();
      }));

  pending_receiver.ResetWithReason(456u, "farewell");
  run_loop.Run();
}

TEST_F(AssociatedInterfaceTest, AssociatedRequestResetWithReason) {
  PendingAssociatedReceiver<IntegerSender> pending_receiver;
  PendingAssociatedRemote<IntegerSender> pending_remote;
  CreateIntegerSender(&pending_remote, &pending_receiver);

  AssociatedRemote<IntegerSender> remote(std::move(pending_remote));

  base::RunLoop run_loop;
  remote.set_disconnect_with_reason_handler(base::BindLambdaForTesting(
      [&](uint32_t custom_reason, const std::string& description) {
        EXPECT_EQ(789u, custom_reason);
        EXPECT_EQ("long time no see", description);
        run_loop.Quit();
      }));

  pending_receiver.ResetWithReason(789u, "long time no see");

  run_loop.Run();
}

TEST_F(AssociatedInterfaceTest, SharedAssociatedRemote) {
  Remote<IntegerSenderConnection> connection_remote;
  IntegerSenderConnectionImpl connection(
      connection_remote.BindNewPipeAndPassReceiver());

  PendingAssociatedRemote<IntegerSender> pending_remote;
  connection_remote->GetSender(
      pending_remote.InitWithNewEndpointAndPassReceiver());

  SharedAssociatedRemote<IntegerSender> shared_sender(
      std::move(pending_remote));

  {
    // Test the thread safe pointer can be used from the interface ptr thread.
    base::RunLoop run_loop;
    shared_sender->Echo(123, base::BindLambdaForTesting([&](int32_t value) {
                          EXPECT_EQ(123, value);
                          run_loop.Quit();
                        }));
    run_loop.Run();
  }

  // Test the thread safe pointer can be used from another thread.
  base::RunLoop run_loop;

  auto sender_task_runner =
      base::CreateSequencedTaskRunner({base::ThreadPool()});
  auto quit_closure = run_loop.QuitClosure();
  sender_task_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        shared_sender->Echo(
            123, base::BindLambdaForTesting([&](int32_t value) {
              EXPECT_EQ(123, value);
              EXPECT_TRUE(sender_task_runner->RunsTasksInCurrentSequence());
              std::move(quit_closure).Run();
            }));
      }));

  // Block until the method callback is called on the background thread.
  run_loop.Run();
}

struct ForwarderTestContext {
  Remote<IntegerSenderConnection> connection_remote;
  std::unique_ptr<IntegerSenderConnectionImpl> interface_impl;
  PendingAssociatedReceiver<IntegerSender> sender_receiver;
};

TEST_F(AssociatedInterfaceTest, SharedAssociatedRemoteWithTaskRunner) {
  const scoped_refptr<base::SequencedTaskRunner> other_thread_task_runner =
      base::CreateSequencedTaskRunner({base::ThreadPool()});

  ForwarderTestContext* context = new ForwarderTestContext();
  PendingAssociatedRemote<IntegerSender> pending_remote;
  base::WaitableEvent sender_bound_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  other_thread_task_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        context->interface_impl = std::make_unique<IntegerSenderConnectionImpl>(
            context->connection_remote.BindNewPipeAndPassReceiver());
        context->connection_remote->GetSender(
            pending_remote.InitWithNewEndpointAndPassReceiver());
        sender_bound_event.Signal();
      }));

  sender_bound_event.Wait();

  // Create a SharedAssociatedRemote that binds on the background thread and is
  // associated with |connection_remote| there.
  SharedAssociatedRemote<IntegerSender> shared_sender(std::move(pending_remote),
                                                      other_thread_task_runner);

  // Issue a call on the shared remote immediately. Note that this may happen
  // before the interface is bound on the background thread, and that must be
  // OK.
  base::RunLoop run_loop;
  shared_sender->Echo(123, base::BindLambdaForTesting([&](int32_t value) {
                        EXPECT_EQ(123, value);
                        run_loop.Quit();
                      }));
  run_loop.Run();

  other_thread_task_runner->DeleteSoon(FROM_HERE, context);

  shared_sender.reset();
}

class DiscardingAssociatedPingProviderProvider
    : public AssociatedPingProviderProvider {
 public:
  void GetPingProvider(
      PendingAssociatedReceiver<AssociatedPingProvider> receiver) override {}
};

TEST_F(AssociatedInterfaceTest, CloseWithoutBindingAssociatedReceiver) {
  DiscardingAssociatedPingProviderProvider ping_provider_provider;
  mojo::Receiver<AssociatedPingProviderProvider> receiver(
      &ping_provider_provider);
  Remote<AssociatedPingProviderProvider> provider_provider;
  receiver.Bind(provider_provider.BindNewPipeAndPassReceiver());
  AssociatedRemote<AssociatedPingProvider> provider;
  provider_provider->GetPingProvider(provider.BindNewEndpointAndPassReceiver());
  AssociatedRemote<PingService> ping;
  provider->GetPing(ping.BindNewEndpointAndPassReceiver());
  base::RunLoop run_loop;
  ping.set_disconnect_handler(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(AssociatedInterfaceTest, AssociateWithDisconnectedPipe) {
  AssociatedRemote<IntegerSender> sender;
  AssociateWithDisconnectedPipe(
      sender.BindNewEndpointAndPassReceiver().PassHandle());
  sender->Send(42);
}

TEST_F(AssociatedInterfaceTest, AsyncErrorHandlersWhenClosingMasterInterface) {
  // Ensures that associated interface error handlers are not invoked
  // synchronously when the master interface pipe is closed. Regression test for
  // https://crbug.com/864731.

  Remote<IntegerSenderConnection> connection_remote;
  IntegerSenderConnectionImpl connection(
      connection_remote.BindNewPipeAndPassReceiver());

  base::RunLoop loop;
  bool error_handler_invoked = false;
  AssociatedRemote<IntegerSender> sender0;
  connection_remote->GetSender(sender0.BindNewEndpointAndPassReceiver());
  sender0.set_disconnect_handler(base::BindLambdaForTesting([&] {
    error_handler_invoked = true;
    loop.Quit();
  }));

  // This should not trigger the error handler synchronously...
  connection_remote.reset();
  EXPECT_FALSE(error_handler_invoked);

  // ...but it should be triggered once we spin the scheduler.
  loop.Run();
  EXPECT_TRUE(error_handler_invoked);
}

}  // namespace
}  // namespace test
}  // namespace mojo

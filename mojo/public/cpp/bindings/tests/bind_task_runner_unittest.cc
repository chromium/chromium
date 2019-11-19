// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/interfaces/bindings/tests/test_associated_interfaces.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

class TestTaskRunner : public base::SequencedTaskRunner {
 public:
  TestTaskRunner()
      : thread_id_(base::PlatformThread::CurrentRef()),
        quit_called_(false),
        task_ready_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                    base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    NOTREACHED();
    return false;
  }

  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    {
      base::AutoLock locker(lock_);
      tasks_.push(std::move(task));
    }
    task_ready_.Signal();
    return true;
  }
  bool RunsTasksInCurrentSequence() const override {
    return base::PlatformThread::CurrentRef() == thread_id_;
  }

  // Only quits when Quit() is called.
  void Run() {
    DCHECK(RunsTasksInCurrentSequence());
    quit_called_ = false;

    while (true) {
      {
        base::AutoLock locker(lock_);
        while (!tasks_.empty()) {
          auto task = std::move(tasks_.front());
          tasks_.pop();

          {
            base::AutoUnlock unlocker(lock_);
            std::move(task).Run();
            if (quit_called_)
              return;
          }
        }
      }
      task_ready_.Wait();
    }
  }

  void Quit() {
    DCHECK(RunsTasksInCurrentSequence());
    quit_called_ = true;
  }

  // Waits until one task is ready and runs it.
  void RunOneTask() {
    DCHECK(RunsTasksInCurrentSequence());

    while (true) {
      {
        base::AutoLock locker(lock_);
        if (!tasks_.empty()) {
          auto task = std::move(tasks_.front());
          tasks_.pop();

          {
            base::AutoUnlock unlocker(lock_);
            std::move(task).Run();
            return;
          }
        }
      }
      task_ready_.Wait();
    }
  }

 private:
  ~TestTaskRunner() override {}

  const base::PlatformThreadRef thread_id_;
  bool quit_called_;
  base::WaitableEvent task_ready_;

  // Protect |tasks_|.
  base::Lock lock_;
  base::queue<base::OnceClosure> tasks_;

  DISALLOW_COPY_AND_ASSIGN(TestTaskRunner);
};

template <typename ReceiverType, typename PendingReceiverType>
class IntegerSenderImpl : public IntegerSender {
 public:
  IntegerSenderImpl(PendingReceiverType receiver,
                    scoped_refptr<base::SequencedTaskRunner> runner)
      : receiver_(this, std::move(receiver), std::move(runner)) {}

  ~IntegerSenderImpl() override = default;

  using EchoHandler = base::RepeatingCallback<void(int32_t, EchoCallback)>;

  void set_echo_handler(const EchoHandler& handler) { echo_handler_ = handler; }

  void Echo(int32_t value, EchoCallback callback) override {
    if (echo_handler_.is_null())
      std::move(callback).Run(value);
    else
      echo_handler_.Run(value, std::move(callback));
  }
  void Send(int32_t value) override { NOTREACHED(); }

  ReceiverType* receiver() { return &receiver_; }

 private:
  ReceiverType receiver_;
  EchoHandler echo_handler_;
};

class IntegerSenderConnectionImpl : public IntegerSenderConnection {
 public:
  using SenderType =
      IntegerSenderImpl<AssociatedReceiver<IntegerSender>,
                        PendingAssociatedReceiver<IntegerSender>>;

  explicit IntegerSenderConnectionImpl(
      PendingReceiver<IntegerSenderConnection> receiver,
      scoped_refptr<base::SequencedTaskRunner> runner,
      scoped_refptr<base::SequencedTaskRunner> sender_runner)
      : receiver_(this, std::move(receiver), std::move(runner)),
        sender_runner_(std::move(sender_runner)) {}

  ~IntegerSenderConnectionImpl() override = default;

  void set_get_sender_notification(base::OnceClosure notification) {
    get_sender_notification_ = std::move(notification);
  }
  void GetSender(PendingAssociatedReceiver<IntegerSender> receiver) override {
    sender_impl_ =
        std::make_unique<SenderType>(std::move(receiver), sender_runner_);
    std::move(get_sender_notification_).Run();
  }

  void AsyncGetSender(AsyncGetSenderCallback callback) override {
    NOTREACHED();
  }

  Receiver<IntegerSenderConnection>* receiver() { return &receiver_; }

  SenderType* sender_impl() { return sender_impl_.get(); }

 private:
  Receiver<IntegerSenderConnection> receiver_;
  std::unique_ptr<SenderType> sender_impl_;
  scoped_refptr<base::SequencedTaskRunner> sender_runner_;
  base::OnceClosure get_sender_notification_;
};

class BindTaskRunnerTest : public testing::Test {
 protected:
  void SetUp() override {
    receiver_task_runner_ = scoped_refptr<TestTaskRunner>(new TestTaskRunner);
    remote_task_runner_ = scoped_refptr<TestTaskRunner>(new TestTaskRunner);

    auto receiver = remote_.BindNewPipeAndPassReceiver(remote_task_runner_);
    impl_.reset(new ImplType(std::move(receiver), receiver_task_runner_));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<TestTaskRunner> receiver_task_runner_;
  scoped_refptr<TestTaskRunner> remote_task_runner_;

  Remote<IntegerSender> remote_;
  using ImplType = IntegerSenderImpl<Receiver<IntegerSender>,
                                     PendingReceiver<IntegerSender>>;
  std::unique_ptr<ImplType> impl_;
};

class AssociatedBindTaskRunnerTest : public testing::Test {
 protected:
  void SetUp() override {
    connection_receiver_task_runner_ =
        scoped_refptr<TestTaskRunner>(new TestTaskRunner);
    connection_remote_task_runner_ =
        scoped_refptr<TestTaskRunner>(new TestTaskRunner);
    sender_receiver_task_runner_ =
        scoped_refptr<TestTaskRunner>(new TestTaskRunner);
    sender_remote_task_runner_ =
        scoped_refptr<TestTaskRunner>(new TestTaskRunner);

    auto connection_receiver = connection_remote_.BindNewPipeAndPassReceiver(
        connection_remote_task_runner_);
    connection_impl_.reset(new IntegerSenderConnectionImpl(
        std::move(connection_receiver), connection_receiver_task_runner_,
        sender_receiver_task_runner_));

    connection_impl_->set_get_sender_notification(base::BindOnce(
        &AssociatedBindTaskRunnerTest::QuitTaskRunner, base::Unretained(this)));

    connection_remote_->GetSender(sender_remote_.BindNewEndpointAndPassReceiver(
        sender_remote_task_runner_));
    connection_receiver_task_runner_->Run();
  }

  void QuitTaskRunner() { connection_receiver_task_runner_->Quit(); }

  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<TestTaskRunner> connection_receiver_task_runner_;
  scoped_refptr<TestTaskRunner> connection_remote_task_runner_;
  scoped_refptr<TestTaskRunner> sender_receiver_task_runner_;
  scoped_refptr<TestTaskRunner> sender_remote_task_runner_;

  Remote<IntegerSenderConnection> connection_remote_;
  std::unique_ptr<IntegerSenderConnectionImpl> connection_impl_;
  AssociatedRemote<IntegerSender> sender_remote_;
};

TEST_F(BindTaskRunnerTest, MethodCall) {
  bool echo_called = false;
  impl_->set_echo_handler(base::BindLambdaForTesting(
      [&](int32_t value, IntegerSender::EchoCallback callback) {
        EXPECT_EQ(1024, value);
        echo_called = true;
        std::move(callback).Run(value);
        receiver_task_runner_->Quit();
      }));

  bool echo_replied = false;
  remote_->Echo(1024, base::BindLambdaForTesting([&](int32_t value) {
                  EXPECT_EQ(1024, value);
                  echo_replied = true;
                  remote_task_runner_->Quit();
                }));
  receiver_task_runner_->Run();
  EXPECT_TRUE(echo_called);
  remote_task_runner_->Run();
  EXPECT_TRUE(echo_replied);
}

TEST_F(BindTaskRunnerTest, ReceiverDisconnectHandler) {
  bool disconnected = false;
  impl_->receiver()->set_disconnect_handler(base::BindLambdaForTesting([&] {
    disconnected = true;
    receiver_task_runner_->Quit();
  }));
  remote_.reset();
  receiver_task_runner_->Run();
  EXPECT_TRUE(disconnected);
}

TEST_F(BindTaskRunnerTest, RemoteDisconnectHandler) {
  bool disconnected = false;
  remote_.set_disconnect_handler(base::BindLambdaForTesting([&] {
    disconnected = true;
    remote_task_runner_->Quit();
  }));
  impl_->receiver()->reset();
  remote_task_runner_->Run();
  EXPECT_TRUE(disconnected);
}

TEST_F(AssociatedBindTaskRunnerTest, MethodCall) {
  bool echo_called = false;
  connection_impl_->sender_impl()->set_echo_handler(base::BindLambdaForTesting(
      [&](int32_t value, IntegerSender::EchoCallback callback) {
        EXPECT_EQ(1024, value);
        echo_called = true;
        std::move(callback).Run(value);
      }));

  bool echo_replied = false;
  sender_remote_->Echo(1024, base::BindLambdaForTesting([&](int32_t value) {
                         EXPECT_EQ(1024, value);
                         echo_replied = true;
                       }));

  // The Echo request first arrives at the master endpoint's task runner, and
  // then is forwarded to the associated endpoint's task runner.
  connection_receiver_task_runner_->RunOneTask();
  sender_receiver_task_runner_->RunOneTask();
  EXPECT_TRUE(echo_called);

  // Similarly, the Echo response arrives at the master endpoint's task runner
  // and then is forwarded to the associated endpoint's task runner.
  connection_remote_task_runner_->RunOneTask();
  sender_remote_task_runner_->RunOneTask();
  EXPECT_TRUE(echo_replied);
}

TEST_F(AssociatedBindTaskRunnerTest, ReceiverDisconnectHandler) {
  bool sender_impl_disconnected = false;
  connection_impl_->sender_impl()->receiver()->set_disconnect_handler(
      base::BindLambdaForTesting([&] {
        sender_impl_disconnected = true;
        sender_receiver_task_runner_->Quit();
      }));
  bool connection_impl_disconnected = false;
  connection_impl_->receiver()->set_disconnect_handler(
      base::BindLambdaForTesting([&] {
        connection_impl_disconnected = true;
        connection_receiver_task_runner_->Quit();
      }));
  bool sender_remote_disconnected = false;
  sender_remote_.set_disconnect_handler(base::BindLambdaForTesting([&] {
    sender_remote_disconnected = true;
    sender_remote_task_runner_->Quit();
  }));
  connection_remote_.reset();
  sender_remote_task_runner_->Run();
  EXPECT_TRUE(sender_remote_disconnected);
  connection_receiver_task_runner_->Run();
  EXPECT_TRUE(connection_impl_disconnected);
  sender_receiver_task_runner_->Run();
  EXPECT_TRUE(sender_impl_disconnected);
}

TEST_F(AssociatedBindTaskRunnerTest, RemoteDisconnectHandler) {
  bool sender_impl_disconnected = false;
  connection_impl_->sender_impl()->receiver()->set_disconnect_handler(
      base::BindLambdaForTesting([&] {
        sender_impl_disconnected = true;
        sender_receiver_task_runner_->Quit();
      }));
  bool connection_remote_disconnected = false;
  connection_remote_.set_disconnect_handler(base::BindLambdaForTesting([&] {
    connection_remote_disconnected = true;
    connection_remote_task_runner_->Quit();
  }));
  bool sender_remote_disconnected = false;
  sender_remote_.set_disconnect_handler(base::BindLambdaForTesting([&] {
    sender_remote_disconnected = true;
    sender_remote_task_runner_->Quit();
  }));
  connection_impl_->receiver()->reset();
  sender_receiver_task_runner_->Run();
  EXPECT_TRUE(sender_impl_disconnected);
  connection_remote_task_runner_->Run();
  EXPECT_TRUE(connection_remote_disconnected);
  sender_remote_task_runner_->Run();
  EXPECT_TRUE(sender_remote_disconnected);
}

}  // namespace
}  // namespace test
}  // namespace mojo

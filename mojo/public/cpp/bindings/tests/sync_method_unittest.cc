// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/sequence_token.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/interfaces/bindings/tests/test_sync_methods.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

class TestSyncCommonImpl {
 public:
  TestSyncCommonImpl() = default;

  using PingHandler = base::RepeatingCallback<void(base::OnceClosure)>;
  template <typename Func>
  void set_ping_handler(Func handler) {
    ping_handler_ = base::BindLambdaForTesting(handler);
  }

  using EchoHandler =
      base::RepeatingCallback<void(int32_t, base::OnceCallback<void(int32_t)>)>;
  template <typename Func>
  void set_echo_handler(Func handler) {
    echo_handler_ = base::BindLambdaForTesting(handler);
  }

  using AsyncEchoHandler =
      base::RepeatingCallback<void(int32_t, base::OnceCallback<void(int32_t)>)>;
  template <typename Func>
  void set_async_echo_handler(Func handler) {
    async_echo_handler_ = base::BindLambdaForTesting(handler);
  }

  using SendRemoteHandler =
      base::RepeatingCallback<void(PendingAssociatedRemote<TestSync>)>;
  template <typename Func>
  void set_send_remote_handler(Func handler) {
    send_remote_handler_ = base::BindLambdaForTesting(handler);
  }

  using SendReceiverHandler =
      base::RepeatingCallback<void(PendingAssociatedReceiver<TestSync>)>;
  template <typename Func>
  void set_send_receiver_handler(Func handler) {
    send_receiver_handler_ = base::BindLambdaForTesting(handler);
  }

  void PingImpl(base::OnceCallback<void()> callback) {
    if (ping_handler_.is_null()) {
      std::move(callback).Run();
      return;
    }
    ping_handler_.Run(std::move(callback));
  }
  void EchoImpl(int32_t value, base::OnceCallback<void(int32_t)> callback) {
    if (echo_handler_.is_null()) {
      std::move(callback).Run(value);
      return;
    }
    echo_handler_.Run(value, std::move(callback));
  }
  void AsyncEchoImpl(int32_t value,
                     base::OnceCallback<void(int32_t)> callback) {
    if (async_echo_handler_.is_null()) {
      std::move(callback).Run(value);
      return;
    }
    async_echo_handler_.Run(value, std::move(callback));
  }
  void SendRemoteImpl(PendingAssociatedRemote<TestSync> remote) {
    send_remote_handler_.Run(std::move(remote));
  }
  void SendReceiverImpl(PendingAssociatedReceiver<TestSync> receiver) {
    send_receiver_handler_.Run(std::move(receiver));
  }

 private:
  PingHandler ping_handler_;
  EchoHandler echo_handler_;
  AsyncEchoHandler async_echo_handler_;
  SendRemoteHandler send_remote_handler_;
  SendReceiverHandler send_receiver_handler_;

  DISALLOW_COPY_AND_ASSIGN(TestSyncCommonImpl);
};

class TestSyncImpl : public TestSync, public TestSyncCommonImpl {
 public:
  explicit TestSyncImpl(PendingReceiver<TestSync> receiver)
      : receiver_(this, std::move(receiver)) {}

  // TestSync implementation:
  void Ping(PingCallback callback) override { PingImpl(std::move(callback)); }
  void Echo(int32_t value, EchoCallback callback) override {
    EchoImpl(value, std::move(callback));
  }
  void AsyncEcho(int32_t value, AsyncEchoCallback callback) override {
    AsyncEchoImpl(value, std::move(callback));
  }

  Receiver<TestSync>* receiver() { return &receiver_; }

 private:
  Receiver<TestSync> receiver_;

  DISALLOW_COPY_AND_ASSIGN(TestSyncImpl);
};

class TestSyncMasterImpl : public TestSyncMaster, public TestSyncCommonImpl {
 public:
  explicit TestSyncMasterImpl(PendingReceiver<TestSyncMaster> receiver)
      : receiver_(this, std::move(receiver)) {}

  // TestSyncMaster implementation:
  void Ping(PingCallback callback) override { PingImpl(std::move(callback)); }
  void Echo(int32_t value, EchoCallback callback) override {
    EchoImpl(value, std::move(callback));
  }
  void AsyncEcho(int32_t value, AsyncEchoCallback callback) override {
    AsyncEchoImpl(value, std::move(callback));
  }
  void SendRemote(PendingAssociatedRemote<TestSync> remote) override {
    SendRemoteImpl(std::move(remote));
  }
  void SendReceiver(PendingAssociatedReceiver<TestSync> receiver) override {
    SendReceiverImpl(std::move(receiver));
  }

  Receiver<TestSyncMaster>* receiver() { return &receiver_; }

 private:
  Receiver<TestSyncMaster> receiver_;

  DISALLOW_COPY_AND_ASSIGN(TestSyncMasterImpl);
};

class TestSyncAssociatedImpl : public TestSync, public TestSyncCommonImpl {
 public:
  explicit TestSyncAssociatedImpl(PendingAssociatedReceiver<TestSync> receiver)
      : receiver_(this, std::move(receiver)) {}

  // TestSync implementation:
  void Ping(PingCallback callback) override { PingImpl(std::move(callback)); }
  void Echo(int32_t value, EchoCallback callback) override {
    EchoImpl(value, std::move(callback));
  }
  void AsyncEcho(int32_t value, AsyncEchoCallback callback) override {
    AsyncEchoImpl(value, std::move(callback));
  }

  AssociatedReceiver<TestSync>* receiver() { return &receiver_; }

 private:
  AssociatedReceiver<TestSync> receiver_;

  DISALLOW_COPY_AND_ASSIGN(TestSyncAssociatedImpl);
};

template <typename Interface>
struct ImplTraits;

template <>
struct ImplTraits<TestSync> {
  using Type = TestSyncImpl;
};

template <>
struct ImplTraits<TestSyncMaster> {
  using Type = TestSyncMasterImpl;
};

template <typename Interface>
using ImplTypeFor = typename ImplTraits<Interface>::Type;

// A wrapper for either a Remote or SharedRemote that exposes the Remote
// interface.
template <typename Interface>
class RemoteWrapper {
 public:
  explicit RemoteWrapper(Remote<Interface> remote)
      : remote_(std::move(remote)) {}

  explicit RemoteWrapper(SharedRemote<Interface> shared_remote)
      : shared_remote_(std::move(shared_remote)) {}

  RemoteWrapper(RemoteWrapper&& other) = default;

  Interface* operator->() {
    return shared_remote_ ? shared_remote_.get() : remote_.get();
  }

  void set_disconnect_handler(base::OnceClosure handler) {
    DCHECK(!shared_remote_);
    remote_.set_disconnect_handler(std::move(handler));
  }

  void reset() {
    remote_.reset();
    shared_remote_.reset();
  }

 private:
  Remote<Interface> remote_;
  SharedRemote<Interface> shared_remote_;

  DISALLOW_COPY_AND_ASSIGN(RemoteWrapper);
};

// The type parameter for SyncMethodCommonTests and
// SyncMethodOnSequenceCommonTests for varying the Interface and whether to use
// Remote or SharedRemote.
template <typename InterfaceT,
          bool use_shared_remote,
          BindingsTestSerializationMode serialization_mode>
struct TestParams {
  using Interface = InterfaceT;
  static const bool kIsSharedRemoteTest = use_shared_remote;

  static RemoteWrapper<InterfaceT> Wrap(PendingRemote<Interface> remote) {
    if (kIsSharedRemoteTest) {
      return RemoteWrapper<Interface>(
          SharedRemote<Interface>(std::move(remote)));
    } else {
      return RemoteWrapper<Interface>(Remote<Interface>(std::move(remote)));
    }
  }

  static const BindingsTestSerializationMode kSerializationMode =
      serialization_mode;
};

template <typename Interface>
class TestSyncServiceSequence {
 public:
  TestSyncServiceSequence()
      : task_runner_(base::CreateSequencedTaskRunner({base::ThreadPool()})),
        ping_called_(false) {}

  void SetUp(InterfaceRequest<Interface> request) {
    CHECK(task_runner()->RunsTasksInCurrentSequence());
    impl_ = std::make_unique<ImplTypeFor<Interface>>(std::move(request));
    impl_->set_ping_handler([this](typename Interface::PingCallback callback) {
      {
        base::AutoLock locker(lock_);
        ping_called_ = true;
      }
      std::move(callback).Run();
    });
  }

  void TearDown() {
    CHECK(task_runner()->RunsTasksInCurrentSequence());
    impl_.reset();
  }

  base::SequencedTaskRunner* task_runner() { return task_runner_.get(); }
  bool ping_called() const {
    base::AutoLock locker(lock_);
    return ping_called_;
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<ImplTypeFor<Interface>> impl_;

  mutable base::Lock lock_;
  bool ping_called_;

  DISALLOW_COPY_AND_ASSIGN(TestSyncServiceSequence);
};

class SyncMethodTest : public testing::Test {
 public:
  SyncMethodTest() = default;
  ~SyncMethodTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment;
};

template <typename TypeParam>
class SyncMethodCommonTest : public SyncMethodTest {
 public:
  SyncMethodCommonTest() = default;
  ~SyncMethodCommonTest() override = default;

  void SetUp() override {
    BindingsTestBase::SetupSerializationBehavior(TypeParam::kSerializationMode);
  }
};

class SyncMethodAssociatedTest : public SyncMethodTest {
 public:
  SyncMethodAssociatedTest() = default;
  ~SyncMethodAssociatedTest() override = default;

 protected:
  void SetUp() override {
    master_impl_ = std::make_unique<TestSyncMasterImpl>(
        master_remote_.BindNewPipeAndPassReceiver());

    impl_pending_sync_receiver_ =
        impl_pending_sync_remote_.InitWithNewEndpointAndPassReceiver();
    client_pending_sync_receiver_ =
        client_pending_sync_remote_.InitWithNewEndpointAndPassReceiver();

    master_impl_->set_send_remote_handler(
        [this](PendingAssociatedRemote<TestSync> remote) {
          client_pending_sync_remote_ = std::move(remote);
        });
    base::RunLoop run_loop;
    master_impl_->set_send_receiver_handler(
        [this, &run_loop](PendingAssociatedReceiver<TestSync> receiver) {
          impl_pending_sync_receiver_ = std::move(receiver);
          run_loop.Quit();
        });

    master_remote_->SendRemote(std::move(client_pending_sync_remote_));
    master_remote_->SendReceiver(std::move(impl_pending_sync_receiver_));
    run_loop.Run();
  }

  void TearDown() override {
    impl_pending_sync_remote_.reset();
    impl_pending_sync_receiver_.reset();
    client_pending_sync_remote_.reset();
    client_pending_sync_receiver_.reset();
    master_remote_.reset();
    master_impl_.reset();
  }

  Remote<TestSyncMaster> master_remote_;
  std::unique_ptr<TestSyncMasterImpl> master_impl_;

  // An associated interface whose receiver lives at the |master_impl_| side.
  PendingAssociatedRemote<TestSync> impl_pending_sync_remote_;
  PendingAssociatedReceiver<TestSync> impl_pending_sync_receiver_;

  // An associated interface whose receiver lives at the |master_remote_| side.
  PendingAssociatedRemote<TestSync> client_pending_sync_remote_;
  PendingAssociatedReceiver<TestSync> client_pending_sync_receiver_;
};

class SequencedTaskRunnerTestBase;

void RunTestOnSequencedTaskRunner(
    std::unique_ptr<SequencedTaskRunnerTestBase> test);

class SequencedTaskRunnerTestBase {
 public:
  virtual ~SequencedTaskRunnerTestBase() = default;

  void RunTest() {
    SetUp();
    Run();
  }

  virtual void Run() = 0;

  virtual void SetUp() {}
  virtual void TearDown() {}

 protected:
  void Done() {
    TearDown();
    task_runner_->PostTask(FROM_HERE, std::move(quit_closure_));
    delete this;
  }

  base::OnceClosure DoneClosure() {
    return base::BindOnce(&SequencedTaskRunnerTestBase::Done,
                          base::Unretained(this));
  }

 private:
  friend void RunTestOnSequencedTaskRunner(
      std::unique_ptr<SequencedTaskRunnerTestBase> test);

  void Init(base::OnceClosure quit_closure) {
    task_runner_ = base::SequencedTaskRunnerHandle::Get();
    quit_closure_ = std::move(quit_closure);
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::OnceClosure quit_closure_;
};

// A helper class to launch tests on a SequencedTaskRunner. This is necessary
// so gtest can instantiate copies for each |TypeParam|.
template <typename TypeParam>
class SequencedTaskRunnerTestLauncher : public testing::Test {
  base::test::TaskEnvironment task_environment;
};

// Similar to SyncMethodCommonTest, but the test body runs on a
// SequencedTaskRunner.
template <typename TypeParam>
class SyncMethodOnSequenceCommonTest : public SequencedTaskRunnerTestBase {
 public:
  void SetUp() override {
    BindingsTestBase::SetupSerializationBehavior(TypeParam::kSerializationMode);
    impl_ = std::make_unique<ImplTypeFor<typename TypeParam::Interface>>(
        remote_.BindNewPipeAndPassReceiver());
  }

 protected:
  Remote<typename TypeParam::Interface> remote_;
  std::unique_ptr<ImplTypeFor<typename TypeParam::Interface>> impl_;
};

void RunTestOnSequencedTaskRunner(
    std::unique_ptr<SequencedTaskRunnerTestBase> test) {
  base::RunLoop run_loop;
  test->Init(run_loop.QuitClosure());
  base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::WithBaseSyncPrimitives()})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&SequencedTaskRunnerTestBase::RunTest,
                                base::Unretained(test.release())));
  run_loop.Run();
}

// TestSync (without associated interfaces) and TestSyncMaster (with associated
// interfaces) exercise MultiplexRouter with different configurations.
// Each test is run once with a Remote and once with a SharedRemote to ensure
// that they behave the same with respect to sync calls. Finally, all such
// combinations are tested in different message serialization modes.
using InterfaceTypes = testing::Types<
    TestParams<TestSync,
               true,
               BindingsTestSerializationMode::kSerializeBeforeSend>,
    TestParams<TestSync,
               false,
               BindingsTestSerializationMode::kSerializeBeforeSend>,
    TestParams<TestSyncMaster,
               true,
               BindingsTestSerializationMode::kSerializeBeforeSend>,
    TestParams<TestSyncMaster,
               false,
               BindingsTestSerializationMode::kSerializeBeforeSend>,
    TestParams<TestSync,
               true,
               BindingsTestSerializationMode::kSerializeBeforeDispatch>,
    TestParams<TestSync,
               false,
               BindingsTestSerializationMode::kSerializeBeforeDispatch>,
    TestParams<TestSyncMaster,
               true,
               BindingsTestSerializationMode::kSerializeBeforeDispatch>,
    TestParams<TestSyncMaster,
               false,
               BindingsTestSerializationMode::kSerializeBeforeDispatch>,
    TestParams<TestSync, true, BindingsTestSerializationMode::kNeverSerialize>,
    TestParams<TestSync, false, BindingsTestSerializationMode::kNeverSerialize>,
    TestParams<TestSyncMaster,
               true,
               BindingsTestSerializationMode::kNeverSerialize>,
    TestParams<TestSyncMaster,
               false,
               BindingsTestSerializationMode::kNeverSerialize>>;

TYPED_TEST_SUITE(SyncMethodCommonTest, InterfaceTypes);
TYPED_TEST_SUITE(SequencedTaskRunnerTestLauncher, InterfaceTypes);

TYPED_TEST(SyncMethodCommonTest, CallSyncMethodAsynchronously) {
  using Interface = typename TypeParam::Interface;
  PendingRemote<Interface> remote;
  ImplTypeFor<Interface> impl(remote.InitWithNewPipeAndPassReceiver());
  auto wrapped_remote = TypeParam::Wrap(std::move(remote));

  base::RunLoop run_loop;
  wrapped_remote->Echo(123, base::BindLambdaForTesting([&](int32_t value) {
                         EXPECT_EQ(123, value);
                         run_loop.Quit();
                       }));
  run_loop.Run();
}

#define SEQUENCED_TASK_RUNNER_TYPED_TEST_NAME(fixture_name, name) \
  fixture_name##name##_SequencedTaskRunnerTestSuffix

#define SEQUENCED_TASK_RUNNER_TYPED_TEST(fixture_name, name)        \
  template <typename TypeParam>                                     \
  class SEQUENCED_TASK_RUNNER_TYPED_TEST_NAME(fixture_name, name)   \
      : public fixture_name<TypeParam> {                            \
    void Run() override;                                            \
  };                                                                \
  TYPED_TEST(SequencedTaskRunnerTestLauncher, name) {               \
    RunTestOnSequencedTaskRunner(                                   \
        std::make_unique<SEQUENCED_TASK_RUNNER_TYPED_TEST_NAME(     \
                             fixture_name, name) < TypeParam>> ()); \
  }                                                                 \
  template <typename TypeParam>                                     \
  void SEQUENCED_TASK_RUNNER_TYPED_TEST_NAME(fixture_name,          \
                                             name)<TypeParam>::Run()

#define SEQUENCED_TASK_RUNNER_TYPED_TEST_F(fixture_name, name)      \
  template <typename TypeParam>                                     \
  class SEQUENCED_TASK_RUNNER_TYPED_TEST_NAME(fixture_name, name);  \
  TYPED_TEST(SequencedTaskRunnerTestLauncher, name) {               \
    RunTestOnSequencedTaskRunner(                                   \
        std::make_unique<SEQUENCED_TASK_RUNNER_TYPED_TEST_NAME(     \
                             fixture_name, name) < TypeParam>> ()); \
  }                                                                 \
  template <typename TypeParam>                                     \
  class SEQUENCED_TASK_RUNNER_TYPED_TEST_NAME(fixture_name, name)   \
      : public fixture_name<TypeParam>

SEQUENCED_TASK_RUNNER_TYPED_TEST(SyncMethodOnSequenceCommonTest,
                                 CallSyncMethodAsynchronously) {
  this->remote_->Echo(123, base::BindLambdaForTesting([&](int32_t value) {
                        EXPECT_EQ(123, value);
                        this->DoneClosure().Run();
                      }));
}

TYPED_TEST(SyncMethodCommonTest, BasicSyncCalls) {
  using Interface = typename TypeParam::Interface;
  PendingRemote<Interface> remote;
  auto receiver = remote.InitWithNewPipeAndPassReceiver();
  auto wrapped_remote = TypeParam::Wrap(std::move(remote));

  TestSyncServiceSequence<Interface> service_sequence;
  service_sequence.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TestSyncServiceSequence<Interface>::SetUp,
                     base::Unretained(&service_sequence), std::move(receiver)));
  ASSERT_TRUE(wrapped_remote->Ping());
  ASSERT_TRUE(service_sequence.ping_called());

  int32_t output_value = -1;
  ASSERT_TRUE(wrapped_remote->Echo(42, &output_value));
  ASSERT_EQ(42, output_value);

  base::RunLoop run_loop;
  service_sequence.task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&TestSyncServiceSequence<Interface>::TearDown,
                     base::Unretained(&service_sequence)),
      run_loop.QuitClosure());
  run_loop.Run();
}

TYPED_TEST(SyncMethodCommonTest, ReenteredBySyncMethodReceiver) {
  // Test that a remote waiting for a sync call response can be reentered by a
  // receiver serving sync methods on the same thread.

  using Interface = typename TypeParam::Interface;
  PendingRemote<Interface> remote;
  // The binding lives on the same thread as the interface pointer.
  ImplTypeFor<Interface> impl(remote.InitWithNewPipeAndPassReceiver());
  auto wrapped_remote = TypeParam::Wrap(std::move(remote));
  int32_t output_value = -1;
  ASSERT_TRUE(wrapped_remote->Echo(42, &output_value));
  EXPECT_EQ(42, output_value);
}

SEQUENCED_TASK_RUNNER_TYPED_TEST(SyncMethodOnSequenceCommonTest,
                                 ReenteredBySyncMethodReceiver) {
  // Test that an interface pointer waiting for a sync call response can be
  // reentered by a binding serving sync methods on the same thread.

  int32_t output_value = -1;
  ASSERT_TRUE(this->remote_->Echo(42, &output_value));
  EXPECT_EQ(42, output_value);
  this->Done();
}

TYPED_TEST(SyncMethodCommonTest, RemoteDestroyedDuringSyncCall) {
  // Test that it won't result in crash or hang if a remote is destroyed while
  // waiting for a sync call response.

  using Interface = typename TypeParam::Interface;
  PendingRemote<Interface> remote;
  ImplTypeFor<Interface> impl(remote.InitWithNewPipeAndPassReceiver());
  auto wrapped_remote = TypeParam::Wrap(std::move(remote));
  impl.set_ping_handler([&](TestSync::PingCallback callback) {
    wrapped_remote.reset();
    std::move(callback).Run();
  });
  ASSERT_FALSE(wrapped_remote->Ping());
}

SEQUENCED_TASK_RUNNER_TYPED_TEST(SyncMethodOnSequenceCommonTest,
                                 RemoteDestroyedDuringSyncCall) {
  // Test that it won't result in crash or hang if a remote is destroyed while
  // waiting for a sync call response.

  this->impl_->set_ping_handler([&](TestSync::PingCallback callback) {
    this->remote_.reset();
    std::move(callback).Run();
  });
  ASSERT_FALSE(this->remote_->Ping());
  this->Done();
}

TYPED_TEST(SyncMethodCommonTest, ReceiverDestroyedDuringSyncCall) {
  // Test that it won't result in crash or hang if a receiver is reset
  // (and therefore the message pipe handle is closed) while its corresponding
  // remote is waiting for a sync call response.

  using Interface = typename TypeParam::Interface;
  PendingRemote<Interface> remote;
  ImplTypeFor<Interface> impl(remote.InitWithNewPipeAndPassReceiver());
  auto wrapped_remote = TypeParam::Wrap(std::move(remote));
  impl.set_ping_handler([&](TestSync::PingCallback callback) {
    impl.receiver()->reset();
    std::move(callback).Run();
  });
  ASSERT_FALSE(wrapped_remote->Ping());
}

SEQUENCED_TASK_RUNNER_TYPED_TEST(SyncMethodOnSequenceCommonTest,
                                 ReceiverDestroyedDuringSyncCall) {
  // Test that it won't result in crash or hang if a receiver is reset
  // (and therefore the message pipe handle is closed) while its corresponding
  // remote is waiting for a sync call response.

  this->impl_->set_ping_handler([&](TestSync::PingCallback callback) {
    this->impl_->receiver()->reset();
    std::move(callback).Run();
  });
  ASSERT_FALSE(this->remote_->Ping());
  this->Done();
}

TYPED_TEST(SyncMethodCommonTest, NestedSyncCallsWithInOrderResponses) {
  // Test that we can call a sync method on a remote, while there is already a
  // sync call ongoing. The responses arrive in order.

  using Interface = typename TypeParam::Interface;
  PendingRemote<Interface> remote;
  ImplTypeFor<Interface> impl(remote.InitWithNewPipeAndPassReceiver());
  auto wrapped_remote = TypeParam::Wrap(std::move(remote));

  // The same variable is used to store the output of the two sync calls, in
  // order to test that responses are handled in the correct order.
  int32_t result_value = -1;

  bool first_call = true;
  impl.set_echo_handler([&](int32_t value, TestSync::EchoCallback callback) {
    if (first_call) {
      first_call = false;
      ASSERT_TRUE(wrapped_remote->Echo(456, &result_value));
      EXPECT_EQ(456, result_value);
    }
    std::move(callback).Run(value);
  });

  ASSERT_TRUE(wrapped_remote->Echo(123, &result_value));
  EXPECT_EQ(123, result_value);
}

SEQUENCED_TASK_RUNNER_TYPED_TEST(SyncMethodOnSequenceCommonTest,
                                 NestedSyncCallsWithInOrderResponses) {
  // Test that we can call a sync method on a remote, while there is already a
  // sync call ongoing. The responses arrive in order.

  // The same variable is used to store the output of the two sync calls, in
  // order to test that responses are handled in the correct order.
  int32_t result_value = -1;

  bool first_call = true;
  this->impl_->set_echo_handler(
      [&](int32_t value, TestSync::EchoCallback callback) {
        if (first_call) {
          first_call = false;
          ASSERT_TRUE(this->remote_->Echo(456, &result_value));
          EXPECT_EQ(456, result_value);
        }
        std::move(callback).Run(value);
      });

  ASSERT_TRUE(this->remote_->Echo(123, &result_value));
  EXPECT_EQ(123, result_value);
  this->Done();
}

TYPED_TEST(SyncMethodCommonTest, NestedSyncCallsWithOutOfOrderResponses) {
  // Test that we can call a sync method on a remote, while there is
  // already a sync call ongoing. The responses arrive out of order.

  using Interface = typename TypeParam::Interface;
  PendingRemote<Interface> remote;
  ImplTypeFor<Interface> impl(remote.InitWithNewPipeAndPassReceiver());
  auto wrapped_remote = TypeParam::Wrap(std::move(remote));

  // The same variable is used to store the output of the two sync calls, in
  // order to test that responses are handled in the correct order.
  int32_t result_value = -1;

  bool first_call = true;
  impl.set_echo_handler([&](int32_t value, TestSync::EchoCallback callback) {
    std::move(callback).Run(value);
    if (first_call) {
      first_call = false;
      ASSERT_TRUE(wrapped_remote->Echo(456, &result_value));
      EXPECT_EQ(456, result_value);
    }
  });

  ASSERT_TRUE(wrapped_remote->Echo(123, &result_value));
  EXPECT_EQ(123, result_value);
}

SEQUENCED_TASK_RUNNER_TYPED_TEST(SyncMethodOnSequenceCommonTest,
                                 NestedSyncCallsWithOutOfOrderResponses) {
  // Test that we can call a sync method on a remote while there is already a
  // sync call ongoing. The responses arrive out of order.

  // The same variable is used to store the output of the two sync calls, in
  // order to test that responses are handled in the correct order.
  int32_t result_value = -1;

  bool first_call = true;
  this->impl_->set_echo_handler(
      [&](int32_t value, TestSync::EchoCallback callback) {
        std::move(callback).Run(value);
        if (first_call) {
          first_call = false;
          ASSERT_TRUE(this->remote_->Echo(456, &result_value));
          EXPECT_EQ(456, result_value);
        }
      });

  ASSERT_TRUE(this->remote_->Echo(123, &result_value));
  EXPECT_EQ(123, result_value);
  this->Done();
}

TYPED_TEST(SyncMethodCommonTest, AsyncResponseQueuedDuringSyncCall) {
  // Test that while a remote is waiting for the response to a sync call, async
  // responses are queued until the sync call completes.

  using Interface = typename TypeParam::Interface;
  PendingRemote<Interface> remote;
  ImplTypeFor<Interface> impl(remote.InitWithNewPipeAndPassReceiver());
  auto wrapped_remote = TypeParam::Wrap(std::move(remote));

  int32_t async_echo_request_value = -1;
  TestSync::AsyncEchoCallback async_echo_request_callback;
  base::RunLoop run_loop1;
  impl.set_async_echo_handler(
      [&](int32_t value, TestSync::AsyncEchoCallback callback) {
        async_echo_request_value = value;
        async_echo_request_callback = std::move(callback);
        run_loop1.Quit();
      });

  bool async_echo_response_dispatched = false;
  base::RunLoop run_loop2;
  wrapped_remote->AsyncEcho(123,
                            base::BindLambdaForTesting([&](int32_t result) {
                              async_echo_response_dispatched = true;
                              EXPECT_EQ(123, result);
                              run_loop2.Quit();
                            }));
  // Run until the AsyncEcho request reaches the service side.
  run_loop1.Run();

  impl.set_echo_handler([&](int32_t value, TestSync::EchoCallback callback) {
    // Send back the async response first.
    EXPECT_FALSE(async_echo_request_callback.is_null());
    std::move(async_echo_request_callback).Run(async_echo_request_value);

    std::move(callback).Run(value);
  });

  int32_t result_value = -1;
  ASSERT_TRUE(wrapped_remote->Echo(456, &result_value));
  EXPECT_EQ(456, result_value);

  // Although the AsyncEcho response arrives before the Echo response, it should
  // be queued and not yet dispatched.
  EXPECT_FALSE(async_echo_response_dispatched);

  // Run until the AsyncEcho response is dispatched.
  run_loop2.Run();

  EXPECT_TRUE(async_echo_response_dispatched);
}

SEQUENCED_TASK_RUNNER_TYPED_TEST_F(SyncMethodOnSequenceCommonTest,
                                   AsyncResponseQueuedDuringSyncCall) {
  // Test that while a remote is waiting for the response to a sync call, async
  // responses are queued until the sync call completes.

  void Run() override {
    this->impl_->set_async_echo_handler(
        [this](int32_t value, TestSync::AsyncEchoCallback callback) {
          async_echo_request_value_ = value;
          async_echo_request_callback_ = std::move(callback);
          OnAsyncEchoReceived();
        });

    this->remote_->AsyncEcho(123,
                             base::BindLambdaForTesting([&](int32_t result) {
                               async_echo_response_dispatched_ = true;
                               EXPECT_EQ(123, result);
                               EXPECT_TRUE(async_echo_response_dispatched_);
                               this->Done();
                             }));
  }

  // Called when the AsyncEcho request reaches the service side.
  void OnAsyncEchoReceived() {
    this->impl_->set_echo_handler([this](int32_t value,
                                         TestSync::EchoCallback callback) {
      // Send back the async response first.
      EXPECT_FALSE(async_echo_request_callback_.is_null());
      std::move(async_echo_request_callback_).Run(async_echo_request_value_);

      std::move(callback).Run(value);
    });

    int32_t result_value = -1;
    ASSERT_TRUE(this->remote_->Echo(456, &result_value));
    EXPECT_EQ(456, result_value);

    // Although the AsyncEcho response arrives before the Echo response, it
    // should be queued and not yet dispatched.
    EXPECT_FALSE(async_echo_response_dispatched_);
  }

  int32_t async_echo_request_value_ = -1;
  TestSync::AsyncEchoCallback async_echo_request_callback_;
  bool async_echo_response_dispatched_ = false;
};

TYPED_TEST(SyncMethodCommonTest, AsyncRequestQueuedDuringSyncCall) {
  // Test that while a remote is waiting for the response to a sync call, async
  // requests for a binding running on the same thread are queued until the sync
  // call completes.

  using Interface = typename TypeParam::Interface;
  PendingRemote<Interface> remote;
  ImplTypeFor<Interface> impl(remote.InitWithNewPipeAndPassReceiver());
  auto wrapped_remote = TypeParam::Wrap(std::move(remote));

  bool async_echo_request_dispatched = false;
  impl.set_async_echo_handler(
      [&](int32_t value, TestSync::AsyncEchoCallback callback) {
        async_echo_request_dispatched = true;
        std::move(callback).Run(value);
      });

  bool async_echo_response_dispatched = false;
  base::RunLoop run_loop;
  wrapped_remote->AsyncEcho(123,
                            base::BindLambdaForTesting([&](int32_t result) {
                              async_echo_response_dispatched = true;
                              EXPECT_EQ(123, result);
                              run_loop.Quit();
                            }));

  impl.set_echo_handler([&](int32_t value, TestSync::EchoCallback callback) {
    // Although the AsyncEcho request is sent before the Echo request, it
    // shouldn't be dispatched yet at this point, because there is an ongoing
    // sync call on the same thread.
    EXPECT_FALSE(async_echo_request_dispatched);
    std::move(callback).Run(value);
  });

  int32_t result_value = -1;
  ASSERT_TRUE(wrapped_remote->Echo(456, &result_value));
  EXPECT_EQ(456, result_value);

  // Although the AsyncEcho request is sent before the Echo request, it
  // shouldn't be dispatched yet.
  EXPECT_FALSE(async_echo_request_dispatched);

  // Run until the AsyncEcho response is dispatched.
  run_loop.Run();

  EXPECT_TRUE(async_echo_response_dispatched);
}

SEQUENCED_TASK_RUNNER_TYPED_TEST_F(SyncMethodOnSequenceCommonTest,
                                   AsyncRequestQueuedDuringSyncCall) {
  // Test that while a remote is waiting for the response to a sync call, async
  // requests for a binding running on the same thread are queued until the sync
  // call completes.
  void Run() override {
    this->impl_->set_async_echo_handler(
        [this](int32_t value, TestSync::AsyncEchoCallback callback) {
          async_echo_request_dispatched_ = true;
          std::move(callback).Run(value);
        });

    this->remote_->AsyncEcho(123,
                             base::BindLambdaForTesting([&](int32_t result) {
                               EXPECT_EQ(123, result);
                               this->Done();
                             }));

    this->impl_->set_echo_handler(
        [this](int32_t value, TestSync::EchoCallback callback) {
          // Although the AsyncEcho request is sent before the Echo request, it
          // shouldn't be dispatched yet at this point, because there is an
          // ongoing sync call on the same thread.
          EXPECT_FALSE(async_echo_request_dispatched_);
          std::move(callback).Run(value);
        });

    int32_t result_value = -1;
    ASSERT_TRUE(this->remote_->Echo(456, &result_value));
    EXPECT_EQ(456, result_value);

    // Although the AsyncEcho request is sent before the Echo request, it
    // shouldn't be dispatched yet.
    EXPECT_FALSE(async_echo_request_dispatched_);
  }
  bool async_echo_request_dispatched_ = false;
};

TYPED_TEST(SyncMethodCommonTest,
           QueuedMessagesProcessedBeforeErrorNotification) {
  // Test that while a remote is waiting for the response to a sync call, async
  // responses are queued. If the message pipe is disconnected before the queued
  // queued messages are processed, the disconnect handler is delayed until all
  // the queued messages are processed.

  // SharedRemote doesn't guarantee that messages are delivered before the
  // disconnect handler, so skip it for this test.
  if (TypeParam::kIsSharedRemoteTest)
    return;

  using Interface = typename TypeParam::Interface;
  Remote<Interface> remote;
  ImplTypeFor<Interface> impl(remote.BindNewPipeAndPassReceiver());

  int32_t async_echo_request_value = -1;
  TestSync::AsyncEchoCallback async_echo_request_callback;
  base::RunLoop run_loop1;
  impl.set_async_echo_handler(
      [&](int32_t value, TestSync::AsyncEchoCallback callback) {
        async_echo_request_value = value;
        async_echo_request_callback = std::move(callback);
        run_loop1.Quit();
      });

  bool async_echo_response_dispatched = false;
  bool disconnect_dispatched = false;
  base::RunLoop run_loop2;
  remote->AsyncEcho(123, base::BindLambdaForTesting([&](int32_t result) {
                      async_echo_response_dispatched = true;
                      // At this point, error notification should not be
                      // dispatched yet.
                      EXPECT_FALSE(disconnect_dispatched);
                      EXPECT_TRUE(remote.is_connected());
                      EXPECT_EQ(123, result);
                      run_loop2.Quit();
                    }));
  // Run until the AsyncEcho request reaches the service side.
  run_loop1.Run();

  impl.set_echo_handler([&](int32_t value, TestSync::EchoCallback callback) {
    // Send back the async response first.
    EXPECT_FALSE(async_echo_request_callback.is_null());
    std::move(async_echo_request_callback).Run(async_echo_request_value);

    impl.receiver()->reset();
  });

  base::RunLoop run_loop3;
  remote.set_disconnect_handler(base::BindLambdaForTesting([&] {
    disconnect_dispatched = true;
    run_loop3.Quit();
  }));

  int32_t result_value = -1;
  ASSERT_FALSE(remote->Echo(456, &result_value));
  EXPECT_EQ(-1, result_value);
  ASSERT_FALSE(disconnect_dispatched);
  EXPECT_TRUE(remote.is_connected());

  // Although the AsyncEcho response arrives before the Echo response, it should
  // be queued and not yet dispatched.
  EXPECT_FALSE(async_echo_response_dispatched);

  // Run until the AsyncEcho response is dispatched.
  run_loop2.Run();

  EXPECT_TRUE(async_echo_response_dispatched);

  // Run until the error notification is dispatched.
  run_loop3.Run();

  ASSERT_TRUE(disconnect_dispatched);
  EXPECT_FALSE(remote.is_connected());
}

SEQUENCED_TASK_RUNNER_TYPED_TEST_F(
    SyncMethodOnSequenceCommonTest,
    QueuedMessagesProcessedBeforeErrorNotification) {
  // Test that while a remote is waiting for the response to a sync call, async
  // responses are queued. If the message pipe is disconnected before the queued
  // messages are processed, the disconnect notification is delayed until all
  // the queued messages are processed.

  void Run() override {
    this->impl_->set_async_echo_handler(
        [this](int32_t value, TestSync::AsyncEchoCallback callback) {
          OnAsyncEchoReachedService(value, std::move(callback));
        });

    this->remote_->AsyncEcho(123,
                             base::BindLambdaForTesting([&](int32_t result) {
                               async_echo_response_dispatched_ = true;
                               // At this point, error notification should not
                               // be dispatched yet.
                               EXPECT_FALSE(disconnect_dispatched_);
                               EXPECT_TRUE(this->remote_.is_connected());
                               EXPECT_EQ(123, result);
                               EXPECT_TRUE(async_echo_response_dispatched_);
                             }));
  }

  void OnAsyncEchoReachedService(int32_t value,
                                 TestSync::AsyncEchoCallback callback) {
    async_echo_request_value_ = value;
    async_echo_request_callback_ = std::move(callback);
    this->impl_->set_echo_handler([this](int32_t value,
                                         TestSync::EchoCallback callback) {
      // Send back the async response first.
      EXPECT_FALSE(async_echo_request_callback_.is_null());
      std::move(async_echo_request_callback_).Run(async_echo_request_value_);

      this->impl_->receiver()->reset();
    });

    this->remote_.set_disconnect_handler(base::BindLambdaForTesting([&] {
      disconnect_dispatched_ = true;
      OnErrorNotificationDispatched();
    }));

    int32_t result_value = -1;
    ASSERT_FALSE(this->remote_->Echo(456, &result_value));
    EXPECT_EQ(-1, result_value);
    ASSERT_FALSE(disconnect_dispatched_);
    EXPECT_TRUE(this->remote_.is_connected());

    // Although the AsyncEcho response arrives before the Echo response, it
    // should
    // be queued and not yet dispatched.
    EXPECT_FALSE(async_echo_response_dispatched_);
  }

  void OnErrorNotificationDispatched() {
    ASSERT_TRUE(disconnect_dispatched_);
    EXPECT_FALSE(this->remote_.is_connected());
    this->Done();
  }

  int32_t async_echo_request_value_ = -1;
  TestSync::AsyncEchoCallback async_echo_request_callback_;
  bool async_echo_response_dispatched_ = false;
  bool disconnect_dispatched_ = false;
};

TYPED_TEST(SyncMethodCommonTest, InvalidMessageDuringSyncCall) {
  // Test that while a remote is waiting for the response to a sync call, an
  // invalid incoming message will disconnect the message pipe, cause the sync
  // call to return false, and run the disconnect handler asynchronously.

  using Interface = typename TypeParam::Interface;
  MessagePipe pipe;

  PendingRemote<Interface> remote;
  ScopedMessagePipeHandle receiving_handle =
      remote.InitWithNewPipeAndPassReceiver().PassPipe();
  auto wrapped_remote = TypeParam::Wrap(std::move(remote));

  auto raw_receiving_handle = receiving_handle.get();
  ImplTypeFor<Interface> impl(
      PendingReceiver<Interface>(std::move(receiving_handle)));

  impl.set_echo_handler([&](int32_t value, TestSync::EchoCallback callback) {
    // Write a 1-byte message, which is considered invalid.
    char invalid_message = 0;
    MojoResult result =
        WriteMessageRaw(raw_receiving_handle, &invalid_message, 1u, nullptr, 0u,
                        MOJO_WRITE_MESSAGE_FLAG_NONE);
    ASSERT_EQ(MOJO_RESULT_OK, result);
    std::move(callback).Run(value);
  });

  bool disconnect_dispatched = false;
  base::RunLoop run_loop;
  // SharedRemote doesn't support setting disconnect handlers.
  if (!TypeParam::kIsSharedRemoteTest) {
    wrapped_remote.set_disconnect_handler(base::BindLambdaForTesting([&] {
      disconnect_dispatched = true;
      run_loop.Quit();
    }));
  }

  int32_t result_value = -1;
  ASSERT_FALSE(wrapped_remote->Echo(456, &result_value));
  EXPECT_EQ(-1, result_value);
  ASSERT_FALSE(disconnect_dispatched);

  if (!TypeParam::kIsSharedRemoteTest) {
    run_loop.Run();
    ASSERT_TRUE(disconnect_dispatched);
  }
}

SEQUENCED_TASK_RUNNER_TYPED_TEST_F(SyncMethodOnSequenceCommonTest,
                                   InvalidMessageDuringSyncCall) {
  // Test that while a remote is waiting for the response to a sync call, an
  // invalid incoming message will disconnect the message pipe and cause the
  // sync call to return false, and run the disconnect handler asynchronously.

  void Run() override {
    MessagePipeHandle raw_receiving_handle =
        this->impl_->receiver()->internal_state()->handle();

    this->impl_->set_echo_handler(
        [&](int32_t value, TestSync::EchoCallback callback) {
          // Write a 1-byte message, which is considered invalid.
          char invalid_message = 0;
          MojoResult result =
              WriteMessageRaw(raw_receiving_handle, &invalid_message, 1u,
                              nullptr, 0u, MOJO_WRITE_MESSAGE_FLAG_NONE);
          ASSERT_EQ(MOJO_RESULT_OK, result);
          std::move(callback).Run(value);
        });

    this->remote_.set_disconnect_handler(base::BindLambdaForTesting([this]() {
      disconnect_dispatched_ = true;
      this->Done();
    }));

    int32_t result_value = -1;
    ASSERT_FALSE(this->remote_->Echo(456, &result_value));
    EXPECT_EQ(-1, result_value);
    ASSERT_FALSE(disconnect_dispatched_);
  }
  bool disconnect_dispatched_ = false;
};

TEST_F(SyncMethodAssociatedTest,
       ReenteredBySyncMethodAssociatedReceiverOfSameRouter) {
  // Test that a remote waiting for a sync call response can be reentered by an
  // associated receiver serving sync methods on the same thread. The associated
  // receiver belongs to the same MultiplexRouter as the waiting remote.

  TestSyncAssociatedImpl client_impl(std::move(client_pending_sync_receiver_));
  AssociatedRemote<TestSync> client_remote(
      std::move(client_pending_sync_remote_));

  master_impl_->set_echo_handler(
      [&](int32_t value, TestSyncMaster::EchoCallback callback) {
        int32_t result_value = -1;

        ASSERT_TRUE(client_remote->Echo(123, &result_value));
        EXPECT_EQ(123, result_value);
        std::move(callback).Run(value);
      });

  int32_t result_value = -1;
  ASSERT_TRUE(master_remote_->Echo(456, &result_value));
  EXPECT_EQ(456, result_value);
}

TEST_F(SyncMethodAssociatedTest,
       ReenteredBySyncMethodAssociatedReceiverOfDifferentRouter) {
  // Test that a remote waiting for a sync call response can be reentered by an
  // associated receiver serving sync methods on the same thread. The associated
  // receiver belongs to the same MultiplexRouter as the waiting remote.

  TestSyncAssociatedImpl impl(std::move(impl_pending_sync_receiver_));
  AssociatedRemote<TestSync> remote(std::move(impl_pending_sync_remote_));

  master_impl_->set_echo_handler(
      [&](int32_t value, TestSyncMaster::EchoCallback callback) {
        int32_t result_value = -1;

        ASSERT_TRUE(remote->Echo(123, &result_value));
        EXPECT_EQ(123, result_value);
        std::move(callback).Run(value);
      });

  int32_t result_value = -1;
  ASSERT_TRUE(master_remote_->Echo(456, &result_value));
  EXPECT_EQ(456, result_value);
}

// TODO(yzshen): Add more tests related to associated interfaces.

}  // namespace
}  // namespace test
}  // namespace mojo

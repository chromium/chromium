// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <utility>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/sequence_token.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/lib/message_fragment.h"
#include "mojo/public/cpp/bindings/lib/send_message_helper.h"
#include "mojo/public/cpp/bindings/lib/serialization_util.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/shared_associated_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/cpp/bindings/tests/sync_method_unittest.test-mojom-shared-message-ids.h"
#include "mojo/public/cpp/bindings/tests/sync_method_unittest.test-mojom.h"
#include "mojo/public/interfaces/bindings/tests/test_sync_methods.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

// This needs to be included last, since it forward declares a bunch of classes
// but depends on those definitions to be included by headers that sort
// lexicographically after.
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/tests/sync_method_unittest.test-mojom-params-data.h"

namespace mojo {
namespace test {
namespace sync_method_unittest {
namespace {

class TestSyncCommonImpl {
 public:
  TestSyncCommonImpl() = default;

  TestSyncCommonImpl(const TestSyncCommonImpl&) = delete;
  TestSyncCommonImpl& operator=(const TestSyncCommonImpl&) = delete;

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
};

class TestSyncImpl : public TestSync, public TestSyncCommonImpl {
 public:
  explicit TestSyncImpl(PendingReceiver<TestSync> receiver)
      : receiver_(this, std::move(receiver)) {}

  TestSyncImpl(const TestSyncImpl&) = delete;
  TestSyncImpl& operator=(const TestSyncImpl&) = delete;

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
};

class TestSyncPrimaryImpl : public TestSyncPrimary, public TestSyncCommonImpl {
 public:
  explicit TestSyncPrimaryImpl(PendingReceiver<TestSyncPrimary> receiver)
      : receiver_(this, std::move(receiver)) {}

  TestSyncPrimaryImpl(const TestSyncPrimaryImpl&) = delete;
  TestSyncPrimaryImpl& operator=(const TestSyncPrimaryImpl&) = delete;

  // TestSyncPrimary implementation:
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

  Receiver<TestSyncPrimary>* receiver() { return &receiver_; }

 private:
  Receiver<TestSyncPrimary> receiver_;
};

class TestSyncAssociatedImpl : public TestSync, public TestSyncCommonImpl {
 public:
  explicit TestSyncAssociatedImpl(PendingAssociatedReceiver<TestSync> receiver)
      : receiver_(this, std::move(receiver)) {}

  TestSyncAssociatedImpl(const TestSyncAssociatedImpl&) = delete;
  TestSyncAssociatedImpl& operator=(const TestSyncAssociatedImpl&) = delete;

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
};

template <typename Interface>
struct ImplTraits;

template <>
struct ImplTraits<TestSync> {
  using Type = TestSyncImpl;
};

template <>
struct ImplTraits<TestSyncPrimary> {
  using Type = TestSyncPrimaryImpl;
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

  RemoteWrapper(const RemoteWrapper&) = delete;
  RemoteWrapper& operator=(const RemoteWrapper&) = delete;

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
      : task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})),
        ping_called_(false) {}

  TestSyncServiceSequence(const TestSyncServiceSequence&) = delete;
  TestSyncServiceSequence& operator=(const TestSyncServiceSequence&) = delete;

  void SetUp(PendingReceiver<Interface> receiver) {
    CHECK(task_runner()->RunsTasksInCurrentSequence());
    impl_ = std::make_unique<ImplTypeFor<Interface>>(std::move(receiver));
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
    primary_impl_ = std::make_unique<TestSyncPrimaryImpl>(
        primary_remote_.BindNewPipeAndPassReceiver());

    impl_pending_sync_receiver_ =
        impl_pending_sync_remote_.InitWithNewEndpointAndPassReceiver();
    client_pending_sync_receiver_ =
        client_pending_sync_remote_.InitWithNewEndpointAndPassReceiver();

    primary_impl_->set_send_remote_handler(
        [this](PendingAssociatedRemote<TestSync> remote) {
          client_pending_sync_remote_ = std::move(remote);
        });
    base::RunLoop run_loop;
    primary_impl_->set_send_receiver_handler(
        [this, &run_loop](PendingAssociatedReceiver<TestSync> receiver) {
          impl_pending_sync_receiver_ = std::move(receiver);
          run_loop.Quit();
        });

    primary_remote_->SendRemote(std::move(client_pending_sync_remote_));
    primary_remote_->SendReceiver(std::move(impl_pending_sync_receiver_));
    run_loop.Run();
  }

  void TearDown() override {
    impl_pending_sync_remote_.reset();
    impl_pending_sync_receiver_.reset();
    client_pending_sync_remote_.reset();
    client_pending_sync_receiver_.reset();
    primary_remote_.reset();
    primary_impl_.reset();
  }

  Remote<TestSyncPrimary> primary_remote_;
  std::unique_ptr<TestSyncPrimaryImpl> primary_impl_;

  // An associated interface whose receiver lives at the |primary_impl_| side.
  PendingAssociatedRemote<TestSync> impl_pending_sync_remote_;
  PendingAssociatedReceiver<TestSync> impl_pending_sync_receiver_;

  // An associated interface whose receiver lives at the |primary_remote_| side.
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
    task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
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
  base::ThreadPool::CreateSequencedTaskRunner({base::WithBaseSyncPrimitives()})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&SequencedTaskRunnerTestBase::RunTest,
                                base::Unretained(test.release())));
  run_loop.Run();
}

// TestSync (without associated interfaces) and TestSyncPrimary (with associated
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
    TestParams<TestSyncPrimary,
               true,
               BindingsTestSerializationMode::kSerializeBeforeSend>,
    TestParams<TestSyncPrimary,
               false,
               BindingsTestSerializationMode::kSerializeBeforeSend>,
    TestParams<TestSync,
               true,
               BindingsTestSerializationMode::kSerializeBeforeDispatch>,
    TestParams<TestSync,
               false,
               BindingsTestSerializationMode::kSerializeBeforeDispatch>,
    TestParams<TestSyncPrimary,
               true,
               BindingsTestSerializationMode::kSerializeBeforeDispatch>,
    TestParams<TestSyncPrimary,
               false,
               BindingsTestSerializationMode::kSerializeBeforeDispatch>,
    TestParams<TestSync, true, BindingsTestSerializationMode::kNeverSerialize>,
    TestParams<TestSync, false, BindingsTestSerializationMode::kNeverSerialize>,
    TestParams<TestSyncPrimary,
               true,
               BindingsTestSerializationMode::kNeverSerialize>,
    TestParams<TestSyncPrimary,
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

  primary_impl_->set_echo_handler(
      [&](int32_t value, TestSyncPrimary::EchoCallback callback) {
        int32_t result_value = -1;

        ASSERT_TRUE(client_remote->Echo(123, &result_value));
        EXPECT_EQ(123, result_value);
        std::move(callback).Run(value);
      });

  int32_t result_value = -1;
  ASSERT_TRUE(primary_remote_->Echo(456, &result_value));
  EXPECT_EQ(456, result_value);
}

TEST_F(SyncMethodAssociatedTest,
       ReenteredBySyncMethodAssociatedReceiverOfDifferentRouter) {
  // Test that a remote waiting for a sync call response can be reentered by an
  // associated receiver serving sync methods on the same thread. The associated
  // receiver belongs to the same MultiplexRouter as the waiting remote.

  TestSyncAssociatedImpl impl(std::move(impl_pending_sync_receiver_));
  AssociatedRemote<TestSync> remote(std::move(impl_pending_sync_remote_));

  primary_impl_->set_echo_handler(
      [&](int32_t value, TestSyncPrimary::EchoCallback callback) {
        int32_t result_value = -1;

        ASSERT_TRUE(remote->Echo(123, &result_value));
        EXPECT_EQ(123, result_value);
        std::move(callback).Run(value);
      });

  int32_t result_value = -1;
  ASSERT_TRUE(primary_remote_->Echo(456, &result_value));
  EXPECT_EQ(456, result_value);
}

class PingerImpl : public mojom::Pinger, public mojom::SimplePinger {
 public:
  PingerImpl() = default;
  ~PingerImpl() override = default;

 private:
  // We use synchronous Pong messages to exercise wake-up behavior when such
  // messages are received on a thread already waiting on some other sync call.
  // Namely, the main thread can be waiting on a reply to Ping or
  // PingNoInterrupt, and we want to target the main thread with sync Pong
  // messages before sending the corresponding Ping reply. This helper lives on
  // a background thread and sends the sync Pong messages when asked.
  class PongSender {
   public:
    PongSender(mojom::PongSendMode pong_send_mode,
               PendingRemote<mojom::Ponger> ponger)
        : pong_send_mode_(pong_send_mode), ponger_(std::move(ponger)) {}

    PongSender(mojom::PongSendMode pong_send_mode,
               PendingAssociatedRemote<mojom::Ponger> same_pipe_ponger)
        : pong_send_mode_(pong_send_mode),
          same_pipe_ponger_(std::move(same_pipe_ponger)) {}

    void SendPong(base::OnceClosure reply_callback) {
      mojom::Ponger& ponger =
          ponger_.is_bound() ? *ponger_.get() : *same_pipe_ponger_.get();
      if (pong_send_mode_ == mojom::PongSendMode::kSyncBlockReply) {
        // Here we expect the Pong to be dispatched, so we wait for it to
        // complete before allowing the ping reply to be sent.
        ponger.Pong();
        std::move(reply_callback).Run();
        return;
      }

      if (pong_send_mode_ == mojom::PongSendMode::kAsync) {
        ponger.PongAsync();
        std::move(reply_callback).Run();
        return;
      }

      // In cases where we know this Pong should not be dispatchable until the
      // reply is sent, we obviously can't wait for the Pong to be dispatched
      // before replying because that would trivially deadlock.
      //
      // Instead we reply first and let the reply race with the Pong (since it's
      // always on a different pipe, in practice). This means the test will be
      // non-deterministic in the presence of sync interrupt bugs, but we accept
      // that and take other measures (like delaying the actual reply within the
      // Ping implementation) to make such bugs more likely to trigger failures.
      DCHECK_EQ(pong_send_mode_, mojom::PongSendMode::kSyncDoNotBlockReply);
      std::move(reply_callback).Run();
      ponger.Pong();
    }

   private:
    const mojom::PongSendMode pong_send_mode_;
    Remote<mojom::Ponger> ponger_;
    AssociatedRemote<mojom::Ponger> same_pipe_ponger_;
  };

  // mojom::Pinger:
  void BindAssociated(PendingAssociatedReceiver<mojom::Pinger> receiver,
                      BindAssociatedCallback callback) override {
    associated_receivers_.Add(this, std::move(receiver));
    std::move(callback).Run();
  }

  void SetPonger(mojom::PongSendMode send_mode,
                 PendingRemote<mojom::Ponger> ponger,
                 SetPongerCallback callback) override {
    DCHECK(!pong_sender_thread_.IsRunning());
    DCHECK(!pong_sender_);
    pong_sender_thread_.Start();
    pong_sender_ = base::SequenceBound<PongSender>(
        pong_sender_thread_.task_runner(), send_mode, std::move(ponger));
    std::move(callback).Run();
  }

  void SetSamePipePonger(
      mojom::PongSendMode send_mode,
      PendingAssociatedRemote<mojom::Ponger> same_pipe_ponger,
      SetSamePipePongerCallback callback) override {
    DCHECK(!same_pipe_pong_sender_thread_.IsRunning());
    DCHECK(!same_pipe_pong_sender_);
    same_pipe_pong_sender_thread_.Start();
    same_pipe_pong_sender_ = base::SequenceBound<PongSender>(
        same_pipe_pong_sender_thread_.task_runner(), send_mode,
        std::move(same_pipe_ponger));
    std::move(callback).Run();
  }

  void Ping(PingCallback callback) override {
    if (pong_sender_ && same_pipe_pong_sender_)
      DoPong();
    std::move(callback).Run();
  }

  void PingNoInterrupt(PingNoInterruptCallback callback) override {
    if (pong_sender_ && same_pipe_pong_sender_)
      DoPong();
    std::move(callback).Run();
  }

  void SimplePing(SimplePingCallback callback) override {
    std::move(callback).Run();
  }

  void SimplePingNoInterrupt(SimplePingNoInterruptCallback callback) override {
    std::move(callback).Run();
  }

  void DoPong() {
    DCHECK(pong_sender_);
    DCHECK(same_pipe_pong_sender_);
    base::RunLoop wait_to_reply(base::RunLoop::Type::kNestableTasksAllowed);
    base::RepeatingClosure barrier =
        base::BarrierClosure(3, wait_to_reply.QuitClosure());
    pong_sender_.AsyncCall(&PongSender::SendPong).WithArgs(barrier);
    same_pipe_pong_sender_.AsyncCall(&PongSender::SendPong).WithArgs(barrier);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, barrier, base::Milliseconds(10));
    wait_to_reply.Run();
  }

  Receiver<mojom::Pinger> receiver_{this};
  AssociatedReceiverSet<mojom::Pinger> associated_receivers_;

  base::Thread pong_sender_thread_{"Pong Sender"};
  base::SequenceBound<PongSender> pong_sender_;

  base::Thread same_pipe_pong_sender_thread_{"Pong Sender"};
  base::SequenceBound<PongSender> same_pipe_pong_sender_;
};

class PongerImpl : public mojom::Ponger {
 public:
  PongerImpl() = default;
  ~PongerImpl() override = default;

  int num_sync_pongs() const { return num_sync_pongs_; }
  int num_async_pongs() const { return num_async_pongs_; }

  PendingRemote<mojom::Ponger> MakeRemote() {
    PendingRemote<mojom::Ponger> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  PendingAssociatedRemote<mojom::Ponger> MakeAssociatedRemote() {
    PendingAssociatedRemote<mojom::Ponger> remote;
    associated_receivers_.Add(this,
                              remote.InitWithNewEndpointAndPassReceiver());
    return remote;
  }

  // mojom::Ponger:
  void Pong(PongCallback callback) override {
    ++num_sync_pongs_;
    std::move(callback).Run();
  }

  void PongAsync() override { ++num_async_pongs_; }

 private:
  int num_sync_pongs_ = 0;
  int num_async_pongs_ = 0;
  ReceiverSet<mojom::Ponger> receivers_;
  AssociatedReceiverSet<mojom::Ponger> associated_receivers_;
};

class SyncInterruptTest : public BindingsTestBase {
 public:
  SyncInterruptTest() {
    PendingRemote<mojom::Pinger> shared_remote;
    // Note that we cannot test [NoInterrupt] properly if the caller and
    // receiver live on the same thread, because the caller's own message is
    // unable to wake up the receiver during a [NoInterrupt] wait. Hence we run
    // the Pinger implementation on a background thread.
    receiver_thread_.Start();
    receiver_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](PendingReceiver<mojom::Pinger> receiver,
                          PendingReceiver<mojom::SimplePinger> simple_receiver,
                          PendingReceiver<mojom::Pinger> shared_receiver) {
                         MakeSelfOwnedReceiver(std::make_unique<PingerImpl>(),
                                               std::move(receiver));
                         MakeSelfOwnedReceiver(std::make_unique<PingerImpl>(),
                                               std::move(simple_receiver));
                         MakeSelfOwnedReceiver(std::make_unique<PingerImpl>(),
                                               std::move(shared_receiver));
                       },
                       pinger_.BindNewPipeAndPassReceiver(),
                       simple_pinger_.BindNewPipeAndPassReceiver(),
                       shared_remote.InitWithNewPipeAndPassReceiver()));

    shared_pinger_thread_.Start();
    shared_pinger_ = SharedRemote<mojom::Pinger>(
        std::move(shared_remote), shared_pinger_thread_.task_runner());

    PendingAssociatedRemote<mojom::Pinger> associated_remote;
    CHECK(shared_pinger_->BindAssociated(
        associated_remote.InitWithNewEndpointAndPassReceiver()));
    shared_associated_pinger_ = SharedAssociatedRemote<mojom::Pinger>(
        std::move(associated_remote), shared_pinger_thread_.task_runner());
  }

  ~SyncInterruptTest() override = default;

  mojom::Pinger& pinger() { return *pinger_.get(); }
  mojom::SimplePinger& simple_pinger() { return *simple_pinger_.get(); }
  mojom::Pinger& shared_pinger() { return *shared_pinger_.get(); }
  mojom::Pinger& shared_associated_pinger() {
    return *shared_associated_pinger_.get();
  }

  const PongerImpl& ponger() const { return ponger_; }
  const PongerImpl& same_pipe_ponger() const { return same_pipe_ponger_; }

  void InitPonger(mojom::PongSendMode send_mode) {
    pinger_->SetPonger(send_mode, ponger_.MakeRemote());
    shared_pinger_->SetPonger(send_mode, ponger_.MakeRemote());
  }

  void InitSamePipePonger(mojom::PongSendMode send_mode) {
    pinger_->SetSamePipePonger(send_mode,
                               same_pipe_ponger_.MakeAssociatedRemote());
    shared_pinger_->SetSamePipePonger(send_mode,
                                      same_pipe_ponger_.MakeAssociatedRemote());
  }

 private:
  base::Thread receiver_thread_{"Pinger Receiver Thread"};
  base::Thread shared_pinger_thread_{"Shared Pinger IO"};
  Remote<mojom::Pinger> pinger_;
  Remote<mojom::SimplePinger> simple_pinger_;
  SharedRemote<mojom::Pinger> shared_pinger_;
  SharedAssociatedRemote<mojom::Pinger> shared_associated_pinger_;
  PongerImpl ponger_;
  PongerImpl same_pipe_ponger_;
};

TEST_P(SyncInterruptTest, AsyncCannotInterruptSync) {
  // Verifies that async messages will not dispatch on a thread while that
  // thread is waiting for any sync reply.
  InitPonger(mojom::PongSendMode::kAsync);
  InitSamePipePonger(mojom::PongSendMode::kAsync);
  pinger().Ping();
  EXPECT_EQ(0, ponger().num_async_pongs());
  EXPECT_EQ(0, ponger().num_sync_pongs());
  EXPECT_EQ(0, same_pipe_ponger().num_async_pongs());
  EXPECT_EQ(0, same_pipe_ponger().num_sync_pongs());
}

TEST_P(SyncInterruptTest, SyncCanInterruptSync) {
  // Verifies that incoming sync messages can normally interrupt a sync wait on
  // the same thread, even when received on a different pipe.
  InitPonger(mojom::PongSendMode::kSyncBlockReply);
  InitSamePipePonger(mojom::PongSendMode::kSyncBlockReply);
  pinger().Ping();
  EXPECT_EQ(0, ponger().num_async_pongs());
  EXPECT_EQ(1, ponger().num_sync_pongs());
  EXPECT_EQ(0, same_pipe_ponger().num_async_pongs());
  EXPECT_EQ(1, same_pipe_ponger().num_sync_pongs());
}

TEST_P(SyncInterruptTest, NothingCanInterruptSyncNoInterrupt) {
  // Verifies that no incoming messages can interrupt a [NoInterrupt] sync wait
  // except for the exact reply we're waiting on.

  InitPonger(mojom::PongSendMode::kSyncDoNotBlockReply);
  InitSamePipePonger(mojom::PongSendMode::kSyncDoNotBlockReply);
  pinger().PingNoInterrupt();
  EXPECT_EQ(0, ponger().num_async_pongs());
  EXPECT_EQ(0, ponger().num_sync_pongs());
  EXPECT_EQ(0, same_pipe_ponger().num_async_pongs());
  EXPECT_EQ(0, same_pipe_ponger().num_sync_pongs());

  // We also need to test an interface with no associated interface support,
  // such as SimplePinger. Should behave the same. We send an async message
  // first.
  bool async_replies_expected = false;
  bool got_first_async_reply = false;
  simple_pinger().SimplePing(base::BindLambdaForTesting([&] {
    EXPECT_TRUE(async_replies_expected);
    got_first_async_reply = true;
  }));

  // This must complete without the above async reply dispatching.
  EXPECT_TRUE(simple_pinger().SimplePingNoInterrupt());

  // Now send another async Ping.
  base::RunLoop loop;
  auto quit = loop.QuitClosure();
  simple_pinger().SimplePing(base::BindLambdaForTesting([&] {
    EXPECT_TRUE(async_replies_expected);
    loop.Quit();
  }));

  // This time send a regular sync message and then an uninterruptible one. This
  // exercises a slightly different code path since an async reply will arrive
  // during the regular sync wait. It should still not be dispatched.
  EXPECT_TRUE(simple_pinger().SimplePing());
  EXPECT_TRUE(simple_pinger().SimplePingNoInterrupt());

  // Finally, confirm that if we go back to spinning a RunLoop, the deferred
  // async replies will dispatch as expected.
  async_replies_expected = true;
  loop.Run();
  EXPECT_TRUE(got_first_async_reply);
}

TEST_P(SyncInterruptTest, SharedRemoteNoInterrupt) {
  // Verifies that [NoInterrupt] behavior also works as expected when doing a
  // sync call through a SharedRemote. A key difference between this case and
  // Remote case is that with a SharedRemote caller, the only possible
  // same-thread dispatches during the wait are either the reply we're waiting
  // for, or an outer [NoInterrupt] sync call on the same thread (implying that
  // the wait is nested within another.)

  InitPonger(mojom::PongSendMode::kSyncDoNotBlockReply);
  InitSamePipePonger(mojom::PongSendMode::kSyncDoNotBlockReply);
  shared_pinger().PingNoInterrupt();
  EXPECT_EQ(0, ponger().num_async_pongs());
  EXPECT_EQ(0, ponger().num_sync_pongs());
  EXPECT_EQ(0, same_pipe_ponger().num_async_pongs());
  EXPECT_EQ(0, same_pipe_ponger().num_sync_pongs());
}

TEST_P(SyncInterruptTest, SharedAssociatedRemoteNoInterrupt) {
  // Verifies that [NoInterrupt] behavior also works as expected when doing a
  // sync call through a SharedAssociatedRemote. Expectations are identical to
  // the SharedRemote case in the test above.

  InitPonger(mojom::PongSendMode::kSyncDoNotBlockReply);
  InitSamePipePonger(mojom::PongSendMode::kSyncDoNotBlockReply);
  shared_associated_pinger().PingNoInterrupt();
  EXPECT_EQ(0, ponger().num_async_pongs());
  EXPECT_EQ(0, ponger().num_sync_pongs());
  EXPECT_EQ(0, same_pipe_ponger().num_async_pongs());
  EXPECT_EQ(0, same_pipe_ponger().num_sync_pongs());
}

class SyncService : public mojom::SyncService {
 public:
  explicit SyncService(PendingReceiver<mojom::SyncService> receiver)
      : receiver_(this, std::move(receiver)) {}

  void SetCallHandler(base::OnceClosure call_handler) {
    call_handler_ = std::move(call_handler);
  }

  // mojom::SyncService:
  void SyncCall(SyncCallCallback callback) override {
    std::move(callback).Run();
    if (call_handler_) {
      std::move(call_handler_).Run();
    }
  }

 private:
  Receiver<mojom::SyncService> receiver_;
  base::OnceClosure call_handler_;
};

class DisableSyncInterruptTest : public BindingsTestBase {
 public:
  void SetUp() override {
    mojo::SyncCallRestrictions::DisableSyncCallInterrupts();
  }

  void TearDown() override {
    mojo::SyncCallRestrictions::EnableSyncCallInterruptsForTesting();
  }
};

TEST_P(DisableSyncInterruptTest, NoInterruptWhenDisabled) {
  PendingRemote<mojom::SyncService> interrupter;
  SyncService service(interrupter.InitWithNewPipeAndPassReceiver());

  base::RunLoop wait_for_main_thread_service_call;
  bool main_thread_service_called = false;
  service.SetCallHandler(base::BindLambdaForTesting([&] {
    main_thread_service_called = true;
    wait_for_main_thread_service_call.Quit();
  }));

  Remote<mojom::SyncService> caller;
  base::Thread background_service_thread("SyncService");
  background_service_thread.Start();
  base::SequenceBound<SyncService> background_service{
      background_service_thread.task_runner(),
      caller.BindNewPipeAndPassReceiver()};

  base::Thread interrupter_thread("Interrupter");
  interrupter_thread.Start();
  interrupter_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&interrupter] {
        // Issue a sync call to the SyncService on the main thread. This should
        // never be dispatched until *after* the sync call *from* the main
        // thread completes below.
        Remote<mojom::SyncService>(std::move(interrupter))->SyncCall();
      }));

  // The key test expectation here is that `main_thread_service_called` cannot
  // be set to true until after SyncCall() returns and we can pump the thread's
  // message loop. If sync interrupts are not properly disabled, this
  // expectation can fail flakily (and often.)
  caller->SyncCall();
  EXPECT_FALSE(main_thread_service_called);

  // Now the incoming sync call can be dispatched.
  wait_for_main_thread_service_call.Run();
  EXPECT_TRUE(main_thread_service_called);

  background_service.SynchronouslyResetForTest();
  interrupter_thread.Stop();
  background_service_thread.Stop();
}

TEST_P(DisableSyncInterruptTest, SharedRemoteNoInterruptWhenDisabled) {
  PendingRemote<mojom::SyncService> interrupter;
  SyncService service(interrupter.InitWithNewPipeAndPassReceiver());

  base::RunLoop wait_for_main_thread_service_call;
  bool main_thread_service_called = false;
  service.SetCallHandler(base::BindLambdaForTesting([&] {
    main_thread_service_called = true;
    wait_for_main_thread_service_call.Quit();
  }));

  // Bind a SharedRemote to another background thread so that we exercise
  // SharedRemote's own sync wait codepath when called into from the main
  // thread.
  base::Thread background_client_thread("Client");
  background_client_thread.Start();

  base::Thread background_service_thread("Service");
  background_service_thread.Start();

  SharedRemote<mojom::SyncService> caller;
  base::SequenceBound<SyncService> background_service{
      background_service_thread.task_runner(),
      caller.BindNewPipeAndPassReceiver(
          background_client_thread.task_runner())};

  base::Thread interrupter_thread("Interrupter");
  interrupter_thread.Start();
  interrupter_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&interrupter] {
        // Issue a sync call to the SyncService on the main thread. This should
        // never be dispatched until *after* the sync call *from* the main
        // thread completes below.
        Remote<mojom::SyncService>(std::move(interrupter))->SyncCall();
      }));

  // The key test expectation here is that `main_thread_service_called` cannot
  // be set to true until after SyncCall() returns and we can pump the thread's
  // message loop. If sync interrupts are not properly disabled, this
  // expectation can fail flakily (and often.)
  caller->SyncCall();
  EXPECT_FALSE(main_thread_service_called);

  // Now the incoming sync call can be dispatched.
  wait_for_main_thread_service_call.Run();
  EXPECT_TRUE(main_thread_service_called);

  background_service.SynchronouslyResetForTest();

  // We need to reset the SharedRemote before the client thread is stopped, to
  // ensure the necessary teardown work is executed on that thread. Otherwise
  // the underlying pipe and related state will leak, and ASan will complain.
  caller.reset();

  interrupter_thread.Stop();
  background_service_thread.Stop();
  background_client_thread.Stop();
}

INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(SyncInterruptTest);
INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(DisableSyncInterruptTest);

class OneSyncImpl;

class NoSyncImpl : public mojom::NoSync {
 public:
  explicit NoSyncImpl(PendingReceiver<mojom::NoSync> receiver)
      : receiver_(this, std::move(receiver)) {}

  explicit NoSyncImpl(
      PendingAssociatedReceiver<mojom::NoSync> associated_receiver)
      : associated_receiver_(this, std::move(associated_receiver)) {}

  // mojom::NoSync implementation:
  void Method(MethodCallback callback) override;
  void BindNoSync(PendingAssociatedReceiver<mojom::NoSync> receiver) override;
  void BindOneSync(PendingAssociatedReceiver<mojom::OneSync> receiver) override;

 private:
  Receiver<mojom::NoSync> receiver_{this};
  AssociatedReceiver<mojom::NoSync> associated_receiver_{this};

  std::unique_ptr<NoSyncImpl> associated_no_sync_;
  std::unique_ptr<OneSyncImpl> associated_one_sync_;
};

class OneSyncImpl : public mojom::OneSync {
 public:
  explicit OneSyncImpl(PendingReceiver<mojom::OneSync> receiver)
      : receiver_(this, std::move(receiver)) {}

  explicit OneSyncImpl(
      PendingAssociatedReceiver<mojom::OneSync> associated_receiver)
      : associated_receiver_(this, std::move(associated_receiver)) {}

  // mojom::OneSync implementation:
  void Method(MethodCallback callback) override;
  void SyncMethod(SyncMethodCallback callback) override;
  void BindNoSync(PendingAssociatedReceiver<mojom::NoSync> receiver) override;
  void BindOneSync(PendingAssociatedReceiver<mojom::OneSync> receiver) override;

 private:
  Receiver<mojom::OneSync> receiver_{this};
  AssociatedReceiver<mojom::OneSync> associated_receiver_{this};

  std::unique_ptr<NoSyncImpl> associated_no_sync_;
  std::unique_ptr<OneSyncImpl> associated_one_sync_;
};

void NoSyncImpl::Method(MethodCallback callback) {
  EXPECT_TRUE(false);
  std::move(callback).Run();
}

void NoSyncImpl::BindNoSync(PendingAssociatedReceiver<mojom::NoSync> receiver) {
  associated_no_sync_ = std::make_unique<NoSyncImpl>(std::move(receiver));
}

void NoSyncImpl::BindOneSync(
    PendingAssociatedReceiver<mojom::OneSync> receiver) {
  associated_one_sync_ = std::make_unique<OneSyncImpl>(std::move(receiver));
}

void OneSyncImpl::Method(MethodCallback callback) {
  EXPECT_TRUE(false);
  std::move(callback).Run();
}

void OneSyncImpl::SyncMethod(MethodCallback callback) {
  std::move(callback).Run();
}

void OneSyncImpl::BindNoSync(
    PendingAssociatedReceiver<mojom::NoSync> receiver) {
  associated_no_sync_ = std::make_unique<NoSyncImpl>(std::move(receiver));
}

void OneSyncImpl::BindOneSync(
    PendingAssociatedReceiver<mojom::OneSync> receiver) {
  associated_one_sync_ = std::make_unique<OneSyncImpl>(std::move(receiver));
}

class NoResponseExpectedResponder : public MessageReceiver {
 public:
  explicit NoResponseExpectedResponder() = default;

  // MessageReceiver implementation:
  bool Accept(Message* message) override {
    EXPECT_TRUE(false);
    return true;
  }
};

class SyncFlagValidationTest : public ::testing::TestWithParam<uint32_t> {
 protected:
  Message MakeNoSyncMethodMessage() {
    const uint32_t flags =
        // Always set the sync flag, as that's the primary point of the test.
        Message::kFlagIsSync |
        // InterfaceEndpointClient requires this flag if sending a message with
        // a responder.
        Message::kFlagExpectsResponse | GetParam();
    Message message(base::to_underlying(mojom::messages::NoSync::kMethod),
                    flags, 0, 0, nullptr);
    ::mojo::internal::MessageFragment<
        mojom::internal::NoSync_Method_Params_Data>
        params(message);
    params.Allocate();
    return message;
  }

  Message MakeOneSyncMethodMessage() {
    const uint32_t flags =
        // Always set the sync flag, as that's the primary point of the test.
        Message::kFlagIsSync |
        // InterfaceEndpointClient requires this flag if sending a message with
        // a responder.
        Message::kFlagExpectsResponse | GetParam();
    Message message(base::to_underlying(mojom::messages::OneSync::kMethod),
                    flags, 0, 0, nullptr);
    ::mojo::internal::MessageFragment<
        mojom::internal::NoSync_Method_Params_Data>
        params(message);
    params.Allocate();
    return message;
  }

  void FlushPostedTasks() {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment;
};

TEST_P(SyncFlagValidationTest, NonSync) {
  Remote<mojom::NoSync> remote;
  NoSyncImpl impl(remote.BindNewPipeAndPassReceiver());

  Message message = MakeNoSyncMethodMessage();
  auto responder = std::make_unique<NoResponseExpectedResponder>();
  ASSERT_TRUE(remote.internal_state()->endpoint_client_for_test());
  ::mojo::internal::SendMojoMessage(
      *remote.internal_state()->endpoint_client_for_test(), message,
      std::move(responder));
}

TEST_P(SyncFlagValidationTest, OneSync) {
  Remote<mojom::OneSync> remote;
  OneSyncImpl impl(remote.BindNewPipeAndPassReceiver());

  Message message = MakeOneSyncMethodMessage();
  auto responder = std::make_unique<NoResponseExpectedResponder>();
  ASSERT_TRUE(remote.internal_state()->endpoint_client_for_test());
  ::mojo::internal::SendMojoMessage(
      *remote.internal_state()->endpoint_client_for_test(), message,
      std::move(responder));
}

TEST_P(SyncFlagValidationTest, NoSyncAssociatedWithNoSync) {
  Remote<mojom::NoSync> remote;
  NoSyncImpl impl(remote.BindNewPipeAndPassReceiver());

  AssociatedRemote<mojom::NoSync> associated_remote;
  remote->BindNoSync(associated_remote.BindNewEndpointAndPassReceiver());

  FlushPostedTasks();

  Message message = MakeNoSyncMethodMessage();
  auto responder = std::make_unique<NoResponseExpectedResponder>();
  ASSERT_TRUE(remote.internal_state()->endpoint_client_for_test());
  ::mojo::internal::SendMojoMessage(
      *associated_remote.internal_state()->endpoint_client_for_test(), message,
      std::move(responder));
}

TEST_P(SyncFlagValidationTest, OneSyncAssociatedWithNoSync) {
  Remote<mojom::NoSync> remote;
  NoSyncImpl impl(remote.BindNewPipeAndPassReceiver());

  AssociatedRemote<mojom::OneSync> associated_remote;
  remote->BindOneSync(associated_remote.BindNewEndpointAndPassReceiver());

  FlushPostedTasks();

  Message message = MakeOneSyncMethodMessage();
  auto responder = std::make_unique<NoResponseExpectedResponder>();
  ASSERT_TRUE(remote.internal_state()->endpoint_client_for_test());
  ::mojo::internal::SendMojoMessage(
      *associated_remote.internal_state()->endpoint_client_for_test(), message,
      std::move(responder));
}

TEST_P(SyncFlagValidationTest, NoSyncAssociatedWithOneSync) {
  Remote<mojom::OneSync> remote;
  OneSyncImpl impl(remote.BindNewPipeAndPassReceiver());

  AssociatedRemote<mojom::NoSync> associated_remote;
  remote->BindNoSync(associated_remote.BindNewEndpointAndPassReceiver());

  FlushPostedTasks();

  Message message = MakeNoSyncMethodMessage();
  auto responder = std::make_unique<NoResponseExpectedResponder>();
  ASSERT_TRUE(remote.internal_state()->endpoint_client_for_test());
  ::mojo::internal::SendMojoMessage(
      *associated_remote.internal_state()->endpoint_client_for_test(), message,
      std::move(responder));
}

TEST_P(SyncFlagValidationTest, OneSyncAssociatedWithOneSync) {
  Remote<mojom::OneSync> remote;
  OneSyncImpl impl(remote.BindNewPipeAndPassReceiver());

  AssociatedRemote<mojom::OneSync> associated_remote;
  remote->BindOneSync(associated_remote.BindNewEndpointAndPassReceiver());

  FlushPostedTasks();

  Message message = MakeOneSyncMethodMessage();
  auto responder = std::make_unique<NoResponseExpectedResponder>();
  ASSERT_TRUE(remote.internal_state()->endpoint_client_for_test());
  ::mojo::internal::SendMojoMessage(
      *associated_remote.internal_state()->endpoint_client_for_test(), message,
      std::move(responder));
}

INSTANTIATE_TEST_SUITE_P(,
                         SyncFlagValidationTest,
                         ::testing::Values(0, Message::kFlagIsResponse));

}  // namespace
}  // namespace sync_method_unittest
}  // namespace test
}  // namespace mojo

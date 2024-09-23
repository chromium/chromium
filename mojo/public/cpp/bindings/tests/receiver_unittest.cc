// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/receiver.h"

#include <stdint.h>

#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/threading/thread.h"
#include "build/blink_buildflags.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/cpp/bindings/tests/receiver_unittest.test-mojom.h"
#include "mojo/public/cpp/system/functions.h"
#include "mojo/public/interfaces/bindings/tests/ping_service.mojom.h"
#include "mojo/public/interfaces/bindings/tests/sample_interfaces.mojom.h"
#include "mojo/public/interfaces/bindings/tests/sample_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace receiver_unittest {

class ServiceImpl : public sample::Service {
 public:
  ServiceImpl() = default;
  explicit ServiceImpl(bool* was_deleted)
      : destruction_callback_(base::BindLambdaForTesting(
            [was_deleted] { *was_deleted = true; })) {}
  explicit ServiceImpl(base::OnceClosure destruction_callback)
      : destruction_callback_(std::move(destruction_callback)) {}

  ServiceImpl(const ServiceImpl&) = delete;
  ServiceImpl& operator=(const ServiceImpl&) = delete;

  ~ServiceImpl() override {
    if (destruction_callback_)
      std::move(destruction_callback_).Run();
  }

 private:
  // sample::Service implementation
  void Frobinate(sample::FooPtr foo,
                 BazOptions options,
                 PendingRemote<sample::Port> port,
                 FrobinateCallback callback) override {
    std::move(callback).Run(1);
  }
  void GetPort(PendingReceiver<sample::Port> port) override {}

  base::OnceClosure destruction_callback_;
};

using ReceiverTest = BindingsTestBase;

TEST_P(ReceiverTest, Reset) {
  bool called = false;
  Remote<sample::Service> remote;

  ServiceImpl impl;
  Receiver<sample::Service> receiver(&impl,
                                     remote.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  remote.set_disconnect_handler(base::BindLambdaForTesting([&] {
    run_loop.Quit();
    called = true;
  }));

  receiver.reset();
  EXPECT_FALSE(called);
  run_loop.Run();
  EXPECT_TRUE(called);
}

// Tests that destroying a mojo::Binding closes the bound message pipe handle.
TEST_P(ReceiverTest, DestroyClosesMessagePipe) {
  bool encountered_error = false;
  ServiceImpl impl;
  Remote<sample::Service> remote;
  auto pending_receiver = remote.BindNewPipeAndPassReceiver();
  base::RunLoop run_loop;
  remote.set_disconnect_handler(base::BindLambdaForTesting([&] {
    run_loop.Quit();
    encountered_error = true;
  }));

  bool called = false;
  base::RunLoop run_loop2;
  {
    Receiver<sample::Service> binding(&impl, std::move(pending_receiver));
    remote->Frobinate(nullptr, sample::Service::BazOptions::REGULAR,
                      NullRemote(), base::BindLambdaForTesting([&](int32_t) {
                        run_loop2.Quit();
                        called = true;
                      }));
    run_loop2.Run();
    EXPECT_TRUE(called);
    EXPECT_FALSE(encountered_error);
  }
  // Now that the Binding is out of scope we should detect an error on the other
  // end of the pipe.
  run_loop.Run();
  EXPECT_TRUE(encountered_error);

  // And calls should fail.
  called = false;
  remote->Frobinate(
      nullptr, sample::Service::BazOptions::REGULAR, NullRemote(),
      base::BindLambdaForTesting([&](int32_t) { called = true; }));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(called);
}

// Tests that the binding's connection error handler gets called when the other
// end is closed.
TEST_P(ReceiverTest, Disconnect) {
  bool called = false;
  {
    ServiceImpl impl;
    Remote<sample::Service> remote;
    Receiver<sample::Service> receiver(&impl,
                                       remote.BindNewPipeAndPassReceiver());
    base::RunLoop run_loop;
    receiver.set_disconnect_handler(base::BindLambdaForTesting([&] {
      called = true;
      run_loop.Quit();
    }));
    remote.reset();
    EXPECT_FALSE(called);
    run_loop.Run();
    EXPECT_TRUE(called);
    // We want to make sure that it isn't called again during destruction.
    called = false;
  }
  EXPECT_FALSE(called);
}

// Tests that calling Close doesn't result in the connection error handler being
// called.
TEST_P(ReceiverTest, ResetDoesntCallDisconnectHandler) {
  ServiceImpl impl;
  Remote<sample::Service> remote;
  Receiver<sample::Service> receiver(&impl,
                                     remote.BindNewPipeAndPassReceiver());
  bool called = false;
  receiver.set_disconnect_handler(
      base::BindLambdaForTesting([&] { called = true; }));
  receiver.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(called);

  // We can also close the other end, and the error handler still won't be
  // called.
  remote.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(called);
}

class ServiceImplWithReceiver : public ServiceImpl {
 public:
  ServiceImplWithReceiver(bool* was_deleted,
                          base::OnceClosure closure,
                          PendingReceiver<sample::Service> receiver)
      : ServiceImpl(was_deleted),
        receiver_(this, std::move(receiver)),
        closure_(std::move(closure)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &ServiceImplWithReceiver::OnDisconnect, base::Unretained(this)));
  }

  ServiceImplWithReceiver(const ServiceImplWithReceiver&) = delete;
  ServiceImplWithReceiver& operator=(const ServiceImplWithReceiver&) = delete;

 private:
  ~ServiceImplWithReceiver() override { std::move(closure_).Run(); }

  void OnDisconnect() { delete this; }

  Receiver<sample::Service> receiver_;
  base::OnceClosure closure_;
};

// Tests that the receiver may be deleted in its disconnect handler.
TEST_P(ReceiverTest, SelfDeleteOnDisconnect) {
  bool was_deleted = false;
  Remote<sample::Service> remote;
  base::RunLoop run_loop;
  // This will delete itself on disconnect.
  new ServiceImplWithReceiver(&was_deleted, run_loop.QuitClosure(),
                              remote.BindNewPipeAndPassReceiver());
  remote.reset();
  EXPECT_FALSE(was_deleted);
  run_loop.Run();
  EXPECT_TRUE(was_deleted);
}

// Tests that explicitly calling Unbind followed by rebinding works.
TEST_P(ReceiverTest, Unbind) {
  ServiceImpl impl;
  Remote<sample::Service> remote;
  Receiver<sample::Service> receiver(&impl,
                                     remote.BindNewPipeAndPassReceiver());

  bool called = false;
  base::RunLoop run_loop;
  remote->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, NullRemote(),
                    base::BindLambdaForTesting([&](int32_t) {
                      called = true;
                      run_loop.Quit();
                    }));
  run_loop.Run();
  EXPECT_TRUE(called);

  called = false;
  auto pending_receiver = receiver.Unbind();
  EXPECT_FALSE(receiver.is_bound());
  EXPECT_TRUE(pending_receiver);

  // All calls should be withheld when the receiver is not bound...
  remote->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, NullRemote(),
                    base::BindLambdaForTesting([&](int32_t) {
                      called = true;
                      run_loop.Quit();
                    }));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(called);

  called = false;
  receiver.Bind(std::move(pending_receiver));
  EXPECT_TRUE(receiver.is_bound());
  EXPECT_FALSE(pending_receiver);

  // ...and calls should resume again when the receiver is
  // rebound.
  base::RunLoop run_loop2;
  remote->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, NullRemote(),
                    base::BindLambdaForTesting([&](int32_t) {
                      called = true;
                      run_loop2.Quit();
                    }));
  run_loop2.Run();
  EXPECT_TRUE(called);
}

class IntegerAccessorImpl : public sample::IntegerAccessor {
 public:
  IntegerAccessorImpl() = default;

  IntegerAccessorImpl(const IntegerAccessorImpl&) = delete;
  IntegerAccessorImpl& operator=(const IntegerAccessorImpl&) = delete;

  ~IntegerAccessorImpl() override = default;

 private:
  // sample::IntegerAccessor implementation.
  void GetInteger(GetIntegerCallback callback) override {
    std::move(callback).Run(1, sample::Enum::VALUE);
  }
  void SetInteger(int64_t data, sample::Enum type) override {}
};

TEST_P(ReceiverTest, PauseResume) {
  bool called = false;
  base::RunLoop run_loop;
  Remote<sample::Service> remote;
  ServiceImpl impl;
  Receiver<sample::Service> receiver(&impl,
                                     remote.BindNewPipeAndPassReceiver());
  receiver.Pause();
  remote->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, NullRemote(),
                    base::BindLambdaForTesting([&](int32_t) {
                      called = true;
                      run_loop.Quit();
                    }));

  EXPECT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  // Frobinate() should not be called as the receiver is paused.
  EXPECT_FALSE(called);

  // Resume the receiver, which should trigger processing.
  receiver.Resume();
  run_loop.Run();
  EXPECT_TRUE(called);
}

// Verifies the disconnect handler is not run while a receiver is paused.
TEST_P(ReceiverTest, ErrorHandleNotRunWhilePaused) {
  bool called = false;
  base::RunLoop run_loop;
  Remote<sample::Service> remote;
  ServiceImpl impl;
  Receiver<sample::Service> receiver(&impl,
                                     remote.BindNewPipeAndPassReceiver());
  receiver.set_disconnect_handler(base::BindLambdaForTesting([&] {
    called = true;
    run_loop.Quit();
  }));
  receiver.Pause();

  remote.reset();
  base::RunLoop().RunUntilIdle();
  // The disconnect handler should not be called as the receiver is paused.
  EXPECT_FALSE(called);

  // Resume the receiver, which should trigger the disconnect handler.
  receiver.Resume();
  run_loop.Run();
  EXPECT_TRUE(called);
}

class PingServiceImpl : public test::PingService {
 public:
  PingServiceImpl() = default;

  PingServiceImpl(const PingServiceImpl&) = delete;
  PingServiceImpl& operator=(const PingServiceImpl&) = delete;

  ~PingServiceImpl() override = default;

  // test::PingService:
  void Ping(PingCallback callback) override {
    if (ping_handler_)
      ping_handler_.Run();
    std::move(callback).Run();
  }

  void set_ping_handler(base::RepeatingClosure handler) {
    ping_handler_ = std::move(handler);
  }

 private:
  base::RepeatingClosure ping_handler_;
};

class CallbackFilter : public MessageFilter {
 public:
  explicit CallbackFilter(const base::RepeatingClosure& will_dispatch_callback,
                          const base::RepeatingClosure& did_dispatch_callback)
      : will_dispatch_callback_(will_dispatch_callback),
        did_dispatch_callback_(did_dispatch_callback) {}
  ~CallbackFilter() override {}

  static std::unique_ptr<CallbackFilter> Wrap(
      const base::RepeatingClosure& will_dispatch_callback,
      const base::RepeatingClosure& did_dispatch_callback) {
    return std::make_unique<CallbackFilter>(will_dispatch_callback,
                                            did_dispatch_callback);
  }

  // MessageFilter:
  bool WillDispatch(Message* message) override {
    will_dispatch_callback_.Run();
    return true;
  }

  void DidDispatchOrReject(Message* message, bool accepted) override {
    did_dispatch_callback_.Run();
  }

 private:
  const base::RepeatingClosure will_dispatch_callback_;
  const base::RepeatingClosure did_dispatch_callback_;
};

// Verifies that message filters are notified in the order they were added and
// are always notified before a message is dispatched.
TEST_P(ReceiverTest, MessageFilter) {
  Remote<test::PingService> remote;
  PingServiceImpl impl;
  Receiver<test::PingService> receiver(&impl,
                                       remote.BindNewPipeAndPassReceiver());

  int status = 0;
  receiver.SetFilter(CallbackFilter::Wrap(base::BindLambdaForTesting([&] {
                                            EXPECT_EQ(0, status);
                                            status = 1;
                                          }),
                                          base::BindLambdaForTesting([&] {
                                            EXPECT_EQ(2, status);
                                            status = 3;
                                          })));

  impl.set_ping_handler(base::BindLambdaForTesting([&] {
    EXPECT_EQ(1, status);
    status = 2;
  }));

  for (int i = 0; i < 10; ++i) {
    status = 0;
    base::RunLoop loop;
    remote->Ping(loop.QuitClosure());
    loop.Run();
    EXPECT_EQ(3, status);
  }
}

void Fail() {
  FAIL() << "Unexpected connection error";
}

TEST_P(ReceiverTest, FlushForTesting) {
  bool called = false;
  Remote<sample::Service> remote;
  ServiceImpl impl;
  Receiver<sample::Service> receiver(&impl,
                                     remote.BindNewPipeAndPassReceiver());
  receiver.set_disconnect_handler(base::BindOnce(&Fail));

  remote->Frobinate(
      nullptr, sample::Service::BazOptions::REGULAR, NullRemote(),
      base::BindLambdaForTesting([&](int32_t) { called = true; }));
  EXPECT_FALSE(called);
  // Because the flush is sent from the receiver, it only guarantees that the
  // request has been received, not the response. The second flush waits for
  // the response to be received.
  receiver.FlushForTesting();
  receiver.FlushForTesting();
  EXPECT_TRUE(called);
}

TEST_P(ReceiverTest, FlushForTestingWithClosedPeer) {
  bool called = false;
  Remote<sample::Service> remote;
  ServiceImpl impl;
  Receiver<sample::Service> receiver(&impl,
                                     remote.BindNewPipeAndPassReceiver());
  receiver.set_disconnect_handler(
      base::BindLambdaForTesting([&] { called = true; }));
  remote.reset();

  EXPECT_FALSE(called);
  receiver.FlushForTesting();
  EXPECT_TRUE(called);
  receiver.FlushForTesting();
}

TEST_P(ReceiverTest, DisconnectWithReason) {
  Remote<sample::Service> remote;
  ServiceImpl impl;
  Receiver<sample::Service> receiver(&impl,
                                     remote.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  receiver.set_disconnect_with_reason_handler(base::BindLambdaForTesting(
      [&](uint32_t custom_reason, const std::string& description) {
        EXPECT_EQ(1234u, custom_reason);
        EXPECT_EQ("hello", description);
        run_loop.Quit();
      }));

  remote.ResetWithReason(1234u, "hello");
  run_loop.Run();
}

TEST_P(ReceiverTest, PendingRemoteResetWithReason) {
  ServiceImpl impl;
  Receiver<sample::Service> receiver(&impl);
  PendingRemote<sample::Service> pending_remote =
      receiver.BindNewPipeAndPassRemote();

  base::RunLoop run_loop;
  receiver.set_disconnect_with_reason_handler(base::BindLambdaForTesting(
      [&](uint32_t custom_reason, const std::string& description) {
        EXPECT_EQ(1234u, custom_reason);
        EXPECT_EQ("hello", description);
        run_loop.Quit();
      }));

  pending_remote.ResetWithReason(1234u, "hello");
  run_loop.Run();
}

template <typename T>
struct WeakPtrImplRefTraits {
  using PointerType = base::WeakPtr<T>;

  static bool IsNull(const base::WeakPtr<T>& ptr) { return !ptr; }
  static T* GetRawPointer(base::WeakPtr<T>* ptr) { return ptr->get(); }
};

template <typename T>
using WeakReceiver = Receiver<T, WeakPtrImplRefTraits<T>>;

TEST_P(ReceiverTest, CustomImplPointerType) {
  PingServiceImpl impl;
  base::WeakPtrFactory<test::PingService> weak_factory(&impl);

  Remote<test::PingService> remote;
  WeakReceiver<test::PingService> receiver(weak_factory.GetWeakPtr(),
                                           remote.BindNewPipeAndPassReceiver());

  {
    // Ensure the receiver is functioning.
    base::RunLoop run_loop;
    remote->Ping(run_loop.QuitClosure());
    run_loop.Run();
  }

  {
    // Attempt to dispatch another message after the WeakPtr is invalidated.
    impl.set_ping_handler(base::BindRepeating([] { NOTREACHED(); }));
    remote->Ping(base::BindOnce([] { NOTREACHED(); }));

    // The receiver will close its end of the pipe which will trigger a
    // disconnect on |remote|.
    base::RunLoop run_loop;
    remote.set_disconnect_handler(run_loop.QuitClosure());
    weak_factory.InvalidateWeakPtrs();
    run_loop.Run();
  }
}

TEST_P(ReceiverTest, ReportBadMessage) {
  bool called = false;
  Remote<test::PingService> remote;
  auto pending_receiver = remote.BindNewPipeAndPassReceiver();
  base::RunLoop run_loop;
  remote.set_disconnect_handler(base::BindLambdaForTesting([&] {
    called = true;
    run_loop.Quit();
  }));
  PingServiceImpl impl;
  Receiver<test::PingService> receiver(&impl, std::move(pending_receiver));
  impl.set_ping_handler(base::BindLambdaForTesting(
      [&] { receiver.ReportBadMessage("received bad message"); }));

  std::string received_error;
  SetDefaultProcessErrorHandler(base::BindLambdaForTesting(
      [&](const std::string& error) { received_error = error; }));

  remote->Ping(base::DoNothing());
  EXPECT_FALSE(called);
  run_loop.Run();
  EXPECT_TRUE(called);
  EXPECT_EQ("received bad message", received_error);

  SetDefaultProcessErrorHandler(base::NullCallback());
}

TEST_P(ReceiverTest, GetBadMessageCallback) {
  Remote<test::PingService> remote;
  base::RunLoop run_loop;
  PingServiceImpl impl;
  ReportBadMessageCallback bad_message_callback;

  std::string received_error;
  SetDefaultProcessErrorHandler(base::BindLambdaForTesting(
      [&](const std::string& error) { received_error = error; }));

  {
    Receiver<test::PingService> receiver(&impl,
                                         remote.BindNewPipeAndPassReceiver());
    impl.set_ping_handler(base::BindLambdaForTesting(
        [&] { bad_message_callback = receiver.GetBadMessageCallback(); }));
    remote->Ping(run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_TRUE(received_error.empty());
    EXPECT_TRUE(bad_message_callback);
  }

  std::move(bad_message_callback).Run("delayed bad message");
  EXPECT_EQ("delayed bad message", received_error);

  SetDefaultProcessErrorHandler(base::NullCallback());
}

TEST_P(ReceiverTest, InvalidPendingReceivers) {
  PendingReceiver<sample::Service> uninitialized_pending;
  EXPECT_FALSE(uninitialized_pending);

  // A "null" receiver is just a generic helper for an uninitialized
  // PendingReceiver. Verify that it's equivalent to above.
  PendingReceiver<sample::Service> null_pending{NullReceiver()};
  EXPECT_FALSE(null_pending);
}

TEST_P(ReceiverTest, GenericPendingReceiver) {
  Remote<sample::Service> remote;
  GenericPendingReceiver receiver;
  EXPECT_FALSE(receiver.is_valid());
  EXPECT_FALSE(receiver.interface_name().has_value());

  receiver = GenericPendingReceiver(remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(receiver.is_valid());
  EXPECT_EQ(sample::Service::Name_, receiver.interface_name());

  auto ping_receiver = receiver.As<test::PingService>();
  EXPECT_FALSE(ping_receiver.is_valid());
  EXPECT_TRUE(receiver.is_valid());

  auto sample_receiver = receiver.As<sample::Service>();
  EXPECT_TRUE(sample_receiver.is_valid());
  EXPECT_FALSE(receiver.is_valid());
}

TEST_P(ReceiverTest, GenericPendingAssociatedReceiver) {
  AssociatedRemote<sample::Service> remote;
  GenericPendingAssociatedReceiver receiver;
  EXPECT_FALSE(receiver.is_valid());
  EXPECT_FALSE(receiver.interface_name().has_value());

  receiver =
      GenericPendingAssociatedReceiver(remote.BindNewEndpointAndPassReceiver());
  ASSERT_TRUE(receiver.is_valid());
  EXPECT_EQ(sample::Service::Name_, receiver.interface_name());

  auto ping_receiver = receiver.As<test::PingService>();
  EXPECT_FALSE(ping_receiver.is_valid());
  EXPECT_TRUE(receiver.is_valid());

  auto sample_receiver = receiver.As<sample::Service>();
  EXPECT_TRUE(sample_receiver.is_valid());
  EXPECT_FALSE(receiver.is_valid());
}

class RebindTestImpl : public mojom::RebindTestInterface {
 public:
  explicit RebindTestImpl(base::WaitableEvent* event) : event_(event) {
    DCHECK(event_);
  }
  ~RebindTestImpl() override = default;

  // mojom::RebindTestInterface
  void BlockingUntilExternalSignalCall() override { event_->Wait(); }
  void NormalCall() override {}
  void SyncCall(SyncCallCallback callback) override {
    std::move(callback).Run();
  }

 private:
  raw_ptr<base::WaitableEvent> event_;
};

TEST_P(ReceiverTest, RebindWithScheduledSyncMessage) {
  base::WaitableEvent event{base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED};
  RebindTestImpl impl{&event};
  base::Thread receiver_thread{"receiver"};
  Remote<mojom::RebindTestInterface> remote;
  // Accessible only on receiver thread
  Receiver<mojom::RebindTestInterface> receiver1{&impl};
  Receiver<mojom::RebindTestInterface> receiver2{&impl};

  receiver_thread.Start();

  // Setup of remote and receiver
  auto pending_receiver = remote.BindNewPipeAndPassReceiver();
  receiver_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [&]() { receiver1.Bind(std::move(pending_receiver)); }));
  receiver_thread.FlushForTesting();

  // Perform test
  remote->BlockingUntilExternalSignalCall();
  remote->NormalCall();

  receiver_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [&]() { receiver2.Bind(receiver1.Unbind()); }));
  event.Signal();

  remote->SyncCall();

  // Cleanup
  remote.reset();
  receiver_thread.task_runner()->PostTask(FROM_HERE,
                                          base::BindLambdaForTesting([&]() {
                                            receiver1.reset();
                                            receiver2.reset();
                                          }));
  receiver_thread.FlushForTesting();
}

class TestGenericBinderImpl : public mojom::TestGenericBinder {
 public:
  explicit TestGenericBinderImpl(
      PendingReceiver<mojom::TestGenericBinder> receiver)
      : receiver_(this, std::move(receiver)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &TestGenericBinderImpl::OnDisconnect, base::Unretained(this)));
  }

  TestGenericBinderImpl(const TestGenericBinderImpl&) = delete;
  TestGenericBinderImpl& operator=(const TestGenericBinderImpl&) = delete;

  ~TestGenericBinderImpl() override = default;

  bool connected() const { return connected_; }

  void WaitForNextReceiver(GenericPendingReceiver* storage) {
    wait_loop_.emplace();
    next_receiver_storage_ = storage;
    wait_loop_->Run();
  }

  void WaitForNextAssociatedReceiver(
      GenericPendingAssociatedReceiver* storage) {
    wait_loop_.emplace();
    next_associated_receiver_storage_ = storage;
    wait_loop_->Run();
  }

  // mojom::TestGenericBinder:
  void BindOptionalReceiver(GenericPendingReceiver receiver) override {
    if (next_receiver_storage_) {
      *next_receiver_storage_ = std::move(receiver);
      next_receiver_storage_ = nullptr;
    }
    if (wait_loop_)
      wait_loop_->Quit();
  }

  void BindReceiver(GenericPendingReceiver receiver) override {
    if (next_receiver_storage_) {
      *next_receiver_storage_ = std::move(receiver);
      next_receiver_storage_ = nullptr;
    }
    if (wait_loop_)
      wait_loop_->Quit();
  }

  void BindOptionalAssociatedReceiver(
      GenericPendingAssociatedReceiver receiver) override {
    if (next_associated_receiver_storage_) {
      *next_associated_receiver_storage_ = std::move(receiver);
      next_associated_receiver_storage_ = nullptr;
    }
    if (wait_loop_)
      wait_loop_->Quit();
  }

  void BindAssociatedReceiver(
      GenericPendingAssociatedReceiver receiver) override {
    if (next_associated_receiver_storage_) {
      *next_associated_receiver_storage_ = std::move(receiver);
      next_associated_receiver_storage_ = nullptr;
    }
    if (wait_loop_)
      wait_loop_->Quit();
  }

 private:
  void OnDisconnect() {
    if (wait_loop_)
      wait_loop_->Quit();
    connected_ = false;
  }

  Receiver<mojom::TestGenericBinder> receiver_;
  bool connected_ = true;
  std::optional<base::RunLoop> wait_loop_;
  raw_ptr<GenericPendingReceiver> next_receiver_storage_ = nullptr;
  raw_ptr<GenericPendingAssociatedReceiver> next_associated_receiver_storage_ =
      nullptr;
};

using ReceiverSerializationTest = ReceiverTest;

TEST_P(ReceiverSerializationTest, NullGenericPendingReceiver) {
  Remote<mojom::TestGenericBinder> remote;
  GenericPendingReceiver receiver;
  TestGenericBinderImpl binder(remote.BindNewPipeAndPassReceiver());

  // Bind a null, nullable receiver.
  remote->BindOptionalReceiver(GenericPendingReceiver());
  binder.WaitForNextReceiver(&receiver);
  EXPECT_FALSE(receiver.is_valid());

  // Bind some valid non-null, non-nullable receivers.
  remote->BindReceiver(
      mojo::Remote<mojom::TestInterface1>().BindNewPipeAndPassReceiver());
  binder.WaitForNextReceiver(&receiver);
  EXPECT_TRUE(receiver.is_valid());
  EXPECT_FALSE(receiver.As<mojom::TestInterface2>());
  EXPECT_TRUE(receiver.As<mojom::TestInterface1>());

  remote->BindReceiver(
      mojo::Remote<mojom::TestInterface2>().BindNewPipeAndPassReceiver());
  binder.WaitForNextReceiver(&receiver);
  EXPECT_TRUE(receiver.is_valid());
  EXPECT_FALSE(receiver.As<mojom::TestInterface1>());
  EXPECT_TRUE(receiver.As<mojom::TestInterface2>());

  mojo::internal::SerializationWarningObserverForTesting observer;

  // Now attempt to send a null receiver for a non-nullable argument.
  EXPECT_TRUE(binder.connected());
  remote->BindReceiver(GenericPendingReceiver());

  // We should see a validation warning at serialization time. Normally this
  // results in a DCHECK, but it's suppressed by the testing observer we have on
  // the stack. Note that this only works for DCHECK-enabled builds. For
  // non-DCHECK-enabled builds, serialization will succeed above with no errors,
  // but the receiver below will still reject the message and disconnect.
#if DCHECK_IS_ON()
  EXPECT_EQ(mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
            observer.last_warning());
#endif

  // |receiver| should not be modified again by the implementation in |binder|,
  // because the it must never receive the invalid request. Instead the Wait
  // should be terminated by disconnection.
  receiver = mojo::Remote<mojom::TestInterface1>().BindNewPipeAndPassReceiver();
  binder.WaitForNextReceiver(&receiver);
  EXPECT_TRUE(receiver.is_valid());
  EXPECT_TRUE(receiver.As<mojom::TestInterface1>());
  EXPECT_FALSE(binder.connected());
}

TEST_P(ReceiverSerializationTest, NullGenericPendingAssociatedReceiver) {
  Remote<mojom::TestGenericBinder> remote;
  GenericPendingAssociatedReceiver receiver;
  TestGenericBinderImpl binder(remote.BindNewPipeAndPassReceiver());

  // Bind a null, nullable associated receiver.
  remote->BindOptionalAssociatedReceiver(GenericPendingAssociatedReceiver());
  binder.WaitForNextAssociatedReceiver(&receiver);
  EXPECT_FALSE(receiver.is_valid());

  // Bind some valid non-null, non-nullable associated receivers.
  remote->BindAssociatedReceiver(mojo::AssociatedRemote<mojom::TestInterface1>()
                                     .BindNewEndpointAndPassReceiver());
  binder.WaitForNextAssociatedReceiver(&receiver);
  EXPECT_TRUE(receiver.is_valid());
  EXPECT_FALSE(receiver.As<mojom::TestInterface2>());
  EXPECT_TRUE(receiver.As<mojom::TestInterface1>());

  remote->BindAssociatedReceiver(mojo::AssociatedRemote<mojom::TestInterface2>()
                                     .BindNewEndpointAndPassReceiver());
  binder.WaitForNextAssociatedReceiver(&receiver);
  EXPECT_TRUE(receiver.is_valid());
  EXPECT_FALSE(receiver.As<mojom::TestInterface1>());
  EXPECT_TRUE(receiver.As<mojom::TestInterface2>());

  mojo::internal::SerializationWarningObserverForTesting observer;

  // Now attempt to send a null associated receiver for a non-nullable argument.
  EXPECT_TRUE(binder.connected());
  remote->BindAssociatedReceiver(GenericPendingAssociatedReceiver());

  // We should see a validation warning at serialization time. Normally this
  // results in a DCHECK, but it's suppressed by the testing observer we have on
  // the stack. Note that this only works for DCHECK-enabled builds. For
  // non-DCHECK-enabled builds, serialization will succeed above with no errors,
  // but the receiver below will still reject the message and disconnect.
#if DCHECK_IS_ON()
  EXPECT_EQ(mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
            observer.last_warning());
#endif

  // `receiver` should not be modified again by the implementation in `binder`,
  // because the it must never receive the invalid request. Instead the Wait
  // should be terminated by disconnection.
  receiver = mojo::AssociatedRemote<mojom::TestInterface1>()
                 .BindNewEndpointAndPassReceiver();
  binder.WaitForNextAssociatedReceiver(&receiver);
  EXPECT_TRUE(receiver.is_valid());
  EXPECT_TRUE(receiver.As<mojom::TestInterface1>());
  EXPECT_FALSE(binder.connected());
}

using SelfOwnedReceiverTest = BindingsTestBase;

TEST_P(SelfOwnedReceiverTest, CloseDestroysImplAndPipe) {
  base::RunLoop run_loop;
  bool disconnected = false;
  bool was_deleted = false;
  Remote<sample::Service> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();
  remote.set_disconnect_handler(base::BindLambdaForTesting([&] {
    disconnected = true;
    run_loop.Quit();
  }));
  bool called = false;
  base::RunLoop run_loop2;

  auto binding = MakeSelfOwnedReceiver<sample::Service>(
      std::make_unique<ServiceImpl>(&was_deleted), std::move(receiver));
  remote->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, NullRemote(),
                    base::BindLambdaForTesting([&](int32_t) {
                      called = true;
                      run_loop2.Quit();
                    }));
  run_loop2.Run();
  EXPECT_TRUE(called);
  EXPECT_FALSE(disconnected);
  binding->Close();

  // Now that the SelfOwnedReceiver is closed we should detect an error on the
  // other end of the pipe.
  run_loop.Run();
  EXPECT_TRUE(disconnected);

  // Destroying the SelfOwnedReceiver also destroys the impl.
  ASSERT_TRUE(was_deleted);
}

TEST_P(SelfOwnedReceiverTest, DisconnectDestroysImplAndPipe) {
  Remote<sample::Service> remote;
  bool was_deleted = false;
  base::RunLoop run_loop;

  MakeSelfOwnedReceiver<sample::Service>(
      std::make_unique<ServiceImpl>(base::BindLambdaForTesting([&] {
        was_deleted = true;
        run_loop.Quit();
      })),
      remote.BindNewPipeAndPassReceiver());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(was_deleted);

  remote.reset();
  EXPECT_FALSE(was_deleted);
  run_loop.Run();
  EXPECT_TRUE(was_deleted);
}

class MultiprocessReceiverTest : public core::test::MojoTestBase,
                                 public mojom::TestInterface1 {
 public:
  ~MultiprocessReceiverTest() override = default;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

class InterfaceDropper : public mojom::InterfaceDropper {
 public:
  explicit InterfaceDropper(PendingReceiver<mojom::InterfaceDropper> receiver)
      : receiver_(this, std::move(receiver)) {}

  void set_disconnect_handler(base::OnceClosure callback) {
    receiver_.set_disconnect_handler(std::move(callback));
  }

 private:
  // mojom::InterfaceDropper:
  void Drop(PendingRemote<mojom::TestInterface1> remote) override {
    // Nothing to do but let `remote` go out of scope, effectively closing it
    // and signaling peer closure to the Receiver in another process.
  }

  Receiver<mojom::InterfaceDropper> receiver_;
};

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(MultiprocessReceiverClient,
                                  MultiprocessReceiverTest,
                                  test_pipe) {
  // This client sits in a loop listening for new interface remotes to drop.
  // Each dropped remote sends a signal back to the main test process to notify
  // the receiver there, and those notifications are meant to elicit a potential
  // race in the internal dispatch machinery of that receiver. The client
  // terminates when the InterfaceDropper is disconnected, which happens once
  // the test is done throwing remotes at it.
  MojoHandle dropper_pipe;
  ReadMessageWithHandles(test_pipe, &dropper_pipe, 1);
  base::RunLoop loop;
  InterfaceDropper dropper{PendingReceiver<mojom::InterfaceDropper>(
      MakeScopedHandle(MessagePipeHandle(dropper_pipe)))};
  dropper.set_disconnect_handler(loop.QuitClosure());
  loop.Run();
  MojoClose(test_pipe);
}

// iOS doesn't have the ability to fork processes yet.
#if !BUILDFLAG(IS_IOS)
TEST_F(MultiprocessReceiverTest, MultiprocessReceiver) {
  // Regression test for https://crbug.com/1371860.
  //
  // This test establishes a remote connection to an InterfaceDropper
  // implementation in the client process; then it continually creates new test
  // interface endpoints, sending one end to the client to be destroyed from
  // there; and binding the other end as a local receiver that is also
  // immediately destroyed.
  //
  // This effectively exercises potential races between receiver lifetime and
  // incoming IO thread activity, as an event to signal peer closure on the new
  // receiver may arrive on the IO thread during receiver teardown on this
  // thread.
  if (!mojo::core::IsMojoIpczEnabled()) {
    GTEST_SKIP() << "This is a regression test specifically for MojoIpcz. When "
                 << "MojoIpcz is disabled, the test is flaky for unrelated "
                 << "reasons which stem from a long-standing bug in Mojo Core "
                 << "shutdown.";
  }
  RunTestClient("MultiprocessReceiverClient", [&](MojoHandle client) {
    Remote<mojom::InterfaceDropper> dropper;
    MojoHandle dropper_pipe =
        dropper.BindNewPipeAndPassReceiver().PassPipe().release().value();
    WriteMessageWithHandles(client, "", &dropper_pipe, 1);

    constexpr size_t kNumIterations = 1000;
    constexpr size_t kNumReceiversPerIteration = 10;
    for (size_t i = 0; i < kNumIterations; ++i) {
      std::vector<std::optional<Receiver<mojom::TestInterface1>>> receivers(
          kNumReceiversPerIteration);
      for (auto& receiver : receivers) {
        receiver.emplace(this);
        auto remote = receiver->BindNewPipeAndPassRemote();

        // Installing a disconnect handler ensures that we set up the machinery
        // necessary to watch for incoming IO events targeting this receiver.
        receiver->set_disconnect_handler(base::BindOnce([] {}));

        dropper->Drop(std::move(remote));
      }
    }
  });
}
#endif  // BUILDFLAG(USE_BLINK)

INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(ReceiverTest);
INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(SelfOwnedReceiverTest);

// These tests only make sense for serialized messages.
INSTANTIATE_TEST_SUITE_P(
    All,
    ReceiverSerializationTest,
    testing::Values(mojo::BindingsTestSerializationMode::kSerializeBeforeSend));

}  // namespace receiver_unittest
}  // namespace test
}  // namespace mojo

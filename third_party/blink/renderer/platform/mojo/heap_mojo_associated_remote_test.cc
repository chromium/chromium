// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"

#include "base/test/null_task_runner.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/interfaces/bindings/tests/sample_service.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap_observer_list.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

class ServiceImpl : public sample::blink::Service {
 public:
  mojo::AssociatedReceiver<sample::blink::Service>& associated_receiver() {
    return associated_receiver_;
  }

 private:
  // sample::blink::Service implementation
  void Frobinate(sample::blink::FooPtr foo,
                 sample::blink::Service::BazOptions options,
                 mojo::PendingRemote<sample::blink::Port> port,
                 sample::blink::Service::FrobinateCallback callback) override {}
  void GetPort(mojo::PendingReceiver<sample::blink::Port> port) override {}

  mojo::AssociatedReceiver<sample::blink::Service> associated_receiver_{this};
};

template <HeapMojoWrapperMode Mode>
class AssociatedRemoteOwner
    : public GarbageCollected<AssociatedRemoteOwner<Mode>> {
 public:
  explicit AssociatedRemoteOwner(MockContextLifecycleNotifier* context)
      : associated_remote_(context) {}
  explicit AssociatedRemoteOwner(
      HeapMojoAssociatedRemote<sample::blink::Service, Mode> associated_remote)
      : associated_remote_(std::move(associated_remote)) {}

  HeapMojoAssociatedRemote<sample::blink::Service, Mode>& associated_remote() {
    return associated_remote_;
  }

  void Trace(Visitor* visitor) const { visitor->Trace(associated_remote_); }

  HeapMojoAssociatedRemote<sample::blink::Service, Mode> associated_remote_;
};

template <HeapMojoWrapperMode Mode>
class HeapMojoAssociatedRemoteDestroyContextBaseTest : public TestSupportingGC {
 protected:
  void SetUp() override {
    context_ = MakeGarbageCollected<MockContextLifecycleNotifier>();
    owner_ = MakeGarbageCollected<AssociatedRemoteOwner<Mode>>(context_);
    scoped_refptr<base::NullTaskRunner> null_task_runner =
        base::MakeRefCounted<base::NullTaskRunner>();
    impl_.associated_receiver().Bind(
        owner_->associated_remote().BindNewEndpointAndPassReceiver(
            null_task_runner));
  }

  ServiceImpl impl_;
  Persistent<MockContextLifecycleNotifier> context_;
  Persistent<AssociatedRemoteOwner<Mode>> owner_;
};

template <HeapMojoWrapperMode Mode>
class HeapMojoAssociatedRemoteDisconnectWithReasonHandlerBaseTest
    : public TestSupportingGC {
 public:
  base::RunLoop& run_loop() { return run_loop_; }
  bool& disconnected_with_reason() { return disconnected_with_reason_; }

 protected:
  void SetUp() override {
    CHECK(!disconnected_with_reason_);
    context_ = MakeGarbageCollected<MockContextLifecycleNotifier>();
    owner_ = MakeGarbageCollected<AssociatedRemoteOwner<Mode>>(context_);
    scoped_refptr<base::NullTaskRunner> null_task_runner =
        base::MakeRefCounted<base::NullTaskRunner>();
    impl_.associated_receiver().Bind(
        owner_->associated_remote().BindNewEndpointAndPassReceiver(
            null_task_runner));
    impl_.associated_receiver().set_disconnect_with_reason_handler(
        WTF::BindOnce(
            [](HeapMojoAssociatedRemoteDisconnectWithReasonHandlerBaseTest*
                   associated_remote_test,
               const uint32_t custom_reason, const std::string& description) {
              associated_remote_test->run_loop().Quit();
              associated_remote_test->disconnected_with_reason() = true;
            },
            WTF::Unretained(this)));
  }

  ServiceImpl impl_;
  Persistent<MockContextLifecycleNotifier> context_;
  Persistent<AssociatedRemoteOwner<Mode>> owner_;
  base::RunLoop run_loop_;
  bool disconnected_with_reason_ = false;
};

template <HeapMojoWrapperMode Mode>
class HeapMojoAssociatedRemoteMoveBaseTest : public TestSupportingGC {
 protected:
  void SetUp() override {
    context_ = MakeGarbageCollected<MockContextLifecycleNotifier>();
    HeapMojoAssociatedRemote<sample::blink::Service, Mode> associated_remote(
        context_);
    owner_ = MakeGarbageCollected<AssociatedRemoteOwner<Mode>>(
        std::move(associated_remote));
    scoped_refptr<base::NullTaskRunner> null_task_runner =
        base::MakeRefCounted<base::NullTaskRunner>();
    impl_.associated_receiver().Bind(
        owner_->associated_remote().BindNewEndpointAndPassReceiver(
            null_task_runner));
  }

  ServiceImpl impl_;
  Persistent<MockContextLifecycleNotifier> context_;
  Persistent<AssociatedRemoteOwner<Mode>> owner_;
};

}  // namespace

class HeapMojoAssociatedRemoteDestroyContextWithContextObserverTest
    : public HeapMojoAssociatedRemoteDestroyContextBaseTest<
          HeapMojoWrapperMode::kWithContextObserver> {};
class HeapMojoAssociatedRemoteDestroyContextWithoutContextObserverTest
    : public HeapMojoAssociatedRemoteDestroyContextBaseTest<
          HeapMojoWrapperMode::kForceWithoutContextObserver> {};
class HeapMojoAssociatedRemoteDisconnectWithReasonHandlerWithContextObserverTest
    : public HeapMojoAssociatedRemoteDisconnectWithReasonHandlerBaseTest<
          HeapMojoWrapperMode::kWithContextObserver> {};
class
    HeapMojoAssociatedRemoteDisconnectWithReasonHandlerWithoutContextObserverTest
    : public HeapMojoAssociatedRemoteDisconnectWithReasonHandlerBaseTest<
          HeapMojoWrapperMode::kForceWithoutContextObserver> {};
class HeapMojoAssociatedRemoteMoveWithContextObserverTest
    : public HeapMojoAssociatedRemoteMoveBaseTest<
          HeapMojoWrapperMode::kWithContextObserver> {};
class HeapMojoAssociatedRemoteMoveWithoutContextObserverTest
    : public HeapMojoAssociatedRemoteMoveBaseTest<
          HeapMojoWrapperMode::kForceWithoutContextObserver> {};

// Destroy the context with context observer and check that the connection is
// disconnected.
TEST_F(HeapMojoAssociatedRemoteDestroyContextWithContextObserverTest,
       ResetsOnContextDestroyed) {
  EXPECT_TRUE(owner_->associated_remote().is_bound());
  context_->NotifyContextDestroyed();
  EXPECT_FALSE(owner_->associated_remote().is_bound());
}

// Destroy the context without context observer and check that the connection is
// still connected.
TEST_F(HeapMojoAssociatedRemoteDestroyContextWithoutContextObserverTest,
       ResetsOnContextDestroyed) {
  EXPECT_TRUE(owner_->associated_remote().is_bound());
  context_->NotifyContextDestroyed();
  EXPECT_TRUE(owner_->associated_remote().is_bound());
}

// Reset the AssociatedRemote with custom reason and check that the specified
// handler is fired.
TEST_F(
    HeapMojoAssociatedRemoteDisconnectWithReasonHandlerWithContextObserverTest,
    ResetWithReason) {
  EXPECT_FALSE(disconnected_with_reason());
  const std::string message = "test message";
  const uint32_t reason = 0;
  owner_->associated_remote().ResetWithReason(reason, message);
  run_loop().Run();
  EXPECT_TRUE(disconnected_with_reason());
}

// Reset the AssociatedRemote with custom reason and check that the specified
// handler is fired.
TEST_F(
    HeapMojoAssociatedRemoteDisconnectWithReasonHandlerWithoutContextObserverTest,
    ResetWithReason) {
  EXPECT_FALSE(disconnected_with_reason());
  const std::string message = "test message";
  const uint32_t reason = 0;
  owner_->associated_remote().ResetWithReason(reason, message);
  run_loop().Run();
  EXPECT_TRUE(disconnected_with_reason());
}

// Move the AssociatedRemote from the outside of Owner class.
TEST_F(HeapMojoAssociatedRemoteMoveWithContextObserverTest, MoveSemantics) {
  EXPECT_TRUE(owner_->associated_remote().is_bound());
  context_->NotifyContextDestroyed();
  EXPECT_FALSE(owner_->associated_remote().is_bound());
}

// Move the AssociatedRemote from the outside of Owner class.
TEST_F(HeapMojoAssociatedRemoteMoveWithoutContextObserverTest, MoveSemantics) {
  EXPECT_TRUE(owner_->associated_remote().is_bound());
  context_->NotifyContextDestroyed();
  EXPECT_TRUE(owner_->associated_remote().is_bound());
}

}  // namespace blink

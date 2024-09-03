// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"

#include "base/memory/raw_ptr.h"
#include "base/test/null_task_runner.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/interfaces/bindings/tests/sample_service.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/heap_observer_list.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

template <HeapMojoWrapperMode Mode>
class HeapMojoAssociatedReceiverGCBaseTest;

template <HeapMojoWrapperMode Mode>
class AssociatedReceiverOwner
    : public GarbageCollected<AssociatedReceiverOwner<Mode>>,
      public sample::blink::Service {
  USING_PRE_FINALIZER(AssociatedReceiverOwner, Dispose);

 public:
  explicit AssociatedReceiverOwner(
      MockContextLifecycleNotifier* context,
      HeapMojoAssociatedReceiverGCBaseTest<Mode>* test = nullptr)
      : associated_receiver_(this, context), test_(test) {
    if (test_)
      test_->set_is_owner_alive(true);
  }

  void Dispose() {
    if (test_)
      test_->set_is_owner_alive(false);
  }

  HeapMojoAssociatedReceiver<sample::blink::Service,
                             AssociatedReceiverOwner,
                             Mode>&
  associated_receiver() {
    return associated_receiver_;
  }

  void Trace(Visitor* visitor) const { visitor->Trace(associated_receiver_); }

 private:
  // sample::blink::Service implementation
  void Frobinate(sample::blink::FooPtr foo,
                 sample::blink::Service::BazOptions options,
                 mojo::PendingRemote<sample::blink::Port> port,
                 sample::blink::Service::FrobinateCallback callback) override {}
  void GetPort(mojo::PendingReceiver<sample::blink::Port> port) override {}

  HeapMojoAssociatedReceiver<sample::blink::Service,
                             AssociatedReceiverOwner,
                             Mode>
      associated_receiver_;
  raw_ptr<HeapMojoAssociatedReceiverGCBaseTest<Mode>> test_;
};

template <HeapMojoWrapperMode Mode>
class HeapMojoAssociatedReceiverGCBaseTest : public TestSupportingGC {
 public:
  base::RunLoop& run_loop() { return run_loop_; }
  bool& disconnected() { return disconnected_; }

  void set_is_owner_alive(bool alive) { is_owner_alive_ = alive; }
  void ClearOwner() { owner_ = nullptr; }

 protected:
  void SetUp() override {
    disconnected_ = false;
    context_ = MakeGarbageCollected<MockContextLifecycleNotifier>();
    owner_ =
        MakeGarbageCollected<AssociatedReceiverOwner<Mode>>(context_, this);
    scoped_refptr<base::NullTaskRunner> null_task_runner =
        base::MakeRefCounted<base::NullTaskRunner>();
    associated_remote_ = mojo::AssociatedRemote<sample::blink::Service>(
        owner_->associated_receiver().BindNewEndpointAndPassRemote(
            null_task_runner));
    associated_remote_.set_disconnect_handler(WTF::BindOnce(
        [](HeapMojoAssociatedReceiverGCBaseTest* associated_receiver_test) {
          associated_receiver_test->run_loop().Quit();
          associated_receiver_test->disconnected() = true;
        },
        WTF::Unretained(this)));
  }
  void TearDown() {
    owner_ = nullptr;
    PreciselyCollectGarbage();
  }

  Persistent<MockContextLifecycleNotifier> context_;
  Persistent<AssociatedReceiverOwner<Mode>> owner_;
  bool is_owner_alive_ = false;
  base::RunLoop run_loop_;
  mojo::AssociatedRemote<sample::blink::Service> associated_remote_;
  bool disconnected_ = false;
};

template <HeapMojoWrapperMode Mode>
class HeapMojoAssociatedReceiverDestroyContextBaseTest
    : public TestSupportingGC {
 protected:
  void SetUp() override {
    context_ = MakeGarbageCollected<MockContextLifecycleNotifier>();
    owner_ = MakeGarbageCollected<AssociatedReceiverOwner<Mode>>(context_);
    scoped_refptr<base::NullTaskRunner> null_task_runner =
        base::MakeRefCounted<base::NullTaskRunner>();
    associated_remote_ = mojo::AssociatedRemote<sample::blink::Service>(
        owner_->associated_receiver().BindNewEndpointAndPassRemote(
            null_task_runner));
  }

  Persistent<MockContextLifecycleNotifier> context_;
  Persistent<AssociatedReceiverOwner<Mode>> owner_;
  mojo::AssociatedRemote<sample::blink::Service> associated_remote_;
};

}  // namespace

class HeapMojoAssociatedReceiverGCWithContextObserverTest
    : public HeapMojoAssociatedReceiverGCBaseTest<
          HeapMojoWrapperMode::kWithContextObserver> {};
class HeapMojoAssociatedReceiverGCWithoutContextObserverTest
    : public HeapMojoAssociatedReceiverGCBaseTest<
          HeapMojoWrapperMode::kForceWithoutContextObserver> {};
class HeapMojoAssociatedReceiverDestroyContextWithContextObserverTest
    : public HeapMojoAssociatedReceiverDestroyContextBaseTest<
          HeapMojoWrapperMode::kWithContextObserver> {};
class HeapMojoAssociatedReceiverDestroyContextWithoutContextObserverTest
    : public HeapMojoAssociatedReceiverDestroyContextBaseTest<
          HeapMojoWrapperMode::kForceWithoutContextObserver> {};

// Make HeapMojoAssociatedReceiver with context observer garbage collected and
// check that the connection is disconnected right after the marking phase.
TEST_F(HeapMojoAssociatedReceiverGCWithContextObserverTest, ResetsOnGC) {
  ClearOwner();
  EXPECT_FALSE(disconnected());
  PreciselyCollectGarbage();
  run_loop().Run();
  EXPECT_TRUE(disconnected());
}

// Check that the owner
TEST_F(HeapMojoAssociatedReceiverGCWithContextObserverTest,
       NoResetOnConservativeGC) {
  auto* wrapper = owner_->associated_receiver().wrapper_.Get();
  EXPECT_TRUE(owner_->associated_receiver().is_bound());
  ClearOwner();
  EXPECT_TRUE(is_owner_alive_);
  // The stack scanning should find |wrapper| and keep the Wrapper alive.
  ConservativelyCollectGarbage();
  EXPECT_TRUE(wrapper->associated_receiver().is_bound());
  EXPECT_TRUE(is_owner_alive_);
}

// Make HeapMojoAssociatedReceiver without context observer garbage collected
// and check that the connection is disconnected right after the marking phase.
TEST_F(HeapMojoAssociatedReceiverGCWithoutContextObserverTest, ResetsOnGC) {
  ClearOwner();
  EXPECT_FALSE(disconnected());
  PreciselyCollectGarbage();
  run_loop().Run();
  EXPECT_TRUE(disconnected());
}

// Destroy the context with context observer and check that the connection is
// disconnected.
TEST_F(HeapMojoAssociatedReceiverDestroyContextWithContextObserverTest,
       ResetsOnContextDestroyed) {
  EXPECT_TRUE(owner_->associated_receiver().is_bound());
  context_->NotifyContextDestroyed();
  EXPECT_FALSE(owner_->associated_receiver().is_bound());
}

// Destroy the context without context observer and check that the connection is
// still connected.
TEST_F(HeapMojoAssociatedReceiverDestroyContextWithoutContextObserverTest,
       ResetsOnContextDestroyed) {
  EXPECT_TRUE(owner_->associated_receiver().is_bound());
  context_->NotifyContextDestroyed();
  EXPECT_TRUE(owner_->associated_receiver().is_bound());
}

}  // namespace blink

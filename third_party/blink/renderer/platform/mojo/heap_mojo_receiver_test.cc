// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

#include "base/memory/raw_ptr.h"
#include "base/test/null_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/bindings/remote.h"
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
class HeapMojoReceiverGCBaseTest;

template <HeapMojoWrapperMode Mode>
class ReceiverOwner : public GarbageCollected<ReceiverOwner<Mode>>,
                      public sample::blink::Service {
  USING_PRE_FINALIZER(ReceiverOwner, Dispose);

 public:
  explicit ReceiverOwner(MockContextLifecycleNotifier* context,
                         HeapMojoReceiverGCBaseTest<Mode>* test = nullptr)
      : receiver_(this, context), test_(test) {
    if (test_)
      test_->set_is_owner_alive(true);
  }

  void Dispose() {
    if (test_)
      test_->set_is_owner_alive(false);
  }

  HeapMojoReceiver<sample::blink::Service, ReceiverOwner, Mode>& receiver() {
    return receiver_;
  }

  void Trace(Visitor* visitor) const { visitor->Trace(receiver_); }

 private:
  // sample::blink::Service implementation
  void Frobinate(sample::blink::FooPtr foo,
                 sample::blink::Service::BazOptions options,
                 mojo::PendingRemote<sample::blink::Port> port,
                 sample::blink::Service::FrobinateCallback callback) override {}
  void GetPort(mojo::PendingReceiver<sample::blink::Port> port) override {}

  HeapMojoReceiver<sample::blink::Service, ReceiverOwner, Mode> receiver_;
  raw_ptr<HeapMojoReceiverGCBaseTest<Mode>> test_;
};

template <HeapMojoWrapperMode Mode>
class HeapMojoReceiverGCBaseTest : public TestSupportingGC {
 public:
  base::RunLoop& run_loop() { return run_loop_; }
  bool& disconnected() { return disconnected_; }

  void set_is_owner_alive(bool alive) { is_owner_alive_ = alive; }
  void ClearOwner() { owner_ = nullptr; }

 protected:
  void SetUp() override {
    disconnected_ = false;
    context_ = MakeGarbageCollected<MockContextLifecycleNotifier>();
    owner_ = MakeGarbageCollected<ReceiverOwner<Mode>>(context_, this);
    scoped_refptr<base::NullTaskRunner> null_task_runner =
        base::MakeRefCounted<base::NullTaskRunner>();
    remote_ = mojo::Remote<sample::blink::Service>(
        owner_->receiver().BindNewPipeAndPassRemote(null_task_runner));
    remote_.set_disconnect_handler(WTF::BindOnce(
        [](HeapMojoReceiverGCBaseTest* receiver_test) {
          receiver_test->run_loop().Quit();
          receiver_test->disconnected() = true;
        },
        WTF::Unretained(this)));
  }
  void TearDown() override {
    owner_ = nullptr;
    PreciselyCollectGarbage();
  }

  Persistent<MockContextLifecycleNotifier> context_;
  Persistent<ReceiverOwner<Mode>> owner_;
  bool is_owner_alive_ = false;
  base::RunLoop run_loop_;
  mojo::Remote<sample::blink::Service> remote_;
  bool disconnected_ = false;
};

template <HeapMojoWrapperMode Mode>
class HeapMojoReceiverDisconnectWithReasonHandlerBaseTest
    : public HeapMojoReceiverGCBaseTest<Mode> {
 public:
  std::string& disconnected_reason() { return disconnected_reason_; }

 protected:
  void SetUp() override {
    CHECK(disconnected_reason_.empty());
    this->disconnected_ = false;
    this->context_ = MakeGarbageCollected<MockContextLifecycleNotifier>();
    this->owner_ =
        MakeGarbageCollected<ReceiverOwner<Mode>>(this->context_, this);
    scoped_refptr<base::NullTaskRunner> null_task_runner =
        base::MakeRefCounted<base::NullTaskRunner>();
    this->remote_ = mojo::Remote<sample::blink::Service>(
        this->owner_->receiver().BindNewPipeAndPassRemote(null_task_runner));
    this->remote_.set_disconnect_with_reason_handler(WTF::BindOnce(
        [](HeapMojoReceiverDisconnectWithReasonHandlerBaseTest* receiver_test,
           const uint32_t custom_reason, const std::string& description) {
          receiver_test->run_loop().Quit();
          receiver_test->disconnected_reason() = description;
        },
        WTF::Unretained(this)));
  }

  std::string disconnected_reason_;
};

template <HeapMojoWrapperMode Mode>
class HeapMojoReceiverDestroyContextBaseTest : public TestSupportingGC {
 protected:
  void SetUp() override {
    context_ = MakeGarbageCollected<MockContextLifecycleNotifier>();
    owner_ = MakeGarbageCollected<ReceiverOwner<Mode>>(context_);
    scoped_refptr<base::NullTaskRunner> null_task_runner =
        base::MakeRefCounted<base::NullTaskRunner>();
    remote_ = mojo::Remote<sample::blink::Service>(
        owner_->receiver().BindNewPipeAndPassRemote(null_task_runner));
  }

  Persistent<MockContextLifecycleNotifier> context_;
  Persistent<ReceiverOwner<Mode>> owner_;
  mojo::Remote<sample::blink::Service> remote_;
};

}  // namespace

class HeapMojoReceiverGCWithContextObserverTest
    : public HeapMojoReceiverGCBaseTest<
          HeapMojoWrapperMode::kWithContextObserver> {};
class HeapMojoReceiverGCWithoutContextObserverTest
    : public HeapMojoReceiverGCBaseTest<
          HeapMojoWrapperMode::kForceWithoutContextObserver> {};
class HeapMojoReceiverDestroyContextWithContextObserverTest
    : public HeapMojoReceiverDestroyContextBaseTest<
          HeapMojoWrapperMode::kWithContextObserver> {};
class HeapMojoReceiverDestroyContextForceWithoutContextObserverTest
    : public HeapMojoReceiverDestroyContextBaseTest<
          HeapMojoWrapperMode::kForceWithoutContextObserver> {};
class HeapMojoReceiverDisconnectWithReasonHandlerWithContextObserverTest
    : public HeapMojoReceiverDisconnectWithReasonHandlerBaseTest<
          HeapMojoWrapperMode::kWithContextObserver> {};
class HeapMojoReceiverDisconnectWithReasonHandlerWithoutContextObserverTest
    : public HeapMojoReceiverDisconnectWithReasonHandlerBaseTest<
          HeapMojoWrapperMode::kForceWithoutContextObserver> {};

// Make HeapMojoReceiver with context observer garbage collected and check that
// the connection is disconnected right after the marking phase.
TEST_F(HeapMojoReceiverGCWithContextObserverTest, ResetsOnGC) {
  ClearOwner();
  EXPECT_FALSE(disconnected());
  PreciselyCollectGarbage();
  run_loop().Run();
  EXPECT_TRUE(disconnected());
}

// Check that the owner
TEST_F(HeapMojoReceiverGCWithContextObserverTest, NoResetOnConservativeGC) {
  auto* wrapper = owner_->receiver().wrapper_.Get();
  EXPECT_TRUE(owner_->receiver().is_bound());
  ClearOwner();
  EXPECT_TRUE(is_owner_alive_);
  // The stack scanning should find |wrapper| and keep the Wrapper alive.
  ConservativelyCollectGarbage();
  EXPECT_TRUE(wrapper->receiver().is_bound());
  EXPECT_TRUE(is_owner_alive_);
}

// Make HeapMojoReceiver without context observer garbage collected and check
// that the connection is disconnected right after the marking phase.
TEST_F(HeapMojoReceiverGCWithoutContextObserverTest, ResetsOnGC) {
  ClearOwner();
  EXPECT_FALSE(disconnected());
  PreciselyCollectGarbage();
  run_loop().Run();
  EXPECT_TRUE(disconnected());
}

// Destroy the context with context observer and check that the connection is
// disconnected.
TEST_F(HeapMojoReceiverDestroyContextWithContextObserverTest,
       ResetsOnContextDestroyed) {
  EXPECT_TRUE(owner_->receiver().is_bound());
  context_->NotifyContextDestroyed();
  EXPECT_FALSE(owner_->receiver().is_bound());
}

// Destroy the context without context observer and check that the connection is
// still connected.
TEST_F(HeapMojoReceiverDestroyContextForceWithoutContextObserverTest,
       ResetsOnContextDestroyed) {
  EXPECT_TRUE(owner_->receiver().is_bound());
  context_->NotifyContextDestroyed();
  EXPECT_TRUE(owner_->receiver().is_bound());
}

// Reset the receiver with custom reason and check that the specified handler is
// fired.
TEST_F(HeapMojoReceiverDisconnectWithReasonHandlerWithContextObserverTest,
       ResetWithReason) {
  EXPECT_TRUE(disconnected_reason().empty());
  const std::string message = "test message";
  const uint32_t reason = 0;
  owner_->receiver().ResetWithReason(reason, message);
  run_loop().Run();
  EXPECT_EQ(disconnected_reason(), message);
}

// Reset the receiver with custom reason and check that the specified handler is
// fired.
TEST_F(HeapMojoReceiverDisconnectWithReasonHandlerWithoutContextObserverTest,
       ResetWithReason) {
  EXPECT_TRUE(disconnected_reason().empty());
  const std::string message = "test message";
  const uint32_t reason = 0;
  owner_->receiver().ResetWithReason(reason, message);
  run_loop().Run();
  EXPECT_EQ(disconnected_reason(), message);
}

}  // namespace blink

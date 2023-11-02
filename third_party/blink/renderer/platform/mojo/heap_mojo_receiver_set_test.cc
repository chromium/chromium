// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver_set.h"

#include <utility>

#include <string>
#include "base/test/null_task_runner.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/interfaces/bindings/tests/sample_service.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap_observer_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"

namespace blink {

namespace {

template <HeapMojoWrapperMode Mode, typename ContextType>
class HeapMojoReceiverSetGCBaseTest;

template <HeapMojoWrapperMode Mode, typename ContextType>
class GCOwner final : public GarbageCollected<GCOwner<Mode, ContextType>>,
                      public sample::blink::Service {
 public:
  explicit GCOwner(MockContextLifecycleNotifier* context,
                   HeapMojoReceiverSetGCBaseTest<Mode, ContextType>* test)
      : receiver_set_(this, context), test_(test) {
    test_->set_is_owner_alive(true);
  }
  void Dispose() { test_->set_is_owner_alive(false); }
  void Trace(Visitor* visitor) const { visitor->Trace(receiver_set_); }

  HeapMojoReceiverSet<sample::blink::Service, GCOwner, Mode, ContextType>&
  receiver_set() {
    return receiver_set_;
  }

  void Frobinate(sample::blink::FooPtr foo,
                 Service::BazOptions baz,
                 mojo::PendingRemote<sample::blink::Port> port,
                 FrobinateCallback callback) override {}
  void GetPort(mojo::PendingReceiver<sample::blink::Port> receiver) override {}

 private:
  HeapMojoReceiverSet<sample::blink::Service, GCOwner, Mode, ContextType>
      receiver_set_;
  HeapMojoReceiverSetGCBaseTest<Mode, ContextType>* test_;
};

template <HeapMojoWrapperMode Mode, typename ContextType>
class HeapMojoReceiverSetGCBaseTest : public TestSupportingGC {
 public:
  MockContextLifecycleNotifier* context() { return context_; }
  scoped_refptr<base::NullTaskRunner> task_runner() {
    return null_task_runner_;
  }
  GCOwner<Mode, ContextType>* owner() { return owner_; }
  void set_is_owner_alive(bool alive) { is_owner_alive_ = alive; }

  void ClearOwner() { owner_ = nullptr; }

 protected:
  void SetUp() override {
    context_ = MakeGarbageCollected<MockContextLifecycleNotifier>();
    owner_ = MakeGarbageCollected<GCOwner<Mode, ContextType>>(context(), this);
  }
  void TearDown() override {
    owner_ = nullptr;
    PreciselyCollectGarbage();
  }

  Persistent<MockContextLifecycleNotifier> context_;
  Persistent<GCOwner<Mode, ContextType>> owner_;
  bool is_owner_alive_ = false;
  scoped_refptr<base::NullTaskRunner> null_task_runner_ =
      base::MakeRefCounted<base::NullTaskRunner>();
};

}  // namespace

class HeapMojoReceiverSetGCWithContextObserverTest
    : public HeapMojoReceiverSetGCBaseTest<
          HeapMojoWrapperMode::kWithContextObserver,
          void> {};
class HeapMojoReceiverSetStringContextGCWithContextObserverTest
    : public HeapMojoReceiverSetGCBaseTest<
          HeapMojoWrapperMode::kWithContextObserver,
          std::string> {};
class HeapMojoReceiverSetGCWithoutContextObserverTest
    : public HeapMojoReceiverSetGCBaseTest<
          HeapMojoWrapperMode::kForceWithoutContextObserver,
          void> {};

// GC the HeapMojoReceiverSet with context observer and verify that the receiver
// is no longer part of the set, and that the service was deleted.
TEST_F(HeapMojoReceiverSetGCWithContextObserverTest, RemovesReceiver) {
  auto& receiver_set = owner()->receiver_set();
  auto receiver = mojo::PendingReceiver<sample::blink::Service>(
      mojo::MessagePipe().handle0);

  mojo::ReceiverId rid = receiver_set.Add(std::move(receiver), task_runner());
  EXPECT_TRUE(receiver_set.HasReceiver(rid));

  receiver_set.Remove(rid);

  EXPECT_FALSE(receiver_set.HasReceiver(rid));
}

// Check that the wrapper does not outlive the owner when ConservativeGC finds
// the wrapper.
TEST_F(HeapMojoReceiverSetGCWithContextObserverTest, NoClearOnConservativeGC) {
  auto* wrapper = owner_->receiver_set().wrapper_.Get();

  auto receiver = mojo::PendingReceiver<sample::blink::Service>(
      mojo::MessagePipe().handle0);

  mojo::ReceiverId rid =
      owner()->receiver_set().Add(std::move(receiver), task_runner());
  EXPECT_TRUE(wrapper->receiver_set().HasReceiver(rid));

  ClearOwner();
  EXPECT_TRUE(is_owner_alive_);

  ConservativelyCollectGarbage();

  EXPECT_TRUE(wrapper->receiver_set().HasReceiver(rid));
  EXPECT_TRUE(is_owner_alive_);
}

// GC the HeapMojoReceiverSet without context observer and verify that the
// receiver is no longer part of the set, and that the service was deleted.
TEST_F(HeapMojoReceiverSetGCWithoutContextObserverTest, RemovesReceiver) {
  auto& receiver_set = owner()->receiver_set();
  auto receiver = mojo::PendingReceiver<sample::blink::Service>(
      mojo::MessagePipe().handle0);

  mojo::ReceiverId rid = receiver_set.Add(std::move(receiver), task_runner());
  EXPECT_TRUE(receiver_set.HasReceiver(rid));

  receiver_set.Remove(rid);

  EXPECT_FALSE(receiver_set.HasReceiver(rid));
}

// GC the HeapMojoReceiverSet with context observer and verify that the receiver
// is no longer part of the set, and that the service was deleted.
TEST_F(HeapMojoReceiverSetGCWithContextObserverTest, ClearLeavesSetEmpty) {
  auto& receiver_set = owner()->receiver_set();
  auto receiver = mojo::PendingReceiver<sample::blink::Service>(
      mojo::MessagePipe().handle0);

  mojo::ReceiverId rid = receiver_set.Add(std::move(receiver), task_runner());
  EXPECT_TRUE(receiver_set.HasReceiver(rid));

  receiver_set.Clear();

  EXPECT_FALSE(receiver_set.HasReceiver(rid));
}

// GC the HeapMojoReceiverSet without context observer and verify that the
// receiver is no longer part of the set, and that the service was deleted.
TEST_F(HeapMojoReceiverSetGCWithoutContextObserverTest, ClearLeavesSetEmpty) {
  auto& receiver_set = owner()->receiver_set();
  auto receiver = mojo::PendingReceiver<sample::blink::Service>(
      mojo::MessagePipe().handle0);

  mojo::ReceiverId rid = receiver_set.Add(std::move(receiver), task_runner());
  EXPECT_TRUE(receiver_set.HasReceiver(rid));

  receiver_set.Clear();

  EXPECT_FALSE(receiver_set.HasReceiver(rid));
}

// Add several receiver and confirm that receiver_set holds properly.
TEST_F(HeapMojoReceiverSetGCWithContextObserverTest, AddSeveralReceiverSet) {
  auto& receiver_set = owner()->receiver_set();

  EXPECT_TRUE(receiver_set.empty());
  EXPECT_EQ(receiver_set.size(), 0u);

  auto receiver_1 = mojo::PendingReceiver<sample::blink::Service>(
      mojo::MessagePipe().handle0);
  mojo::ReceiverId rid_1 =
      receiver_set.Add(std::move(receiver_1), task_runner());
  EXPECT_TRUE(receiver_set.HasReceiver(rid_1));
  EXPECT_FALSE(receiver_set.empty());
  EXPECT_EQ(receiver_set.size(), 1u);

  auto receiver_2 = mojo::PendingReceiver<sample::blink::Service>(
      mojo::MessagePipe().handle0);
  mojo::ReceiverId rid_2 =
      receiver_set.Add(std::move(receiver_2), task_runner());
  EXPECT_TRUE(receiver_set.HasReceiver(rid_1));
  EXPECT_TRUE(receiver_set.HasReceiver(rid_2));
  EXPECT_FALSE(receiver_set.empty());
  EXPECT_EQ(receiver_set.size(), 2u);

  receiver_set.Clear();

  EXPECT_FALSE(receiver_set.HasReceiver(rid_1));
  EXPECT_FALSE(receiver_set.HasReceiver(rid_2));
  EXPECT_TRUE(receiver_set.empty());
  EXPECT_EQ(receiver_set.size(), 0u);
}

// Add several receiver with context and confirm that receiver_set holds
// properly.
TEST_F(HeapMojoReceiverSetStringContextGCWithContextObserverTest,
       AddSeveralReceiverSetWithContext) {
  auto& receiver_set = owner()->receiver_set();

  EXPECT_TRUE(receiver_set.empty());
  EXPECT_EQ(receiver_set.size(), 0u);

  auto receiver_1 = mojo::PendingReceiver<sample::blink::Service>(
      mojo::MessagePipe().handle0);
  mojo::ReceiverId rid_1 = receiver_set.Add(
      std::move(receiver_1), std::string("context1"), task_runner());
  EXPECT_TRUE(receiver_set.HasReceiver(rid_1));
  EXPECT_FALSE(receiver_set.empty());
  EXPECT_EQ(receiver_set.size(), 1u);

  auto receiver_2 = mojo::PendingReceiver<sample::blink::Service>(
      mojo::MessagePipe().handle0);
  mojo::ReceiverId rid_2 = receiver_set.Add(
      std::move(receiver_2), std::string("context2"), task_runner());
  EXPECT_TRUE(receiver_set.HasReceiver(rid_1));
  EXPECT_TRUE(receiver_set.HasReceiver(rid_2));
  EXPECT_FALSE(receiver_set.empty());
  EXPECT_EQ(receiver_set.size(), 2u);

  receiver_set.Clear();

  EXPECT_FALSE(receiver_set.HasReceiver(rid_1));
  EXPECT_FALSE(receiver_set.HasReceiver(rid_2));
  EXPECT_TRUE(receiver_set.empty());
  EXPECT_EQ(receiver_set.size(), 0u);
}

}  // namespace blink

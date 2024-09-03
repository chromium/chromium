// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver_set.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/null_task_runner.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/interfaces/bindings/tests/sample_service.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap_observer_list.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"

namespace blink {

namespace {

template <HeapMojoWrapperMode Mode>
class GCOwner;

template <HeapMojoWrapperMode Mode>
class HeapMojoAssociatedReceiverSetGCBaseTest : public TestSupportingGC {
 public:
  MockContextLifecycleNotifier* context() { return context_; }
  scoped_refptr<base::NullTaskRunner> task_runner() {
    return null_task_runner_;
  }
  GCOwner<Mode>* owner() { return owner_; }
  void set_is_owner_alive(bool alive) { is_owner_alive_ = alive; }

  void ClearOwner() { owner_ = nullptr; }

 protected:
  void SetUp() override {
    context_ = MakeGarbageCollected<MockContextLifecycleNotifier>();
    owner_ = MakeGarbageCollected<GCOwner<Mode>>(context(), this);
  }
  void TearDown() override {
    owner_ = nullptr;
    PreciselyCollectGarbage();
  }

  Persistent<MockContextLifecycleNotifier> context_;
  Persistent<GCOwner<Mode>> owner_;
  bool is_owner_alive_ = false;
  scoped_refptr<base::NullTaskRunner> null_task_runner_ =
      base::MakeRefCounted<base::NullTaskRunner>();
};

template <HeapMojoWrapperMode Mode>
class GCOwner : public GarbageCollected<GCOwner<Mode>>,
                public sample::blink::Service {
 public:
  explicit GCOwner(MockContextLifecycleNotifier* context,
                   HeapMojoAssociatedReceiverSetGCBaseTest<Mode>* test)
      : associated_receiver_set_(this, context), test_(test) {
    test_->set_is_owner_alive(true);
  }
  void Dispose() { test_->set_is_owner_alive(false); }
  void Trace(Visitor* visitor) const {
    visitor->Trace(associated_receiver_set_);
  }

  HeapMojoAssociatedReceiverSet<sample::blink::Service, GCOwner, Mode>&
  associated_receiver_set() {
    return associated_receiver_set_;
  }

  void Frobinate(sample::blink::FooPtr foo,
                 Service::BazOptions baz,
                 mojo::PendingRemote<sample::blink::Port> port,
                 FrobinateCallback callback) override {}
  void GetPort(mojo::PendingReceiver<sample::blink::Port> receiver) override {}

 private:
  HeapMojoAssociatedReceiverSet<sample::blink::Service, GCOwner, Mode>
      associated_receiver_set_;
  raw_ptr<HeapMojoAssociatedReceiverSetGCBaseTest<Mode>> test_;
};

}  // namespace

class HeapMojoAssociatedReceiverSetGCWithContextObserverTest
    : public HeapMojoAssociatedReceiverSetGCBaseTest<
          HeapMojoWrapperMode::kWithContextObserver> {};
class HeapMojoAssociatedReceiverSetGCWithoutContextObserverTest
    : public HeapMojoAssociatedReceiverSetGCBaseTest<
          HeapMojoWrapperMode::kForceWithoutContextObserver> {};

// Remove() a PendingAssociatedReceiver from HeapMojoAssociatedReceiverSet and
// verify that the receiver is no longer part of the set.
TEST_F(HeapMojoAssociatedReceiverSetGCWithContextObserverTest,
       RemovesReceiver) {
  auto& associated_receiver_set = owner()->associated_receiver_set();
  mojo::AssociatedRemote<sample::blink::Service> associated_remote;
  auto associated_receiver =
      associated_remote.BindNewEndpointAndPassDedicatedReceiver();

  mojo::ReceiverId rid = associated_receiver_set.Add(
      std::move(associated_receiver), task_runner());
  EXPECT_TRUE(associated_receiver_set.HasReceiver(rid));

  associated_receiver_set.Remove(rid);

  EXPECT_FALSE(associated_receiver_set.HasReceiver(rid));
}

// Same, without ContextObserver.
TEST_F(HeapMojoAssociatedReceiverSetGCWithoutContextObserverTest,
       RemovesReceiver) {
  auto& associated_receiver_set = owner()->associated_receiver_set();
  mojo::AssociatedRemote<sample::blink::Service> associated_remote;
  auto associated_receiver =
      associated_remote.BindNewEndpointAndPassDedicatedReceiver();

  mojo::ReceiverId rid = associated_receiver_set.Add(
      std::move(associated_receiver), task_runner());
  EXPECT_TRUE(associated_receiver_set.HasReceiver(rid));

  associated_receiver_set.Remove(rid);

  EXPECT_FALSE(associated_receiver_set.HasReceiver(rid));
}

// Check that the wrapper does not outlive the owner when ConservativeGC finds
// the wrapper.
TEST_F(HeapMojoAssociatedReceiverSetGCWithContextObserverTest,
       NoClearOnConservativeGC) {
  auto* wrapper = owner_->associated_receiver_set().wrapper_.Get();

  mojo::AssociatedRemote<sample::blink::Service> associated_remote;
  auto associated_receiver =
      associated_remote.BindNewEndpointAndPassDedicatedReceiver();

  mojo::ReceiverId rid = owner()->associated_receiver_set().Add(
      std::move(associated_receiver), task_runner());
  EXPECT_TRUE(wrapper->associated_receiver_set().HasReceiver(rid));

  ClearOwner();
  EXPECT_TRUE(is_owner_alive_);

  ConservativelyCollectGarbage();

  EXPECT_TRUE(wrapper->associated_receiver_set().HasReceiver(rid));
  EXPECT_TRUE(is_owner_alive_);
}

// Clear() a HeapMojoAssociatedReceiverSet and verify that it is empty.
TEST_F(HeapMojoAssociatedReceiverSetGCWithContextObserverTest,
       ClearLeavesSetEmpty) {
  auto& associated_receiver_set = owner()->associated_receiver_set();
  mojo::AssociatedRemote<sample::blink::Service> associated_remote;
  auto associated_receiver =
      associated_remote.BindNewEndpointAndPassDedicatedReceiver();

  mojo::ReceiverId rid = associated_receiver_set.Add(
      std::move(associated_receiver), task_runner());
  EXPECT_TRUE(associated_receiver_set.HasReceiver(rid));

  associated_receiver_set.Clear();

  EXPECT_FALSE(associated_receiver_set.HasReceiver(rid));
}

// Same, without ContextObserver.
TEST_F(HeapMojoAssociatedReceiverSetGCWithoutContextObserverTest,
       ClearLeavesSetEmpty) {
  auto& associated_receiver_set = owner()->associated_receiver_set();
  mojo::AssociatedRemote<sample::blink::Service> associated_remote;
  auto associated_receiver =
      associated_remote.BindNewEndpointAndPassDedicatedReceiver();

  mojo::ReceiverId rid = associated_receiver_set.Add(
      std::move(associated_receiver), task_runner());
  EXPECT_TRUE(associated_receiver_set.HasReceiver(rid));

  associated_receiver_set.Clear();

  EXPECT_FALSE(associated_receiver_set.HasReceiver(rid));
}

}  // namespace blink

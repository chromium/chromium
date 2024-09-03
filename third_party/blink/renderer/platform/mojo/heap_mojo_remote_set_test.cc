// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote_set.h"

#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/null_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
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
class HeapMojoRemoteSetGCBaseTest;

template <HeapMojoWrapperMode Mode>
class GCOwner final : public GarbageCollected<GCOwner<Mode>> {
 public:
  explicit GCOwner(MockContextLifecycleNotifier* context,
                   HeapMojoRemoteSetGCBaseTest<Mode>* test)
      : remote_set_(context), test_(test) {
    test_->set_is_owner_alive(true);
  }
  void Dispose() { test_->set_is_owner_alive(false); }
  void Trace(Visitor* visitor) const { visitor->Trace(remote_set_); }

  HeapMojoRemoteSet<sample::blink::Service, Mode>& remote_set() {
    return remote_set_;
  }

 private:
  HeapMojoRemoteSet<sample::blink::Service, Mode> remote_set_;
  raw_ptr<HeapMojoRemoteSetGCBaseTest<Mode>> test_;
};

template <HeapMojoWrapperMode Mode>
class HeapMojoRemoteSetGCBaseTest : public TestSupportingGC {
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

}  // namespace

class HeapMojoRemoteSetGCWithContextObserverTest
    : public HeapMojoRemoteSetGCBaseTest<
          HeapMojoWrapperMode::kWithContextObserver> {};
class HeapMojoRemoteSetGCWithoutContextObserverTest
    : public HeapMojoRemoteSetGCBaseTest<
          HeapMojoWrapperMode::kForceWithoutContextObserver> {};

// GC the HeapMojoRemoteSet with context observer and verify that the remote
// is no longer part of the set, and that the service was deleted.
TEST_F(HeapMojoRemoteSetGCWithContextObserverTest, RemovesRemote) {
  auto& remote_set = owner()->remote_set();
  auto remote = mojo::PendingRemote<sample::blink::Service>(
      mojo::MessagePipe().handle0, 0);

  mojo::RemoteSetElementId rid =
      remote_set.Add(std::move(remote), task_runner());
  EXPECT_TRUE(remote_set.Contains(rid));

  remote_set.Remove(rid);

  EXPECT_FALSE(remote_set.Contains(rid));
}

// Check that the wrapper does not outlive the owner when ConservativeGC finds
// the wrapper.
TEST_F(HeapMojoRemoteSetGCWithContextObserverTest, NoClearOnConservativeGC) {
  auto* wrapper = owner_->remote_set().wrapper_.Get();

  auto remote = mojo::PendingRemote<sample::blink::Service>(
      mojo::MessagePipe().handle0, 0);

  mojo::RemoteSetElementId rid =
      owner()->remote_set().Add(std::move(remote), task_runner());
  EXPECT_TRUE(wrapper->remote_set().Contains(rid));

  ClearOwner();
  EXPECT_TRUE(is_owner_alive_);

  ConservativelyCollectGarbage();

  EXPECT_TRUE(wrapper->remote_set().Contains(rid));
  EXPECT_TRUE(is_owner_alive_);
}

// GC the HeapMojoRemoteSet without context observer and verify that the
// remote is no longer part of the set, and that the service was deleted.
TEST_F(HeapMojoRemoteSetGCWithoutContextObserverTest, RemovesRemote) {
  auto& remote_set = owner()->remote_set();
  auto remote = mojo::PendingRemote<sample::blink::Service>(
      mojo::MessagePipe().handle0, 0);

  mojo::RemoteSetElementId rid =
      remote_set.Add(std::move(remote), task_runner());
  EXPECT_TRUE(remote_set.Contains(rid));

  remote_set.Remove(rid);

  EXPECT_FALSE(remote_set.Contains(rid));
}

// GC the HeapMojoRemoteSet with context observer and verify that the remote
// is no longer part of the set, and that the service was deleted.
TEST_F(HeapMojoRemoteSetGCWithContextObserverTest, ClearLeavesSetEmpty) {
  auto& remote_set = owner()->remote_set();
  auto remote = mojo::PendingRemote<sample::blink::Service>(
      mojo::MessagePipe().handle0, 0);

  mojo::RemoteSetElementId rid =
      remote_set.Add(std::move(remote), task_runner());
  EXPECT_TRUE(remote_set.Contains(rid));

  remote_set.Clear();

  EXPECT_FALSE(remote_set.Contains(rid));
}

// GC the HeapMojoRemoteSet without context observer and verify that the
// remote is no longer part of the set, and that the service was deleted.
TEST_F(HeapMojoRemoteSetGCWithoutContextObserverTest, ClearLeavesSetEmpty) {
  auto& remote_set = owner()->remote_set();
  auto remote = mojo::PendingRemote<sample::blink::Service>(
      mojo::MessagePipe().handle0, 0);

  mojo::RemoteSetElementId rid =
      remote_set.Add(std::move(remote), task_runner());
  EXPECT_TRUE(remote_set.Contains(rid));

  remote_set.Clear();

  EXPECT_FALSE(remote_set.Contains(rid));
}

// Add several remote and confirm that remote_set holds properly.
TEST_F(HeapMojoRemoteSetGCWithContextObserverTest, AddSeveralRemoteSet) {
  auto& remote_set = owner()->remote_set();

  EXPECT_TRUE(remote_set.empty());
  EXPECT_EQ(remote_set.size(), 0u);

  auto remote_1 = mojo::PendingRemote<sample::blink::Service>(
      mojo::MessagePipe().handle0, 0);
  mojo::RemoteSetElementId rid_1 =
      remote_set.Add(std::move(remote_1), task_runner());
  EXPECT_TRUE(remote_set.Contains(rid_1));
  EXPECT_FALSE(remote_set.empty());
  EXPECT_EQ(remote_set.size(), 1u);

  auto remote_2 = mojo::PendingRemote<sample::blink::Service>(
      mojo::MessagePipe().handle0, 0);
  mojo::RemoteSetElementId rid_2 =
      remote_set.Add(std::move(remote_2), task_runner());
  EXPECT_TRUE(remote_set.Contains(rid_1));
  EXPECT_TRUE(remote_set.Contains(rid_2));
  EXPECT_FALSE(remote_set.empty());
  EXPECT_EQ(remote_set.size(), 2u);

  remote_set.Clear();

  EXPECT_FALSE(remote_set.Contains(rid_1));
  EXPECT_FALSE(remote_set.Contains(rid_2));
  EXPECT_TRUE(remote_set.empty());
  EXPECT_EQ(remote_set.size(), 0u);
}

}  // namespace blink

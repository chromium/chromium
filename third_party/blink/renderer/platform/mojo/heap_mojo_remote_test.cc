// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

#include "base/memory/raw_ptr.h"
#include "base/test/null_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/interfaces/bindings/tests/sample_service.test-mojom-blink.h"
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

class ServiceImpl : public sample::blink::Service {
 public:
  explicit ServiceImpl() = default;

  mojo::Receiver<sample::blink::Service>& receiver() { return receiver_; }

 private:
  // sample::blink::Service implementation
  void Frobinate(sample::blink::FooPtr foo,
                 sample::blink::Service::BazOptions options,
                 mojo::PendingRemote<sample::blink::Port> port,
                 sample::blink::Service::FrobinateCallback callback) override {}
  void GetPort(mojo::PendingReceiver<sample::blink::Port> port) override {}

  mojo::Receiver<sample::blink::Service> receiver_{this};
};

template <HeapMojoWrapperMode Mode>
class HeapMojoRemoteGCBaseTest;

template <HeapMojoWrapperMode Mode>
class RemoteOwner : public GarbageCollected<RemoteOwner<Mode>> {
  USING_PRE_FINALIZER(RemoteOwner, Dispose);

 public:
  explicit RemoteOwner(MockContextLifecycleNotifier* context,
                       HeapMojoRemoteGCBaseTest<Mode>* test = nullptr)
      : remote_(context), test_(test) {
    if (test_) {
      test_->set_is_owner_alive(true);
    }
  }
  explicit RemoteOwner(HeapMojoRemote<sample::blink::Service, Mode> remote)
      : remote_(std::move(remote)) {}

  void Dispose() {
    if (test_) {
      test_->set_is_owner_alive(false);
    }
  }

  HeapMojoRemote<sample::blink::Service, Mode>& remote() { return remote_; }

  void Trace(Visitor* visitor) const { visitor->Trace(remote_); }

  HeapMojoRemote<sample::blink::Service, Mode> remote_;
  raw_ptr<HeapMojoRemoteGCBaseTest<Mode>> test_;
};

template <HeapMojoWrapperMode Mode>
class HeapMojoRemoteDestroyContextBaseTest : public TestSupportingGC {
 protected:
  void SetUp() override {
    owner_ = MakeGarbageCollected<RemoteOwner<Mode>>(context_.Notifier());
    scoped_refptr<base::NullTaskRunner> null_task_runner =
        base::MakeRefCounted<base::NullTaskRunner>();
    impl_.receiver().Bind(
        owner_->remote().BindNewPipeAndPassReceiver(null_task_runner));
  }

  ServiceImpl impl_;
  ScopedMockContextLifecycleNotifier context_;
  Persistent<RemoteOwner<Mode>> owner_;
};

template <HeapMojoWrapperMode Mode>
class HeapMojoRemoteDisconnectWithReasonHandlerBaseTest
    : public TestSupportingGC {
 public:
  base::RunLoop& run_loop() { return run_loop_; }
  bool& disconnected_with_reason() { return disconnected_with_reason_; }

 protected:
  void SetUp() override {
    CHECK(!disconnected_with_reason_);
    owner_ = MakeGarbageCollected<RemoteOwner<Mode>>(context_.Notifier());
    scoped_refptr<base::NullTaskRunner> null_task_runner =
        base::MakeRefCounted<base::NullTaskRunner>();
    impl_.receiver().Bind(
        owner_->remote().BindNewPipeAndPassReceiver(null_task_runner));
    impl_.receiver().set_disconnect_with_reason_handler(BindOnce(
        [](HeapMojoRemoteDisconnectWithReasonHandlerBaseTest* remote_test,
           const uint32_t custom_reason, const std::string& description) {
          remote_test->run_loop().Quit();
          remote_test->disconnected_with_reason() = true;
        },
        Unretained(this)));
  }

  ServiceImpl impl_;
  ScopedMockContextLifecycleNotifier context_;
  Persistent<RemoteOwner<Mode>> owner_;
  base::RunLoop run_loop_;
  bool disconnected_with_reason_ = false;
};

template <HeapMojoWrapperMode Mode>
class HeapMojoRemoteMoveBaseTest : public TestSupportingGC {
 protected:
  void SetUp() override {
    HeapMojoRemote<sample::blink::Service, Mode> remote(context_.Notifier());
    owner_ = MakeGarbageCollected<RemoteOwner<Mode>>(std::move(remote));
    scoped_refptr<base::NullTaskRunner> null_task_runner =
        base::MakeRefCounted<base::NullTaskRunner>();
    impl_.receiver().Bind(
        owner_->remote().BindNewPipeAndPassReceiver(null_task_runner));
  }

  ServiceImpl impl_;
  ScopedMockContextLifecycleNotifier context_;
  Persistent<RemoteOwner<Mode>> owner_;
};

template <HeapMojoWrapperMode Mode>
class HeapMojoRemoteGCBaseTest : public TestSupportingGC {
 public:
  base::RunLoop& run_loop() { return run_loop_; }
  bool& disconnected() { return disconnected_; }

  void set_is_owner_alive(bool alive) { is_owner_alive_ = alive; }
  void ClearOwner() { owner_ = nullptr; }

 protected:
  void SetUp() override {
    disconnected_ = false;
    owner_ = MakeGarbageCollected<RemoteOwner<Mode>>(context_.Notifier(), this);
    scoped_refptr<base::NullTaskRunner> null_task_runner =
        base::MakeRefCounted<base::NullTaskRunner>();
    impl_.receiver().Bind(
        owner_->remote().BindNewPipeAndPassReceiver(null_task_runner));
    impl_.receiver().set_disconnect_handler(BindOnce(
        [](HeapMojoRemoteGCBaseTest* remote_test) {
          remote_test->run_loop().Quit();
          remote_test->disconnected() = true;
        },
        Unretained(this)));
  }
  void TearDown() override {
    owner_ = nullptr;
    PreciselyCollectGarbage();
  }

  ServiceImpl impl_;
  ScopedMockContextLifecycleNotifier context_;
  Persistent<RemoteOwner<Mode>> owner_;
  bool is_owner_alive_ = false;
  base::RunLoop run_loop_;
  bool disconnected_ = false;
};

}  // namespace

class HeapMojoRemoteDestroyContextWithContextObserverTest
    : public HeapMojoRemoteDestroyContextBaseTest<
          HeapMojoWrapperMode::kWithContextObserver> {};
class HeapMojoRemoteDestroyContextForceWithoutContextObserverTest
    : public HeapMojoRemoteDestroyContextBaseTest<
          HeapMojoWrapperMode::kForceWithoutContextObserver> {};
class HeapMojoRemoteDisconnectWithReasonHandlerWithContextObserverTest
    : public HeapMojoRemoteDisconnectWithReasonHandlerBaseTest<
          HeapMojoWrapperMode::kWithContextObserver> {};
class HeapMojoRemoteDisconnectWithReasonHandlerWithoutContextObserverTest
    : public HeapMojoRemoteDisconnectWithReasonHandlerBaseTest<
          HeapMojoWrapperMode::kForceWithoutContextObserver> {};
class HeapMojoRemoteMoveWithContextObserverTest
    : public HeapMojoRemoteMoveBaseTest<
          HeapMojoWrapperMode::kWithContextObserver> {};
class HeapMojoRemoteMoveWithoutContextObserverTest
    : public HeapMojoRemoteMoveBaseTest<
          HeapMojoWrapperMode::kForceWithoutContextObserver> {};

// Destroy the context with context observer and check that the connection is
// disconnected.
TEST_F(HeapMojoRemoteDestroyContextWithContextObserverTest,
       ResetsOnContextDestroyed) {
  EXPECT_TRUE(owner_->remote().is_bound());
  context_->NotifyContextDestroyed();
  EXPECT_FALSE(owner_->remote().is_bound());
}

// Destroy the context without context observer and check that the connection is
// still connected.
TEST_F(HeapMojoRemoteDestroyContextForceWithoutContextObserverTest,
       ResetsOnContextDestroyed) {
  EXPECT_TRUE(owner_->remote().is_bound());
  context_->NotifyContextDestroyed();
  EXPECT_TRUE(owner_->remote().is_bound());
}

// Reset the remote with custom reason and check that the specified handler is
// fired.
TEST_F(HeapMojoRemoteDisconnectWithReasonHandlerWithContextObserverTest,
       ResetWithReason) {
  EXPECT_FALSE(disconnected_with_reason());
  const std::string message = "test message";
  const uint32_t reason = 0;
  owner_->remote().ResetWithReason(reason, message);
  run_loop().Run();
  EXPECT_TRUE(disconnected_with_reason());
}

// Reset the remote with custom reason and check that the specified handler is
// fired.
TEST_F(HeapMojoRemoteDisconnectWithReasonHandlerWithoutContextObserverTest,
       ResetWithReason) {
  EXPECT_FALSE(disconnected_with_reason());
  const std::string message = "test message";
  const uint32_t reason = 0;
  owner_->remote().ResetWithReason(reason, message);
  run_loop().Run();
  EXPECT_TRUE(disconnected_with_reason());
}

// Move the remote from the outside of Owner class.
TEST_F(HeapMojoRemoteMoveWithContextObserverTest, MoveSemantics) {
  EXPECT_TRUE(owner_->remote().is_bound());
  context_->NotifyContextDestroyed();
  EXPECT_FALSE(owner_->remote().is_bound());
}

// Move the remote from the outside of Owner class.
TEST_F(HeapMojoRemoteMoveWithoutContextObserverTest, MoveSemantics) {
  EXPECT_TRUE(owner_->remote().is_bound());
  context_->NotifyContextDestroyed();
  EXPECT_TRUE(owner_->remote().is_bound());
}

class HeapMojoRemoteGCWithContextObserverTest
    : public HeapMojoRemoteGCBaseTest<
          HeapMojoWrapperMode::kWithContextObserver> {};
class HeapMojoRemoteGCWithoutContextObserverTest
    : public HeapMojoRemoteGCBaseTest<
          HeapMojoWrapperMode::kForceWithoutContextObserver> {};

// Make HeapMojoRemote with context observer garbage collected and check that
// the connection is disconnected right after the marking phase.
TEST_F(HeapMojoRemoteGCWithContextObserverTest, ResetsOnGC) {
  ClearOwner();
  EXPECT_FALSE(disconnected());
  PreciselyCollectGarbage();
  run_loop().Run();
  EXPECT_TRUE(disconnected());
}

// Make HeapMojoRemote without context observer garbage collected and check that
// the connection is disconnected right after the marking phase.
TEST_F(HeapMojoRemoteGCWithoutContextObserverTest, ResetsOnGC) {
  ClearOwner();
  EXPECT_FALSE(disconnected());
  PreciselyCollectGarbage();
  run_loop().Run();
  EXPECT_TRUE(disconnected());
}

}  // namespace blink

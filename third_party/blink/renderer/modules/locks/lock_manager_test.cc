// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/locks/lock_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/feature_observer/feature_observer.mojom-blink.h"
#include "third_party/blink/public/mojom/locks/lock_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_lock_granted_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_lock_manager_snapshot.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_lock_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

class FakeLockManagerService : public mojom::blink::LockManager {
 public:
  mojo::PendingRemote<mojom::blink::LockManager> BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void RequestLock(const String& name,
                   mojom::blink::LockMode mode,
                   WaitMode wait,
                   mojo::PendingAssociatedRemote<mojom::blink::LockRequest>
                       request) override {
    pending_request_.Bind(std::move(request));
    request_loop_.Quit();
  }

  void QueryState(QueryStateCallback callback) override {}

  void WaitForRequest() { request_loop_.Run(); }

  void GrantPendingRequest() {
    ASSERT_TRUE(pending_request_.is_bound());
    mojo::PendingAssociatedRemote<mojom::blink::LockHandle> handle_remote;
    held_lock_receiver_ = handle_remote.InitWithNewEndpointAndPassReceiver();
    pending_request_->Granted(std::move(handle_remote));
  }

 private:
  mojo::Receiver<mojom::blink::LockManager> receiver_{this};
  mojo::AssociatedRemote<mojom::blink::LockRequest> pending_request_;
  mojo::PendingAssociatedReceiver<mojom::blink::LockHandle> held_lock_receiver_;
  base::RunLoop request_loop_;
};

V8LockGrantedCallback* CreateLockGrantedCallback(V8TestingScope& scope) {
  v8::Local<v8::Function> fn =
      v8::Function::New(scope.GetContext(),
                        [](const v8::FunctionCallbackInfo<v8::Value>& info) {
                          info.GetReturnValue().Set(42);
                        })
          .ToLocalChecked();
  return V8LockGrantedCallback::Create(fn);
}

}  // namespace

// Tests various scenarios where the remote is unbound and lock methods are
// called. See http://b/496259836.
class LockManagerUnboundTest : public testing::Test {
 protected:
  LockManager* UnboundRemote(V8TestingScope& scope,
                             std::string interface_name) {
    auto* manager =
        MakeGarbageCollected<LockManager>(*scope.GetWindow().navigator());
    // Intercept the interface request and reset the remote to
    // simulate a failed bind that results in an unbound remote.
    scope.GetExecutionContext()
        ->GetBrowserInterfaceBroker()
        .SetBinderForTesting(
            interface_name,
            blink::BindRepeating(
                [](base::RepeatingCallback<void(LockManager*)> reset_cb,
                   LockManager* manager, mojo::ScopedMessagePipeHandle handle) {
                  reset_cb.Run(manager);
                },
                GetResetCallback(interface_name), WrapPersistent(manager)));
    return manager;
  }

  base::RepeatingCallback<void(LockManager*)> GetResetCallback(
      std::string interface_name) {
    if (interface_name == mojom::blink::LockManager::Name_) {
      return base::BindRepeating(&ResetService);
    } else if (interface_name == mojom::blink::FeatureObserver::Name_) {
      return base::BindRepeating(&ResetObserver);
    }
    NOTREACHED();
  }
  static void ResetService(LockManager* manager) { manager->service_.reset(); }
  static void ResetObserver(LockManager* manager) {
    manager->observer_.reset();
  }

  // These access private methods, so are on the test class which is a friend.
  void RequestImpl(LockManager* manager,
                   const LockOptions* options,
                   const String& name,
                   V8LockGrantedCallback* callback,
                   mojom::blink::LockMode mode,
                   ScriptPromiseResolver<IDLAny>* resolver) {
    manager->RequestImpl(options, name, callback, mode, resolver);
  }

  void QueryImpl(LockManager* manager,
                 ScriptPromiseResolver<LockManagerSnapshot>* resolver) {
    manager->QueryImpl(resolver);
  }

  // Helper to clear binders on a given scope.
  void ClearBinder(V8TestingScope& scope, const char* name) {
    auto& broker = scope.GetExecutionContext()->GetBrowserInterfaceBroker();
    broker.SetBinderForTesting(name, base::NullCallback());
  }

  test::TaskEnvironment task_environment_;
};

TEST_F(LockManagerUnboundTest, RequestImplUnboundService) {
  V8TestingScope scope;
  const char* name = mojom::blink::LockManager::Name_;
  auto* manager = UnboundRemote(scope, name);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      scope.GetScriptState());
  auto promise = resolver->Promise();

  // This should not crash even if the service fails to bind and remains
  // unbound.
  RequestImpl(manager, LockOptions::Create(), "test", nullptr,
              mojom::blink::LockMode::EXCLUSIVE, resolver);

  EXPECT_EQ(v8::Promise::kRejected, promise.V8Promise()->State());

  ClearBinder(scope, name);
}

TEST_F(LockManagerUnboundTest, RequestImplUnboundObserver) {
  V8TestingScope scope;
  const char* name = mojom::blink::FeatureObserver::Name_;
  auto* manager = UnboundRemote(scope, name);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      scope.GetScriptState());
  auto promise = resolver->Promise();

  // This should not crash even if the observer fails to bind and remains
  // unbound.
  RequestImpl(manager, LockOptions::Create(), "test", nullptr,
              mojom::blink::LockMode::EXCLUSIVE, resolver);

  EXPECT_EQ(v8::Promise::kRejected, promise.V8Promise()->State());

  ClearBinder(scope, name);
}

TEST_F(LockManagerUnboundTest, QueryImplUnboundService) {
  V8TestingScope scope;
  const char* name = mojom::blink::LockManager::Name_;
  auto* manager = UnboundRemote(scope, name);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<LockManagerSnapshot>>(
          scope.GetScriptState());
  auto promise = resolver->Promise();

  // This should not crash even if the service fails to bind and remains
  // unbound.
  QueryImpl(manager, resolver);

  EXPECT_EQ(v8::Promise::kRejected, promise.V8Promise()->State());

  ClearBinder(scope, name);
}

TEST_F(LockManagerUnboundTest, GrantDeferredWhilePageFrozen) {
  V8TestingScope scope(KURL("https://example.com"));
  FakeLockManagerService fake_service;
  auto* manager = LockManager::locks(*scope.GetWindow().navigator());
  manager->SetManager(fake_service.BindNewPipeAndPassRemote(),
                      scope.GetExecutionContext());

  auto promise = manager->request(scope.GetScriptState(), "test",
                                  CreateLockGrantedCallback(scope),
                                  scope.GetExceptionState());
  fake_service.WaitForRequest();

  // Grant the lock, queuing the pipe read on `kMiscPlatformAPI`, followed by a
  // task on the same queue that freezes the page after `MultiplexRouter` has
  // posted the `Granted()` dispatch task to `kWebLocks`.
  fake_service.GrantPendingRequest();
  base::RunLoop freeze_loop;
  scope.GetExecutionContext()
      ->GetTaskRunner(TaskType::kMiscPlatformAPI)
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     [](Page* page, ExecutionContext* context,
                        base::OnceClosure quit) {
                       page->GetPageScheduler()->SetPageVisible(false);
                       page->GetPageScheduler()->SetPageFrozen(true);
                       context->GetTaskRunner(TaskType::kInternalTest)
                           ->PostTask(FROM_HERE, std::move(quit));
                     },
                     WrapPersistent(&scope.GetPage()),
                     WrapPersistent(scope.GetExecutionContext()),
                     freeze_loop.QuitClosure()));
  freeze_loop.Run();
  scope.PerformMicrotaskCheckpoint();
  EXPECT_EQ(v8::Promise::kPending, promise.V8Promise()->State());

  base::RunLoop unfreeze_loop;
  scope.GetPage().GetPageScheduler()->SetPageFrozen(false);
  scope.GetExecutionContext()
      ->GetTaskRunner(TaskType::kWebLocks)
      ->PostTask(FROM_HERE, unfreeze_loop.QuitClosure());
  unfreeze_loop.Run();
  scope.PerformMicrotaskCheckpoint();
  ASSERT_EQ(v8::Promise::kFulfilled, promise.V8Promise()->State());
  EXPECT_EQ(
      42,
      promise.V8Promise()->Result()->Int32Value(scope.GetContext()).FromJust());
}

TEST_F(LockManagerUnboundTest, GrantDeferredWhilePagePaused) {
  V8TestingScope scope(KURL("https://example.com"));
  FakeLockManagerService fake_service;
  auto* manager = LockManager::locks(*scope.GetWindow().navigator());
  manager->SetManager(fake_service.BindNewPipeAndPassRemote(),
                      scope.GetExecutionContext());

  auto promise = manager->request(scope.GetScriptState(), "test",
                                  CreateLockGrantedCallback(scope),
                                  scope.GetExceptionState());
  fake_service.WaitForRequest();

  // Grant the lock, queuing the pipe read on `kMiscPlatformAPI`, followed by a
  // task on the same queue that pauses the page after `MultiplexRouter` has
  // posted the `Granted()` dispatch task to `kWebLocks`.
  fake_service.GrantPendingRequest();
  base::RunLoop pause_loop;
  scope.GetExecutionContext()
      ->GetTaskRunner(TaskType::kMiscPlatformAPI)
      ->PostTask(FROM_HERE, base::BindOnce(
                                [](Page* page, ExecutionContext* context,
                                   base::OnceClosure quit) {
                                  page->SetPaused(true);
                                  context
                                      ->GetTaskRunner(TaskType::kInternalTest)
                                      ->PostTask(FROM_HERE, std::move(quit));
                                },
                                WrapPersistent(&scope.GetPage()),
                                WrapPersistent(scope.GetExecutionContext()),
                                pause_loop.QuitClosure()));
  pause_loop.Run();
  scope.PerformMicrotaskCheckpoint();
  EXPECT_EQ(v8::Promise::kPending, promise.V8Promise()->State());

  base::RunLoop unpause_loop;
  scope.GetPage().SetPaused(false);
  scope.GetExecutionContext()
      ->GetTaskRunner(TaskType::kWebLocks)
      ->PostTask(FROM_HERE, unpause_loop.QuitClosure());
  unpause_loop.Run();
  scope.PerformMicrotaskCheckpoint();
  ASSERT_EQ(v8::Promise::kFulfilled, promise.V8Promise()->State());
  EXPECT_EQ(
      42,
      promise.V8Promise()->Result()->Int32Value(scope.GetContext()).FromJust());
}

}  // namespace blink

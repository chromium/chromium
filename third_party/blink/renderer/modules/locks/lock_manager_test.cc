// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/locks/lock_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/feature_observer/feature_observer.mojom-blink.h"
#include "third_party/blink/public/mojom/locks/lock_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_lock_manager_snapshot.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_lock_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

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

}  // namespace blink

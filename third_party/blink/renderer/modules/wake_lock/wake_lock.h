// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_H_

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_global_scope.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_type.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"

namespace WTF {

class String;

}  // namespace WTF

namespace blink {

class ExceptionState;
class LocalDOMWindow;
class ScriptState;
class WakeLockManager;

class MODULES_EXPORT WakeLock final : public ScriptWrappable,
                                      public ExecutionContextLifecycleObserver,
                                      public PageVisibilityObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit WakeLock(LocalDOMWindow&);
  explicit WakeLock(DedicatedWorkerGlobalScope&);

  ScriptPromise request(ScriptState*,
                        const WTF::String& type,
                        ExceptionState& exception_state);

  void Trace(Visitor*) const override;

 private:
  // While this could be part of request() itself, having it as a separate
  // function makes testing (which uses a custom ScriptPromiseResolver) a lot
  // easier.
  void DoRequest(WakeLockType, ScriptPromiseResolver*);

  void DidReceivePermissionResponse(WakeLockType,
                                    ScriptPromiseResolver*,
                                    mojom::blink::PermissionStatus);

  // ExecutionContextLifecycleObserver implementation
  void ContextDestroyed() override;

  // PageVisibilityObserver implementation
  void PageVisibilityChanged() override;

  // Permission handling
  void ObtainPermission(
      WakeLockType,
      base::OnceCallback<void(mojom::blink::PermissionStatus)> callback);
  mojom::blink::PermissionService* GetPermissionService();

  HeapMojoRemote<mojom::blink::PermissionService,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      permission_service_;

  // https://w3c.github.io/screen-wake-lock/#concepts-and-state-record
  // Each platform wake lock (one per wake lock type) has an associated state
  // record per responsible document [...] internal slots.
  Member<WakeLockManager> managers_[kWakeLockTypeCount];

  FRIEND_TEST_ALL_PREFIXES(WakeLockSentinelTest, ContextDestruction);
  FRIEND_TEST_ALL_PREFIXES(WakeLockTest, RequestWakeLockGranted);
  FRIEND_TEST_ALL_PREFIXES(WakeLockTest, RequestWakeLockDenied);
  FRIEND_TEST_ALL_PREFIXES(WakeLockTest, LossOfDocumentActivity);
  FRIEND_TEST_ALL_PREFIXES(WakeLockTest, PageVisibilityHidden);
  FRIEND_TEST_ALL_PREFIXES(WakeLockTest,
                           PageVisibilityHiddenBeforeLockAcquisition);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_H_

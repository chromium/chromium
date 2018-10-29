// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_H_

#include "services/device/public/mojom/wake_lock.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"
#include "third_party/blink/renderer/core/dom/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class WakeLockRequest;

class WakeLock final : public EventTargetWithInlineData,
                       public ActiveScriptWrappable<WakeLock>,
                       public ContextLifecycleObserver,
                       public PageVisibilityObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(WakeLock);

 public:
  ~WakeLock() override;

  // wake_lock.idl implementation
  AtomicString type() const;
  bool active() const;
  DEFINE_ATTRIBUTE_EVENT_LISTENER(activechange);
  WakeLockRequest* createRequest();

  // Called by NavigatorWakeLock to create Screen Wake Lock
  static WakeLock* CreateScreenWakeLock(ScriptState*);
  // Called by NavigatorWakeLock to create System Wake Lock
  static WakeLock* CreateSystemWakeLock(ScriptState*);

  // Resolves and returns same promise of that particular WakeLockType each time
  ScriptPromise GetPromise(ScriptState*);

  // EventTarget overrides.
  const WTF::AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ActiveScriptWrappable overrides.
  bool HasPendingActivity() const final;

  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;

  // PageVisibilityObserver overrides.
  void PageVisibilityChanged() override;

  void Trace(blink::Visitor*) override;

  // Called by WakeLockRequest to decrease the request counter by one
  void CancelRequest();

 private:
  enum class LockType { kSystem, kScreen };

  WakeLock(ScriptState*, LockType);

  // Error handler in case of failure to connect to Wake Lock mojo service
  void OnConnectionError();

  // Depending on active_ status change, requests or cancels Wake Lock
  void ChangeActiveStatus(bool);

  // Binds to the Wake Lock mojo service
  void BindToServiceIfNeeded();

  device::mojom::blink::WakeLockPtr wake_lock_service_;

  int request_counter_ = 0;
  LockType type_;
  bool active_ = false;

  // We use ScriptPromiseProperty instead of ScriptPromiseResolver or other
  // mechanism because we need to return same promise of that WakeLockType for
  // any subsequent calls to navigator.getWakeLock(WakeLockType).
  using WakeLockProperty = ScriptPromiseProperty<Member<WakeLock>,
                                                 Member<WakeLock>,
                                                 Member<DOMException>>;
  Member<WakeLockProperty> wake_lock_property_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_H_

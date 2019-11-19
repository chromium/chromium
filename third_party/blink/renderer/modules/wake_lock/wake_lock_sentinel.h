// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_SENTINEL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_SENTINEL_H_

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_type.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;
class ScriptState;
class WakeLockManager;

class MODULES_EXPORT WakeLockSentinel final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<WakeLockSentinel>,
      public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(WakeLockSentinel);

 public:
  WakeLockSentinel(ScriptState* script_state,
                   WakeLockType type,
                   WakeLockManager* manager);
  ~WakeLockSentinel() override;

  // Web-exposed interfaces
  DEFINE_ATTRIBUTE_EVENT_LISTENER(release, kRelease)
  ScriptPromise release(ScriptState*);
  String type() const;

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;
  void Trace(blink::Visitor*) override;

  // ActiveScriptWrappable overrides.
  bool HasPendingActivity() const override;

  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;

 private:
  friend class WakeLockManager;

  // This function, which only has any effect once, detaches this sentinel from
  // its |manager_|, and fires a "release" event.
  // It is implemented separately from release() itself so that |manager_| can
  // call it without triggering the creation of a new ScriptPromise, as it is
  // not relevant to |manager_| and this function may be called from a context
  // where |script_state_|'s context is no longer valid.
  void DoRelease();

  Member<WakeLockManager> manager_;
  const WakeLockType type_;

  FRIEND_TEST_ALL_PREFIXES(WakeLockSentinelTest, MultipleReleaseCalls);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_SENTINEL_H_

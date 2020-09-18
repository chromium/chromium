// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_IDLE_IDLE_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_IDLE_IDLE_DETECTOR_H_

#include "base/macros.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_idle_options.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class AbortSignal;
class ExceptionState;

class IdleDetector final : public EventTargetWithInlineData,
                           public ActiveScriptWrappable<IdleDetector>,
                           public ExecutionContextClient,
                           public mojom::blink::IdleMonitor {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static IdleDetector* Create(ScriptState*);

  explicit IdleDetector(ExecutionContext*);
  ~IdleDetector() override;

  IdleDetector(const IdleDetector&) = delete;
  IdleDetector& operator=(const IdleDetector&) = delete;

  // EventTarget implementation.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ActiveScriptWrappable implementation.
  bool HasPendingActivity() const final;

  // IdleDetector IDL interface.
  String userState() const;
  String screenState() const;
  ScriptPromise start(ScriptState*, const IdleOptions*, ExceptionState&);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(change, kChange)

  void Trace(Visitor*) const override;

 private:
  // mojom::blink::IdleMonitor implementation. Invoked on a state change, and
  // causes an event to be dispatched.
  void Update(mojom::blink::IdleStatePtr state) override;

  void Abort(AbortSignal*);
  void OnServiceDisconnected();
  void OnAddMonitor(ScriptPromiseResolver*,
                    mojom::blink::IdleManagerError,
                    mojom::blink::IdleStatePtr);

  mojom::blink::IdleStatePtr state_;

  base::TimeDelta threshold_ = base::TimeDelta::FromSeconds(60);
  Member<AbortSignal> signal_;
  Member<ScriptPromiseResolver> resolver_;

  // Holds a pipe which the service uses to notify this object
  // when the idle state has changed.
  HeapMojoReceiver<mojom::blink::IdleMonitor,
                   IdleDetector,
                   HeapMojoWrapperMode::kWithoutContextObserver>
      receiver_;
  HeapMojoRemote<mojom::blink::IdleManager,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      idle_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_IDLE_IDLE_DETECTOR_H_

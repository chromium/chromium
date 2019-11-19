// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_IDLE_IDLE_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_IDLE_IDLE_DETECTOR_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/idle/idle_options.h"
#include "third_party/blink/renderer/modules/idle/idle_state.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class IdleDetector final : public EventTargetWithInlineData,
                           public ActiveScriptWrappable<IdleDetector>,
                           public ContextClient,
                           public mojom::blink::IdleMonitor {
  USING_GARBAGE_COLLECTED_MIXIN(IdleDetector);
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(IdleDetector, Dispose);

 public:
  static IdleDetector* Create(ScriptState*,
                              const IdleOptions*,
                              ExceptionState&);
  static IdleDetector* Create(ScriptState*, ExceptionState&);

  IdleDetector(ExecutionContext*, base::TimeDelta threshold);

  ~IdleDetector() override;

  void Dispose();

  // EventTarget implementation.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ActiveScriptWrappable implementation.
  bool HasPendingActivity() const final;

  // IdleDetector IDL interface.
  ScriptPromise start(ScriptState*);
  void stop();
  blink::IdleState* state() const;
  DEFINE_ATTRIBUTE_EVENT_LISTENER(change, kChange)

  void OnAddMonitor(mojom::blink::IdleStatePtr);

  // mojom::blink::IdleMonitor implementation. Invoked on a state change, and
  // causes an event to be dispatched.
  void Update(mojom::blink::IdleStatePtr state) override;

  void Trace(blink::Visitor*) override;

 private:
  Member<blink::IdleState> state_;

  const base::TimeDelta threshold_;

  // Holds a pipe which the service uses to notify this object
  // when the idle state has changed.
  mojo::Receiver<mojom::blink::IdleMonitor> receiver_;

  void StartMonitoring();
  void StopMonitoring();

  mojo::Remote<mojom::blink::IdleManager> service_;

  DISALLOW_COPY_AND_ASSIGN(IdleDetector);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_IDLE_IDLE_DETECTOR_H_

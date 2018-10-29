// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ExecutionContext;
class ScriptState;
class SerialPortRequestOptions;

class Serial final : public EventTargetWithInlineData,
                     public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Serial);

 public:
  static Serial* Create(ExecutionContext& executionContext);

  // EventTarget
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(connect);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(disconnect);
  ScriptPromise getPorts(ScriptState*);
  ScriptPromise requestPort(ScriptState*, const SerialPortRequestOptions&);

  void Trace(Visitor*) override;

 private:
  explicit Serial(ExecutionContext&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_H_

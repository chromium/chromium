// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_UNDERLYING_SOURCE_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_UNDERLYING_SOURCE_BASE_H_

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ReadableStreamDefaultControllerWithScriptScope;
class ScriptState;

class CORE_EXPORT UnderlyingSourceBase
    : public ScriptWrappable,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  void Trace(Visitor*) const override;
  ~UnderlyingSourceBase() override = default;

  ScriptPromise startWrapper(ScriptState*, ScriptValue controller);
  virtual ScriptPromise Start(ScriptState*);

  virtual ScriptPromise pull(ScriptState*);

  ScriptPromise cancelWrapper(ScriptState*);
  ScriptPromise cancelWrapper(ScriptState*, ScriptValue reason);
  virtual ScriptPromise Cancel(ScriptState*, ScriptValue reason);

  ScriptValue type(ScriptState*) const;

  // ExecutionContextLifecycleObserver implementation:

  // This is needed to prevent stream operations being performed after the
  // window or worker is destroyed.
  void ContextDestroyed() override;

 protected:
  explicit UnderlyingSourceBase(ScriptState* script_state)
      : ExecutionContextLifecycleObserver(
            ExecutionContext::From(script_state)) {}

  ReadableStreamDefaultControllerWithScriptScope* Controller() const {
    return controller_;
  }

 private:
  Member<ReadableStreamDefaultControllerWithScriptScope> controller_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_UNDERLYING_SOURCE_BASE_H_

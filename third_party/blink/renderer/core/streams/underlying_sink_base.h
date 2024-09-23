// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_UNDERLYING_SINK_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_UNDERLYING_SINK_BASE_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class ExceptionState;
class ScriptState;
class WritableStreamDefaultController;

class CORE_EXPORT UnderlyingSinkBase : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~UnderlyingSinkBase() override = default;

  // We define non-virtual |start| and |write| which take ScriptValue for
  // |controller| and are called from IDL. Also we define virtual |start| and
  // |write| which take WritableStreamDefaultController.
  virtual ScriptPromise<IDLUndefined> start(ScriptState*,
                                            WritableStreamDefaultController*,
                                            ExceptionState&) = 0;
  virtual ScriptPromise<IDLUndefined> write(ScriptState*,
                                            ScriptValue chunk,
                                            WritableStreamDefaultController*,
                                            ExceptionState&) = 0;
  virtual ScriptPromise<IDLUndefined> close(ScriptState*, ExceptionState&) = 0;
  virtual ScriptPromise<IDLUndefined> abort(ScriptState*,
                                            ScriptValue reason,
                                            ExceptionState&) = 0;

  ScriptPromise<IDLUndefined> start(ScriptState*,
                                    ScriptValue controller,
                                    ExceptionState&);

  ScriptPromise<IDLUndefined> write(ScriptState* script_state,
                                    ScriptValue chunk,
                                    ScriptValue controller,
                                    ExceptionState& exception_state) {
    DCHECK(controller_);
    return write(script_state, chunk, controller_, exception_state);
  }

  // Returns a JavaScript "undefined" value. This is required by the
  // WritableStream Create() method.
  ScriptValue type(ScriptState*) const;

  void Trace(Visitor*) const override;

 protected:
  WritableStreamDefaultController* Controller() const {
    return controller_.Get();
  }

 private:
  Member<WritableStreamDefaultController> controller_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_UNDERLYING_SINK_BASE_H_

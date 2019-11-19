// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;
class MessagePort;
class ScriptState;
class UnderlyingSinkBase;

// This is an implementation of the corresponding IDL interface.
class CORE_EXPORT WritableStream : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Create function selects an implementation of WritableStream to use at
  // runtime.
  static WritableStream* Create(ScriptState*, ExceptionState&);
  static WritableStream* Create(ScriptState*,
                                ScriptValue underlying_sink,
                                ExceptionState&);
  static WritableStream* Create(ScriptState*,
                                ScriptValue underlying_sink,
                                ScriptValue strategy,
                                ExceptionState&);

  // Creates a WritableStream from C++. If |high_water_mark| is set to 0 then
  // piping to this writable stream will not work, because there will always be
  // backpressure. Usually 1 is the right choice.
  static WritableStream* CreateWithCountQueueingStrategy(
      ScriptState*,
      UnderlyingSinkBase*,
      size_t high_water_mark);

  // IDL defined functions
  virtual bool locked(ScriptState*, ExceptionState&) const = 0;
  virtual ScriptPromise abort(ScriptState*, ExceptionState&) = 0;
  virtual ScriptPromise abort(ScriptState*,
                              ScriptValue reason,
                              ExceptionState&) = 0;
  virtual ScriptValue getWriter(ScriptState*, ExceptionState&) = 0;

  virtual base::Optional<bool> IsLocked(ScriptState*,
                                        ExceptionState&) const = 0;

  // Serialize this stream to |port|. The stream will be locked by this
  // operation.
  virtual void Serialize(ScriptState*, MessagePort* port, ExceptionState&) = 0;

  // Given a |port| which is entangled with a MessagePort that was previously
  // passed to Serialize(), returns a new WritableStream which behaves like it
  // was the original.
  static WritableStream* Deserialize(ScriptState*,
                                     MessagePort* port,
                                     ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_H_

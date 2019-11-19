// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class ScriptPromise;
class ScriptState;
class UnderlyingSourceBase;
class MessagePort;

// This is an implementation of the corresponding IDL interface.
class CORE_EXPORT ReadableStream : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // ReadHandle is used to read from a stream. Each call to Read() corresponds
  // to a call to ReadableStreamDefaultReaderRead on the underlying stream.
  //
  // This is a transitional interface while the streams C++ port is in progress.
  // Eventually callers will just use ReadableStreamDefaultReader objects
  // directly.
  //
  // TODO(ricea): Remove this when the V8 Extras implementation is removed.
  class ReadHandle : public GarbageCollected<ReadHandle> {
   public:
    ReadHandle() = default;
    virtual ~ReadHandle() = default;

    virtual ScriptPromise Read(ScriptState*) = 0;

    virtual void Trace(Visitor*) {}

   private:
    DISALLOW_COPY_AND_ASSIGN(ReadHandle);
  };

  // Create* functions create an appropriate subclass depending on which
  // implementation is selected by blink features.
  static ReadableStream* Create(ScriptState*, ExceptionState&);
  static ReadableStream* Create(ScriptState*,
                                ScriptValue underlying_source,
                                ExceptionState&);
  static ReadableStream* Create(ScriptState*,
                                ScriptValue underlying_source,
                                ScriptValue strategy,
                                ExceptionState&);

  // This function doesn't take ExceptionState because the caller cannot have
  // one. Returns null when an error happens.
  static ReadableStream* CreateWithCountQueueingStrategy(
      ScriptState*,
      UnderlyingSourceBase* underlying_source,
      size_t high_water_mark);

  // IDL defined functions
  virtual bool locked(ScriptState*, ExceptionState&) const = 0;
  virtual ScriptPromise cancel(ScriptState*, ExceptionState&) = 0;
  virtual ScriptPromise cancel(ScriptState*,
                               ScriptValue reason,
                               ExceptionState&) = 0;
  virtual ScriptValue getReader(ScriptState*, ExceptionState&) = 0;
  virtual ScriptValue getReader(ScriptState*,
                                ScriptValue options,
                                ExceptionState&) = 0;
  virtual ScriptValue pipeThrough(ScriptState*,
                                  ScriptValue transform_stream,
                                  ExceptionState&) = 0;
  virtual ScriptValue pipeThrough(ScriptState*,
                                  ScriptValue transform_stream,
                                  ScriptValue options,
                                  ExceptionState&) = 0;
  virtual ScriptPromise pipeTo(ScriptState*,
                               ScriptValue destination,
                               ExceptionState&) = 0;
  virtual ScriptPromise pipeTo(ScriptState*,
                               ScriptValue destination,
                               ScriptValue options,
                               ExceptionState&) = 0;
  virtual ScriptValue tee(ScriptState*, ExceptionState&) = 0;

  virtual void Tee(ScriptState*,
                   ReadableStream** branch1,
                   ReadableStream** branch2,
                   ExceptionState&) = 0;

  // Lock the stream and return a handle that permits reading from the stream.
  // This is a temporary API to abstract away the difference between the two
  // implementations. It is not possible to unlock the stream again after
  // calling this.
  virtual ReadHandle* GetReadHandle(ScriptState*, ExceptionState&) = 0;

  virtual base::Optional<bool> IsLocked(ScriptState*,
                                        ExceptionState&) const = 0;
  virtual base::Optional<bool> IsDisturbed(ScriptState*,
                                           ExceptionState&) const = 0;
  virtual base::Optional<bool> IsReadable(ScriptState*,
                                          ExceptionState&) const = 0;
  virtual base::Optional<bool> IsClosed(ScriptState*,
                                        ExceptionState&) const = 0;
  virtual base::Optional<bool> IsErrored(ScriptState*,
                                         ExceptionState&) const = 0;

  // Makes this stream locked and disturbed.
  virtual void LockAndDisturb(ScriptState*, ExceptionState&) = 0;

  // Serialize this stream to |port|. The stream will be locked by this
  // operation.
  virtual void Serialize(ScriptState*, MessagePort* port, ExceptionState&) = 0;

  // Given a |port| which is entangled with a MessagePort that was previously
  // passed to Serialize(), returns a new ReadableStream which behaves like it
  // was the original.
  static ReadableStream* Deserialize(ScriptState*,
                                     MessagePort* port,
                                     ExceptionState&);

  // In some cases we are known to fail to trace the stream correctly. In such
  // cases internal references will be silently lost. This function is for
  // detecting the issue. Use this function at places where an actual crash
  // happens. Do not use this function to write "just in case" code.
  // TODO(ricea): Remove this after switching to the new implementation.
  virtual bool IsBroken() const = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_H_

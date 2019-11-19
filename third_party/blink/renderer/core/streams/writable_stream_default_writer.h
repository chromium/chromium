// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_DEFAULT_WRITER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_DEFAULT_WRITER_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class ScriptPromise;
class ScriptState;
class ScriptValue;
class StreamPromiseResolver;
class Visitor;
class WritableStream;
class WritableStreamNative;

// https://streams.spec.whatwg.org/#default-writer-class
class CORE_EXPORT WritableStreamDefaultWriter final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // JavaScript-exposed constructor.
  static WritableStreamDefaultWriter* Create(ScriptState*,
                                             WritableStream* stream,
                                             ExceptionState&);

  // https://streams.spec.whatwg.org/#default-writer-constructor
  WritableStreamDefaultWriter(ScriptState*,
                              WritableStreamNative* stream,
                              ExceptionState&);
  ~WritableStreamDefaultWriter() override;

  // Getters

  // https://streams.spec.whatwg.org/#default-writer-closed
  ScriptPromise closed(ScriptState*) const;

  // https://streams.spec.whatwg.org/#default-writer-desired-size
  ScriptValue desiredSize(ScriptState*, ExceptionState&) const;

  // https://streams.spec.whatwg.org/#default-writer-ready
  ScriptPromise ready(ScriptState*) const;

  // Methods

  // https://streams.spec.whatwg.org/#default-writer-abort
  ScriptPromise abort(ScriptState*);
  ScriptPromise abort(ScriptState*, ScriptValue reason);

  // https://streams.spec.whatwg.org/#default-writer-close
  ScriptPromise close(ScriptState*);

  // https://streams.spec.whatwg.org/#default-writer-release-lock
  void releaseLock(ScriptState*);

  // https://streams.spec.whatwg.org/#default-writer-write
  ScriptPromise write(ScriptState*);
  ScriptPromise write(ScriptState*, ScriptValue chunk);

  //
  // Methods used by WritableStreamNative
  //

  // https://streams.spec.whatwg.org/#writable-stream-default-writer-ensure-ready-promise-rejected
  static void EnsureReadyPromiseRejected(ScriptState*,
                                         WritableStreamDefaultWriter*,
                                         v8::Local<v8::Value> error);

  //
  // Methods used by ReadableStreamNative
  //

  // https://streams.spec.whatwg.org/#writable-stream-default-writer-close-with-error-propagation
  static v8::Local<v8::Promise> CloseWithErrorPropagation(
      ScriptState*,
      WritableStreamDefaultWriter*);

  // https://streams.spec.whatwg.org/#writable-stream-default-writer-release
  static void Release(ScriptState*, WritableStreamDefaultWriter*);

  // https://streams.spec.whatwg.org/#writable-stream-default-writer-write
  static v8::Local<v8::Promise> Write(ScriptState*,
                                      WritableStreamDefaultWriter*,
                                      v8::Local<v8::Value> chunk);

  //
  // Accessors used by ReadableStreamNative and WritableStreamNative. These do
  // not appear in the standard.
  //

  StreamPromiseResolver* ClosedPromise() { return closed_promise_; }
  StreamPromiseResolver* ReadyPromise() { return ready_promise_; }
  WritableStreamNative* OwnerWritableStream() { return owner_writable_stream_; }

  // This is a variant of GetDesiredSize() that doesn't create an intermediate
  // JavaScript object. Instead it returns base::nullopt where the JavaScript
  // version would return null.
  base::Optional<double> GetDesiredSizeInternal() const;

  void SetReadyPromise(StreamPromiseResolver*);

  void Trace(Visitor*) override;

 private:
  // https://streams.spec.whatwg.org/#writable-stream-default-writer-abort
  static v8::Local<v8::Promise> Abort(ScriptState*,
                                      WritableStreamDefaultWriter*,
                                      v8::Local<v8::Value> reason);

  // https://streams.spec.whatwg.org/#writable-stream-default-writer-close
  static v8::Local<v8::Promise> Close(ScriptState*,
                                      WritableStreamDefaultWriter*);

  // https://streams.spec.whatwg.org/#writable-stream-default-writer-ensure-closed-promise-rejected
  static void EnsureClosedPromiseRejected(ScriptState*,
                                          WritableStreamDefaultWriter*,
                                          v8::Local<v8::Value> error);

  // https://streams.spec.whatwg.org/#writable-stream-default-writer-get-desired-size
  static v8::Local<v8::Value> GetDesiredSize(
      v8::Isolate* isolate,
      const WritableStreamDefaultWriter*);

  // |closed_promise_| and |ready_promise_| are implemented as resolvers. The
  // names come from the slots [[closedPromise]] and [[readyPromise]] in the
  // standard.
  Member<StreamPromiseResolver> closed_promise_;
  Member<WritableStreamNative> owner_writable_stream_;
  Member<StreamPromiseResolver> ready_promise_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_DEFAULT_WRITER_H_

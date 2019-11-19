// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_NATIVE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_NATIVE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class ScriptState;
class StrategySizeAlgorithm;
class StreamAlgorithm;
class StreamPromiseResolver;
class StreamStartAlgorithm;
class UnderlyingSinkBase;
class WritableStreamDefaultController;
class WritableStreamDefaultWriter;

class CORE_EXPORT WritableStreamNative : public WritableStream {
 public:
  enum State : uint8_t {
    kWritable,
    kClosed,
    kErroring,
    kErrored,
  };

  // https://streams.spec.whatwg.org/#ws-constructor
  static WritableStreamNative* Create(ScriptState*,
                                      ScriptValue raw_underlying_sink,
                                      ScriptValue raw_strategy,
                                      ExceptionState&);

  // https://streams.spec.whatwg.org/#create-writable-stream
  // Unlike in the standard, |high_water_mark| and |size_algorithm| are
  // required parameters.
  static WritableStreamNative* Create(ScriptState*,
                                      StreamStartAlgorithm* start_algorithm,
                                      StreamAlgorithm* write_algorithm,
                                      StreamAlgorithm* close_algorithm,
                                      StreamAlgorithm* abort_algorithm,
                                      double high_water_mark,
                                      StrategySizeAlgorithm* size_algorithm,
                                      ExceptionState&);

  static WritableStreamNative* CreateWithCountQueueingStrategy(
      ScriptState*,
      UnderlyingSinkBase*,
      size_t high_water_mark);

  // Called by Create().
  WritableStreamNative();
  ~WritableStreamNative() override;

  // IDL defined functions

  // https://streams.spec.whatwg.org/#ws-locked
  bool locked(ScriptState*, ExceptionState&) const override;

  // https://streams.spec.whatwg.org/#ws-abort
  ScriptPromise abort(ScriptState*, ExceptionState&) override;
  ScriptPromise abort(ScriptState*,
                      ScriptValue reason,
                      ExceptionState&) override;

  // https://streams.spec.whatwg.org/#ws-get-writer
  ScriptValue getWriter(ScriptState*, ExceptionState&) override;

  // Inherited methods used internally.

  // https://streams.spec.whatwg.org/#is-writable-stream-locked
  // TODO(ricea): Delete this variant once the V8 extras implementation is
  // removed.
  base::Optional<bool> IsLocked(ScriptState*, ExceptionState&) const override {
    return IsLocked(this);
  }

  // This version can't fail.
  static bool IsLocked(const WritableStreamNative* stream) {
    return stream->writer_;
  }

  void Serialize(ScriptState*, MessagePort*, ExceptionState&) override;

  static WritableStreamNative* Deserialize(ScriptState*,
                                           MessagePort*,
                                           ExceptionState&);

  //
  // Methods used by ReadableStreamNative::PipeTo
  //

  // https://streams.spec.whatwg.org/#acquire-writable-stream-default-writer
  static WritableStreamDefaultWriter*
  AcquireDefaultWriter(ScriptState*, WritableStreamNative*, ExceptionState&);

  //
  // Methods used by WritableStreamDefaultWriter.
  //

  // https://streams.spec.whatwg.org/#writable-stream-abort
  static v8::Local<v8::Promise> Abort(ScriptState*,
                                      WritableStreamNative*,
                                      v8::Local<v8::Value> reason);

  // https://streams.spec.whatwg.org/#writable-stream-add-write-request
  static v8::Local<v8::Promise> AddWriteRequest(ScriptState*,
                                                WritableStreamNative*);

  // https://streams.spec.whatwg.org/#writable-stream-close-queued-or-in-flight
  static bool CloseQueuedOrInFlight(const WritableStreamNative*);

  //
  // Methods used by WritableStreamDefaultController.
  //

  // https://streams.spec.whatwg.org/#writable-stream-deal-with-rejection
  static void DealWithRejection(ScriptState*,
                                WritableStreamNative*,
                                v8::Local<v8::Value> error);

  // https://streams.spec.whatwg.org/#writable-stream-start-erroring
  static void StartErroring(ScriptState*,
                            WritableStreamNative*,
                            v8::Local<v8::Value> reason);

  // https://streams.spec.whatwg.org/#writable-stream-finish-erroring
  static void FinishErroring(ScriptState*, WritableStreamNative*);

  // https://streams.spec.whatwg.org/#writable-stream-finish-in-flight-write
  static void FinishInFlightWrite(ScriptState*, WritableStreamNative*);

  // https://streams.spec.whatwg.org/#writable-stream-finish-in-flight-write-with-error
  static void FinishInFlightWriteWithError(ScriptState*,
                                           WritableStreamNative*,
                                           v8::Local<v8::Value> error);

  // https://streams.spec.whatwg.org/#writable-stream-finish-in-flight-close
  static void FinishInFlightClose(ScriptState*, WritableStreamNative*);

  // https://streams.spec.whatwg.org/#writable-stream-finish-in-flight-close-with-error
  static void FinishInFlightCloseWithError(ScriptState*,
                                           WritableStreamNative*,
                                           v8::Local<v8::Value> error);

  // https://streams.spec.whatwg.org/#writable-stream-mark-close-request-in-flight
  static void MarkCloseRequestInFlight(WritableStreamNative*);

  // https://streams.spec.whatwg.org/#writable-stream-mark-first-write-request-in-flight
  static void MarkFirstWriteRequestInFlight(WritableStreamNative*);

  // https://streams.spec.whatwg.org/#writable-stream-update-backpressure
  static void UpdateBackpressure(ScriptState*,
                                 WritableStreamNative*,
                                 bool backpressure);

  // Accessors for use by other stream classes.
  State GetState() const { return state_; }
  bool IsErrored() const { return state_ == kErrored; }
  bool IsErroring() const { return state_ == kErroring; }
  bool IsWritable() const { return state_ == kWritable; }

  bool HasBackpressure() const { return has_backpressure_; }

  const StreamPromiseResolver* InFlightWriteRequest() const {
    return in_flight_write_request_;
  }

  bool IsClosingOrClosed() const {
    return CloseQueuedOrInFlight(this) || state_ == kClosed;
  }

  v8::Local<v8::Value> GetStoredError(v8::Isolate*) const;

  WritableStreamDefaultController* Controller() {
    return writable_stream_controller_;
  }
  const WritableStreamDefaultController* Controller() const {
    return writable_stream_controller_;
  }

  const WritableStreamDefaultWriter* Writer() const { return writer_; }

  void SetCloseRequest(StreamPromiseResolver*);
  void SetController(WritableStreamDefaultController*);
  void SetWriter(WritableStreamDefaultWriter*);

  void Trace(Visitor*) override;

 private:
  using PromiseQueue = HeapDeque<Member<StreamPromiseResolver>>;

  class PendingAbortRequest;

  // Used when creating a stream from JavaScript. Called from Create().
  // https://streams.spec.whatwg.org/#ws-constructor
  void InitInternal(ScriptState*,
                    ScriptValue raw_underlying_sink,
                    ScriptValue raw_strategy,
                    ExceptionState&);

  // https://streams.spec.whatwg.org/#writable-stream-has-operation-marked-in-flight
  static bool HasOperationMarkedInFlight(const WritableStreamNative*);

  // https://streams.spec.whatwg.org/#writable-stream-reject-close-and-closed-promise-if-needed
  static void RejectCloseAndClosedPromiseIfNeeded(ScriptState*,
                                                  WritableStreamNative*);

  // Functions that don't appear in the standard.

  // Rejects all the promises in |queue| with value |e|.
  static void RejectPromises(ScriptState*,
                             PromiseQueue* queue,
                             v8::Local<v8::Value> e);

  // Member variables correspond 1:1 to the internal slots in the standard.
  // See https://streams.spec.whatwg.org/#ws-internal-slots

  // |has_backpressure_| corresponds to the [[backpressure]] slot in the
  // |standard.
  bool has_backpressure_ = false;

  // |state_| is here out of order so it doesn't require 7 bytes of padding.
  State state_ = kWritable;

  Member<StreamPromiseResolver> close_request_;
  Member<StreamPromiseResolver> in_flight_write_request_;
  Member<StreamPromiseResolver> in_flight_close_request_;
  Member<PendingAbortRequest> pending_abort_request_;
  TraceWrapperV8Reference<v8::Value> stored_error_;
  Member<WritableStreamDefaultController> writable_stream_controller_;
  Member<WritableStreamDefaultWriter> writer_;
  PromiseQueue write_requests_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_NATIVE_H_

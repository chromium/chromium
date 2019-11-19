// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_NATIVE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_NATIVE_H_

#include <stdint.h>

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "v8/include/v8.h"

namespace blink {

class AbortSignal;
class ExceptionState;
class ReadableStreamDefaultController;
class ReadableStreamReader;
class ScriptState;
class StrategySizeAlgorithm;
class StreamAlgorithm;
class StreamPromiseResolver;
class StreamStartAlgorithm;
class UnderlyingSourceBase;
class Visitor;
class WritableStreamNative;

// C++ implementation of ReadableStream.
// See https://streams.spec.whatwg.org/#rs-model for background.
class ReadableStreamNative : public ReadableStream {
 public:
  class PipeOptions : public GarbageCollected<PipeOptions> {
   public:
    PipeOptions();
    PipeOptions(ScriptState* script_state,
                ScriptValue options,
                ExceptionState& exception_state);

    bool PreventClose() const { return prevent_close_; }
    bool PreventAbort() const { return prevent_abort_; }
    bool PreventCancel() const { return prevent_cancel_; }
    AbortSignal* Signal() const { return signal_; }

    void Trace(Visitor*);

   private:
    bool GetBoolean(ScriptState* script_state,
                    v8::Local<v8::Object> dictionary,
                    const char* property_name,
                    ExceptionState& exception_state);

    bool prevent_close_ = false;
    bool prevent_abort_ = false;
    bool prevent_cancel_ = false;
    Member<AbortSignal> signal_;
  };

  enum State : uint8_t { kReadable, kClosed, kErrored };

  // Implements ReadableStream::Create() when this implementation is enabled.
  static ReadableStreamNative* Create(ScriptState* script_state,
                                      ScriptValue underlying_source,
                                      ScriptValue strategy,
                                      ExceptionState& exception_state);

  // Implements ReadableStream::CreateWithCountQueueingStrategy() when this
  // implementation is enabled.
  //
  // TODO(ricea): Replace this API with something more efficient when the old
  // implementation is gone.
  static ReadableStreamNative* CreateWithCountQueueingStrategy(
      ScriptState* script_state,
      UnderlyingSourceBase* underlying_source,
      size_t high_water_mark);

  // CreateReadableStream():
  // https://streams.spec.whatwg.org/#create-readable-stream
  static ReadableStreamNative* Create(ScriptState*,
                                      StreamStartAlgorithm* start_algorithm,
                                      StreamAlgorithm* pull_algorithm,
                                      StreamAlgorithm* cancel_algorithm,
                                      double high_water_mark,
                                      StrategySizeAlgorithm* size_algorithm,
                                      ExceptionState&);

  ReadableStreamNative();

  ~ReadableStreamNative() override;

  // https://streams.spec.whatwg.org/#rs-constructor
  bool locked(ScriptState*, ExceptionState&) const override;

  ScriptPromise cancel(ScriptState*, ExceptionState&) override;

  // https://streams.spec.whatwg.org/#rs-cancel
  ScriptPromise cancel(ScriptState*,
                       ScriptValue reason,
                       ExceptionState&) override;

  ScriptValue getReader(ScriptState*, ExceptionState&) override;

  // https://streams.spec.whatwg.org/#rs-get-reader
  ScriptValue getReader(ScriptState*,
                        ScriptValue options,
                        ExceptionState&) override;

  ScriptValue pipeThrough(ScriptState*,
                          ScriptValue transform_stream,
                          ExceptionState&) override;

  // https://streams.spec.whatwg.org/#rs-pipe-through
  ScriptValue pipeThrough(ScriptState*,
                          ScriptValue transform_stream,
                          ScriptValue options,
                          ExceptionState&) override;

  ScriptPromise pipeTo(ScriptState*,
                       ScriptValue destination,
                       ExceptionState&) override;

  // https://streams.spec.whatwg.org/#rs-pipe-to
  ScriptPromise pipeTo(ScriptState*,
                       ScriptValue destination_value,
                       ScriptValue options,
                       ExceptionState&) override;

  // https://streams.spec.whatwg.org/#rs-tee
  ScriptValue tee(ScriptState*, ExceptionState&) override;

  // TODO(domenic): cloneForBranch2 argument from spec not supported yet
  void Tee(ScriptState*,
           ReadableStream** branch1,
           ReadableStream** branch2,
           ExceptionState&) override;

  ReadHandle* GetReadHandle(ScriptState*, ExceptionState&) override;

  base::Optional<bool> IsLocked(ScriptState*, ExceptionState&) const override {
    return IsLocked(this);
  }

  base::Optional<bool> IsDisturbed(ScriptState*,
                                   ExceptionState&) const override {
    return IsDisturbed(this);
  }

  base::Optional<bool> IsReadable(ScriptState*,
                                  ExceptionState&) const override {
    return IsReadable(this);
  }

  base::Optional<bool> IsClosed(ScriptState*, ExceptionState&) const override {
    return IsClosed(this);
  }

  base::Optional<bool> IsErrored(ScriptState*, ExceptionState&) const override {
    return IsErrored(this);
  }

  void LockAndDisturb(ScriptState*, ExceptionState&) override;

  void Serialize(ScriptState*, MessagePort* port, ExceptionState&) override;

  static ReadableStreamNative* Deserialize(ScriptState*,
                                           MessagePort* port,
                                           ExceptionState&);

  bool IsBroken() const override { return false; }

  //
  // Readable stream abstract operations
  //

  // https://streams.spec.whatwg.org/#is-readable-stream-disturbed
  static bool IsDisturbed(const ReadableStreamNative* stream) {
    return stream->is_disturbed_;
  }

  // https://streams.spec.whatwg.org/#is-readable-stream-locked
  static bool IsLocked(const ReadableStreamNative* stream) {
    return stream->reader_;
  }

  // https://streams.spec.whatwg.org/#readable-stream-pipe-to
  static ScriptPromise PipeTo(ScriptState*,
                              ReadableStreamNative*,
                              WritableStreamNative*,
                              PipeOptions*);

  //
  // Functions exported for use by TransformStream. Not part of the standard.
  //

  static bool IsReadable(const ReadableStreamNative* stream) {
    return stream->state_ == kReadable;
  }

  static bool IsClosed(const ReadableStreamNative* stream) {
    return stream->state_ == kClosed;
  }

  static bool IsErrored(const ReadableStreamNative* stream) {
    return stream->state_ == kErrored;
  }

  ReadableStreamDefaultController* GetController() {
    return readable_stream_controller_;
  }

  v8::Local<v8::Value> GetStoredError(v8::Isolate*) const;

  void Trace(Visitor*) override;

 private:
  friend class ReadableStreamDefaultController;
  friend class ReadableStreamReader;

  class PipeToEngine;
  class ReadHandleImpl;
  class TeeEngine;

  // https://streams.spec.whatwg.org/#rs-constructor
  void InitInternal(ScriptState*,
                    ScriptValue raw_underlying_source,
                    ScriptValue raw_strategy,
                    bool created_by_ua,
                    ExceptionState&);

  // https://streams.spec.whatwg.org/#initialize-readable-stream
  static void Initialize(ReadableStreamNative*);

  // https://streams.spec.whatwg.org/#acquire-readable-stream-reader
  static ReadableStreamReader* AcquireDefaultReader(ScriptState*,
                                                    ReadableStreamNative*,
                                                    bool for_author_code,
                                                    ExceptionState&);

  // https://streams.spec.whatwg.org/#readable-stream-add-read-request
  static StreamPromiseResolver* AddReadRequest(ScriptState*,
                                               ReadableStreamNative*);

  // https://streams.spec.whatwg.org/#readable-stream-cancel
  static v8::Local<v8::Promise> Cancel(ScriptState*,
                                       ReadableStreamNative*,
                                       v8::Local<v8::Value> reason);

  // https://streams.spec.whatwg.org/#readable-stream-close
  static void Close(ScriptState*, ReadableStreamNative*);

  // https://streams.spec.whatwg.org/#readable-stream-create-read-result
  static v8::Local<v8::Value> CreateReadResult(ScriptState*,
                                               v8::Local<v8::Value> value,
                                               bool done,
                                               bool for_author_code);

  // https://streams.spec.whatwg.org/#readable-stream-error
  static void Error(ScriptState*,
                    ReadableStreamNative*,
                    v8::Local<v8::Value> e);

  // https://streams.spec.whatwg.org/#readable-stream-fulfill-read-request
  static void FulfillReadRequest(ScriptState*,
                                 ReadableStreamNative*,
                                 v8::Local<v8::Value> chunk,
                                 bool done);

  // https://streams.spec.whatwg.org/#readable-stream-get-num-read-requests
  static int GetNumReadRequests(const ReadableStreamNative*);

  //
  // TODO(ricea): Functions for transferable streams.
  //

  static void UnpackPipeOptions(ScriptState*,
                                ScriptValue options,
                                PipeOptions*,
                                ExceptionState&);

  static bool GetBoolean(ScriptState*,
                         v8::Local<v8::Object> dictionary,
                         const char* property_name,
                         ExceptionState&);


  bool is_disturbed_ = false;
  State state_ = kReadable;
  Member<ReadableStreamDefaultController> readable_stream_controller_;
  Member<ReadableStreamReader> reader_;
  TraceWrapperV8Reference<v8::Value> stored_error_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_NATIVE_H_

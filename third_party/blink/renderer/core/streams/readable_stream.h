// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_H_

#include <stdint.h>
#include <memory>

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/streams/readable_stream_byob_reader.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "v8/include/v8.h"

namespace blink {

class AbortSignal;
class ExceptionState;
class MessagePort;
class ReadableByteStreamController;
class ReadableStreamController;
class ReadableStreamDefaultController;
class ReadableStreamDefaultReaderOrReadableStreamBYOBReader;
class ReadableStreamGetReaderOptions;
class ReadableStreamTransferringOptimizer;
class ReadableWritablePair;
class ScriptPromise;
class ScriptState;
class StrategySizeAlgorithm;
class StreamAlgorithm;
class StreamPipeOptions;
class StreamPromiseResolver;
class StreamStartAlgorithm;
class UnderlyingSourceBase;
class WritableStream;

// C++ implementation of ReadableStream.
// See https://streams.spec.whatwg.org/#rs-model for background.
class CORE_EXPORT ReadableStream : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class PipeOptions : public GarbageCollected<PipeOptions> {
   public:
    PipeOptions();
    explicit PipeOptions(const StreamPipeOptions* options);

    bool PreventClose() const { return prevent_close_; }
    bool PreventAbort() const { return prevent_abort_; }
    bool PreventCancel() const { return prevent_cancel_; }
    AbortSignal* Signal() const { return signal_; }

    void Trace(Visitor*) const;

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

  // Zero-argument form of the constructor called from JavaScript.
  static ReadableStream* Create(ScriptState*, ExceptionState&);

  // One-argument constructor called from JavaScript.
  static ReadableStream* Create(ScriptState*,
                                ScriptValue underlying_source,
                                ExceptionState&);

  // Two-argument constructor called from JavaScript.
  static ReadableStream* Create(ScriptState* script_state,
                                ScriptValue underlying_source,
                                ScriptValue strategy,
                                ExceptionState& exception_state);

  // Entry point to create a ReadableStream from other C++ APIs.
  static ReadableStream* CreateWithCountQueueingStrategy(
      ScriptState* script_state,
      UnderlyingSourceBase* underlying_source,
      size_t high_water_mark);
  static ReadableStream* CreateWithCountQueueingStrategy(
      ScriptState* script_state,
      UnderlyingSourceBase* underlying_source,
      size_t high_water_mark,
      std::unique_ptr<ReadableStreamTransferringOptimizer> optimizer);

  // CreateReadableStream():
  // https://streams.spec.whatwg.org/#create-readable-stream
  static ReadableStream* Create(ScriptState*,
                                StreamStartAlgorithm* start_algorithm,
                                StreamAlgorithm* pull_algorithm,
                                StreamAlgorithm* cancel_algorithm,
                                double high_water_mark,
                                StrategySizeAlgorithm* size_algorithm,
                                ExceptionState&);

  ReadableStream();

  ~ReadableStream() override;

  // https://streams.spec.whatwg.org/#rs-constructor
  bool locked() const;

  ScriptPromise cancel(ScriptState*, ExceptionState&);

  // https://streams.spec.whatwg.org/#rs-cancel
  ScriptPromise cancel(ScriptState*, ScriptValue reason, ExceptionState&);

  void getReader(
      ScriptState*,
      ReadableStreamDefaultReaderOrReadableStreamBYOBReader& return_value,
      ExceptionState&);

  // https://streams.spec.whatwg.org/#rs-get-reader
  void getReader(
      ScriptState*,
      ReadableStreamGetReaderOptions* options,
      ReadableStreamDefaultReaderOrReadableStreamBYOBReader& return_value,
      ExceptionState&);

  ReadableStreamDefaultReader* GetDefaultReaderForTesting(ScriptState*,
                                                          ExceptionState&);

  ReadableStream* pipeThrough(ScriptState*,
                              ReadableWritablePair* transform,
                              ExceptionState&);

  // https://streams.spec.whatwg.org/#rs-pipe-through
  ReadableStream* pipeThrough(ScriptState*,
                              ReadableWritablePair* transform,
                              const StreamPipeOptions* options,
                              ExceptionState&);

  ScriptPromise pipeTo(ScriptState*,
                       WritableStream* destination,
                       ExceptionState&);

  // https://streams.spec.whatwg.org/#rs-pipe-to
  ScriptPromise pipeTo(ScriptState*,
                       WritableStream* destination,
                       const StreamPipeOptions* options,
                       ExceptionState&);

  // https://streams.spec.whatwg.org/#rs-tee
  HeapVector<Member<ReadableStream>> tee(ScriptState*, ExceptionState&);

  // TODO(domenic): cloneForBranch2 argument from spec not supported yet
  void Tee(ScriptState*,
           ReadableStream** branch1,
           ReadableStream** branch2,
           ExceptionState&);

  bool IsLocked() const { return IsLocked(this); }

  bool IsDisturbed() const { return IsDisturbed(this); }

  bool IsReadable() const { return IsReadable(this); }

  bool IsClosed() const { return IsClosed(this); }

  bool IsErrored() const { return IsErrored(this); }

  void LockAndDisturb(ScriptState*);

  void Serialize(ScriptState*, MessagePort* port, ExceptionState&);

  static ReadableStream* Deserialize(
      ScriptState*,
      MessagePort* port,
      std::unique_ptr<ReadableStreamTransferringOptimizer> optimizer,
      ExceptionState&);

  // Returns a reader that doesn't have the |for_author_code_| flag set. This is
  // used in contexts where reads should not be interceptable by user code. This
  // corresponds to calling AcquireReadableStreamDefaultReader(stream, false) in
  // specification language. The caller must ensure that the stream is not
  // locked.
  ReadableStreamDefaultReader* GetReaderNotForAuthorCode(ScriptState*);

  //
  // Readable stream abstract operations
  //

  // https://streams.spec.whatwg.org/#is-readable-stream-disturbed
  static bool IsDisturbed(const ReadableStream* stream) {
    return stream->is_disturbed_;
  }

  // https://streams.spec.whatwg.org/#is-readable-stream-locked
  static bool IsLocked(const ReadableStream* stream) { return stream->reader_; }

  // https://streams.spec.whatwg.org/#readable-stream-pipe-to
  static ScriptPromise PipeTo(ScriptState*,
                              ReadableStream*,
                              WritableStream*,
                              PipeOptions*);

  // https://streams.spec.whatwg.org/#acquire-readable-stream-reader
  static ReadableStreamDefaultReader* AcquireDefaultReader(ScriptState*,
                                                           ReadableStream*,
                                                           bool for_author_code,
                                                           ExceptionState&);

  // https://streams.spec.whatwg.org/#acquire-readable-stream-byob-reader
  static ReadableStreamBYOBReader* AcquireBYOBReader(ScriptState*,
                                                     ReadableStream*,
                                                     ExceptionState&);

  //
  // Functions exported for use by TransformStream. Not part of the standard.
  //

  static bool IsReadable(const ReadableStream* stream) {
    return stream->state_ == kReadable;
  }

  static bool IsClosed(const ReadableStream* stream) {
    return stream->state_ == kClosed;
  }

  static bool IsErrored(const ReadableStream* stream) {
    return stream->state_ == kErrored;
  }

  ReadableStreamController* GetController() {
    return readable_stream_controller_;
  }

  v8::Local<v8::Value> GetStoredError(v8::Isolate*) const;

  std::unique_ptr<ReadableStreamTransferringOptimizer>
  TakeTransferringOptimizer();

  void Trace(Visitor*) const override;

 private:
  friend class ReadableByteStreamController;
  friend class ReadableStreamBYOBReader;
  friend class ReadableStreamDefaultController;
  friend class ReadableStreamDefaultReader;
  friend class ReadableStreamGenericReader;

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
  static void Initialize(ReadableStream*);

  static void AddReadIntoRequest(ScriptState*,
                                 ReadableStream*,
                                 ReadableStreamBYOBReader::ReadIntoRequest*);

  // https://streams.spec.whatwg.org/#readable-stream-add-read-request
  static StreamPromiseResolver* AddReadRequest(ScriptState*, ReadableStream*);

  // https://streams.spec.whatwg.org/#readable-stream-cancel
  static v8::Local<v8::Promise> Cancel(ScriptState*,
                                       ReadableStream*,
                                       v8::Local<v8::Value> reason);

  // https://streams.spec.whatwg.org/#readable-stream-close
  static void Close(ScriptState*, ReadableStream*);

  // https://streams.spec.whatwg.org/#readable-stream-create-read-result
  static v8::Local<v8::Value> CreateReadResult(ScriptState*,
                                               v8::Local<v8::Value> value,
                                               bool done,
                                               bool for_author_code);

  // https://streams.spec.whatwg.org/#readable-stream-error
  static void Error(ScriptState*, ReadableStream*, v8::Local<v8::Value> e);

  // https://streams.spec.whatwg.org/#readable-stream-fulfill-read-into-request
  static void FulfillReadIntoRequest(ScriptState*,
                                     ReadableStream*,
                                     DOMArrayBufferView* chunk,
                                     bool done);

  // https://streams.spec.whatwg.org/#readable-stream-fulfill-read-request
  static void FulfillReadRequest(ScriptState*,
                                 ReadableStream*,
                                 v8::Local<v8::Value> chunk,
                                 bool done);

  // https://streams.spec.whatwg.org/#readable-stream-get-num-read-into-requests
  static int GetNumReadIntoRequests(const ReadableStream*);

  // https://streams.spec.whatwg.org/#readable-stream-get-num-read-requests
  static int GetNumReadRequests(const ReadableStream*);

  // https://streams.spec.whatwg.org/#readable-stream-has-byob-reader
  static bool HasBYOBReader(const ReadableStream*);

  // https://streams.spec.whatwg.org/#readable-stream-has-default-reader
  static bool HasDefaultReader(const ReadableStream*);

  //
  // TODO(ricea): Functions for transferable streams.
  //

  // Calls Tee() on |readable|, converts the two branches to a JavaScript array
  // and returns them.
  static HeapVector<Member<ReadableStream>> CallTeeAndReturnBranchArray(
      ScriptState* script_state,
      ReadableStream* readable,
      ExceptionState& exception_state);

  bool is_disturbed_ = false;
  State state_ = kReadable;
  Member<ReadableStreamController> readable_stream_controller_;
  Member<ReadableStreamGenericReader> reader_;
  TraceWrapperV8Reference<v8::Value> stored_error_;
  std::unique_ptr<ReadableStreamTransferringOptimizer> transferring_optimizer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_H_

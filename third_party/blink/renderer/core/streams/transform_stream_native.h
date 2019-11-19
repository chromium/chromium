// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFORM_STREAM_NATIVE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFORM_STREAM_NATIVE_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class ReadableStream;
class ReadableStreamNative;
class ScriptState;
class StrategySizeAlgorithm;
class StreamAlgorithm;
class StreamPromiseResolver;
class StreamStartAlgorithm;
class TransformStreamDefaultController;
class TransformStreamTransformer;
class Visitor;
class WritableStream;
class WritableStreamNative;

// Implementation of TransformStream for Blink.  See
// https://streams.spec.whatwg.org/#ts. The implementation closely follows the
// standard, except where required for performance or integration with Blink.
// In particular, classes, methods and abstract operations are implemented in
// the same order as in the standard, to simplify side-by-side reading.

class CORE_EXPORT TransformStreamNative final
    : public GarbageCollected<TransformStreamNative> {
 public:
  // Implements the interface used by TransformStream to implement the
  // JS-visible constructor.
  static void InitFromJS(ScriptState*,
                         ScriptValue raw_transformer,
                         ScriptValue raw_writable_strategy,
                         ScriptValue raw_readable_strategy,
                         Member<ReadableStream>* readable,
                         Member<WritableStream>* writable,
                         ExceptionState&);

  // Creates a TransformStream from a C++ object.
  static void Init(ScriptState*,
                   TransformStreamTransformer*,
                   Member<ReadableStream>*,
                   Member<WritableStream>*,
                   ExceptionState&);

  // https://streams.spec.whatwg.org/#create-transform-stream
  static TransformStreamNative* Create(
      ScriptState*,
      StreamStartAlgorithm* start_algorithm,
      StreamAlgorithm* transform_algorithm,
      StreamAlgorithm* flush_algorithm,
      double writable_high_water_mark,
      StrategySizeAlgorithm* writable_size_algorithm,
      double readable_high_water_mark,
      StrategySizeAlgorithm* readable_size_algorithm,
      ExceptionState&);

  TransformStreamNative();

  TransformStreamNative(ScriptState*,
                        ScriptValue raw_transformer,
                        ScriptValue raw_writable_strategy,
                        ScriptValue raw_readable_strategy,
                        ExceptionState&);

  void Trace(Visitor*);

 private:
  friend class TransformStreamDefaultController;

  class ControllerInterface;
  class FlushAlgorithm;
  class TransformAlgorithm;
  class ReturnStartPromiseAlgorithm;
  class DefaultSinkWriteAlgorithm;
  class DefaultSinkAbortAlgorithm;
  class DefaultSinkCloseAlgorithm;
  class DefaultSourcePullAlgorithm;
  class DefaultSourceCancelAlgorithm;

  // Performs the functions performed by the constructor in the standard.
  // https://streams.spec.whatwg.org/#ts-constructor
  void InitInternal(ScriptState*,
                    ScriptValue raw_transformer,
                    ScriptValue raw_writable_strategy,
                    ScriptValue raw_readable_strategy,
                    ExceptionState&);

  // https://streams.spec.whatwg.org/#initialize-transform-stream
  static void Initialize(ScriptState*,
                         TransformStreamNative*,
                         StreamPromiseResolver* start_promise,
                         double writable_high_water_mark,
                         StrategySizeAlgorithm* writable_size_algorithm,
                         double readable_high_water_mark,
                         StrategySizeAlgorithm* readable_size_algorithm);

  // https://streams.spec.whatwg.org/#transform-stream-error
  static void Error(ScriptState*,
                    TransformStreamNative*,
                    v8::Local<v8::Value> e);

  // https://streams.spec.whatwg.org/#transform-stream-error-writable-and-unblock-write
  static void ErrorWritableAndUnblockWrite(ScriptState*,
                                           TransformStreamNative*,
                                           v8::Local<v8::Value> e);

  // https://streams.spec.whatwg.org/#transform-stream-set-backpressure
  static void SetBackpressure(ScriptState*,
                              TransformStreamNative*,
                              bool backpressure);

  // The [[backpressure]] internal slot from the standard is here called
  // |had_backpressure_| to conform to Blink style. The initial value is
  // *undefined* in the standard, but it is set to *true* by
  // InitializeTransformStream(), so that is the initial value used here.
  bool had_backpressure_ = true;
  Member<StreamPromiseResolver> backpressure_change_promise_;
  Member<ReadableStreamNative> readable_;
  Member<TransformStreamDefaultController> transform_stream_controller_;
  Member<WritableStreamNative> writable_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFORM_STREAM_NATIVE_H_

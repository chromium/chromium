// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_MISCELLANEOUS_OPERATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_MISCELLANEOUS_OPERATIONS_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/v8.h"

namespace blink {

// Operations from the "Miscellaneous Operations" section of the standard:
// https://streams.spec.whatwg.org/#misc-abstract-ops

class ExceptionState;
class ReadableStream;
class ScriptState;
class StrategySizeAlgorithm;
class StreamAlgorithm;
class StreamStartAlgorithm;
class WritableStream;

// This is slightly different than the version in the standard
// https://streams.spec.whatwg.org/#create-algorithm-from-underlying-method as
// it doesn't require the number of expected arguments, algoArgCount, to be
// specified. Also, it only supports 0 or 1 extraArgs, instead of an arbitrary
// number. Use an empty value for |extra_arg| to mean 0 extraArgs.
// |method_name_for_error| supplies a more helpful name for the method to be
// used in exception messages. For example, if |method_name| is "write",
// |method_name_for_error| might be "underlyingSink.write".
CORE_EXPORT StreamAlgorithm* CreateAlgorithmFromUnderlyingMethod(
    ScriptState*,
    v8::Local<v8::Object> underlying_object,
    const char* method_name,
    const char* method_name_for_error,
    v8::MaybeLocal<v8::Value> extra_arg,
    ExceptionState&);

// Looks up |method_name| on |object|. Will throw an exception if the lookup
// fails or if the resolved method is neither a function nor undefined.
// |name_for_error| is used for the name of the method in exception messages.
CORE_EXPORT v8::MaybeLocal<v8::Value> ResolveMethod(
    ScriptState*,
    v8::Local<v8::Object> object,
    const char* method_name,
    const char* name_for_error,
    ExceptionState&);

// This works like CreateAlgorithmFromUnderlyingMethod() but |method| must
// already have been resolved and verified to be a v8::Function.
CORE_EXPORT StreamAlgorithm* CreateAlgorithmFromResolvedMethod(
    ScriptState*,
    v8::Local<v8::Object> underlying_object,
    v8::Local<v8::Value> method,
    v8::MaybeLocal<v8::Value> extra_arg);

// Create a StreamStartAlgorithm from the "start" method on |underlying_object|.
// Unlike other algorithms, the lookup of the method on the object is done at
// execution time rather than algorithm creation time. |method_name_for_error|
// is used in exception messages. It is not copied so must remain valid until
// the algorithm is run.
CORE_EXPORT StreamStartAlgorithm* CreateStartAlgorithm(
    ScriptState*,
    v8::Local<v8::Object> underlying_object,
    const char* method_name_for_error,
    v8::Local<v8::Value> controller);

// Returns a startAlgorithm that always returns a promise resolved with
// undefined.
CORE_EXPORT StreamStartAlgorithm* CreateTrivialStartAlgorithm();

// Used in place of InvokeOrNoop in spec. Always takes 1 argument.
// https://streams.spec.whatwg.org/#invoke-or-noop
CORE_EXPORT v8::MaybeLocal<v8::Value> CallOrNoop1(ScriptState*,
                                                  v8::Local<v8::Object> object,
                                                  const char* method_name,
                                                  const char* name_for_error,
                                                  v8::Local<v8::Value> arg0,
                                                  ExceptionState&);

// https://streams.spec.whatwg.org/#promise-call
// "PromiseCall(F, V, args)"
// "F" is called |method| here
// "V" is called |recv| here
// "args" becomes |argc| and |argv| here.
CORE_EXPORT v8::Local<v8::Promise> PromiseCall(ScriptState*,
                                               v8::Local<v8::Function> method,
                                               v8::Local<v8::Object> recv,
                                               int argc,
                                               v8::Local<v8::Value> argv[]);

// Unlike in the standard, the caller needs to handle the conversion of the
// value to a Number.
// https://streams.spec.whatwg.org/#validate-and-normalize-high-water-mark
CORE_EXPORT double ValidateAndNormalizeHighWaterMark(double high_water_mark,
                                                     ExceptionState&);

// https://streams.spec.whatwg.org/#make-size-algorithm-from-size-function
CORE_EXPORT StrategySizeAlgorithm* MakeSizeAlgorithmFromSizeFunction(
    ScriptState*,
    v8::Local<v8::Value> size,
    ExceptionState&);

CORE_EXPORT StrategySizeAlgorithm* CreateDefaultSizeAlgorithm();

// Implements "a promise rejected with" from the INFRA standard.
// https://www.w3.org/2001/tag/doc/promises-guide/#a-promise-rejected-with
CORE_EXPORT v8::Local<v8::Promise> PromiseReject(ScriptState*,
                                                 v8::Local<v8::Value>);

// Implements "a promise resolved with" from the INFRA standard.
// https://www.w3.org/2001/tag/doc/promises-guide/#a-promise-resolved-with
CORE_EXPORT v8::Local<v8::Promise> PromiseResolve(ScriptState*,
                                                  v8::Local<v8::Value>);

// Implements "a promise resolved with *undefined*".
CORE_EXPORT v8::Local<v8::Promise> PromiseResolveWithUndefined(ScriptState*);

// Validates the "options" argument to ReadableStream::getReader(). This
// implementation is shared between ReadableStreamWrapper and
// ReadableStreamNative. If an exception is thrown validation failed.
// TODO(ricea): Move it into the native implementation once
// ReadableStreamWrapper is deleted.
CORE_EXPORT void GetReaderValidateOptions(ScriptState*,
                                          ScriptValue options,
                                          ExceptionState&);

// Extracts the "readable" and "writable" streams from the |transform_stream|
// dictionary, validates them, and returns them via the |readable_stream| and
// |writable_stream| out parameters. The types of |readable_stream| and
// |writable_stream| are asymmetric because |readable_stream| is returned
// directly to JavaScript and so there is no point in converting it to an
// internal type.
// TODO(ricea): Move it into the native implementation once
// ReadableStreamWrapper is deleted.
CORE_EXPORT void PipeThroughExtractReadableWritable(
    ScriptState*,
    const ReadableStream* stream,
    ScriptValue transform_stream,
    ScriptValue* readable_stream,
    WritableStream** writable_stream,
    ExceptionState&);

// Verifies that |destination_value| is a WritableStream and that both it and
// |source| are unlocked. Returns the WritableStream that was wrapped by
// |destination_value|.
CORE_EXPORT WritableStream* PipeToCheckSourceAndDestination(
    ScriptState*,
    ReadableStream* source,
    ScriptValue destination_value,
    ExceptionState&);

// Calls Tee() on |readable|, converts the two branches to a JavaScript array
// and returns them.
// TODO(ricea): Move it into the native implementation once
// ReadableStreamWrapper is deleted.
ScriptValue CallTeeAndReturnBranchArray(ScriptState* script_state,
                                        ReadableStream* readable,
                                        ExceptionState& exception_state);

// Converts |value| to an object. |value| must not be empty. If |value| is
// undefined, an empty object will be returned. If |value| is JavaScript null,
// then an exception will be thrown. In the standard, this is performed as part
// of the GetV() operation, but in this implementation we do it explicitly
// before looking up any properties.
void ScriptValueToObject(ScriptState* script_state,
                         ScriptValue value,
                         v8::Local<v8::Object>* object,
                         ExceptionState& exception_state);

// This class is used for unpacking strategies in the constructors of
// ReadableStream, WritableStream and TransformStream. For example, steps 2.,
// 3., 6., 7. and 8. of https://streams.spec.whatwg.org/#ws-constructor.
class StrategyUnpacker final {
  STACK_ALLOCATED();

 public:
  // Looks up the "size" and "highWaterMark" properties on |strategy|. May run
  // arbitrary user code. The object cannot be used if
  // exception_state.HadException() is true.
  StrategyUnpacker(ScriptState*, ScriptValue strategy, ExceptionState&);
  ~StrategyUnpacker() = default;

  // Performs MakeSizeAlgorithmFromSizeFunction on |size_|. Because this method
  // can throw an exception, the timing when it is called is observable.
  StrategySizeAlgorithm* MakeSizeAlgorithm(ScriptState*, ExceptionState&) const;

  // If |high_water_mark_| is defined, converts it to a number and call
  // ValidateAndNormalizeHighWaterMark on it. Otherwise returns |default_value|.
  // May run arbitrary user code.
  double GetHighWaterMark(ScriptState*,
                          int default_value,
                          ExceptionState&) const;

 private:
  v8::Local<v8::Value> size_;
  v8::Local<v8::Value> high_water_mark_;

  DISALLOW_COPY_AND_ASSIGN(StrategyUnpacker);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_MISCELLANEOUS_OPERATIONS_H_

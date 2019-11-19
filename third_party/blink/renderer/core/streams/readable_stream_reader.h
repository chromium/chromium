// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_READER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class ScriptPromise;
class ScriptState;
class ReadableStream;
class ReadableStreamNative;
class StreamPromiseResolver;
class Visitor;

// The specification sometimes treats ReadableStreamDefaultReader and
// ReadableStreamBYOBReader generically. Currently ReadableStreamBYOBReader
// isn't implemented in Blink. In order to make the generic operations align
// with the standard, ReadableStreamDefaultReader is implemented by the
// ReadableStreamReader class.
// TODO(ricea): Refactor this when implementing ReadableStreamBYOBReader.
class CORE_EXPORT ReadableStreamReader : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ReadableStreamReader* Create(ScriptState*,
                                      ReadableStream* stream,
                                      ExceptionState&);

  // https://streams.spec.whatwg.org/#default-reader-constructor
  ReadableStreamReader(ScriptState*,
                       ReadableStreamNative* stream,
                       ExceptionState&);
  ~ReadableStreamReader() override;

  // https://streams.spec.whatwg.org/#default-reader-closed
  ScriptPromise closed(ScriptState*) const;

  // https://streams.spec.whatwg.org/#default-reader-cancel
  ScriptPromise cancel(ScriptState*);
  ScriptPromise cancel(ScriptState*, ScriptValue reason);

  // https://streams.spec.whatwg.org/#default-reader-read
  ScriptPromise read(ScriptState*);

  // https://streams.spec.whatwg.org/#default-reader-release-lock
  void releaseLock(ScriptState*, ExceptionState&);

  //
  // Readable stream reader abstract operations
  //

  // https://streams.spec.whatwg.org/#readable-stream-default-reader-read
  static StreamPromiseResolver* Read(ScriptState* script_state,
                                     ReadableStreamReader* reader);

  // https://streams.spec.whatwg.org/#readable-stream-reader-generic-release
  static void GenericRelease(ScriptState*, ReadableStreamReader*);

  StreamPromiseResolver* ClosedPromise() { return closed_promise_; }

  void Trace(Visitor*) override;

 private:
  friend class ReadableStreamDefaultController;
  friend class ReadableStreamNative;

  // https://streams.spec.whatwg.org/#readable-stream-reader-generic-cancel
  static v8::Local<v8::Promise> GenericCancel(ScriptState*,
                                              ReadableStreamReader*,
                                              v8::Local<v8::Value> reason);

  // https://streams.spec.whatwg.org/#readable-stream-reader-generic-initialize
  static void GenericInitialize(ScriptState*,
                                ReadableStreamReader*,
                                ReadableStreamNative*);

  Member<StreamPromiseResolver> closed_promise_;
  bool for_author_code_ = true;
  Member<ReadableStreamNative> owner_readable_stream_;
  HeapDeque<Member<StreamPromiseResolver>> read_requests_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_READER_H_

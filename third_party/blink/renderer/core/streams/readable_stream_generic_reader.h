// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_GENERIC_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_GENERIC_READER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class ReadableStream;
class ScriptPromise;
class ScriptState;
class StreamPromiseResolver;
class Visitor;

// The specification sometimes treats ReadableStreamDefaultReader and
// ReadableStreamBYOBReader generically. Currently ReadableStreamBYOBReader
// isn't implemented in Blink. In order to make the generic operations align
// with the standard, ReadableStreamDefaultReader is implemented by the
// ReadableStreamGenericReader class.
// TODO(ricea): Refactor this when implementing ReadableStreamBYOBReader.
class CORE_EXPORT ReadableStreamGenericReader : public ScriptWrappable {

 public:
  ReadableStreamGenericReader();
  ~ReadableStreamGenericReader() override;

  // https://streams.spec.whatwg.org/#generic-reader-closed
  ScriptPromise closed(ScriptState*) const;

  // https://streams.spec.whatwg.org/#generic-reader-cancel
  ScriptPromise cancel(ScriptState*, ExceptionState&);
  ScriptPromise cancel(ScriptState*, ScriptValue reason, ExceptionState&);

  //
  // Readable stream reader abstract operations
  //

  // https://streams.spec.whatwg.org/#readable-stream-reader-generic-release
  static void GenericRelease(ScriptState*, ReadableStreamGenericReader*);

  // https://streams.spec.whatwg.org/#readable-stream-reader-generic-cancel
  static v8::Local<v8::Promise> GenericCancel(ScriptState*,
                                              ReadableStreamGenericReader*,
                                              v8::Local<v8::Value> reason);

  // https://streams.spec.whatwg.org/#readable-stream-reader-generic-initialize
  static void GenericInitialize(ScriptState*,
                                ReadableStreamGenericReader*,
                                ReadableStream*);

  StreamPromiseResolver* ClosedPromise() const { return closed_promise_; }

  void Trace(Visitor*) const override;

 private:
  friend class ReadableStreamDefaultController;
  friend class ReadableStream;

  Member<StreamPromiseResolver> closed_promise_;

 protected:
  Member<ReadableStream> owner_readable_stream_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_GENERIC_READER_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_DEFAULT_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_DEFAULT_READER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/streams/readable_stream_generic_reader.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;
class ReadableStream;
class ScriptPromise;
class ScriptState;
class StreamPromiseResolver;

class CORE_EXPORT ReadableStreamDefaultReader
    : public ReadableStreamGenericReader {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ReadableStreamDefaultReader* Create(ScriptState*,
                                             ReadableStream* stream,
                                             ExceptionState&);

  // https://streams.spec.whatwg.org/#default-reader-constructor
  ReadableStreamDefaultReader(ScriptState*,
                              ReadableStream* stream,
                              ExceptionState&);
  ~ReadableStreamDefaultReader() override;

  // https://streams.spec.whatwg.org/#default-reader-read
  ScriptPromise read(ScriptState*, ExceptionState&);

  // https://streams.spec.whatwg.org/#default-reader-release-lock
  void releaseLock(ScriptState*, ExceptionState&);

  static void SetUpDefaultReader(ScriptState*,
                                 ReadableStreamDefaultReader* reader,
                                 ReadableStream* stream,
                                 ExceptionState&);

  //
  // Readable stream reader abstract operations
  //

  // https://streams.spec.whatwg.org/#readable-stream-default-reader-read
  static StreamPromiseResolver* Read(ScriptState*,
                                     ReadableStreamDefaultReader* reader);

  void Trace(Visitor*) const override;

 private:
  friend class ReadableStreamDefaultController;
  friend class ReadableStream;

  HeapDeque<Member<StreamPromiseResolver>> read_requests_;
  bool for_author_code_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_DEFAULT_READER_H_

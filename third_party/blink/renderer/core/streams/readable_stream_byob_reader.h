// Copyright 2020 The Chromium AUthors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_BYOB_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_BYOB_READER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;
class ScriptPromise;
class ScriptState;
class ReadableStream;
class DOMArrayBufferView;

class CORE_EXPORT ReadableStreamBYOBReader : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ReadableStreamBYOBReader* Create(ScriptState*,
                                          ReadableStream* stream,
                                          ExceptionState&);

  // https://streams.spec.whatwg.org/#byob-reader-constructor
  ReadableStreamBYOBReader(ScriptState*,
                           ReadableStream* stream,
                           ExceptionState&);
  ~ReadableStreamBYOBReader() override;

  // https://streams.spec.whatwg.org/#byob-reader-read
  ScriptPromise read(ScriptState*,
                     NotShared<DOMArrayBufferView> view,
                     ExceptionState&);

  // https://streams.spec.whatwg.org/#byob-reader-release-lock
  void releaseLock(ScriptState*, ExceptionState&);

  // https://streams.spec.whatwg.org/#generic-reader-closed
  ScriptPromise closed(ScriptState*) const;

  // https://streams.spec.whatwg.org/#generic-reader-cancel
  ScriptPromise cancel(ScriptState*, ExceptionState&);
  ScriptPromise cancel(ScriptState*, ScriptValue reason, ExceptionState&);

 private:
  friend class ReadableStream;
  static void ThrowUnimplemented(ExceptionState&);
  static ScriptPromise RejectUnimplemented(ScriptState*);
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_BYOB_READER_H_

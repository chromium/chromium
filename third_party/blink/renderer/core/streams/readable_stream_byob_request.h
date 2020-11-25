// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_BYOB_REQUEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_BYOB_REQUEST_H_

#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;
class ScriptState;
class DOMArrayBufferView;

class ReadableStreamBYOBRequest : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // https://streams.spec.whatwg.org/#rs-byob-request-view
  NotShared<DOMArrayBufferView> view(ExceptionState&) const;

  // https://streams.spec.whatwg.org/#rs-byob-request-respond
  void respond(ScriptState*, uint64_t bytesWritten, ExceptionState&);

  // https://streams.spec.whatwg.org/#rs-byob-request-respond-with-new-view
  void respondWithNewView(ScriptState*,
                          NotShared<DOMArrayBufferView> view,
                          ExceptionState&);

 private:
  static void ThrowUnimplemented(ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_BYOB_REQUEST_H_

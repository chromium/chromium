// Copyright 2020 The Chromium AUthors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_BYTE_STREAM_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_BYTE_STREAM_CONTROLLER_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/streams/readable_stream_controller.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "v8/include/v8.h"

namespace blink {

class DOMArrayBufferView;
class ExceptionState;
class ReadableStreamBYOBRequest;
class ScriptState;
class StreamPromiseResolver;

class ReadableByteStreamController : public ReadableStreamController {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // https://streams.spec.whatwg.org/#rbs-controller-byob-request
  ReadableStreamBYOBRequest* byobRequest(ExceptionState&) const;

  // https://streams.spec.whatwg.org/#rbs-controller-desired-size
  base::Optional<double> desiredSize(ExceptionState&) const;

  // https://streams.spec.whatwg.org/#rbs-controller-close
  void close(ScriptState*, ExceptionState&);

  // https://streams.spec.whatwg.org/#rbs-controller-enqueue
  void enqueue(ScriptState*,
               NotShared<DOMArrayBufferView> chunk,
               ExceptionState&);

  // https://streams.spec.whatwg.org/#rbs-controller-error
  void error(ScriptState*, ExceptionState&);
  void error(ScriptState*, ScriptValue e, ExceptionState&);

  // https://streams.spec.whatwg.org/#rbs-controller-private-cancel
  v8::Local<v8::Promise> CancelSteps(ScriptState*,
                                     v8::Local<v8::Value> reason) override;

  // https://streams.spec.whatwg.org/#rbs-controller-private-pull
  StreamPromiseResolver* PullSteps(ScriptState*) override;

 private:
  static void ThrowUnimplemented(ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_BYTE_STREAM_CONTROLLER_H_

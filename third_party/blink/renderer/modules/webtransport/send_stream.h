// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_SEND_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_SEND_STREAM_H_

#include <stdint.h>

#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webtransport/outgoing_stream.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_stream.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class ExceptionState;
class WebTransport;
class ScriptState;

class MODULES_EXPORT SendStream final : public WritableStream,
                                        public WebTransportStream,
                                        public OutgoingStream::Client {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // SendStream doesn't have a JavaScript constructor. It is only constructed
  // from C++.
  explicit SendStream(ScriptState*,
                      WebTransport*,
                      uint32_t stream_id,
                      mojo::ScopedDataPipeProducerHandle);
  ~SendStream() override;

  void Init(ExceptionState& exception_state) {
    outgoing_stream_->InitWithExistingWritableStream(this, exception_state);
  }

  // Methods for backwards compatibility.
  // TODO(ricea): Remove them when they have been removed from the IDL file.
  SendStream* writable() { return this; }

  ScriptPromise writingAborted() const {
    return outgoing_stream_->WritingAborted();
  }

  void abortWriting(StreamAbortInfo* abort_info) {
    outgoing_stream_->AbortWriting(abort_info);
  }

  // Implementation of WebTransportStream.
  void OnIncomingStreamClosed(bool fin_received) override;
  void Reset() override;
  void ContextDestroyed() override;

  // Implementation of OutgoingStream::Client
  void SendFin() override;
  void OnOutgoingStreamAbort() override;

  void Trace(Visitor*) const override;

 private:
  const Member<OutgoingStream> outgoing_stream_;
  const Member<WebTransport> web_transport_;
  const uint32_t stream_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_SEND_STREAM_H_

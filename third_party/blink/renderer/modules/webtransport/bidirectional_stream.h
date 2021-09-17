// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_BIDIRECTIONAL_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_BIDIRECTIONAL_STREAM_H_

#include <stdint.h>

#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webtransport/incoming_stream.h"
#include "third_party/blink/renderer/modules/webtransport/outgoing_stream.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_stream.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ScriptState;
class WebTransport;

class MODULES_EXPORT BidirectionalStream final : public ScriptWrappable,
                                                 public WebTransportStream,
                                                 public OutgoingStream::Client {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // BidirectionalStream doesn't have a JavaScript constructor. It is only
  // constructed from C++.
  explicit BidirectionalStream(ScriptState*,
                               WebTransport*,
                               uint32_t stream_id,
                               mojo::ScopedDataPipeProducerHandle,
                               mojo::ScopedDataPipeConsumerHandle);

  void Init(ExceptionState&);

  // Implementation of bidirectional_stream.idl. As noted in the IDL file, these
  // properties are implemented on OutgoingStream and IncomingStream in the
  // standard.
  WritableStream* writable() const { return outgoing_stream_->Writable(); }

  ScriptPromise writingAborted() const {
    return outgoing_stream_->WritingAborted();
  }

  void abortWriting(StreamAbortInfo* abort_info) {
    outgoing_stream_->AbortWriting(abort_info);
  }

  ReadableStream* readable() const { return incoming_stream_->Readable(); }

  ScriptPromise readingAborted() const {
    return incoming_stream_->ReadingAborted();
  }

  void abortReading(StreamAbortInfo* info) {
    incoming_stream_->AbortReading(info);
  }

  // Implementation of WebTransportStream.
  void OnIncomingStreamClosed(bool fin_received) override;
  void Reset() override;
  void ContextDestroyed() override;

  // Implementation of OutgoingStream::Client.
  void SendFin() override;
  void OnOutgoingStreamAbort() override;

  void Trace(Visitor*) const override;

 private:
  void OnIncomingStreamAbort();

  const Member<OutgoingStream> outgoing_stream_;
  const Member<IncomingStream> incoming_stream_;
  const Member<WebTransport> web_transport_;
  const uint32_t stream_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_BIDIRECTIONAL_STREAM_H_

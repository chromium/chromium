// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_BIDIRECTIONAL_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_BIDIRECTIONAL_STREAM_H_

#include <stdint.h>

#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class IncomingStream;
class OutgoingStream;
class ScriptState;
class WebTransport;

// https://w3c.github.io/webtransport/#bidirectional-stream
class MODULES_EXPORT BidirectionalStream final : public ScriptWrappable {
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

  // Implementation of web_transport_bidirectional_stream.idl.
  WritableStream* writable() const { return send_stream_.Get(); }

  ReadableStream* readable() const { return receive_stream_.Get(); }

  OutgoingStream* GetOutgoingStream();
  IncomingStream* GetIncomingStream();

  void Trace(Visitor*) const override;

 private:
  // send_stream_ is either a SendStream or a WebTransportSendStream depending
  // on whether the WebTransportSendGroup runtime flag is enabled.
  // TODO(crbug.com/487117768): Remove old SendStream path when
  // WebTransportSendGroup ships.
  Member<WritableStream> send_stream_;
  // receive_stream_ is either a ReceiveStream or a WebTransportReceiveStream
  // depending on whether the WebTransportReceiveStream runtime flag is
  // enabled.
  // TODO(crbug.com/510589920): Remove old ReceiveStream path when
  // WebTransportReceiveStream ships.
  Member<ReadableStream> receive_stream_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_BIDIRECTIONAL_STREAM_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_RECEIVE_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_RECEIVE_STREAM_H_

#include <stdint.h>

#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webtransport/incoming_stream.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class ExceptionState;
class ScriptState;
class WebTransport;
class WebTransportReceiveStreamStats;

// Implements the WebTransportReceiveStream interface.
// https://w3c.github.io/webtransport/#webtransportreceivestream
//
// WebTransportReceiveStream is a ReadableStream subclass that adds a getStats()
// method. When the WebTransportReceiveStream runtime flag is enabled,
// ReceiveStreamVendor (incoming unidirectional streams) and BidirectionalStream
// (incoming half of a bidi stream) construct WebTransportReceiveStream objects
// instead of the legacy ReceiveStream.
class MODULES_EXPORT WebTransportReceiveStream final : public ReadableStream {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // WebTransportReceiveStream doesn't have a JavaScript constructor. It is
  // only constructed from C++.
  WebTransportReceiveStream(ScriptState*,
                            WebTransport*,
                            uint32_t stream_id,
                            mojo::ScopedDataPipeConsumerHandle);
  ~WebTransportReceiveStream() override;

  void Init(ExceptionState& exception_state) {
    incoming_stream_->InitWithExistingReadableStream(this, exception_state);
  }

  IncomingStream* GetIncomingStream() { return incoming_stream_.Get(); }

  // IDL method
  ScriptPromise<WebTransportReceiveStreamStats> getStats(ScriptState*);

  void Trace(Visitor*) const override;

 private:
  const Member<IncomingStream> incoming_stream_;
};

template <>
struct DowncastTraits<WebTransportReceiveStream> {
  static bool AllowFrom(const ReadableStream& stream) {
    return stream.GetWrapperTypeInfo() ==
           WebTransportReceiveStream::GetStaticWrapperTypeInfo();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_RECEIVE_STREAM_H_

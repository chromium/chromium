// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_SEND_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_SEND_STREAM_H_

#include <stdint.h>

#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webtransport/outgoing_stream.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class ExceptionState;
class ScriptState;
class WebTransport;
class WebTransportSendGroup;
class WebTransportSendStreamStats;

// Implements the WebTransportSendStream interface.
// https://w3c.github.io/webtransport/#webtransportsendstream
//
// WebTransportSendStream is a WritableStream subclass that adds sendGroup and
// sendOrder attributes for stream prioritization. When the
// WebTransportSendGroup runtime flag is enabled, createUnidirectionalStream()
// and createBidirectionalStream() return WebTransportSendStream objects instead
// of plain WritableStream/SendStream.
class MODULES_EXPORT WebTransportSendStream final : public WritableStream {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // WebTransportSendStream doesn't have a JavaScript constructor. It is only
  // constructed from C++.
  WebTransportSendStream(ScriptState*,
                         WebTransport*,
                         uint32_t stream_id,
                         mojo::ScopedDataPipeProducerHandle);
  ~WebTransportSendStream() override;

  void Init(ExceptionState& exception_state) {
    outgoing_stream_->InitWithExistingWritableStream(this, exception_state);
  }

  OutgoingStream* GetOutgoingStream() { return outgoing_stream_.Get(); }

  // Returns the WebTransport that owns this stream.
  WebTransport* GetTransport() const { return transport_.Get(); }

  // IDL attributes
  WebTransportSendGroup* sendGroup() const { return send_group_.Get(); }
  void setSendGroup(WebTransportSendGroup*, ExceptionState&);
  int64_t sendOrder() const { return send_order_; }
  void setSendOrder(int64_t order);

  // Applies sendGroup and sendOrder from stream-creation options. Checks for
  // exception after setSendGroup() before proceeding to setSendOrder().
  void ApplySendStreamOptions(WebTransportSendGroup* send_group,
                              int64_t send_order,
                              ExceptionState& exception_state);

  // IDL method
  ScriptPromise<WebTransportSendStreamStats> getStats(ScriptState*);

  void Trace(Visitor*) const override;

 private:
  const Member<WebTransport> transport_;
  const Member<OutgoingStream> outgoing_stream_;
  Member<WebTransportSendGroup> send_group_;  // nullable, default null
  int64_t send_order_ = 0;
};

template <>
struct DowncastTraits<WebTransportSendStream> {
  static bool AllowFrom(const WritableStream& stream) {
    return stream.GetWrapperTypeInfo() ==
           WebTransportSendStream::GetStaticWrapperTypeInfo();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_SEND_STREAM_H_

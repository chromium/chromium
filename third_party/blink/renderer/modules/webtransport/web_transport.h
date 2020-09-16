// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_H_

#include "base/util/type_safety/pass_key.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webtransport/quic_transport.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ScriptState;

class MODULES_EXPORT WebTransport final
    : public ScriptWrappable,
      public ActiveScriptWrappable<WebTransport> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using PassKey = util::PassKey<WebTransport>;
  static WebTransport* Create(ScriptState*,
                              const String& url,
                              QuicTransportOptions*,
                              ExceptionState&);

  WebTransport(PassKey, QuicTransport*);
  ~WebTransport() override;

  // WebTransport IDL implementation.
  ScriptPromise createSendStream(ScriptState* script_state,
                                 ExceptionState& exception_state) {
    return quic_transport_->createSendStream(script_state, exception_state);
  }
  ReadableStream* receiveStreams() { return quic_transport_->receiveStreams(); }

  ScriptPromise createBidirectionalStream(ScriptState* script_state,
                                          ExceptionState& exception_state) {
    return quic_transport_->createBidirectionalStream(script_state,
                                                      exception_state);
  }
  ReadableStream* receiveBidirectionalStreams() {
    return quic_transport_->receiveBidirectionalStreams();
  }
  WritableStream* sendDatagrams() { return quic_transport_->sendDatagrams(); }
  ReadableStream* receiveDatagrams() {
    return quic_transport_->receiveDatagrams();
  }
  void close(const WebTransportCloseInfo* close_info) {
    quic_transport_->close(close_info);
  }
  ScriptPromise ready() { return quic_transport_->ready(); }
  ScriptPromise closed() { return quic_transport_->closed(); }

  bool HasPendingActivity() const override {
    return quic_transport_->HasPendingActivity();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(quic_transport_);
    ScriptWrappable::Trace(visitor);
  }

  ExecutionContext* GetExecutionContext() const {
    return quic_transport_->GetExecutionContext();
  }

 private:
  const Member<QuicTransport> quic_transport_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_H_

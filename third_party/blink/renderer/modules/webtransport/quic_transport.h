// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_QUIC_TRANSPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_QUIC_TRANSPORT_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "base/util/type_safety/pass_key.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/quic_transport.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ExceptionState;
class QuicTransportOptions;
class ReadableStream;
class ReadableStreamDefaultControllerWithScriptScope;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptPromiseResolver;
class ScriptState;
class WebTransportCloseInfo;
class WebTransportStream;
class WritableStream;

// https://wicg.github.io/web-transport/#quic-transport
class MODULES_EXPORT QuicTransport final
    : public ScriptWrappable,
      public ActiveScriptWrappable<QuicTransport>,
      public ExecutionContextLifecycleObserver,
      public network::mojom::blink::QuicTransportHandshakeClient,
      public network::mojom::blink::QuicTransportClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(QuicTransport, Dispose);

 public:
  using PassKey = util::PassKey<QuicTransport>;
  static QuicTransport* Create(ScriptState*,
                               const String& url,
                               QuicTransportOptions*,
                               ExceptionState&);

  QuicTransport(PassKey, ScriptState*, const String& url);
  ~QuicTransport() override;

  // QuicTransport IDL implementation.
  ScriptPromise createSendStream(ScriptState*, ExceptionState&);
  ReadableStream* receiveStreams();

  ScriptPromise createBidirectionalStream(ScriptState*, ExceptionState&);
  ReadableStream* receiveBidirectionalStreams();

  WritableStream* sendDatagrams();
  ReadableStream* receiveDatagrams();
  void close(const WebTransportCloseInfo*);
  ScriptPromise ready() { return ready_; }
  ScriptPromise closed() { return closed_; }

  // QuicTransportHandshakeClient implementation
  void OnConnectionEstablished(
      mojo::PendingRemote<network::mojom::blink::QuicTransport>,
      mojo::PendingReceiver<network::mojom::blink::QuicTransportClient>)
      override;
  void OnHandshakeFailed(network::mojom::blink::QuicTransportErrorPtr) override;

  // QuicTransportClient implementation
  void OnDatagramReceived(base::span<const uint8_t> data) override;
  void OnIncomingStreamClosed(uint32_t stream_id, bool fin_received) override;

  // Implementation of ExecutionContextLifecycleObserver
  void ContextDestroyed() final;

  // Implementation of ActiveScriptWrappable
  bool HasPendingActivity() const final;

  // Forwards a SendFin() message to the mojo interface.
  void SendFin(uint32_t stream_id);

  // Forwards a AbortStream() message to the mojo interface.
  void AbortStream(uint32_t stream_id);

  // Removes the reference to a stream.
  void ForgetStream(uint32_t stream_id);

  // ScriptWrappable implementation
  void Trace(Visitor* visitor) const override;

 private:
  class DatagramUnderlyingSink;
  class DatagramUnderlyingSource;
  class StreamVendingUnderlyingSource;
  class ReceiveStreamVendor;
  class BidirectionalStreamVendor;

  QuicTransport(ScriptState*, const String& url, ExecutionContext* context);

  void Init(const String& url, const QuicTransportOptions&, ExceptionState&);

  // Reset the QuicTransport object and all associated streams.
  void ResetAll();

  void Dispose();
  void OnConnectionError();
  void RejectPendingStreamResolvers();
  void OnCreateSendStreamResponse(ScriptPromiseResolver*,
                                  mojo::ScopedDataPipeProducerHandle,
                                  bool succeeded,
                                  uint32_t stream_id);
  void OnCreateBidirectionalStreamResponse(ScriptPromiseResolver*,
                                           mojo::ScopedDataPipeProducerHandle,
                                           mojo::ScopedDataPipeConsumerHandle,
                                           bool succeeded,
                                           uint32_t stream_id);

  bool cleanly_closed_ = false;
  Member<ReadableStream> received_datagrams_;
  Member<ReadableStreamDefaultControllerWithScriptScope>
      received_datagrams_controller_;

  // This corresponds to the [[SentDatagrams]] internal slot in the standard.
  Member<WritableStream> outgoing_datagrams_;

  const Member<ScriptState> script_state_;

  const KURL url_;

  // Map from stream_id to SendStream, ReceiveStream or BidirectionalStream.
  // Intentionally keeps streams reachable by GC as long as they are open.
  // This doesn't support stream ids of 0xfffffffe or larger.
  // TODO(ricea): Find out if such large stream ids are possible.
  HeapHashMap<uint32_t,
              Member<WebTransportStream>,
              WTF::DefaultHash<uint32_t>::Hash,
              WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>
      stream_map_;

  HeapMojoRemote<network::mojom::blink::QuicTransport> quic_transport_;
  HeapMojoReceiver<network::mojom::blink::QuicTransportHandshakeClient,
                   QuicTransport>
      handshake_client_receiver_;
  HeapMojoReceiver<network::mojom::blink::QuicTransportClient, QuicTransport>
      client_receiver_;
  Member<ScriptPromiseResolver> ready_resolver_;
  ScriptPromise ready_;
  Member<ScriptPromiseResolver> closed_resolver_;
  ScriptPromise closed_;

  // Tracks resolvers for in-progress createSendStream() and
  // createBidirectionalStream() operations so they can be rejected.
  HeapHashSet<Member<ScriptPromiseResolver>> create_stream_resolvers_;

  // The [[ReceivedStreams]] slot.
  // https://wicg.github.io/web-transport/#dom-quictransport-receivedstreams-slot
  Member<ReadableStream> received_streams_;
  Member<StreamVendingUnderlyingSource> received_streams_underlying_source_;

  Member<ReadableStream> received_bidirectional_streams_;
  Member<StreamVendingUnderlyingSource>
      received_bidirectional_streams_underlying_source_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_QUIC_TRANSPORT_H_

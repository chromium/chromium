// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_TCP_SOCKET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_TCP_SOCKET_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/tcp_socket.mojom-blink.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/direct_sockets/tcp_readable_stream_wrapper.h"
#include "third_party/blink/renderer/modules/direct_sockets/tcp_writable_stream_wrapper.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"

namespace net {
class IPEndPoint;
}  // namespace net

namespace blink {

class MODULES_EXPORT TCPSocket final
    : public ScriptWrappable,
      public network::mojom::blink::SocketObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit TCPSocket(ScriptPromiseResolver&);
  ~TCPSocket() override;

  TCPSocket(const TCPSocket&) = delete;
  TCPSocket& operator=(const TCPSocket&) = delete;

  // Called by NavigatorSocket when initiating a connection:
  mojo::PendingReceiver<network::mojom::blink::TCPConnectedSocket>
  GetTCPSocketReceiver();
  mojo::PendingRemote<network::mojom::blink::SocketObserver>
  GetTCPSocketObserver();
  void Init(int32_t result,
            const absl::optional<net::IPEndPoint>& local_addr,
            const absl::optional<net::IPEndPoint>& peer_addr,
            mojo::ScopedDataPipeConsumerHandle receive_stream,
            mojo::ScopedDataPipeProducerHandle send_stream);

  // Web-exposed function
  ScriptPromise close(ScriptState*, ExceptionState&);

  ReadableStream* readable() const;
  WritableStream* writable() const;
  String remoteAddress() const;
  uint16_t remotePort() const;

  // network::mojom::blink::SocketObserver:
  void OnReadError(int32_t net_error) override;
  void OnWriteError(int32_t net_error) override;

  // ScriptWrappable:
  void Trace(Visitor* visitor) const override;

 private:
  void OnSocketObserverConnectionError();

  void OnReadableStreamAbort();
  void OnWritableStreamAbort();

  void DoClose(bool is_local_close);
  void ResetReadableStream();
  void ResetWritableStream();

  Member<ScriptPromiseResolver> resolver_;
  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;

  mojo::Remote<network::mojom::blink::TCPConnectedSocket> tcp_socket_;
  mojo::Receiver<network::mojom::blink::SocketObserver>
      socket_observer_receiver_{this};

  Member<TCPReadableStreamWrapper> tcp_readable_stream_wrapper_;
  Member<TCPWritableStreamWrapper> tcp_writable_stream_wrapper_;
  absl::optional<net::IPEndPoint> local_addr_;
  absl::optional<net::IPEndPoint> peer_addr_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_TCP_SOCKET_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_UDP_SOCKET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_UDP_SOCKET_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/udp_socket.mojom-blink.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"

namespace net {
class IPEndPoint;
}  // namespace net

namespace blink {

class MODULES_EXPORT UDPSocket final
    : public ScriptWrappable,
      public network::mojom::blink::UDPSocketListener {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit UDPSocket(ScriptPromiseResolver&);
  ~UDPSocket() override;

  UDPSocket(const UDPSocket&) = delete;
  UDPSocket& operator=(const UDPSocket&) = delete;

  // Called by NavigatorSocket when initiating a connection:
  mojo::PendingReceiver<blink::mojom::blink::DirectUDPSocket>
  GetUDPSocketReceiver();
  mojo::PendingRemote<network::mojom::blink::UDPSocketListener>
  GetUDPSocketListener();
  void Init(int32_t result,
            const absl::optional<net::IPEndPoint>& local_addr,
            const absl::optional<net::IPEndPoint>& peer_addr);

  // Web-exposed function
  ScriptPromise close(ScriptState*, ExceptionState&);

  // network::mojom::blink::UDPSocketListener:
  void OnReceived(int32_t result,
                  const absl::optional<::net::IPEndPoint>& src_addr,
                  absl::optional<::base::span<const ::uint8_t>> data) override;

  // ScriptWrappable:
  void Trace(Visitor* visitor) const override;

 private:
  void OnSocketListenerConnectionError();

  Member<ScriptPromiseResolver> resolver_;
  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;

  mojo::Remote<blink::mojom::blink::DirectUDPSocket> udp_socket_;
  mojo::Receiver<network::mojom::blink::UDPSocketListener>
      socket_listener_receiver_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_UDP_SOCKET_H_

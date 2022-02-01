// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_UDP_SOCKET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_UDP_SOCKET_H_

#include "base/callback_forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/udp_socket.mojom-blink.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_readable_stream_wrapper.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_socket_mojo_remote.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_writable_stream_wrapper.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"

namespace net {
class IPEndPoint;
}  // namespace net

namespace blink {

class ExecutionContext;

class MODULES_EXPORT UDPSocket final
    : public ScriptWrappable,
      public ActiveScriptWrappable<UDPSocket>,
      public ExecutionContextClient,
      public network::mojom::blink::UDPSocketListener {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit UDPSocket(ExecutionContext* execution_context,
                     ScriptPromiseResolver&);
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

  // Web-exposed functions
  ScriptPromise close(ScriptState* script_state,
                      ExceptionState& exception_state);

  ReadableStream* readable() const;
  WritableStream* writable() const;
  String remoteAddress() const;
  uint16_t remotePort() const;
  uint16_t localPort() const;

  // network::mojom::blink::UDPSocketListener:
  void OnReceived(int32_t result,
                  const absl::optional<::net::IPEndPoint>& src_addr,
                  absl::optional<::base::span<const ::uint8_t>> data) override;

  // ActiveScriptWrappable overrides.
  bool HasPendingActivity() const override;
  void Trace(Visitor* visitor) const override;

 private:
  void OnSocketListenerConnectionError();
  void DoClose();
  void CloseInternal();

  Member<ScriptPromiseResolver> init_resolver_;
  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;

  const Member<UDPSocketMojoRemote> udp_socket_;
  HeapMojoReceiver<network::mojom::blink::UDPSocketListener, UDPSocket>
      socket_listener_receiver_;

  Member<UDPReadableStreamWrapper> udp_readable_stream_wrapper_;
  Member<UDPWritableStreamWrapper> udp_writable_stream_wrapper_;
  absl::optional<net::IPEndPoint> local_addr_;
  absl::optional<net::IPEndPoint> peer_addr_;

  bool closed_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_UDP_SOCKET_H_

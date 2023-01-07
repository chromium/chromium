// Copyright 2020 The Chromium Authors
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
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/direct_sockets/direct_sockets_service_mojo_remote.h"
#include "third_party/blink/renderer/modules/direct_sockets/socket.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_readable_stream_wrapper.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_socket_mojo_remote.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_writable_stream_wrapper.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"

namespace net {
class IPEndPoint;
}  // namespace net

namespace blink {

class UDPSocketOptions;
class ScriptState;
class SocketCloseOptions;

// UDPSocket interface from udp_socket.idl
class MODULES_EXPORT UDPSocket final
    : public ScriptWrappable,
      public Socket,
      public ActiveScriptWrappable<UDPSocket>,
      public network::mojom::blink::UDPSocketListener {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // IDL definitions
  static UDPSocket* Create(ScriptState*,
                           const UDPSocketOptions*,
                           ExceptionState&);

 public:
  explicit UDPSocket(ScriptState*);
  ~UDPSocket() override;

  // Validates options and calls
  // DirectSocketsServiceMojoRemote::OpenUdpSocket(...) with Init(...) passed as
  // callback.
  bool Open(const UDPSocketOptions*, ExceptionState&);

  // On net::OK initializes readable/writable streams and resolves opened
  // promise. Otherwise rejects the opened promise. Serves as callback for
  // Open(...).
  void Init(int32_t result,
            const absl::optional<net::IPEndPoint>& local_addr,
            const absl::optional<net::IPEndPoint>& peer_addr);

  void Trace(Visitor*) const override;

  // ActiveScriptWrappable:
  bool HasPendingActivity() const override;

 private:
  mojo::PendingReceiver<blink::mojom::blink::DirectUDPSocket>
  GetUDPSocketReceiver();
  mojo::PendingRemote<network::mojom::blink::UDPSocketListener>
  GetUDPSocketListener();

  // network::mojom::blink::UDPSocketListener:
  void OnReceived(int32_t result,
                  const absl::optional<net::IPEndPoint>& src_addr,
                  absl::optional<base::span<const uint8_t>> data) override;

  void OnServiceConnectionError() override;
  void OnSocketConnectionError();

  void CloseOnError();

  void OnBothStreamsClosed(std::vector<ScriptValue> args);

  const Member<UDPSocketMojoRemote> udp_socket_;
  HeapMojoReceiver<network::mojom::blink::UDPSocketListener, UDPSocket>
      socket_listener_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_UDP_SOCKET_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_TCP_SOCKET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_TCP_SOCKET_H_

#include "base/gtest_prod_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/tcp_socket.mojom-blink.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/direct_sockets/direct_sockets_service_mojo_remote.h"
#include "third_party/blink/renderer/modules/direct_sockets/socket.h"
#include "third_party/blink/renderer/modules/direct_sockets/tcp_readable_stream_wrapper.h"
#include "third_party/blink/renderer/modules/direct_sockets/tcp_writable_stream_wrapper.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
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

class TCPSocketOptions;
class SocketCloseOptions;

// TCPSocket interface from tcp_socket.idl
class MODULES_EXPORT TCPSocket final
    : public ScriptWrappable,
      public Socket,
      public ActiveScriptWrappable<TCPSocket>,
      public network::mojom::blink::SocketObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // IDL definitions
  static TCPSocket* Create(ScriptState*,
                           const String& remote_address,
                           const uint16_t remote_port,
                           const TCPSocketOptions*,
                           ExceptionState&);

 public:
  explicit TCPSocket(ScriptState*);
  ~TCPSocket() override;

  // Validates options and calls
  // DirectSocketsServiceMojoRemote::OpenTcpSocket(...) with Init(...) passed as
  // callback.
  bool Open(const String& remote_address,
            const uint16_t remote_port,
            const TCPSocketOptions*,
            ExceptionState&);

  // On net::OK initializes readable/writable streams and resolves opened
  // promise. Otherwise rejects the opened promise. Serves as callback for
  // Open(...).
  void Init(int32_t result,
            const absl::optional<net::IPEndPoint>& local_addr,
            const absl::optional<net::IPEndPoint>& peer_addr,
            mojo::ScopedDataPipeConsumerHandle receive_stream,
            mojo::ScopedDataPipeProducerHandle send_stream);

  void Trace(Visitor*) const override;

  // ActiveScriptWrappable:
  bool HasPendingActivity() const override;

 private:
  mojo::PendingReceiver<network::mojom::blink::TCPConnectedSocket>
  GetTCPSocketReceiver();
  mojo::PendingRemote<network::mojom::blink::SocketObserver>
  GetTCPSocketObserver();

  void OnServiceConnectionError() override;
  void OnSocketConnectionError();

  // network::mojom::blink::SocketObserver:
  void OnReadError(int32_t net_error) override;
  void OnWriteError(int32_t net_error) override;

  void OnBothStreamsClosed(std::vector<ScriptValue> args);

  HeapMojoRemote<network::mojom::blink::TCPConnectedSocket> tcp_socket_;
  HeapMojoReceiver<network::mojom::blink::SocketObserver, TCPSocket>
      socket_observer_;

  FRIEND_TEST_ALL_PREFIXES(TCPSocketTest, OnSocketObserverConnectionError);
  FRIEND_TEST_ALL_PREFIXES(TCPSocketCloseTest, OnErrorOrClose);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_TCP_SOCKET_H_

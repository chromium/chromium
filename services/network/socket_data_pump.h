// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SOCKET_DATA_PUMP_H_
#define SERVICES_NETWORK_SOCKET_DATA_PUMP_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/address_family.h"
#include "net/base/ip_endpoint.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"

namespace net {
class StreamSocket;
}  // namespace net

namespace network {

// This class handles reading/writing logic between a network socket and a mojo
// pipe. Specifically, it (1) reads from the network socket and writes to a mojo
// producer pipe, and (2) reads from a mojo consumer pipe and writes to the
// network socket. On network read/write errors, it (3) also notifies the
// mojo::Remote<mojom::SocketObserver> appropriately.
class COMPONENT_EXPORT(NETWORK_SERVICE) SocketDataPump {
 public:
  // Interface to notify a consumer that about network errors and whether both
  // data pipes have been shut down from the client side.
  class Delegate {
   public:
    Delegate() {}
    ~Delegate() {}

    // Called when SocketDataPump detects a network read error.
    virtual void OnNetworkReadError(int net_error) = 0;

    // Called when SocketDataPump detects a network write error.
    virtual void OnNetworkWriteError(int net_error) = 0;

    // Called when SocketDataPump detects both send and receive pipes have shut
    // down.
    virtual void OnShutdown() = 0;
  };

  // Constructs a data pump that pumps data between |socket| and mojo data
  // pipe handles. Data are read from |send_pipe_handle| and sent to |socket|.
  // Data are read from |socket| and written to |receive_pipe_handle|.
  // |traffic_annotation| is attached to all writes to |socket|. Note that
  // |socket| must outlive |this|.
  SocketDataPump(net::StreamSocket* socket,
                 Delegate* delegate,
                 mojo::ScopedDataPipeProducerHandle receive_pipe_handle,
                 mojo::ScopedDataPipeConsumerHandle send_pipe_handle,
                 const net::NetworkTrafficAnnotationTag& traffic_annotation);

  SocketDataPump(const SocketDataPump&) = delete;
  SocketDataPump& operator=(const SocketDataPump&) = delete;

  ~SocketDataPump();

 private:
  // Maybe notifies |delegate_| of the shutdown if both pipes are closed.
  void MaybeNotifyDelegate();

  // "Receiving" in this context means reading from |socket_| and writing to
  // the Mojo |receive_stream_|.
  void ReceiveMore();
  void OnReceiveStreamWritable(MojoResult result);
  void OnReceiveStreamClosed(MojoResult result);
  // `pending_receive_buffer` contains the read data. `nullptr` if this is
  // invoked to indicate that `ReadIfReady()` completed asynchronously, as the
  // buffer isn't used in that case (completion merely indicates that data is
  // available).
  void OnNetworkReadCompleted(
      scoped_refptr<NetToMojoPendingBuffer> pending_receive_buffer,
      int result);
  void ShutdownReceive();

  // "Writing" is reading from the Mojo |send_stream_| and writing to the
  // |socket_|.
  void SendMore();
  void OnSendStreamReadable(MojoResult result);
  void OnNetworkWriteCompleted(int result);
  void ShutdownSend();

  const raw_ptr<net::StreamSocket> socket_;
  const raw_ptr<Delegate> delegate_;

  // The *stream handles will be null when there's a pending read from |socket_|
  // to |pending_receive_buffer_|, or while there is a pending write from
  // |pending_send_buffer_| to |socket_|. During this time, the handles will
  // be owned by the *PendingBuffer.
  //
  // The watchers need to be declared after the corresponding handle, so they
  // can be cleaned up before the handles are.

  // For reading from the network and writing to Mojo pipe.
  //
  // Note: `receive_stream_` is valid if receive wasn't shutdown (see
  // `receive_is_shutdown_`) and there is no pending read (in that case,
  // ownership of the stream is transferred to the read buffer).
  mojo::ScopedDataPipeProducerHandle receive_stream_;
  mojo::SimpleWatcher receive_stream_watcher_;
  // A separate watcher is needed to observe |receive_stream_|'s close event, so
  // that when the client shuts down their end of the pipe for
  // TCPConnectedSocket::UpgradeToTLS() during the waiting of the async callback
  // of ReadIfReady, |this| can be notified of the shutdown.
  mojo::SimpleWatcher receive_stream_close_watcher_;
  bool read_if_ready_pending_ = false;
  bool receive_is_shutdown_ = false;

  // For reading from the Mojo pipe and writing to the network.
  mojo::ScopedDataPipeConsumerHandle send_stream_;
  scoped_refptr<MojoToNetPendingBuffer> pending_send_buffer_;
  mojo::SimpleWatcher send_stream_watcher_;

  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  base::WeakPtrFactory<SocketDataPump> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_SOCKET_DATA_PUMP_H_

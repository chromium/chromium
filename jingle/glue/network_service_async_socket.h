// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An implementation of jingle_xmpp::AsyncSocket that uses Chrome Network Service
// sockets.

#ifndef JINGLE_GLUE_NETWORK_SERVICE_ASYNC_SOCKET_H_
#define JINGLE_GLUE_NETWORK_SERVICE_ASYNC_SOCKET_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "jingle/glue/network_service_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/host_port_pair.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/tls_socket.mojom.h"
#include "third_party/libjingle_xmpp/xmpp/asyncsocket.h"

namespace jingle_glue {

class NetworkServiceAsyncSocket : public jingle_xmpp::AsyncSocket,
                                  public network::mojom::SocketObserver {
 public:
  NetworkServiceAsyncSocket(
      GetProxyResolvingSocketFactoryCallback get_socket_factory_callback,
      bool use_fake_tls_handshake,
      size_t read_buf_size,
      size_t write_buf_size,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // Does not raise any signals.
  ~NetworkServiceAsyncSocket() override;

  // jingle_xmpp::AsyncSocket implementation.

  // The current state (see jingle_xmpp::AsyncSocket::State; all but
  // STATE_CLOSING is used).
  State state() override;

  // The last generated error.  Errors are generated when the main
  // functions below return false or when SignalClosed is raised due
  // to an asynchronous error.
  Error error() override;

  // GetError() (which is of type net::Error) != net::OK only when
  // error() == ERROR_WINSOCK.
  int GetError() override;

  // Tries to connect to the given address.
  //
  // If state() is not STATE_CLOSED, sets error to ERROR_WRONGSTATE
  // and returns false.
  //
  // If |address| has an empty hostname or a zero port, sets error to
  // ERROR_DNS and returns false.  (We don't use the IP address even
  // if it's present, as DNS resolution is done by the network service).
  // But it's perfectly fine if the hostname is a stringified IP address.)
  //
  // Otherwise, starts the connection process and returns true.
  // SignalConnected will be raised when the connection is successful;
  // otherwise, SignalClosed will be raised with a net error set.
  bool Connect(const net::HostPortPair& address) override;

  // Tries to read at most |len| bytes into |data|.
  //
  // If state() is not STATE_TLS_CONNECTING, STATE_OPEN, or
  // STATE_TLS_OPEN, sets error to ERROR_WRONGSTATE and returns false.
  //
  // Otherwise, fills in |len_read| with the number of bytes read and
  // returns true.  If this is called when state() is
  // STATE_TLS_CONNECTING, reads 0 bytes.  (We have to handle this
  // case because StartTls() is called during a slot connected to
  // SignalRead after parsing the final non-TLS reply from the server
  // [see XmppClient::Private::OnSocketRead()].)
  bool Read(char* data, size_t len, size_t* len_read) override;

  // Queues up |len| bytes of |data| for writing.
  //
  // If state() is not STATE_TLS_CONNECTING, STATE_OPEN, or
  // STATE_TLS_OPEN, sets error to ERROR_WRONGSTATE and returns false.
  //
  // If the given data is too big for the internal write buffer, sets
  // error to ERROR_WINSOCK/net::ERR_INSUFFICIENT_RESOURCES and
  // returns false.
  //
  // Otherwise, queues up the data and returns true.  If this is
  // called when state() == STATE_TLS_CONNECTING, the data is will be
  // sent only after the TLS connection succeeds.  (See StartTls()
  // below for why this happens.)
  //
  // Note that there's no guarantee that the data will actually be
  // sent; however, it is guaranteed that the any data sent will be
  // sent in FIFO order.
  bool Write(const char* data, size_t len) override;

  // If the socket is not already closed, closes the socket and raises
  // SignalClosed.  Always returns true.
  bool Close() override;

  // Tries to change to a TLS connection with the given domain name.
  //
  // If state() is not STATE_OPEN or there are pending reads or
  // writes, sets error to ERROR_WRONGSTATE and returns false.  (In
  // practice, this means that StartTls() can only be called from a
  // slot connected to SignalRead.)
  //
  // Otherwise, starts the TLS connection process and returns true.
  // SignalSSLConnected will be raised when the connection is
  // successful; otherwise, SignalClosed will be raised with a net
  // error set.
  bool StartTls(const std::string& domain_name) override;

  // Signal behavior:
  //
  // SignalConnected: raised whenever the connect initiated by a call
  // to Connect() is complete.
  //
  // SignalSSLConnected: raised whenever the connect initiated by a
  // call to StartTls() is complete.  Not actually used by
  // XmppClient. (It just assumes that if SignalRead is raised after a
  // call to StartTls(), the connection has been successfully
  // upgraded.)
  //
  // SignalClosed: raised whenever the socket is closed, either due to
  // an asynchronous error, the other side closing the connection, or
  // when Close() is called.
  //
  // SignalRead: raised whenever the next call to Read() will succeed
  // with a non-zero |len_read| (assuming nothing else happens in the
  // meantime).
  //
  // SignalError: not used.

 private:
  enum AsyncIOState {
    // An I/O op is not in progress or has been handed over to the network
    // service.
    IDLE,
    // Waiting for network service to be ready to handle an operation.
    WAITING,
  };

  bool IsOpen() const;

  // Error functions.
  void DoNonNetError(Error error);
  void DoNetError(net::Error net_error);
  void DoNetErrorFromStatus(int status);
  void ProcessSocketObserverError();

  // SocketObserver implementation
  void OnReadError(int32_t net_error) override;
  void OnWriteError(int32_t net_error) override;

  // Connection functions.
  void ProcessConnectDone(mojo::PendingReceiver<network::mojom::SocketObserver>
                              socket_observer_receiver,
                          int status,
                          const base::Optional<net::IPEndPoint>& local_addr,
                          const base::Optional<net::IPEndPoint>& peer_addr,
                          mojo::ScopedDataPipeConsumerHandle receive_stream,
                          mojo::ScopedDataPipeProducerHandle send_stream);

  // Read loop functions.
  void WatchForReadReady();
  void ProcessReadReady(MojoResult result,
                        const mojo::HandleSignalsState& state);
  void ReportReadError(int net_error);

  // Write loop functions.
  void WatchForWriteReady();
  void ProcessWriteReady(MojoResult result,
                         const mojo::HandleSignalsState& state);
  void ProcessWriteClosed(MojoResult result,
                          const mojo::HandleSignalsState& state);

  // SSL/TLS connection functions.
  void ProcessSSLConnectDone(
      mojo::PendingReceiver<network::mojom::SocketObserver>
          socket_observer_receiver,
      int status,
      mojo::ScopedDataPipeConsumerHandle receive_stream,
      mojo::ScopedDataPipeProducerHandle send_stream);

  // Close functions.
  void DoClose();

  void ConnectPipes(mojo::ScopedDataPipeConsumerHandle receive_stream,
                    mojo::ScopedDataPipeProducerHandle send_stream);
  void BindSocketObserver(mojo::PendingReceiver<network::mojom::SocketObserver>
                              socket_observer_receiver);

  // |socket_factory_| is recreated every time via |get_socket_factory_callback|
  // to handle network service restarts after crashes.
  GetProxyResolvingSocketFactoryCallback get_socket_factory_callback_;
  mojo::Remote<network::mojom::ProxyResolvingSocketFactory> socket_factory_;
  // The handle to the proxy resolving socket for the current connection, if one
  // exists.
  mojo::Remote<network::mojom::ProxyResolvingSocket> socket_;
  // TLS socket, if StartTls has been called.
  mojo::Remote<network::mojom::TLSClientSocket> tls_socket_;

  // Used to route error notifications here.
  mojo::Receiver<network::mojom::SocketObserver> socket_observer_receiver_{
      this};

  bool use_fake_tls_handshake_;

  // jingle_xmpp::AsyncSocket state.
  jingle_xmpp::AsyncSocket::State state_;
  jingle_xmpp::AsyncSocket::Error error_;
  net::Error net_error_;

  // State for the read loop.  |read_start_| <= |read_end_| <=
  // |read_buf_->size()|.  There's a read in flight (i.e.,
  // |read_state_| != IDLE) iff |read_end_| == 0.
  AsyncIOState read_state_;
  std::vector<char> read_buf_;
  size_t read_start_, read_end_;
  mojo::ScopedDataPipeConsumerHandle read_pipe_;
  std::unique_ptr<mojo::SimpleWatcher> read_watcher_;

  // Handling read errors is a bit tricky since the status is reported via
  // |socket_observer_receiver_|, which is unordered compared to |read_pipe_|,
  // so it's possible to see an end of file (or an error) there while there is
  // still useful data pending.  As a result, the code waits to see both happen
  // before reporting error statuses (including EOF). Likewise for write pipes.
  //
  bool saw_error_on_read_pipe_;
  bool saw_error_on_write_pipe_;

  // This is != net::ERR_IO_PENDING (including possibly net::OK for end-of-file)
  // if a read error was reported via socket observer interface.
  int saw_read_error_on_socket_observer_pipe_;
  int saw_write_error_on_socket_observer_pipe_;

  // State for the write loop.  |write_end_| <= |write_buf_->size()|.
  // There's a write in flight (i.e., |write_state_| != IDLE) iff
  // |write_end_| > 0.
  AsyncIOState write_state_;
  std::vector<char> write_buf_;
  size_t write_end_;
  mojo::ScopedDataPipeProducerHandle write_pipe_;
  std::unique_ptr<mojo::SimpleWatcher> write_watcher_;
  std::unique_ptr<mojo::SimpleWatcher> write_close_watcher_;

  // Network traffic annotation for downstream socket write.
  // NetworkServiceAsyncSocket is not reused, hence annotation can be added in
  // constructor and used in all subsequent writes.
  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceAsyncSocket);
};

}  // namespace jingle_glue

#endif  // JINGLE_GLUE_NETWORK_SERVICE_ASYNC_SOCKET_H_

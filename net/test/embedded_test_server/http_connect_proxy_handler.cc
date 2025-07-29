// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/http_connect_proxy_handler.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log_source.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/test/embedded_test_server/http_connection.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace net::test_server {

class HttpConnectProxyHandler::ConnectTunnel {
 public:
  static constexpr size_t kCapacity = 32 * 1024;
  using DeleteCallback = base::OnceCallback<void(ConnectTunnel*)>;

  ConnectTunnel(HttpConnectProxyHandler* http_proxy_handler,
                std::unique_ptr<StreamSocket> socket)
      : http_proxy_handler_(http_proxy_handler), socket_(std::move(socket)) {}

  ~ConnectTunnel() = default;

  // Tries to establish a connection to localhost on `dest_port`, and on
  // success, tells the client socket a tunnel was successfully established, and
  // starts tunnelling data between the connections.
  void Start(uint16_t dest_port) {
    dest_socket_ = std::make_unique<TCPClientSocket>(
        AddressList::CreateFromIPAddress(IPAddress::IPv4Localhost(), dest_port),
        /*socket_performance_watcher=*/nullptr,
        /*network_quality_estimator=*/nullptr, /*net_log=*/nullptr,
        NetLogSource());

    int result = dest_socket_->Connect(base::BindOnce(
        &ConnectTunnel::OnConnectComplete, base::Unretained(this)));
    if (result != ERR_IO_PENDING) {
      OnConnectComplete(result);
    }
  }

 private:
  void OnConnectComplete(int result) {
    // If unable to connect, write a bad gateway error to `socket_` before
    // deleting `this`.
    if (result != OK) {
      VLOG(1) << "Failed to establish tunnel connection.";

      BasicHttpResponse response;
      response.set_code(HttpStatusCode::HTTP_BAD_GATEWAY);
      response.set_reason("Bad Gateway");
      std::string response_string = response.ToResponseString();

      scoped_refptr<GrowableIOBuffer> buffer =
          base::MakeRefCounted<GrowableIOBuffer>();
      buffer->SetCapacity(response_string.size());
      buffer->span().copy_prefix_from(base::as_byte_span(response_string));
      DoWrite(/*src=*/nullptr, /*dest=*/socket_.get(), std::move(buffer),
              response_string.size());
      return;
    }

    // Write HTTP headers to client socket to indicate the connect succeeded,
    // and then start tunnelling.
    BasicHttpResponse response;
    response.set_reason("Connection established");
    StartTunneling(/*src=*/dest_socket_.get(), /*dest=*/socket_.get(),
                   response.ToResponseString());
    // Start tunneling from client socket to destination immediately, no need to
    // write anything else.
    StartTunneling(/*src=*/socket_.get(), /*dest=*/dest_socket_.get());
  }

  // Starts reading from `src` and writing that data to `dest`. If
  // `initial_data` is provided, writes that `dest` before writing to `src`.
  // Since a CONNECT proxy passes data in both directions, this needs to be
  // called twice, flipping `src` and `dest` between calls, to fully set up the
  // tunnelling.
  void StartTunneling(StreamSocket* src,
                      StreamSocket* dest,
                      std::string initial_data = {}) {
    scoped_refptr<GrowableIOBuffer> buffer =
        base::MakeRefCounted<GrowableIOBuffer>();
    buffer->SetCapacity(std::max(kCapacity, initial_data.size()));
    if (!initial_data.empty()) {
      // Start with a write, if `initial_data` is provided.
      buffer->span().copy_prefix_from(base::as_byte_span(initial_data));
      DoWrite(src, dest, std::move(buffer), initial_data.size());
      return;
    }

    DoRead(src, dest, std::move(buffer));
  }

  // Try to read data from `src`. Once data is read, write it all to `dest`, and
  // then repeat the process, until an error is encountered. Uses
  // GrowableIOBuffer because it can track an offset that indicates how much
  // data has been written. DrainableIOBuffer can do the same, but requires a
  // nested IOBuffer, so is a little more complicated to us.
  void DoRead(StreamSocket* src,
              StreamSocket* dest,
              scoped_refptr<GrowableIOBuffer> buffer) {
    int result =
        src->Read(buffer.get(), buffer->size(),
                  base::BindOnce(&ConnectTunnel::OnReadComplete,
                                 base::Unretained(this), src, dest, buffer));
    if (result == ERR_IO_PENDING) {
      return;
    }
    OnReadComplete(src, dest, std::move(buffer), result);
  }

  // Called when a read from `src` completes. On error, tears down the socket.
  // Otherwise, starts writing the data to `dest`, and will start reading from
  // `src` again, once all data is written.
  void OnReadComplete(StreamSocket* src,
                      StreamSocket* dest,
                      scoped_refptr<GrowableIOBuffer> buffer,
                      int result) {
    CHECK_NE(result, ERR_IO_PENDING);

    if (result <= 0) {
      // On error / close, close both sockets - this behavior is good enough,
      // since the client side (Chrome's network stack) only closes the write
      // pipe when it's done reading, and since this code doesn't read from the
      // destination pipe (likely to another EmbeddedTestServer) while there's
      // data in the buffer to write to the client pipe, all data will be
      // written before the EmbeddedTestServer closing the pipe will be
      // observed.
      http_proxy_handler_->DeleteTunnel(this);
      return;
    }

    DoWrite(src, dest, std::move(buffer), result);
  }

  // Writes `remaining_bytes` from `buffer` to `dest`. Once all data has been
  // written, will start reading from `src` again. If `src` is nullptr, writes
  // data to `dest`, and destroys the `ConnectTunnel` once everything has been
  // written.
  void DoWrite(StreamSocket* src,
               StreamSocket* dest,
               scoped_refptr<GrowableIOBuffer> buffer,
               int remaining_bytes) {
    CHECK_GE(remaining_bytes, 0);
    int result = dest->Write(
        buffer.get(), remaining_bytes,
        base::BindOnce(&ConnectTunnel::OnWriteComplete, base::Unretained(this),
                       src, dest, buffer, remaining_bytes),
        TRAFFIC_ANNOTATION_FOR_TESTS);
    if (result == ERR_IO_PENDING) {
      return;
    }
    OnWriteComplete(src, dest, std::move(buffer), remaining_bytes, result);
  }

  // Called once data has been written to `dest` or there was a write error. On
  // error, tears down `this`. Otherwise, keeps writing until all data has been
  // written, and then starts reading from `src` again.
  void OnWriteComplete(StreamSocket* src,
                       StreamSocket* dest,
                       scoped_refptr<GrowableIOBuffer> buffer,
                       int remaining_bytes,
                       int result) {
    CHECK_NE(result, ERR_IO_PENDING);

    if (result < 0) {
      // See OnReadComplete() for explanation on why this is ok to do.
      http_proxy_handler_->DeleteTunnel(this);
      return;
    }

    CHECK_LE(result, remaining_bytes);
    buffer->DidConsume(result);
    remaining_bytes -= result;
    if (remaining_bytes > 0) {
      DoWrite(src, dest, std::move(buffer), remaining_bytes);
      return;
    }

    // `src` will be nullptr when writing a connect error. In that case, once
    // everything has been written, delete `this` to close `socket_`.
    if (!src) {
      http_proxy_handler_->DeleteTunnel(this);
      return;
    }

    buffer->set_offset(0);
    DoRead(src, dest, std::move(buffer));
  }

  raw_ptr<HttpConnectProxyHandler> http_proxy_handler_;

  // The socket to the client (Chrome's network stack).
  std::unique_ptr<StreamSocket> socket_;

  // The socket to the server (typically another EmbeddedTestServer instance).
  std::unique_ptr<TCPClientSocket> dest_socket_;
};

HttpConnectProxyHandler::HttpConnectProxyHandler(
    base::span<const HostPortPair> proxied_destinations)
    : proxied_destinations_(proxied_destinations.begin(),
                            proxied_destinations.end()) {}

HttpConnectProxyHandler::~HttpConnectProxyHandler() = default;

bool HttpConnectProxyHandler::HandleProxyRequest(HttpConnection& connection,
                                                 const HttpRequest& request) {
  // This class only supports HTTP/1.x.
  CHECK_EQ(connection.protocol(), HttpConnection::Protocol::kHttp1);
  CHECK_EQ(request.method, METHOD_CONNECT);

  // For CONNECT requests, `relative_url` is actually a host and port.
  HostPortPair dest = HostPortPair::FromString(request.relative_url);
  std::unique_ptr<BasicHttpResponse> error_response;

  if (dest.IsEmpty()) {
    ADD_FAILURE() << "Invalid CONNECT destination: " << request.relative_url;
    // Returning true on error will result in an HTTP error message being
    // written to the socket.
    return false;
  }
  if (!proxied_destinations_.contains(dest)) {
    // Returning true on error will result in an HTTP error message being
    // written to the socket.
    return false;
  }

  auto tunnel = std::make_unique<ConnectTunnel>(this, connection.TakeSocket());
  auto tunnel_it = connect_tunnels_.insert(std::move(tunnel)).first;
  (*tunnel_it)->Start(dest.port());

  return true;
}

void HttpConnectProxyHandler::DeleteTunnel(ConnectTunnel* tunnel) {
  auto tunnel_it = connect_tunnels_.find(tunnel);
  CHECK(tunnel_it != connect_tunnels_.end());
  connect_tunnels_.erase(tunnel_it);
}

}  // namespace net::test_server

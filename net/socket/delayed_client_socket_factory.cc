// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/delayed_client_socket_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "net/socket/bandwidth_throttle.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/delayed_datagram_socket.h"
#include "net/socket/delayed_stream_socket.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/transport_client_socket.h"

namespace net {

namespace {

// A TransportClientSocket that adds network delay. Delegates all
// Socket/StreamSocket operations to a DelayedStreamSocket (which wraps and
// owns the real transport socket). Transport-specific methods (Bind,
// SetNoDelay, SetKeepAlive) delegate to the original transport socket via a
// raw pointer retained before ownership transfer.
class DelayedTransportClientSocket : public TransportClientSocket {
 public:
  DelayedTransportClientSocket(
      std::unique_ptr<TransportClientSocket> inner_socket,
      const DelayedSocketConfig& config,
      scoped_refptr<BandwidthThrottle> download_throttle,
      scoped_refptr<BandwidthThrottle> upload_throttle)
      : inner_transport_(inner_socket.get()),
        delayed_(std::make_unique<DelayedStreamSocket>(
            std::move(inner_socket),
            config,
            std::move(download_throttle),
            std::move(upload_throttle))) {}

  ~DelayedTransportClientSocket() override {
    // Clear the raw pointer before delayed_ destroys the socket it points to.
    inner_transport_ = nullptr;
  }

  // TransportClientSocket methods — delegate to inner transport.
  int Bind(const IPEndPoint& local_addr) override {
    return inner_transport_->Bind(local_addr);
  }
  bool SetNoDelay(bool no_delay) override {
    return inner_transport_->SetNoDelay(no_delay);
  }
  bool SetKeepAlive(bool enable, int delay_secs) override {
    return inner_transport_->SetKeepAlive(enable, delay_secs);
  }

  // Socket methods — delegate to delayed wrapper.
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override {
    return delayed_->Read(buf, buf_len, std::move(callback));
  }
  int ReadIfReady(IOBuffer* buf,
                  int buf_len,
                  CompletionOnceCallback callback) override {
    return delayed_->ReadIfReady(buf, buf_len, std::move(callback));
  }
  int CancelReadIfReady() override { return delayed_->CancelReadIfReady(); }
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override {
    return delayed_->Write(buf, buf_len, std::move(callback),
                           traffic_annotation);
  }
  int SetReceiveBufferSize(int32_t size) override {
    return delayed_->SetReceiveBufferSize(size);
  }
  int SetSendBufferSize(int32_t size) override {
    return delayed_->SetSendBufferSize(size);
  }
  void SetDnsAliases(base::flat_set<std::string> aliases) override {
    delayed_->SetDnsAliases(std::move(aliases));
  }
  const base::flat_set<std::string>& GetDnsAliases() const override {
    return delayed_->GetDnsAliases();
  }

  // StreamSocket methods — delegate to delayed wrapper.
  int Connect(CompletionOnceCallback callback) override {
    return delayed_->Connect(std::move(callback));
  }
  void SetBeforeConnectCallback(
      const BeforeConnectCallback& before_connect_callback) override {
    delayed_->SetBeforeConnectCallback(before_connect_callback);
  }
  void Disconnect() override { delayed_->Disconnect(); }
  bool IsConnected() const override { return delayed_->IsConnected(); }
  bool IsConnectedAndIdle() const override {
    return delayed_->IsConnectedAndIdle();
  }
  int GetPeerAddress(IPEndPoint* address) const override {
    return delayed_->GetPeerAddress(address);
  }
  int GetLocalAddress(IPEndPoint* address) const override {
    return delayed_->GetLocalAddress(address);
  }
  const NetLogWithSource& NetLog() const override { return delayed_->NetLog(); }
  bool WasEverUsed() const override { return delayed_->WasEverUsed(); }
  NextProto GetNegotiatedProtocol() const override {
    return delayed_->GetNegotiatedProtocol();
  }
  bool GetSSLInfo(SSLInfo* ssl_info) override {
    return delayed_->GetSSLInfo(ssl_info);
  }
  int64_t GetTotalReceivedBytes() const override {
    return delayed_->GetTotalReceivedBytes();
  }
  void ApplySocketTag(const SocketTag& tag) override {
    delayed_->ApplySocketTag(tag);
  }

 private:
  // Raw pointer to the inner transport socket for transport-specific methods.
  // Safe because DelayedStreamSocket (via delayed_) owns the socket, and
  // both are destroyed together with this object.
  raw_ptr<TransportClientSocket> inner_transport_;
  std::unique_ptr<DelayedStreamSocket> delayed_;
};

}  // namespace

// --- DelayedClientSocketFactory ---

DelayedClientSocketFactory::DelayedClientSocketFactory(
    ClientSocketFactory* inner_factory,
    const DelayedSocketConfig& config)
    : inner_factory_(inner_factory), config_(config) {
  RecreateThrottles();
}

DelayedClientSocketFactory::~DelayedClientSocketFactory() = default;

void DelayedClientSocketFactory::SetConfig(const DelayedSocketConfig& config) {
  config_ = config;
  RecreateThrottles();
}

std::unique_ptr<DatagramClientSocket>
DelayedClientSocketFactory::CreateDatagramClientSocket(
    DatagramSocket::BindType bind_type,
    handles::NetworkHandle target_network,
    NetLog* net_log,
    const NetLogSource& source) {
  auto inner = inner_factory_->CreateDatagramClientSocket(
      bind_type, target_network, net_log, source);
  // The throttles are shared with the stream sockets from this factory, so
  // TCP and UDP traffic compete for the same simulated link.
  return std::make_unique<DelayedDatagramSocket>(
      std::move(inner), config_, download_throttle_, upload_throttle_);
}

std::unique_ptr<TransportClientSocket>
DelayedClientSocketFactory::CreateTransportClientSocket(
    const AddressList& addresses,
    handles::NetworkHandle target_network,
    std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
    NetworkQualityEstimator* network_quality_estimator,
    NetLog* net_log,
    const NetLogSource& source) {
  auto inner = inner_factory_->CreateTransportClientSocket(
      addresses, target_network, std::move(socket_performance_watcher),
      network_quality_estimator, net_log, source);

  return std::make_unique<DelayedTransportClientSocket>(
      std::move(inner), config_, download_throttle_, upload_throttle_);
}

std::unique_ptr<SSLClientSocket>
DelayedClientSocketFactory::CreateSSLClientSocket(
    SSLClientContext* context,
    std::unique_ptr<StreamSocket> stream_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config) {
  // SSL wraps the (already delayed) stream socket. No additional wrapping
  // needed — TLS handshake round trips will naturally experience the delay.
  return inner_factory_->CreateSSLClientSocket(
      context, std::move(stream_socket), host_and_port, ssl_config);
}

void DelayedClientSocketFactory::RecreateThrottles() {
  // A null throttle means unconstrained throughput (std::nullopt); a finite
  // throughput requires a shared throttle. This matches the CHECKed
  // invariant in the DelayedStreamSocket / DelayedDatagramSocket ctors.
  if (config_.download_throughput_bytes_per_sec.has_value()) {
    download_throttle_ = base::MakeRefCounted<BandwidthThrottle>(
        *config_.download_throughput_bytes_per_sec);
  } else {
    download_throttle_.reset();
  }

  if (config_.upload_throughput_bytes_per_sec.has_value()) {
    upload_throttle_ = base::MakeRefCounted<BandwidthThrottle>(
        *config_.upload_throughput_bytes_per_sec);
  } else {
    upload_throttle_.reset();
  }
}

}  // namespace net

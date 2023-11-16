// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/socket/tcp_socket.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/api/api_resource.h"
#include "extensions/browser/api/socket/mojo_data_pump.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/ssl_config.mojom.h"

namespace extensions {

namespace {

// Returns true if successfully parsed the SSL protocol version that is
// represented by a string. Returns false if |version_str| is invalid.
bool SSLProtocolVersionFromString(const std::string& version_str,
                                  network::mojom::SSLVersion* version_out,
                                  bool is_min_version) {
  // TLS 1.0 and 1.1 are no longer supported. For compatibility, when set as a
  // minimum version, we clamp to TLS 1.2. When set as a maximum version, they
  // are treated as invalid.
  if (is_min_version && (version_str == "tls1" || version_str == "tls1.1")) {
    *version_out = network::mojom::SSLVersion::kTLS12;
    return true;
  }
  if (version_str == "tls1.2") {
    *version_out = network::mojom::SSLVersion::kTLS12;
    return true;
  }
  if (version_str == "tls1.3") {
    *version_out = network::mojom::SSLVersion::kTLS13;
    return true;
  }
  return false;
}

}  // namespace

const char kTCPSocketTypeInvalidError[] =
    "Cannot call both connect and listen on the same socket.";
const char kSocketListenError[] = "Could not listen on the specified port.";

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    ApiResourceManager<ResumableTCPSocket>>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<ResumableTCPSocket> >*
ApiResourceManager<ResumableTCPSocket>::GetFactoryInstance() {
  return g_factory.Pointer();
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    ApiResourceManager<ResumableTCPServerSocket>>>::DestructorAtExit
    g_server_factory = LAZY_INSTANCE_INITIALIZER;

// static
template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<ResumableTCPServerSocket> >*
ApiResourceManager<ResumableTCPServerSocket>::GetFactoryInstance() {
  return g_server_factory.Pointer();
}

TCPSocket::TCPSocket(content::BrowserContext* browser_context,
                     const std::string& owner_extension_id)
    : Socket(owner_extension_id),
      browser_context_(browser_context),
      socket_mode_(UNKNOWN),
      mojo_data_pump_(nullptr) {}

TCPSocket::TCPSocket(
    mojo::PendingRemote<network::mojom::TCPConnectedSocket> socket,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream,
    const std::optional<net::IPEndPoint>& remote_addr,
    const std::string& owner_extension_id)
    : Socket(owner_extension_id),
      browser_context_(nullptr),
      socket_mode_(CLIENT),
      client_socket_(std::move(socket)),
      mojo_data_pump_(std::make_unique<MojoDataPump>(std::move(receive_stream),
                                                     std::move(send_stream))),
      peer_addr_(remote_addr) {
  is_connected_ = true;
}

TCPSocket::~TCPSocket() {
  Disconnect(true /* socket_destroying */);
}

void TCPSocket::Connect(const net::AddressList& address,
                        net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(callback);

  if (socket_mode_ == SERVER || connect_callback_) {
    std::move(callback).Run(net::ERR_CONNECTION_FAILED);
    return;
  }

  if (is_connected_) {
    std::move(callback).Run(net::ERR_SOCKET_IS_CONNECTED);
    return;
  }

  DCHECK(!server_socket_);
  socket_mode_ = CLIENT;
  connect_callback_ = std::move(callback);

  network::mojom::NetworkContext::CreateTCPConnectedSocketCallback
      completion_callback = base::BindOnce(&TCPSocket::OnConnectComplete,
                                           weak_factory_.GetWeakPtr());

  if (!storage_partition_) {
    storage_partition_ = browser_context_->GetDefaultStoragePartition();
  }
  storage_partition_->GetNetworkContext()->CreateTCPConnectedSocket(
      std::nullopt, address, /*options=*/nullptr,
      net::MutableNetworkTrafficAnnotationTag(
          Socket::GetNetworkTrafficAnnotationTag()),
      client_socket_.BindNewPipeAndPassReceiver(),
      /*oberserver=*/mojo::NullRemote(), std::move(completion_callback));
}

void TCPSocket::Disconnect(bool socket_destroying) {
  // Make sure that any outstanding callbacks from Connect or Listen are
  // aborted.
  weak_factory_.InvalidateWeakPtrs();
  is_connected_ = false;
  local_addr_ = std::nullopt;
  peer_addr_ = std::nullopt;
  mojo_data_pump_ = nullptr;
  client_socket_.reset();
  server_socket_.reset();
  listen_callback_.Reset();
  connect_callback_.Reset();
  accept_callback_.Reset();
  // TODO(devlin): Should we do this for all callbacks?
  if (read_callback_) {
    std::move(read_callback_)
        .Run(net::ERR_CONNECTION_CLOSED, nullptr, socket_destroying);
  }
}

void TCPSocket::Bind(const std::string& address,
                     uint16_t port,
                     net::CompletionOnceCallback callback) {
  std::move(callback).Run(net::ERR_FAILED);
}

void TCPSocket::Read(int count, ReadCompletionCallback callback) {
  DCHECK(callback);

  const bool socket_destroying = false;
  if (socket_mode_ != CLIENT) {
    std::move(callback).Run(net::ERR_FAILED, nullptr, socket_destroying);
    return;
  }

  if (!mojo_data_pump_) {
    std::move(callback).Run(net::ERR_SOCKET_NOT_CONNECTED, nullptr,
                            socket_destroying);
    return;
  }
  if (mojo_data_pump_->HasPendingRead() || connect_callback_) {
    // It's illegal to read a net::TCPSocket while a pending Connect or Read is
    // already in progress.
    std::move(callback).Run(net::ERR_IO_PENDING, nullptr, socket_destroying);
    return;
  }

  read_callback_ = std::move(callback);
  mojo_data_pump_->Read(count, base::BindOnce(&TCPSocket::OnReadComplete,
                                              base::Unretained(this)));
}

void TCPSocket::RecvFrom(int count, RecvFromCompletionCallback callback) {
  std::move(callback).Run(net::ERR_FAILED, nullptr,
                          false /* socket_destroying */, std::string(), 0);
}

void TCPSocket::SendTo(scoped_refptr<net::IOBuffer> io_buffer,
                       int byte_count,
                       const net::IPEndPoint& address,
                       net::CompletionOnceCallback callback) {
  std::move(callback).Run(net::ERR_FAILED);
}

void TCPSocket::SetKeepAlive(bool enable,
                             int delay,
                             SetKeepAliveCallback callback) {
  if (!client_socket_) {
    std::move(callback).Run(false);
    return;
  }
  client_socket_->SetKeepAlive(enable, delay, std::move(callback));
}

void TCPSocket::SetNoDelay(bool no_delay, SetNoDelayCallback callback) {
  if (!client_socket_) {
    std::move(callback).Run(false);
    return;
  }
  client_socket_->SetNoDelay(no_delay, std::move(callback));
}

void TCPSocket::Listen(const std::string& address,
                       uint16_t port,
                       int backlog,
                       ListenCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!server_socket_);
  DCHECK(!client_socket_);
  DCHECK(!listen_callback_);

  if (socket_mode_ == CLIENT) {
    std::move(callback).Run(net::ERR_NOT_IMPLEMENTED,
                            kTCPSocketTypeInvalidError);
    return;
  }

  net::IPEndPoint ip_end_point;
  if (!StringAndPortToIPEndPoint(address, port, &ip_end_point)) {
    std::move(callback).Run(net::ERR_INVALID_ARGUMENT, "");
    return;
  }

  socket_mode_ = SERVER;

  listen_callback_ = std::move(callback);

  // |completion_callback| is called on current thread.
  network::mojom::NetworkContext::CreateTCPServerSocketCallback
      completion_callback = base::BindOnce(&TCPSocket::OnListenComplete,
                                           weak_factory_.GetWeakPtr());

  if (!storage_partition_) {
    storage_partition_ = browser_context_->GetDefaultStoragePartition();
  }

  auto options = network::mojom::TCPServerSocketOptions::New();
  options->backlog = backlog;
  storage_partition_->GetNetworkContext()->CreateTCPServerSocket(
      ip_end_point, std::move(options),
      net::MutableNetworkTrafficAnnotationTag(
          Socket::GetNetworkTrafficAnnotationTag()),
      server_socket_.BindNewPipeAndPassReceiver(),
      std::move(completion_callback));
}

void TCPSocket::Accept(AcceptCompletionCallback callback) {
  if (socket_mode_ != SERVER || !server_socket_) {
    std::move(callback).Run(net::ERR_FAILED, mojo::NullRemote(), std::nullopt,
                            mojo::ScopedDataPipeConsumerHandle(),
                            mojo::ScopedDataPipeProducerHandle());
    return;
  }

  // Limits to only 1 blocked accept call.
  if (accept_callback_) {
    std::move(callback).Run(net::ERR_FAILED, mojo::NullRemote(), std::nullopt,
                            mojo::ScopedDataPipeConsumerHandle(),
                            mojo::ScopedDataPipeProducerHandle());
    return;
  }

  accept_callback_ = std::move(callback);
  server_socket_->Accept(
      mojo::NullRemote() /* observer */,
      base::BindOnce(&TCPSocket::OnAccept, base::Unretained(this)));
}

bool TCPSocket::IsConnected() {
  return is_connected_;
}

bool TCPSocket::GetPeerAddress(net::IPEndPoint* address) {
  if (!peer_addr_)
    return false;
  *address = peer_addr_.value();
  return true;
}

bool TCPSocket::GetLocalAddress(net::IPEndPoint* address) {
  if (!local_addr_)
    return false;
  *address = local_addr_.value();
  return true;
}

Socket::SocketType TCPSocket::GetSocketType() const { return Socket::TYPE_TCP; }

int TCPSocket::WriteImpl(net::IOBuffer* io_buffer,
                         int io_buffer_size,
                         net::CompletionOnceCallback callback) {
  if (!mojo_data_pump_)
    return net::ERR_SOCKET_NOT_CONNECTED;

  mojo_data_pump_->Write(
      io_buffer, io_buffer_size,
      base::BindOnce(&TCPSocket::OnWriteComplete, base::Unretained(this),
                     std::move(callback)));
  return net::ERR_IO_PENDING;
}

void TCPSocket::OnConnectComplete(
    int result,
    const std::optional<net::IPEndPoint>& local_addr,
    const std::optional<net::IPEndPoint>& peer_addr,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!is_connected_);
  DCHECK(connect_callback_);

  if (result == net::OK) {
    is_connected_ = true;
    local_addr_ = local_addr;
    peer_addr_ = peer_addr;
    mojo_data_pump_ = std::make_unique<MojoDataPump>(std::move(receive_stream),
                                                     std::move(send_stream));
  }
  std::move(connect_callback_).Run(result);
}

void TCPSocket::OnListenComplete(
    int result,
    const std::optional<net::IPEndPoint>& local_addr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(listen_callback_);

  if (result != net::OK) {
    server_socket_.reset();
    std::move(listen_callback_).Run(result, kSocketListenError);
    return;
  }
  local_addr_ = local_addr;
  std::move(listen_callback_).Run(result, "");
}

content::StoragePartition* TCPSocket::GetStoragePartitionHelper() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return storage_partition_ ? storage_partition_.get()
                            : browser_context_->GetDefaultStoragePartition();
}

void TCPSocket::OnAccept(
    int result,
    const std::optional<net::IPEndPoint>& remote_addr,
    mojo::PendingRemote<network::mojom::TCPConnectedSocket> connected_socket,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  DCHECK(accept_callback_);
  std::move(accept_callback_)
      .Run(result, std::move(connected_socket), remote_addr,
           std::move(receive_stream), std::move(send_stream));
}

void TCPSocket::OnWriteComplete(net::CompletionOnceCallback callback,
                                int result) {
  if (result < 0) {
    // Write side has terminated. This can be an error or a graceful close.
    // TCPSocketEventDispatcher doesn't distinguish between the two.
    Disconnect(false /* socket_destroying */);
  }
  std::move(callback).Run(result);
}

void TCPSocket::OnReadComplete(int result,
                               scoped_refptr<net::IOBuffer> io_buffer) {
  DCHECK(read_callback_);
  DCHECK_GE(result, 0);

  // Use a local variable for |read_callback_|, because otherwise Disconnect()
  // will try to invoke |read_callback_| with a hardcoded result code.
  ReadCompletionCallback callback = std::move(read_callback_);
  if (result == 0) {
    // Read side has terminated. This can be an error or a graceful close.
    // TCPSocketEventDispatcher doesn't distinguish between the two. Treat them
    // as connection close.
    Disconnect(false /* socket_destroying */);
  }
  std::move(callback).Run(result, io_buffer, false /* socket_destroying */);
}

void TCPSocket::OnUpgradeToTLSComplete(
    UpgradeToTLSCallback callback,
    mojo::PendingRemote<network::mojom::TLSClientSocket> tls_socket,
    const net::IPEndPoint& local_addr,
    const net::IPEndPoint& peer_addr,
    int result,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream,
    const std::optional<net::SSLInfo>& ssl_info) {
  std::move(callback).Run(result, std::move(tls_socket), local_addr, peer_addr,
                          std::move(receive_stream), std::move(send_stream));
}

void TCPSocket::UpgradeToTLS(api::socket::SecureOptions* options,
                             UpgradeToTLSCallback callback) {
  if (!client_socket_ || !mojo_data_pump_ ||
      mojo_data_pump_->HasPendingRead() || mojo_data_pump_->HasPendingWrite()) {
    std::move(callback).Run(net::ERR_FAILED, mojo::NullRemote(),
                            net::IPEndPoint(), net::IPEndPoint(),
                            mojo::ScopedDataPipeConsumerHandle(),
                            mojo::ScopedDataPipeProducerHandle());
    return;
  }
  if (!local_addr_ || !peer_addr_) {
    DVLOG(1) << "Could not get local address or peer address.";
    std::move(callback).Run(net::ERR_FAILED, mojo::NullRemote(),
                            net::IPEndPoint(), net::IPEndPoint(),
                            mojo::ScopedDataPipeConsumerHandle(),
                            mojo::ScopedDataPipeProducerHandle());
    return;
  }

  // Convert any U-LABELs to A-LABELs.
  url::CanonHostInfo host_info;
  std::string canon_host = net::CanonicalizeHost(hostname(), &host_info);

  // Canonicalization shouldn't fail: the socket is already connected with a
  // host, using this hostname.
  if (host_info.family == url::CanonHostInfo::BROKEN) {
    DVLOG(1) << "Could not canonicalize hostname";
    std::move(callback).Run(net::ERR_FAILED, mojo::NullRemote(),
                            net::IPEndPoint(), net::IPEndPoint(),
                            mojo::ScopedDataPipeConsumerHandle(),
                            mojo::ScopedDataPipeProducerHandle());
    return;
  }

  mojo_data_pump_ = nullptr;
  network::mojom::TLSClientSocketOptionsPtr mojo_socket_options =
      network::mojom::TLSClientSocketOptions::New();

  mojo_socket_options->version_max = network::mojom::SSLVersion::kTLS13;

  if (options && options->tls_version) {
    network::mojom::SSLVersion version_min, version_max;
    bool has_version_min = false;
    bool has_version_max = false;
    api::socket::TLSVersionConstraints& versions = *options->tls_version;
    if (versions.min) {
      has_version_min = SSLProtocolVersionFromString(
          *versions.min, &version_min, /*is_min_version=*/true);
    }
    if (versions.max) {
      has_version_max = SSLProtocolVersionFromString(
          *versions.max, &version_max, /*is_min_version=*/false);
    }
    if (has_version_min)
      mojo_socket_options->version_min = version_min;
    if (has_version_max)
      mojo_socket_options->version_max = version_max;
  }
  mojo::PendingRemote<network::mojom::TLSClientSocket> tls_socket;
  auto tls_socket_receiver = tls_socket.InitWithNewPipeAndPassReceiver();
  net::HostPortPair host_port_pair(canon_host, peer_addr_.value().port());
  client_socket_->UpgradeToTLS(
      host_port_pair, std::move(mojo_socket_options),
      net::MutableNetworkTrafficAnnotationTag(
          Socket::GetNetworkTrafficAnnotationTag()),
      std::move(tls_socket_receiver), mojo::NullRemote() /* observer */,
      base::BindOnce(&TCPSocket::OnUpgradeToTLSComplete, base::Unretained(this),
                     std::move(callback), std::move(tls_socket),
                     local_addr_.value(), peer_addr_.value()));
}

ResumableTCPSocket::ResumableTCPSocket(content::BrowserContext* browser_context,
                                       const std::string& owner_extension_id)
    : TCPSocket(browser_context, owner_extension_id),
      persistent_(false),
      buffer_size_(0),
      paused_(false) {}

ResumableTCPSocket::ResumableTCPSocket(
    mojo::PendingRemote<network::mojom::TCPConnectedSocket> socket,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream,
    const std::optional<net::IPEndPoint>& remote_addr,
    const std::string& owner_extension_id)
    : TCPSocket(std::move(socket),
                std::move(receive_stream),
                std::move(send_stream),
                remote_addr,
                owner_extension_id),
      persistent_(false),
      buffer_size_(0),
      paused_(false) {}

ResumableTCPSocket::~ResumableTCPSocket() {
  // Despite ~TCPSocket doing basically the same, we need to disconnect
  // before ResumableTCPSocket is destroyed, because we have some extra
  // state that relies on the socket being ResumableTCPSocket, like
  // read_callback_.
  Disconnect(true /* socket_destroying */);
}

bool ResumableTCPSocket::IsPersistent() const { return persistent(); }

ResumableTCPServerSocket::ResumableTCPServerSocket(
    content::BrowserContext* browser_context,
    const std::string& owner_extension_id)
    : TCPSocket(browser_context, owner_extension_id),
      persistent_(false),
      paused_(false) {}

bool ResumableTCPServerSocket::IsPersistent() const { return persistent(); }

}  // namespace extensions

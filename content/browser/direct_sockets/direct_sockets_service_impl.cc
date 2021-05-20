// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/direct_sockets/direct_sockets_service_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "content/browser/direct_sockets/direct_udp_socket_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/address_list.h"
#include "net/net_buildflags.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if defined(OS_WIN) || defined(OS_MAC)
#include "base/enterprise_util.h"
#elif BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/tpm/install_attributes.h"
#endif

namespace content {

namespace {

constexpr net::NetworkTrafficAnnotationTag kDirectSocketsTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("direct_sockets", R"(
        semantics {
          sender: "Direct Sockets API"
          description: "Web app request to communicate with network device"
          trigger: "User completes network connection dialog"
          data: "Any data sent by web app"
          destination: OTHER
          destination_other: "Address entered by user in connection dialog"
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot yet be controlled by settings."
          policy_exception_justification: "To be implemented"
        }
      )");

absl::optional<bool> g_is_enterprise_managed_for_testing;

constexpr int32_t kMaxBufferSize = 32 * 1024 * 1024;

constexpr char kPermissionDeniedHistogramName[] =
    "DirectSockets.PermissionDeniedFailures";

DirectSocketsServiceImpl::PermissionCallback&
GetPermissionCallbackForTesting() {
  static base::NoDestructor<DirectSocketsServiceImpl::PermissionCallback>
      callback;
  return *callback;
}

network::mojom::NetworkContext*& GetNetworkContextForTesting() {
  static network::mojom::NetworkContext* network_context = nullptr;
  return network_context;
}

// Get local ip address from options.
absl::optional<net::IPEndPoint> GetLocalAddr(
    const blink::mojom::DirectSocketOptions& options) {
  absl::optional<net::IPEndPoint> local_addr = absl::nullopt;
  if (!options.local_hostname)
    return local_addr;

  net::IPAddress local_address;
  bool success = local_address.AssignFromIPLiteral(*options.local_hostname);
  if (success)
    local_addr = net::IPEndPoint(local_address, options.local_port);

  return local_addr;
}

#if BUILDFLAG(ENABLE_MDNS)
bool ResemblesMulticastDNSName(const std::string& hostname) {
  return base::EndsWith(hostname, ".local") ||
         base::EndsWith(hostname, ".local.");
}
#endif  // !BUILDFLAG(ENABLE_MDNS)

bool ContainNonPubliclyRoutableAddress(const net::AddressList& addresses) {
  DCHECK(!addresses.empty());
  for (auto ip : addresses) {
    if (!ip.address().IsPubliclyRoutable())
      return true;
  }
  return false;
}

// TODO(crbug.com/1119662): Now only check for the device, maybe there are some
// methods that can be applied to check for the user profile.
bool IsEnterpriseManaged() {
  // Return the value of the testing flag if it's set.
  if (g_is_enterprise_managed_for_testing.has_value())
    return g_is_enterprise_managed_for_testing.value();

#if defined(OS_WIN) || defined(OS_MAC)
  return base::IsMachineExternallyManaged();
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return chromeos::InstallAttributes::IsInitialized() &&
         chromeos::InstallAttributes::Get()->IsEnterpriseManaged();
#else
  return false;
#endif
}

}  // namespace

DirectSocketsServiceImpl::DirectSocketsServiceImpl(RenderFrameHost& frame_host)
    : WebContentsObserver(WebContents::FromRenderFrameHost(&frame_host)),
      frame_host_(&frame_host) {}

DirectSocketsServiceImpl::~DirectSocketsServiceImpl() = default;

// static
void DirectSocketsServiceImpl::CreateForFrame(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::DirectSocketsService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<DirectSocketsServiceImpl>(*render_frame_host),
      std::move(receiver));
}

class DirectSocketsServiceImpl::ResolveHostAndOpenSocket final
    : public network::ResolveHostClientBase {
 public:
  ResolveHostAndOpenSocket(
      base::WeakPtr<DirectSocketsServiceImpl> direct_sockets_service_impl,
      blink::mojom::DirectSocketOptionsPtr options,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> tcp_receiver,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      OpenTcpSocketCallback tcp_callback)
      : protocol_(ProtocolType::kTcp),
        direct_sockets_service_impl_(std::move(direct_sockets_service_impl)),
        options_(std::move(options)),
        tcp_receiver_(std::move(tcp_receiver)),
        observer_(std::move(observer)),
        tcp_callback_(std::move(tcp_callback)) {}

  ResolveHostAndOpenSocket(
      base::WeakPtr<DirectSocketsServiceImpl> direct_sockets_service_impl,
      blink::mojom::DirectSocketOptionsPtr options,
      mojo::PendingReceiver<blink::mojom::DirectUDPSocket> udp_receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> udp_listener,
      OpenUdpSocketCallback udp_callback)
      : protocol_(ProtocolType::kUdp),
        direct_sockets_service_impl_(std::move(direct_sockets_service_impl)),
        options_(std::move(options)),
        udp_receiver_(std::move(udp_receiver)),
        udp_listener_(std::move(udp_listener)),
        udp_callback_(std::move(udp_callback)) {}

  ~ResolveHostAndOpenSocket() override = default;

  void Start(network::mojom::NetworkContext* network_context) {
    DCHECK(network_context);
    DCHECK(!receiver_.is_bound());
    DCHECK(!resolver_.is_bound());

    if (net::IPAddress().AssignFromIPLiteral(*options_->remote_hostname)) {
      is_raw_address_ = true;
    }

    mojo::PendingRemote<network::mojom::HostResolver> pending_host_resolver;
    network_context->CreateHostResolver(
        absl::nullopt, pending_host_resolver.InitWithNewPipeAndPassReceiver());
    resolver_.Bind(std::move(pending_host_resolver));

    network::mojom::ResolveHostParametersPtr parameters =
        network::mojom::ResolveHostParameters::New();
#if BUILDFLAG(ENABLE_MDNS)
    if (ResemblesMulticastDNSName(*options_->remote_hostname)) {
      parameters->source = net::HostResolverSource::MULTICAST_DNS;
      is_mdns_name_ = true;
    }
#endif  // !BUILDFLAG(ENABLE_MDNS)
    resolver_->ResolveHost(
        net::HostPortPair(*options_->remote_hostname, options_->remote_port),
        net::NetworkIsolationKey::CreateTransient(), std::move(parameters),
        receiver_.BindNewPipeAndPassRemote());
    receiver_.set_disconnect_handler(
        base::BindOnce(&ResolveHostAndOpenSocket::OnComplete,
                       base::Unretained(this), net::ERR_NAME_NOT_RESOLVED,
                       net::ResolveErrorInfo(net::ERR_FAILED), absl::nullopt));
  }

 private:
  // network::mojom::ResolveHostClient implementation:
  void OnComplete(
      int result,
      const net::ResolveErrorInfo& resolve_error_info,
      const absl::optional<net::AddressList>& resolved_addresses) override {
    DCHECK(receiver_.is_bound());
    receiver_.reset();

    // Reject hostnames that resolve to non-public exception unless a raw IP
    // address or a *.local hostname is entered by the user.
    if (!is_raw_address_ && !is_mdns_name_ && resolved_addresses &&
        ContainNonPubliclyRoutableAddress(*resolved_addresses)) {
      result = net::Error::ERR_NETWORK_ACCESS_DENIED;
      base::UmaHistogramEnumeration(kPermissionDeniedHistogramName,
                                    FailureType::kResolvingToNonPublic);
    }
    protocol_ == ProtocolType::kTcp ? OpenTCPSocket(result, resolved_addresses)
                                    : OpenUDPSocket(result, resolved_addresses);
  }

  void OpenTCPSocket(
      int result,
      const absl::optional<net::AddressList>& resolved_addresses) {
    network::mojom::NetworkContext* network_context = nullptr;
    if (DirectSocketsServiceImpl* const direct_sockets_service_impl =
            direct_sockets_service_impl_.get()) {
      network_context = direct_sockets_service_impl->GetNetworkContext();
    }
    if (!network_context) {
      delete this;
      return;
    }

    if (result != net::OK) {
      std::move(tcp_callback_)
          .Run(result, absl::nullopt, absl::nullopt,
               mojo::ScopedDataPipeConsumerHandle(),
               mojo::ScopedDataPipeProducerHandle());
      delete this;
      return;
    }

    DCHECK(resolved_addresses && !resolved_addresses->empty());
    const absl::optional<net::IPEndPoint> local_addr = GetLocalAddr(*options_);

    network::mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options =
        network::mojom::TCPConnectedSocketOptions::New();
    if (options_->send_buffer_size > 0) {
      tcp_connected_socket_options->send_buffer_size =
          std::min(options_->send_buffer_size, kMaxBufferSize);
    }
    if (options_->receive_buffer_size > 0) {
      tcp_connected_socket_options->receive_buffer_size =
          std::min(options_->receive_buffer_size, kMaxBufferSize);
    }
    tcp_connected_socket_options->no_delay = options_->no_delay;

    network_context->CreateTCPConnectedSocket(
        local_addr, *resolved_addresses,
        std::move(tcp_connected_socket_options), TrafficAnnotation(),
        std::move(tcp_receiver_), std::move(observer_),
        std::move(tcp_callback_));
    delete this;
  }

  void OpenUDPSocket(
      int result,
      const absl::optional<net::AddressList>& resolved_addresses) {
    network::mojom::NetworkContext* network_context = nullptr;
    DirectSocketsServiceImpl* const direct_sockets_service_impl =
        direct_sockets_service_impl_.get();
    if (direct_sockets_service_impl) {
      network_context = direct_sockets_service_impl->GetNetworkContext();
    }
    if (!network_context) {
      delete this;
      return;
    }

    if (result != net::OK) {
      std::move(udp_callback_).Run(result, absl::nullopt, absl::nullopt);
      delete this;
      return;
    }

    DCHECK(resolved_addresses && !resolved_addresses->empty());
    absl::optional<net::IPEndPoint> local_addr = GetLocalAddr(*options_);

    network::mojom::UDPSocketOptionsPtr udp_socket_options =
        network::mojom::UDPSocketOptions::New();
    if (options_->send_buffer_size > 0) {
      udp_socket_options->send_buffer_size =
          std::min(options_->send_buffer_size, kMaxBufferSize);
    }
    if (options_->receive_buffer_size > 0) {
      udp_socket_options->receive_buffer_size =
          std::min(options_->receive_buffer_size, kMaxBufferSize);
    }

    net::IPEndPoint peer_addr = resolved_addresses->front();
    auto direct_udp_socket = std::make_unique<DirectUDPSocketImpl>(
        network_context, std::move(udp_listener_));
    direct_udp_socket->Connect(
        peer_addr, std::move(udp_socket_options),
        base::BindOnce(&ResolveHostAndOpenSocket::OnUdpConnectCompleted,
                       base::Unretained(this), peer_addr));
    direct_sockets_service_impl->direct_udp_socket_receivers_.Add(
        std::move(direct_udp_socket), std::move(udp_receiver_));
  }

  void OnUdpConnectCompleted(
      net::IPEndPoint peer_addr,
      int result,
      const absl::optional<net::IPEndPoint>& local_addr) {
    std::move(udp_callback_).Run(result, local_addr, peer_addr);
    delete this;
  }

  bool is_mdns_name_ = false;
  bool is_raw_address_ = false;

  const ProtocolType protocol_;
  const base::WeakPtr<DirectSocketsServiceImpl> direct_sockets_service_impl_;
  blink::mojom::DirectSocketOptionsPtr options_;

  mojo::PendingReceiver<network::mojom::TCPConnectedSocket> tcp_receiver_;
  mojo::PendingRemote<network::mojom::SocketObserver> observer_;
  OpenTcpSocketCallback tcp_callback_;

  mojo::PendingReceiver<blink::mojom::DirectUDPSocket> udp_receiver_;
  mojo::PendingRemote<network::mojom::UDPSocketListener> udp_listener_;
  OpenUdpSocketCallback udp_callback_;

  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
  mojo::Remote<network::mojom::HostResolver> resolver_;
};

void DirectSocketsServiceImpl::OpenTcpSocket(
    blink::mojom::DirectSocketOptionsPtr options,
    mojo::PendingReceiver<network::mojom::TCPConnectedSocket> receiver,
    mojo::PendingRemote<network::mojom::SocketObserver> observer,
    OpenTcpSocketCallback callback) {
  if (!frame_host_ || frame_host_->GetWebExposedIsolationLevel() <
                          RenderFrameHost::WebExposedIsolationLevel::
                              kMaybeIsolatedApplication) {
    mojo::ReportBadMessage("Insufficient isolation to open socket.");
    return;
  }
  network::mojom::NetworkContext* const network_context = GetNetworkContext();
  if (!options || !network_context) {
    mojo::ReportBadMessage("Invalid request to open socket");
    return;
  }

  const net::Error result = ValidateOptions(*options);

  // TODO(crbug.com/1119681): Collect metrics for usage and permission checks

  if (result != net::OK) {
    std::move(callback).Run(result, absl::nullopt, absl::nullopt,
                            mojo::ScopedDataPipeConsumerHandle(),
                            mojo::ScopedDataPipeProducerHandle());
    return;
  }

  ResolveHostAndOpenSocket* resolver = new ResolveHostAndOpenSocket(
      weak_ptr_factory_.GetWeakPtr(), std::move(options), std::move(receiver),
      std::move(observer), std::move(callback));
  resolver->Start(network_context);
}

void DirectSocketsServiceImpl::OpenUdpSocket(
    blink::mojom::DirectSocketOptionsPtr options,
    mojo::PendingReceiver<blink::mojom::DirectUDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
    OpenUdpSocketCallback callback) {
  if (!frame_host_ || frame_host_->GetWebExposedIsolationLevel() <
                          RenderFrameHost::WebExposedIsolationLevel::
                              kMaybeIsolatedApplication) {
    mojo::ReportBadMessage("Insufficient isolation to open socket.");
    return;
  }
  network::mojom::NetworkContext* const network_context = GetNetworkContext();
  if (!options || !network_context) {
    mojo::ReportBadMessage("Invalid request to open socket");
    return;
  }

  const net::Error result = ValidateOptions(*options);

  if (result != net::OK) {
    std::move(callback).Run(result, absl::nullopt, absl::nullopt);
    return;
  }

  // TODO(crbug.com/1119681): Collect metrics for usage and permission checks

  ResolveHostAndOpenSocket* resolver = new ResolveHostAndOpenSocket(
      weak_ptr_factory_.GetWeakPtr(), std::move(options), std::move(receiver),
      std::move(listener), std::move(callback));
  resolver->Start(network_context);
}

// static
net::MutableNetworkTrafficAnnotationTag
DirectSocketsServiceImpl::TrafficAnnotation() {
  return net::MutableNetworkTrafficAnnotationTag(
      kDirectSocketsTrafficAnnotation);
}

// static
void DirectSocketsServiceImpl::SetEnterpriseManagedForTesting(
    bool enterprise_managed) {
  g_is_enterprise_managed_for_testing = enterprise_managed;
}

// static
void DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
    PermissionCallback callback) {
  GetPermissionCallbackForTesting() = std::move(callback);
}

// static
void DirectSocketsServiceImpl::SetNetworkContextForTesting(
    network::mojom::NetworkContext* network_context) {
  GetNetworkContextForTesting() = network_context;
}

// static
absl::optional<net::IPEndPoint>
DirectSocketsServiceImpl::GetLocalAddrForTesting(
    const blink::mojom::DirectSocketOptions& options) {
  return GetLocalAddr(options);
}

void DirectSocketsServiceImpl::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  if (render_frame_host == frame_host_)
    frame_host_ = nullptr;
}

void DirectSocketsServiceImpl::WebContentsDestroyed() {
  frame_host_ = nullptr;
}

network::mojom::NetworkContext* DirectSocketsServiceImpl::GetNetworkContext() {
  if (GetNetworkContextForTesting())
    return GetNetworkContextForTesting();

  return frame_host_->GetStoragePartition()->GetNetworkContext();
}

net::Error DirectSocketsServiceImpl::ValidateOptions(
    const blink::mojom::DirectSocketOptions& options) {
  DCHECK(base::FeatureList::IsEnabled(features::kDirectSockets));

  if (!frame_host_)
    return net::ERR_CONTEXT_SHUT_DOWN;

  // TODO(crbug.com/1119600): Do not consume (or check) transient activation
  // for reconnection attempts.
  bool is_consumed =
      static_cast<RenderFrameHostImpl*>(frame_host_)
          ->frame_tree_node()
          ->UpdateUserActivationState(
              blink::mojom::UserActivationUpdateType::
                  kConsumeTransientActivation,
              blink::mojom::UserActivationNotificationType::kNone);
  if (!is_consumed) {
    base::UmaHistogramEnumeration(kPermissionDeniedHistogramName,
                                  FailureType::kTransientActivation);
    return net::ERR_NETWORK_ACCESS_DENIED;
  }

  if (GetPermissionCallbackForTesting())
    return GetPermissionCallbackForTesting().Run(options);  // IN-TEST

  if (options.send_buffer_size < 0 || options.receive_buffer_size < 0)
    return net::ERR_INVALID_ARGUMENT;

  // By default, we will restrict use of the API when enterprise software
  // policies are in effect.
  if (IsEnterpriseManaged()) {
    base::UmaHistogramEnumeration(kPermissionDeniedHistogramName,
                                  FailureType::kEnterprisePolicy);
    return net::ERR_NETWORK_ACCESS_DENIED;
  }

  // TODO(crbug.com/1119659): Check permissions policy.
  // TODO(crbug.com/1119600): Implement rate limiting.

  if (options.remote_port == 443) {
    base::UmaHistogramEnumeration(kPermissionDeniedHistogramName,
                                  FailureType::kCORS);
    // TODO(crbug.com/1119601): Issue a CORS preflight request.
    return net::ERR_UNSAFE_PORT;
  }

  // ValidateOptions() will need to become asynchronous:
  // TODO(crbug.com/1119597): Show connection dialog.
  // TODO(crbug.com/1119597): Use the hostname provided by the user.
  if (!options.remote_hostname)
    return net::ERR_NAME_NOT_RESOLVED;

  return net::OK;
}

}  // namespace content

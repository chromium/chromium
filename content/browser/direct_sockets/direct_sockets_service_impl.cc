// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/direct_sockets/direct_sockets_service_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "build/build_config.h"
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

#if defined(OS_WIN) || defined(OS_MAC)
#include "base/enterprise_util.h"
#elif BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/tpm/install_attributes.h"
#endif

namespace content {

namespace {

base::Optional<bool> g_is_enterprise_managed_for_testing;

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
base::Optional<net::IPEndPoint> GetLocalAddr(
    const blink::mojom::DirectSocketOptions& options) {
  base::Optional<net::IPEndPoint> local_addr = base::nullopt;
  if (!options.local_hostname)
    return local_addr;

  net::IPAddress local_address;
  bool success = local_address.AssignFromIPLiteral(*options.local_hostname);
  if (success)
    local_addr = net::IPEndPoint(local_address, options.local_port);

  return local_addr;
}

bool ResemblesMulticastDNSName(const std::string& hostname) {
  return base::EndsWith(hostname, ".local") ||
         base::EndsWith(hostname, ".local.");
}

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
      network::mojom::NetworkContext* network_context,
      blink::mojom::DirectSocketOptionsPtr options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> tcp_receiver,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      OpenTcpSocketCallback tcp_callback)
      : protocol_(ProtocolType::kTcp),
        network_context_(network_context),
        options_(std::move(options)),
        traffic_annotation_(traffic_annotation),
        tcp_receiver_(std::move(tcp_receiver)),
        observer_(std::move(observer)),
        tcp_callback_(std::move(tcp_callback)) {}

  ResolveHostAndOpenSocket(
      network::mojom::NetworkContext* network_context,
      blink::mojom::DirectSocketOptionsPtr options,
      mojo::PendingReceiver<network::mojom::UDPSocket> udp_receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
      OpenUdpSocketCallback udp_callback)
      : protocol_(ProtocolType::kUdp),
        network_context_(network_context),
        options_(std::move(options)),
        udp_receiver_(std::move(udp_receiver)),
        listener_(std::move(listener)),
        udp_callback_(std::move(udp_callback)) {}

  ~ResolveHostAndOpenSocket() override = default;

  void Start() {
    DCHECK(network_context_);
    DCHECK(!receiver_.is_bound());
    DCHECK(!resolver_.is_bound());

    if (net::IPAddress().AssignFromIPLiteral(*options_->remote_hostname)) {
      is_raw_address_ = true;
    }

    mojo::PendingRemote<network::mojom::HostResolver> pending_host_resolver;
    network_context_->CreateHostResolver(
        base::nullopt, pending_host_resolver.InitWithNewPipeAndPassReceiver());
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
                       net::ResolveErrorInfo(net::ERR_FAILED), base::nullopt));
  }

 private:
  // network::mojom::ResolveHostClient implementation:
  void OnComplete(
      int result,
      const net::ResolveErrorInfo& resolve_error_info,
      const base::Optional<net::AddressList>& resolved_addresses) override {
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
      const base::Optional<net::AddressList>& resolved_addresses) {
    if (result != net::OK) {
      std::move(tcp_callback_)
          .Run(result, base::nullopt, base::nullopt,
               mojo::ScopedDataPipeConsumerHandle(),
               mojo::ScopedDataPipeProducerHandle());
      delete this;
      return;
    }

    DCHECK(resolved_addresses && !resolved_addresses->empty());
    const base::Optional<net::IPEndPoint> local_addr = GetLocalAddr(*options_);

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

    if (!network_context_) {
      delete this;
      return;
    }

    network_context_->CreateTCPConnectedSocket(
        local_addr, *resolved_addresses,
        std::move(tcp_connected_socket_options), traffic_annotation_,
        std::move(tcp_receiver_), std::move(observer_),
        std::move(tcp_callback_));
    delete this;
  }

  void OpenUDPSocket(
      int result,
      const base::Optional<net::AddressList>& resolved_addresses) {
    if (result != net::OK) {
      std::move(udp_callback_).Run(result, base::nullopt, base::nullopt);
      delete this;
      return;
    }

    DCHECK(resolved_addresses && !resolved_addresses->empty());
    base::Optional<net::IPEndPoint> local_addr = GetLocalAddr(*options_);

    // TODO(crbug.com/1119620): network_context_->CreateUDPSocket
    // TODO(crbug.com/1119620): Connect(remote_addr, udp_socket_options)
    if (!network_context_) {
      delete this;
      return;
    }
    NOTIMPLEMENTED();
  }

  bool is_mdns_name_ = false;
  bool is_raw_address_ = false;

  const ProtocolType protocol_;
  network::mojom::NetworkContext* const network_context_;
  blink::mojom::DirectSocketOptionsPtr options_;

  net::MutableNetworkTrafficAnnotationTag traffic_annotation_;
  mojo::PendingReceiver<network::mojom::TCPConnectedSocket> tcp_receiver_;
  mojo::PendingRemote<network::mojom::SocketObserver> observer_;
  OpenTcpSocketCallback tcp_callback_;

  mojo::PendingReceiver<network::mojom::UDPSocket> udp_receiver_;
  mojo::PendingRemote<network::mojom::UDPSocketListener> listener_;
  OpenUdpSocketCallback udp_callback_;

  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
  mojo::Remote<network::mojom::HostResolver> resolver_;
};

void DirectSocketsServiceImpl::OpenTcpSocket(
    blink::mojom::DirectSocketOptionsPtr options,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<network::mojom::TCPConnectedSocket> receiver,
    mojo::PendingRemote<network::mojom::SocketObserver> observer,
    OpenTcpSocketCallback callback) {
  if (!options) {
    mojo::ReportBadMessage("Invalid request to open socket");
    return;
  }

  const net::Error result = ValidateOptions(*options);

  // TODO(crbug.com/1119681): Collect metrics for usage and permission checks

  if (result != net::OK) {
    std::move(callback).Run(result, base::nullopt, base::nullopt,
                            mojo::ScopedDataPipeConsumerHandle(),
                            mojo::ScopedDataPipeProducerHandle());
    return;
  }

  ResolveHostAndOpenSocket* resolver = new ResolveHostAndOpenSocket(
      GetNetworkContext(), std::move(options), traffic_annotation,
      std::move(receiver), std::move(observer), std::move(callback));
  resolver->Start();
}

void DirectSocketsServiceImpl::OpenUdpSocket(
    blink::mojom::DirectSocketOptionsPtr options,
    mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
    OpenUdpSocketCallback callback) {
  if (!options) {
    mojo::ReportBadMessage("Invalid request to open socket");
    return;
  }

  const net::Error result = ValidateOptions(*options);

  if (result != net::OK) {
    std::move(callback).Run(result, base::nullopt, base::nullopt);
    return;
  }

  // TODO(crbug.com/1119681): Collect metrics for usage and permission checks

  ResolveHostAndOpenSocket* resolver = new ResolveHostAndOpenSocket(
      GetNetworkContext(), std::move(options), std::move(receiver),
      std::move(listener), std::move(callback));
  resolver->Start();
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
base::Optional<net::IPEndPoint>
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

network::mojom::NetworkContext* DirectSocketsServiceImpl::GetNetworkContext() {
  if (network::mojom::NetworkContext* network_context =
          GetNetworkContextForTesting()) {
    return network_context;
  }

  return frame_host_->GetStoragePartition()->GetNetworkContext();
}

}  // namespace content

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network_service_client.h"

#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/sequence_token.h"
#include "base/threading/sequence_bound.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/browsing_data/clear_site_data_handler.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/browser/webrtc/webrtc_connections_observer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/network_change_manager.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/content_uri_utils.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/task/current_thread.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "net/base/address_map_linux.h"
#include "net/base/address_tracker_linux.h"
#endif

namespace content {

#if BUILDFLAG(IS_LINUX)
namespace {

// Takes care of passing updates to AddressTrackerLinux's AddressMap and set of
// online links to the network service to update its cache.
class NetworkInterfaceChangeHelper {
 public:
  explicit NetworkInterfaceChangeHelper(
      mojo::PendingAssociatedRemote<
          network::mojom::NetworkInterfaceChangeListener>
          network_interface_change_listener_pending)
      : network_interface_change_listener_pending_(
            std::move(network_interface_change_listener_pending)) {
    // This is constructed by NetworkServiceClient and only used on
    // AddressTrackerLinux's sequence.
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  NetworkInterfaceChangeHelper(const NetworkInterfaceChangeHelper&) = delete;
  NetworkInterfaceChangeHelper& operator=(const NetworkInterfaceChangeHelper&) =
      delete;

  ~NetworkInterfaceChangeHelper() = default;

  // Callback for AddressTrackerLinux::SetDiffCallback.
  void SendAddressTrackerDiffsToNetworkService(
      const net::AddressMapOwnerLinux::AddressMapDiff& addr_diff,
      const net::AddressMapOwnerLinux::OnlineLinksDiff& online_links_diff) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // On the first call, this binds the |network_interface_change_listener_| on
    // AddressTrackerLinux's sequence using
    // |network_interface_change_listener_pending_|.
    if (!network_interface_change_listener_) {
      DCHECK(network_interface_change_listener_pending_);
      network_interface_change_listener_.Bind(
          std::move(network_interface_change_listener_pending_));
    }
    auto params = network::mojom::NetworkInterfaceChangeParams::New(
        addr_diff, online_links_diff);
    network_interface_change_listener_->OnNetworkInterfacesChanged(
        std::move(params));
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  mojo::PendingAssociatedRemote<network::mojom::NetworkInterfaceChangeListener>
      network_interface_change_listener_pending_;
  mojo::AssociatedRemote<network::mojom::NetworkInterfaceChangeListener>
      network_interface_change_listener_ GUARDED_BY_CONTEXT(sequence_checker_);
};
}  // namespace
#endif

NetworkServiceClient::NetworkServiceClient()
#if BUILDFLAG(IS_ANDROID)
    : app_status_listener_(base::android::ApplicationStatusListener::New(
          base::BindRepeating(&NetworkServiceClient::OnApplicationStateChange,
                              base::Unretained(this))))
#endif
{

#if BUILDFLAG(IS_MAC)
  net::CertDatabase::StartListeningForKeychainEvents();
#endif

  if (IsOutOfProcessNetworkService()) {
    net::CertDatabase::GetInstance()->AddObserver(this);
    memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
        FROM_HERE, base::BindRepeating(&NetworkServiceClient::OnMemoryPressure,
                                       base::Unretained(this)));
  }

  webrtc_connections_observer_ =
      std::make_unique<content::WebRtcConnectionsObserver>(base::BindRepeating(
          &NetworkServiceClient::OnPeerToPeerConnectionsCountChange,
          base::Unretained(this)));
}

NetworkServiceClient::~NetworkServiceClient() {
  if (IsOutOfProcessNetworkService()) {
    net::CertDatabase::GetInstance()->RemoveObserver(this);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
    bool remove_ncn_observers = true;
#if BUILDFLAG(IS_LINUX)
    remove_ncn_observers = base::FeatureList::IsEnabled(
        net::features::kAddressTrackerLinuxIsProxied);
#endif  // BUILDFLAG(IS_LINUX)
    if (remove_ncn_observers) {
      net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
      net::NetworkChangeNotifier::RemoveMaxBandwidthObserver(this);
      net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
    }
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
  }
}

void NetworkServiceClient::OnTrustStoreChanged() {
  GetNetworkService()->OnTrustStoreChanged();
}

void NetworkServiceClient::OnClientCertStoreChanged() {
  GetNetworkService()->OnClientCertStoreChanged();
}

void NetworkServiceClient::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  GetNetworkService()->OnMemoryPressure(memory_pressure_level);
}

void NetworkServiceClient::OnPeerToPeerConnectionsCountChange(uint32_t count) {
  GetNetworkService()->OnPeerToPeerConnectionsCountChange(count);
}

#if BUILDFLAG(IS_ANDROID)
void NetworkServiceClient::OnApplicationStateChange(
    base::android::ApplicationState state) {
  GetNetworkService()->OnApplicationStateChange(state);
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
void NetworkServiceClient::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  network_change_manager_->OnNetworkChanged(
      false /* dns_changed */, false /* ip_address_changed */,
      true /* connection_type_changed */, network::mojom::ConnectionType(type),
      false /* connection_subtype_changed */,
      network::mojom::ConnectionSubtype(
          net::NetworkChangeNotifier::GetConnectionSubtype()));
}

void NetworkServiceClient::OnMaxBandwidthChanged(
    double max_bandwidth_mbps,
    net::NetworkChangeNotifier::ConnectionType type) {
  // The connection subtype change will trigger a max bandwidth change in the
  // network service notifier.
  network_change_manager_->OnNetworkChanged(
      false /* dns_changed */, false /* ip_address_changed */,
      false /* connection_type_changed */, network::mojom::ConnectionType(type),
      true /* connection_subtype_changed */,
      network::mojom::ConnectionSubtype(
          net::NetworkChangeNotifier::GetConnectionSubtype()));
}

void NetworkServiceClient::OnIPAddressChanged() {
  network_change_manager_->OnNetworkChanged(
      false /* dns_changed */, true /* ip_address_changed */,
      false /* connection_type_changed */,
      network::mojom::ConnectionType(
          net::NetworkChangeNotifier::GetConnectionType()),
      false /* connection_subtype_changed */,
      network::mojom::ConnectionSubtype(
          net::NetworkChangeNotifier::GetConnectionSubtype()));
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
mojo::PendingRemote<network::mojom::SocketBroker>
NetworkServiceClient::BindSocketBroker() {
  return socket_broker_.BindNewRemote();
}
#endif  // BUILDFLAG(IS_WIN)

mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
NetworkServiceClient::BindURLLoaderNetworkServiceObserver() {
  mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver> remote;
  url_loader_network_service_observers_.Add(
      this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void NetworkServiceClient::OnNetworkServiceInitialized(
    network::mojom::NetworkService* service) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
  bool add_ncn_observers = true;
#if BUILDFLAG(IS_LINUX)
  add_ncn_observers = base::FeatureList::IsEnabled(
      net::features::kAddressTrackerLinuxIsProxied);
#endif  // BUILDFLAG(IS_LINUX)
  if (IsOutOfProcessNetworkService() && add_ncn_observers) {
    DCHECK(!net::NetworkChangeNotifier::CreateIfNeeded());
    service->GetNetworkChangeManager(
        network_change_manager_.BindNewPipeAndPassReceiver());
#if BUILDFLAG(IS_LINUX)
    // Keep the tracking AddressTrackerLinux in sync with the caching version in
    // the network service, which cannot use AddressTrackerLinux in the sandbox.
    mojo::PendingAssociatedRemote<
        network::mojom::NetworkInterfaceChangeListener>
        network_interface_change_listener_pending;
    network_change_manager_->BindNetworkInterfaceChangeListener(
        network_interface_change_listener_pending
            .InitWithNewEndpointAndPassReceiver());
    // Have the AddressTrackerLinux send any changes to the AddressMap or set of
    // online links over |network_interface_change_listener_pending|.
    auto diff_callback_helper = std::make_unique<NetworkInterfaceChangeHelper>(
        std::move(network_interface_change_listener_pending));
    net::NetworkChangeNotifier::GetAddressMapOwner()
        ->GetAddressTrackerLinux()
        ->SetDiffCallback(
            base::BindRepeating(&NetworkInterfaceChangeHelper::
                                    SendAddressTrackerDiffsToNetworkService,
                                std::move(diff_callback_helper)));
#endif  // BUILDFLAG(IS_LINUX)
    net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
    net::NetworkChangeNotifier::AddMaxBandwidthObserver(this);
    net::NetworkChangeNotifier::AddIPAddressObserver(this);
  }
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
}

void NetworkServiceClient::OnSSLCertificateError(
    const GURL& url,
    int net_error,
    const net::SSLInfo& ssl_info,
    bool fatal,
    OnSSLCertificateErrorCallback response) {
  std::move(response).Run(net_error);
}

void NetworkServiceClient::OnCertificateRequested(
    const std::optional<base::UnguessableToken>& window_id,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
    mojo::PendingRemote<network::mojom::ClientCertificateResponder>
        cert_responder_remote) {
  mojo::Remote<network::mojom::ClientCertificateResponder> cert_responder(
      std::move(cert_responder_remote));
  cert_responder->CancelRequest();
}

void NetworkServiceClient::OnAuthRequired(
    const std::optional<base::UnguessableToken>& window_id,
    int32_t request_id,
    const GURL& url,
    bool first_auth_attempt,
    const net::AuthChallengeInfo& auth_info,
    const scoped_refptr<net::HttpResponseHeaders>& head_headers,
    mojo::PendingRemote<network::mojom::AuthChallengeResponder>
        auth_challenge_responder) {
  mojo::Remote<network::mojom::AuthChallengeResponder>
      auth_challenge_responder_remote(std::move(auth_challenge_responder));
  auth_challenge_responder_remote->OnAuthCredentials(std::nullopt);
}

void NetworkServiceClient::OnPrivateNetworkAccessPermissionRequired(
    const GURL& url,
    const net::IPAddress& ip_address,
    const std::optional<std::string>& private_network_device_id,
    const std::optional<std::string>& private_network_device_name,
    OnPrivateNetworkAccessPermissionRequiredCallback callback) {
  std::move(callback).Run(false);
}

void NetworkServiceClient::OnClearSiteData(
    const GURL& url,
    const std::string& header_value,
    int load_flags,
    const std::optional<net::CookiePartitionKey>& cookie_partition_key,
    bool partitioned_state_allowed_only,
    OnClearSiteDataCallback callback) {
  std::move(callback).Run();
}

void NetworkServiceClient::OnLoadingStateUpdate(
    network::mojom::LoadInfoPtr info,
    OnLoadingStateUpdateCallback callback) {
  std::move(callback).Run();
}

void NetworkServiceClient::OnDataUseUpdate(
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {
  GetContentClient()->browser()->OnNetworkServiceDataUseUpdate(
      GlobalRenderFrameHostId(), network_traffic_annotation_id_hash, recv_bytes,
      sent_bytes);
}

void NetworkServiceClient::OnSharedStorageHeaderReceived(
    const url::Origin& request_origin,
    std::vector<network::mojom::SharedStorageOperationPtr> operations,
    OnSharedStorageHeaderReceivedCallback callback) {
  std::move(callback).Run();
}

void NetworkServiceClient::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderNetworkServiceObserver>
        observer) {
  url_loader_network_service_observers_.Add(this, std::move(observer));
}

void NetworkServiceClient::OnWebSocketConnectedToPrivateNetwork(
    network::mojom::IPAddressSpace ip_address_space) {}

}  // namespace content

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network_service_client.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/threading/sequence_bound.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/browsing_data/clear_site_data_handler.h"
#include "content/browser/buildflags.h"
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
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/network_service_util.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/content_uri_utils.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/task/current_thread.h"
#endif

namespace content {

NetworkServiceClient::NetworkServiceClient()
#if BUILDFLAG(IS_ANDROID)
    : app_status_listener_(base::android::ApplicationStatusListener::New(
          base::BindRepeating(&NetworkServiceClient::OnApplicationStateChange,
                              base::Unretained(this))))
#endif
{

#if BUILDFLAG(IS_MAC)
  if (base::CurrentUIThread::IsSet())  // Not set in some unit tests.
    net::CertDatabase::GetInstance()->StartListeningForKeychainEvents();
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
#if BUILDFLAG(IS_ANDROID)
    net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
    net::NetworkChangeNotifier::RemoveMaxBandwidthObserver(this);
    net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
#endif
  }
}

void NetworkServiceClient::OnCertDBChanged() {
  GetNetworkService()->OnCertDBChanged();
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
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(USE_SOCKET_BROKER)
mojo::PendingRemote<network::mojom::SocketBroker>
NetworkServiceClient::BindSocketBroker() {
  return socket_broker_.BindNewRemote();
}
#endif  // BUILDFLAG(USE_SOCKET_BROKER)

mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
NetworkServiceClient::BindURLLoaderNetworkServiceObserver() {
  mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver> remote;
  url_loader_network_service_observers_.Add(
      this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void NetworkServiceClient::OnNetworkServiceInitialized(
    network::mojom::NetworkService* service) {
#if BUILDFLAG(IS_ANDROID)
  if (IsOutOfProcessNetworkService()) {
    DCHECK(!net::NetworkChangeNotifier::CreateIfNeeded());
    service->GetNetworkChangeManager(
        network_change_manager_.BindNewPipeAndPassReceiver());
    net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
    net::NetworkChangeNotifier::AddMaxBandwidthObserver(this);
    net::NetworkChangeNotifier::AddIPAddressObserver(this);
  }
#endif
}

void NetworkServiceClient::OnSSLCertificateError(
    const GURL& url,
    int net_error,
    const net::SSLInfo& ssl_info,
    bool fatal,
    OnSSLCertificateErrorCallback response) {
  std::move(response).Run(net::ERR_INSECURE_RESPONSE);
}

void NetworkServiceClient::OnCertificateRequested(
    const absl::optional<base::UnguessableToken>& window_id,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
    mojo::PendingRemote<network::mojom::ClientCertificateResponder>
        cert_responder_remote) {
  mojo::Remote<network::mojom::ClientCertificateResponder> cert_responder(
      std::move(cert_responder_remote));

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          network::switches::kIgnoreUrlFetcherCertRequests)) {
    cert_responder->ContinueWithoutCertificate();
    return;
  }
  cert_responder->CancelRequest();
}

void NetworkServiceClient::OnAuthRequired(
    const absl::optional<base::UnguessableToken>& window_id,
    uint32_t request_id,
    const GURL& url,
    bool first_auth_attempt,
    const net::AuthChallengeInfo& auth_info,
    const scoped_refptr<net::HttpResponseHeaders>& head_headers,
    mojo::PendingRemote<network::mojom::AuthChallengeResponder>
        auth_challenge_responder) {
  mojo::Remote<network::mojom::AuthChallengeResponder>
      auth_challenge_responder_remote(std::move(auth_challenge_responder));
  auth_challenge_responder_remote->OnAuthCredentials(absl::nullopt);
}

void NetworkServiceClient::OnClearSiteData(
    const GURL& url,
    const std::string& header_value,
    int load_flags,
    const absl::optional<net::CookiePartitionKey>& cookie_partition_key,
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

void NetworkServiceClient::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderNetworkServiceObserver>
        observer) {
  url_loader_network_service_observers_.Add(this, std::move(observer));
}

}  // namespace content

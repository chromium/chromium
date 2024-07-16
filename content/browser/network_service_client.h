// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_SERVICE_CLIENT_H_
#define CONTENT_BROWSER_NETWORK_SERVICE_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/memory_pressure_listener.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/network/socket_broker_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cert/cert_database.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace content {

class WebRtcConnectionsObserver;

class NetworkServiceClient
    : public network::mojom::URLLoaderNetworkServiceObserver,
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
      public net::NetworkChangeNotifier::ConnectionTypeObserver,
      public net::NetworkChangeNotifier::MaxBandwidthObserver,
      public net::NetworkChangeNotifier::IPAddressObserver,
#endif
      public net::CertDatabase::Observer {
 public:
  NetworkServiceClient();

  NetworkServiceClient(const NetworkServiceClient&) = delete;
  NetworkServiceClient& operator=(const NetworkServiceClient&) = delete;

  ~NetworkServiceClient() override;

  mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
  BindURLLoaderNetworkServiceObserver();

  // Called when SetParams() is called on the associated network service.
  void OnNetworkServiceInitialized(network::mojom::NetworkService* service);

  // net::CertDatabase::Observer implementation:
  void OnTrustStoreChanged() override;
  void OnClientCertStoreChanged() override;

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_presure_level);

  // Called when there is a change in the count of media connections that
  // require low network latency.
  void OnPeerToPeerConnectionsCountChange(uint32_t count);

#if BUILDFLAG(IS_ANDROID)
  void OnApplicationStateChange(base::android::ApplicationState state);
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
  // net::NetworkChangeNotifier::ConnectionTypeObserver implementation:
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // net::NetworkChangeNotifier::MaxBandwidthObserver implementation:
  void OnMaxBandwidthChanged(
      double max_bandwidth_mbps,
      net::NetworkChangeNotifier::ConnectionType type) override;

  // net::NetworkChangeNotifier::IPAddressObserver implementation:
  void OnIPAddressChanged() override;
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
  // Called when the network service sandbox is enabled.
  mojo::PendingRemote<network::mojom::SocketBroker> BindSocketBroker();
#endif

 private:
  // network::mojom::URLLoaderNetworkServiceObserver overrides.
  void OnSSLCertificateError(const GURL& url,
                             int net_error,
                             const net::SSLInfo& ssl_info,
                             bool fatal,
                             OnSSLCertificateErrorCallback response) override;
  void OnCertificateRequested(
      const std::optional<base::UnguessableToken>& window_id,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      mojo::PendingRemote<network::mojom::ClientCertificateResponder>
          cert_responder) override;
  void OnAuthRequired(
      const std::optional<base::UnguessableToken>& window_id,
      int32_t request_id,
      const GURL& url,
      bool first_auth_attempt,
      const net::AuthChallengeInfo& auth_info,
      const scoped_refptr<net::HttpResponseHeaders>& head_headers,
      mojo::PendingRemote<network::mojom::AuthChallengeResponder>
          auth_challenge_responder) override;
  void OnPrivateNetworkAccessPermissionRequired(
      const GURL& url,
      const net::IPAddress& ip_address,
      const std::optional<std::string>& private_network_device_id,
      const std::optional<std::string>& private_network_device_name,
      OnPrivateNetworkAccessPermissionRequiredCallback callback) override;
  void OnClearSiteData(
      const GURL& url,
      const std::string& header_value,
      int load_flags,
      const std::optional<net::CookiePartitionKey>& cookie_partition_key,
      bool partitioned_state_allowed_only,
      OnClearSiteDataCallback callback) override;
  void OnLoadingStateUpdate(network::mojom::LoadInfoPtr info,
                            OnLoadingStateUpdateCallback callback) override;
  void OnDataUseUpdate(int32_t network_traffic_annotation_id_hash,
                       int64_t recv_bytes,
                       int64_t sent_bytes) override;
  void OnSharedStorageHeaderReceived(
      const url::Origin& request_origin,
      std::vector<network::mojom::SharedStorageOperationPtr> operations,
      OnSharedStorageHeaderReceivedCallback callback) override;
  void Clone(
      mojo::PendingReceiver<network::mojom::URLLoaderNetworkServiceObserver>
          listener) override;
  void OnWebSocketConnectedToPrivateNetwork(
      network::mojom::IPAddressSpace ip_address_space) override;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  std::unique_ptr<WebRtcConnectionsObserver> webrtc_connections_observer_;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<base::android::ApplicationStatusListener>
      app_status_listener_;
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
  mojo::Remote<network::mojom::NetworkChangeManager> network_change_manager_;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
  SocketBrokerImpl socket_broker_;
#endif  // BUILDFLAG(IS_WIN)

  mojo::ReceiverSet<network::mojom::URLLoaderNetworkServiceObserver>
      url_loader_network_service_observers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_SERVICE_CLIENT_H_

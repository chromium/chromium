// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_SERVICE_CLIENT_H_
#define CONTENT_BROWSER_NETWORK_SERVICE_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cert/cert_database.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace content {

class WebRtcConnectionsObserver;

class CONTENT_EXPORT NetworkServiceClient
    : public network::mojom::URLLoaderNetworkServiceObserver,
#if defined(OS_ANDROID)
      public net::NetworkChangeNotifier::ConnectionTypeObserver,
      public net::NetworkChangeNotifier::MaxBandwidthObserver,
      public net::NetworkChangeNotifier::IPAddressObserver,
      public net::NetworkChangeNotifier::DNSObserver,
#endif
      public net::CertDatabase::Observer {
 public:
  NetworkServiceClient();
  ~NetworkServiceClient() override;

  mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
  BindURLLoaderNetworkServiceObserver();

  // net::CertDatabase::Observer implementation:
  void OnCertDBChanged() override;

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_presure_level);

  // Called when there is a change in the count of media connections that
  // require low network latency.
  void OnPeerToPeerConnectionsCountChange(uint32_t count);

#if defined(OS_ANDROID)
  void OnApplicationStateChange(base::android::ApplicationState state);

  // net::NetworkChangeNotifier::ConnectionTypeObserver implementation:
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // net::NetworkChangeNotifier::MaxBandwidthObserver implementation:
  void OnMaxBandwidthChanged(
      double max_bandwidth_mbps,
      net::NetworkChangeNotifier::ConnectionType type) override;

  // net::NetworkChangeNotifier::IPAddressObserver implementation:
  void OnIPAddressChanged() override;

  // net::NetworkChangeNotifier::DNSObserver implementation:
  void OnDNSChanged() override;
#endif

 private:
  // network::mojom::URLLoaderNetworkServiceObserver overrides.
  void OnSSLCertificateError(const GURL& url,
                             int net_error,
                             const net::SSLInfo& ssl_info,
                             bool fatal,
                             OnSSLCertificateErrorCallback response) override;
  void OnCertificateRequested(
      const absl::optional<base::UnguessableToken>& window_id,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      mojo::PendingRemote<network::mojom::ClientCertificateResponder>
          cert_responder) override;
  void OnAuthRequired(
      const absl::optional<base::UnguessableToken>& window_id,
      uint32_t request_id,
      const GURL& url,
      bool first_auth_attempt,
      const net::AuthChallengeInfo& auth_info,
      const scoped_refptr<net::HttpResponseHeaders>& head_headers,
      mojo::PendingRemote<network::mojom::AuthChallengeResponder>
          auth_challenge_responder) override;
  void OnClearSiteData(const GURL& url,
                       const std::string& header_value,
                       int load_flags,
                       OnClearSiteDataCallback callback) override;
  void OnLoadingStateUpdate(network::mojom::LoadInfoPtr info,
                            OnLoadingStateUpdateCallback callback) override;
  void OnDataUseUpdate(int32_t network_traffic_annotation_id_hash,
                       int64_t recv_bytes,
                       int64_t sent_bytes) override;
  void Clone(
      mojo::PendingReceiver<network::mojom::URLLoaderNetworkServiceObserver>
          listener) override;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  std::unique_ptr<WebRtcConnectionsObserver> webrtc_connections_observer_;

#if defined(OS_ANDROID)
  std::unique_ptr<base::android::ApplicationStatusListener>
      app_status_listener_;
  mojo::Remote<network::mojom::NetworkChangeManager> network_change_manager_;
#endif

  mojo::ReceiverSet<network::mojom::URLLoaderNetworkServiceObserver>
      url_loader_network_service_observers_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_SERVICE_CLIENT_H_

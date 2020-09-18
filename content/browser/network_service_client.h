// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_SERVICE_IMPL_H_
#define CONTENT_BROWSER_NETWORK_SERVICE_IMPL_H_

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
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cert/cert_database.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace content {

class WebRtcConnectionsObserver;

class CONTENT_EXPORT NetworkServiceClient
    : public network::mojom::NetworkServiceClient,
#if defined(OS_ANDROID)
      public net::NetworkChangeNotifier::ConnectionTypeObserver,
      public net::NetworkChangeNotifier::MaxBandwidthObserver,
      public net::NetworkChangeNotifier::IPAddressObserver,
      public net::NetworkChangeNotifier::DNSObserver,
#endif
      public net::CertDatabase::Observer {
 public:
  explicit NetworkServiceClient(
      mojo::PendingReceiver<network::mojom::NetworkServiceClient>
          network_service_client_receiver);
  ~NetworkServiceClient() override;

  // network::mojom::NetworkServiceClient implementation:
  void OnLoadingStateUpdate(std::vector<network::mojom::LoadInfoPtr> infos,
                            OnLoadingStateUpdateCallback callback) override;
  void OnDataUseUpdate(int32_t network_traffic_annotation_id_hash,
                       int64_t recv_bytes,
                       int64_t sent_bytes) override;
  void OnRawRequest(
      int32_t process_id,
      int32_t routing_id,
      const std::string& devtools_request_id,
      const net::CookieAccessResultList& cookies_with_access_result,
      std::vector<network::mojom::HttpRawHeaderPairPtr> headers) override;
  void OnRawResponse(
      int32_t process_id,
      int32_t routing_id,
      const std::string& devtools_request_id,
      const net::CookieAndLineAccessResultList& cookies_with_access_result,
      std::vector<network::mojom::HttpRawHeaderPairPtr> headers,
      const base::Optional<std::string>& raw_response_headers) override;
  void OnCorsPreflightRequest(int32_t process_id,
                              int32_t render_frame_id,
                              const base::UnguessableToken& devtool_request_id,
                              const network::ResourceRequest& request,
                              const GURL& initiator_url) override;
  void OnCorsPreflightResponse(
      int32_t process_id,
      int32_t render_frame_id,
      const base::UnguessableToken& devtool_request_id,
      const GURL& url,
      network::mojom::URLResponseHeadPtr head) override;
  void OnCorsPreflightRequestCompleted(
      int32_t process_id,
      int32_t render_frame_id,
      const base::UnguessableToken& devtool_request_id,
      const network::URLLoaderCompletionStatus& status) override;

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
  mojo::Receiver<network::mojom::NetworkServiceClient> receiver_;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  std::unique_ptr<WebRtcConnectionsObserver> webrtc_connections_observer_;

#if defined(OS_ANDROID)
  std::unique_ptr<base::android::ApplicationStatusListener>
      app_status_listener_;
  mojo::Remote<network::mojom::NetworkChangeManager> network_change_manager_;
#endif

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_SERVICE_IMPL_H_

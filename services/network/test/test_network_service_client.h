// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_NETWORK_SERVICE_CLIENT_H_
#define SERVICES_NETWORK_TEST_TEST_NETWORK_SERVICE_CLIENT_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace network {

// A helper class with a basic NetworkServiceClient implementation for use in
// unittests, which may need an implementation (for things like file uploads),
// but don't have the real implementation available.
class TestNetworkServiceClient : public network::mojom::NetworkServiceClient {
 public:
  TestNetworkServiceClient();
  explicit TestNetworkServiceClient(mojom::NetworkServiceClientRequest request);
  ~TestNetworkServiceClient() override;

  void DisableUploads();
  void EnableUploads();

  // network::mojom::NetworkServiceClient implementation:
  void OnAuthRequired(
      uint32_t process_id,
      uint32_t routing_id,
      uint32_t request_id,
      const GURL& url,
      const GURL& site_for_cookies,
      bool first_auth_attempt,
      const scoped_refptr<net::AuthChallengeInfo>& auth_info,
      int32_t resource_type,
      const base::Optional<ResourceResponseHead>& head,
      mojom::AuthChallengeResponderPtr auth_challenge_responder) override;
  void OnCertificateRequested(
      uint32_t process_id,
      uint32_t routing_id,
      uint32_t request_id,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      mojom::NetworkServiceClient::OnCertificateRequestedCallback callback)
      override;
  void OnSSLCertificateError(uint32_t process_id,
                             uint32_t routing_id,
                             uint32_t request_id,
                             int32_t resource_type,
                             const GURL& url,
                             const net::SSLInfo& ssl_info,
                             bool fatal,
                             OnSSLCertificateErrorCallback response) override;
  void OnCookiesRead(int process_id,
                     int routing_id,
                     const GURL& url,
                     const GURL& first_party_url,
                     const net::CookieList& cookie_list,
                     bool blocked_by_policy) override;
  void OnCookieChange(int process_id,
                      int routing_id,
                      const GURL& url,
                      const GURL& first_party_url,
                      const net::CanonicalCookie& cookie,
                      bool blocked_by_policy) override;
#if defined(OS_CHROMEOS)
  void OnUsedTrustAnchor(const std::string& username_hash) override;
#endif
  void OnFileUploadRequested(uint32_t process_id,
                             bool async,
                             const std::vector<base::FilePath>& file_paths,
                             OnFileUploadRequestedCallback callback) override;
  void OnLoadingStateUpdate(std::vector<mojom::LoadInfoPtr> infos,
                            OnLoadingStateUpdateCallback callback) override;
  void OnClearSiteData(int process_id,
                       int routing_id,
                       const GURL& url,
                       const std::string& header_value,
                       int load_flags,
                       OnClearSiteDataCallback callback) override;
  void OnDataUseUpdate(int32_t network_traffic_annotation_id_hash,
                       int64_t recv_bytes,
                       int64_t sent_bytes) override;

 private:
  bool enable_uploads_;
  mojo::Binding<mojom::NetworkServiceClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkServiceClient);
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_NETWORK_SERVICE_CLIENT_H_

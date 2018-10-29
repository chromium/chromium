// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_SERVICE_IMPL_H_
#define CONTENT_BROWSER_NETWORK_SERVICE_IMPL_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/cert/cert_database.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace content {

class CONTENT_EXPORT NetworkServiceClient
    : public network::mojom::NetworkServiceClient,
      public net::CertDatabase::Observer {
 public:
  explicit NetworkServiceClient(network::mojom::NetworkServiceClientRequest
                                    network_service_client_request);
  ~NetworkServiceClient() override;

  // network::mojom::NetworkServiceClient implementation:
  void OnAuthRequired(uint32_t process_id,
                      uint32_t routing_id,
                      uint32_t request_id,
                      const GURL& url,
                      const GURL& site_for_cookies,
                      bool first_auth_attempt,
                      const scoped_refptr<net::AuthChallengeInfo>& auth_info,
                      int32_t resource_type,
                      const base::Optional<network::ResourceResponseHead>& head,
                      network::mojom::AuthChallengeResponderPtr
                          auth_challenge_responder) override;
  void OnCertificateRequested(
      uint32_t process_id,
      uint32_t routing_id,
      uint32_t request_id,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      network::mojom::NetworkServiceClient::OnCertificateRequestedCallback
          callback) override;
  void OnSSLCertificateError(uint32_t process_id,
                             uint32_t routing_id,
                             uint32_t request_id,
                             int32_t resource_type,
                             const GURL& url,
                             const net::SSLInfo& ssl_info,
                             bool fatal,
                             OnSSLCertificateErrorCallback response) override;
#if defined(OS_CHROMEOS)
  void OnUsedTrustAnchor(const std::string& username_hash) override;
#endif
  void OnFileUploadRequested(uint32_t process_id,
                             bool async,
                             const std::vector<base::FilePath>& file_paths,
                             OnFileUploadRequestedCallback callback) override;
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
  void OnLoadingStateUpdate(std::vector<network::mojom::LoadInfoPtr> infos,
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

  // net::CertDatabase::Observer implementation:
  void OnCertDBChanged() override;

#if defined(OS_ANDROID)
  void OnApplicationStateChange(base::android::ApplicationState state);
#endif

 private:
  mojo::Binding<network::mojom::NetworkServiceClient> binding_;

#if defined(OS_ANDROID)
  std::unique_ptr<base::android::ApplicationStatusListener>
      app_status_listener_;
#endif

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_SERVICE_IMPL_H_

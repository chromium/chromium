// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_NETWORK_CONTEXT_CLIENT_H_
#define SERVICES_NETWORK_TEST_TEST_NETWORK_CONTEXT_CLIENT_H_

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace network {

// A helper class with a basic NetworkContextClient implementation for use in
// unittests, so they can just override the parts they need.
class TestNetworkContextClient : public network::mojom::NetworkContextClient {
 public:
  TestNetworkContextClient();
  explicit TestNetworkContextClient(
      mojo::PendingReceiver<mojom::NetworkContextClient> receiver);
  ~TestNetworkContextClient() override;

  void set_upload_files_invalid(bool upload_files_invalid) {
    upload_files_invalid_ = upload_files_invalid;
  }
  void set_ignore_last_upload_file(bool ignore_last_upload_file) {
    ignore_last_upload_file_ = ignore_last_upload_file;
  }

  void OnAuthRequired(const base::Optional<base::UnguessableToken>& window_id,
                      uint32_t process_id,
                      uint32_t routing_id,
                      uint32_t request_id,
                      const GURL& url,
                      bool first_auth_attempt,
                      const net::AuthChallengeInfo& auth_info,
                      network::mojom::URLResponseHeadPtr head,
                      mojo::PendingRemote<mojom::AuthChallengeResponder>
                          auth_challenge_responder) override {}
  void OnCertificateRequested(
      const base::Optional<base::UnguessableToken>& window_id,
      uint32_t process_id,
      uint32_t routing_id,
      uint32_t request_id,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      mojo::PendingRemote<mojom::ClientCertificateResponder>
          client_cert_responder) override {}
  void OnSSLCertificateError(uint32_t process_id,
                             uint32_t routing_id,
                             const GURL& url,
                             int net_error,
                             const net::SSLInfo& ssl_info,
                             bool fatal,
                             OnSSLCertificateErrorCallback response) override {}
  void OnFileUploadRequested(uint32_t process_id,
                             bool async,
                             const std::vector<base::FilePath>& file_paths,
                             OnFileUploadRequestedCallback callback) override;
  void OnCanSendReportingReports(
      const std::vector<url::Origin>& origins,
      OnCanSendReportingReportsCallback callback) override {}
  void OnCanSendDomainReliabilityUpload(
      const GURL& origin,
      OnCanSendDomainReliabilityUploadCallback callback) override {}
  void OnClearSiteData(uint32_t process_id,
                       int32_t routing_id,
                       const GURL& url,
                       const std::string& header_value,
                       int32_t load_flags,
                       OnClearSiteDataCallback callback) override {}
  void OnCookiesChanged(
      bool is_service_worker,
      int32_t process_id,
      int32_t routing_id,
      const GURL& url,
      const GURL& site_for_cookies,
      const std::vector<net::CookieWithStatus>& cookie_list) override {}
  void OnCookiesRead(
      bool is_service_worker,
      int32_t process_id,
      int32_t routing_id,
      const GURL& url,
      const GURL& site_for_cookies,
      const std::vector<net::CookieWithStatus>& cookie_list) override {}
#if defined(OS_ANDROID)
  void OnGenerateHttpNegotiateAuthToken(
      const std::string& server_auth_token,
      bool can_delegate,
      const std::string& auth_negotiate_android_account_type,
      const std::string& spn,
      OnGenerateHttpNegotiateAuthTokenCallback callback) override {}
#endif
#if defined(OS_CHROMEOS)
  void OnTrustAnchorUsed() override {}
#endif

 private:
  mojo::Receiver<mojom::NetworkContextClient> receiver_;
  bool upload_files_invalid_ = false;
  bool ignore_last_upload_file_ = false;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_NETWORK_CONTEXT_CLIENT_H_

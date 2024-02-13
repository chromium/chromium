// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_NETWORK_CONTEXT_CLIENT_H_
#define SERVICES_NETWORK_TEST_TEST_NETWORK_CONTEXT_CLIENT_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/mojom/network_context_client.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"

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

  void OnFileUploadRequested(int32_t process_id,
                             bool async,
                             const std::vector<base::FilePath>& file_paths,
                             const GURL& destination_url,
                             OnFileUploadRequestedCallback callback) override;
  void OnCanSendReportingReports(
      const std::vector<url::Origin>& origins,
      OnCanSendReportingReportsCallback callback) override {}
  void OnCanSendDomainReliabilityUpload(
      const url::Origin& origin,
      OnCanSendDomainReliabilityUploadCallback callback) override {}
#if BUILDFLAG(IS_ANDROID)
  void OnGenerateHttpNegotiateAuthToken(
      const std::string& server_auth_token,
      bool can_delegate,
      const std::string& auth_negotiate_android_account_type,
      const std::string& spn,
      OnGenerateHttpNegotiateAuthTokenCallback callback) override {}
#endif
#if BUILDFLAG(IS_CHROMEOS)
  void OnTrustAnchorUsed() override {}
#endif
#if BUILDFLAG(IS_CT_SUPPORTED)
  void OnCanSendSCTAuditingReport(
      OnCanSendSCTAuditingReportCallback callback) override;
  void OnNewSCTAuditingReportSent() override {}
#endif

 private:
  mojo::Receiver<mojom::NetworkContextClient> receiver_;
  bool upload_files_invalid_ = false;
  bool ignore_last_upload_file_ = false;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_NETWORK_CONTEXT_CLIENT_H_

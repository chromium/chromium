// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_NETWORK_CONTEXT_CLIENT_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_NETWORK_CONTEXT_CLIENT_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/mojom/network_context_client.mojom.h"

namespace content {

// This is a NetworkContextClient that purposely does nothing so that no extra
// network traffic can occur during a Prefetch Proxy, potentially causing a
// privacy leak to the user.
class PrefetchNetworkContextClient
    : public network::mojom::NetworkContextClient {
 public:
  PrefetchNetworkContextClient();
  ~PrefetchNetworkContextClient() override;

  // network::mojom::NetworkContextClient implementation:
  void OnFileUploadRequested(int32_t process_id,
                             bool async,
                             const std::vector<base::FilePath>& file_paths,
                             const GURL& destination_url,
                             OnFileUploadRequestedCallback callback) override;
  void OnCanSendReportingReports(
      const std::vector<url::Origin>& origins,
      OnCanSendReportingReportsCallback callback) override;
  void OnCanSendDomainReliabilityUpload(
      const url::Origin& origin,
      OnCanSendDomainReliabilityUploadCallback callback) override;
#if BUILDFLAG(IS_ANDROID)
  void OnGenerateHttpNegotiateAuthToken(
      const std::string& server_auth_token,
      bool can_delegate,
      const std::string& auth_negotiate_android_account_type,
      const std::string& spn,
      OnGenerateHttpNegotiateAuthTokenCallback callback) override;
#endif
#if BUILDFLAG(IS_CHROMEOS)
  void OnTrustAnchorUsed() override;
#endif
#if BUILDFLAG(IS_CT_SUPPORTED)
  void OnCanSendSCTAuditingReport(
      OnCanSendSCTAuditingReportCallback callback) override;
  void OnNewSCTAuditingReportSent() override;
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_NETWORK_CONTEXT_CLIENT_H_

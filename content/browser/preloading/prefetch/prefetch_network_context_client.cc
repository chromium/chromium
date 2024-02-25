// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_network_context_client.h"

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"

namespace content {

PrefetchNetworkContextClient::PrefetchNetworkContextClient() = default;
PrefetchNetworkContextClient::~PrefetchNetworkContextClient() = default;

void PrefetchNetworkContextClient::OnFileUploadRequested(
    int32_t process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    const GURL& destination_url,
    OnFileUploadRequestedCallback callback) {
  std::move(callback).Run(net::ERR_ACCESS_DENIED, std::vector<base::File>());
}

void PrefetchNetworkContextClient::OnCanSendReportingReports(
    const std::vector<url::Origin>& origins,
    OnCanSendReportingReportsCallback callback) {
  std::move(callback).Run(std::vector<url::Origin>());
}

void PrefetchNetworkContextClient::OnCanSendDomainReliabilityUpload(
    const url::Origin& origin,
    OnCanSendDomainReliabilityUploadCallback callback) {
  std::move(callback).Run(false);
}

#if BUILDFLAG(IS_ANDROID)
void PrefetchNetworkContextClient::OnGenerateHttpNegotiateAuthToken(
    const std::string& server_auth_token,
    bool can_delegate,
    const std::string& auth_negotiate_android_account_type,
    const std::string& spn,
    OnGenerateHttpNegotiateAuthTokenCallback callback) {
  std::move(callback).Run(net::ERR_FAILED, server_auth_token);
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
void PrefetchNetworkContextClient::OnTrustAnchorUsed() {}
#endif

#if BUILDFLAG(IS_CT_SUPPORTED)
void PrefetchNetworkContextClient::OnCanSendSCTAuditingReport(
    OnCanSendSCTAuditingReportCallback callback) {
  std::move(callback).Run(false);
}

void PrefetchNetworkContextClient::OnNewSCTAuditingReportSent() {}
#endif

}  // namespace content

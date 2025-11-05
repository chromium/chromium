// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/email_verifier_network_request_manager.h"

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/network_request_manager.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/isolation_info.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/client_security_state.mojom.h"

namespace content::webid {

namespace {
// Path to find the well-known file for email verification.
constexpr char kWellKnownPath[] = "/.well-known/email-verification";

// Well-known file JSON keys
constexpr char kIssuanceEndpointKey[] = "issuance_endpoint";

// Shared between the well-known files and config files
constexpr char kIssuanceTokenKey[] = "issuance_token";

void OnWellKnownParsed(
    EmailVerifierNetworkRequestManager::FetchWellKnownCallback callback,
    const GURL& well_known_url,
    FetchStatus fetch_status,
    data_decoder::DataDecoder::ValueOrError result) {
  EmailVerifierNetworkRequestManager::WellKnown well_known;

  if (fetch_status.parse_status != ParseStatus::kSuccess) {
    std::move(callback).Run(fetch_status, std::move(well_known));
    return;
  }

  const base::Value::Dict* dict = result->GetIfDict();
  if (!dict) {
    std::move(callback).Run(
        {ParseStatus::kInvalidResponseError, fetch_status.response_code},
        std::move(well_known));
    return;
  }

  well_known.issuance_endpoint =
      ExtractEndpoint(well_known_url, *dict, kIssuanceEndpointKey);

  if (well_known.issuance_endpoint.is_empty()) {
    std::move(callback).Run(
        {ParseStatus::kInvalidResponseError, fetch_status.response_code},
        std::move(well_known));
    return;
  }

  std::move(callback).Run({ParseStatus::kSuccess, fetch_status.response_code},
                          std::move(well_known));
}

void OnTokenRequestParsed(
    EmailVerifierNetworkRequestManager::TokenRequestCallback callback,
    FetchStatus fetch_status,
    data_decoder::DataDecoder::ValueOrError result) {
  EmailVerifierNetworkRequestManager::TokenResult token_result;

  bool parse_succeeded = fetch_status.parse_status == ParseStatus::kSuccess;
  if (!parse_succeeded) {
    std::move(callback).Run(fetch_status, std::move(token_result));
    return;
  }

  const base::Value::Dict* response = result->GetIfDict();
  if (!response) {
    fetch_status.parse_status = ParseStatus::kInvalidResponseError;
    std::move(callback).Run(fetch_status, std::move(token_result));
    return;
  }

  const std::string* issuance_token = response->FindString(kIssuanceTokenKey);

  if (issuance_token) {
    token_result.token = base::Value(*issuance_token);
    std::move(callback).Run({ParseStatus::kSuccess, fetch_status.response_code},
                            std::move(token_result));
    return;
  }

  std::move(callback).Run(
      {ParseStatus::kInvalidResponseError, fetch_status.response_code},
      std::move(token_result));
}

}  // namespace

EmailVerifierNetworkRequestManager::WellKnown::WellKnown() = default;
EmailVerifierNetworkRequestManager::WellKnown::~WellKnown() = default;
EmailVerifierNetworkRequestManager::WellKnown::WellKnown(const WellKnown&) =
    default;

EmailVerifierNetworkRequestManager::TokenResult::TokenResult() = default;
EmailVerifierNetworkRequestManager::TokenResult::~TokenResult() = default;
EmailVerifierNetworkRequestManager::TokenResult::TokenResult(TokenResult&&) =
    default;

// static
std::unique_ptr<EmailVerifierNetworkRequestManager>
EmailVerifierNetworkRequestManager::Create(RenderFrameHostImpl* host) {
  return std::make_unique<EmailVerifierNetworkRequestManager>(
      host->GetLastCommittedOrigin(),
      host->GetStoragePartition()->GetURLLoaderFactoryForBrowserProcess(),
      host->BuildClientSecurityState(), host->GetFrameTreeNodeId());
}

EmailVerifierNetworkRequestManager::EmailVerifierNetworkRequestManager(
    const url::Origin& relying_party_origin,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    network::mojom::ClientSecurityStatePtr client_security_state,
    content::FrameTreeNodeId frame_tree_node_id)
    : NetworkRequestManager(
          relying_party_origin,
          loader_factory,
          std::move(client_security_state),
          network::mojom::RequestDestination::kEmailVerification,
          frame_tree_node_id) {}

EmailVerifierNetworkRequestManager::~EmailVerifierNetworkRequestManager() =
    default;

net::NetworkTrafficAnnotationTag
EmailVerifierNetworkRequestManager::CreateTrafficAnnotation() {
  return net::DefineNetworkTrafficAnnotation("email_verification", R"(
        semantics {
          sender: "Email Verification"
          description:
            "The email verification API allows websites to receive a verified "
            "email token from the issuer."
          trigger:
            "A website adds an email field which contains an issuance token, "
            "and the website has an 'emailverified' event listener for the "
            "field."
          data:
            "The request either does not contain any user data or may contain "
            "the email to be verified."
          user_data {
            type: EMAIL
          }
          internal {
            contacts {
              owners: "//content/browser/webid/OWNERS"
            }
          }
          destination: OTHER
          last_reviewed: "2025-10-24"
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "The browser, on behalf of the user, sends a request to the"
            " email provider to obtain a verification token."
          policy_exception_justification:
            "Not implemented, these fetches are only used for web platform "
            "APIs."
        })");
}

void EmailVerifierNetworkRequestManager::FetchWellKnown(
    const GURL& provider,
    FetchWellKnownCallback callback) {
  GURL::Replacements replacements;
  replacements.SetPathStr(kWellKnownPath);
  GURL well_known_url = provider.ReplaceComponents(replacements);

  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateUncredentialedResourceRequest(well_known_url,
                                          /*send_origin=*/false,
                                          /*follow_redirects=*/true);
  DownloadJsonAndParse(
      std::move(resource_request), /*url_encoded_post_data=*/std::nullopt,
      base::BindOnce(&OnWellKnownParsed, std::move(callback), well_known_url));
}

void EmailVerifierNetworkRequestManager::SendTokenRequest(
    const GURL& token_url,
    const std::string& url_encoded_post_data,
    TokenRequestCallback callback) {
  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateCredentialedResourceRequest(
          token_url, CredentialedResourceRequestType::kNoOrigin);
  resource_request->request_initiator = url::Origin();

  DownloadJsonAndParse(
      std::move(resource_request), url_encoded_post_data,
      base::BindOnce(&OnTokenRequestParsed, std::move(callback)));
}

}  // namespace content::webid

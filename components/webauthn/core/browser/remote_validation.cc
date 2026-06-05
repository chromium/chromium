// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/remote_validation.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/content_security_policy/csp_context.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace webauthn {

static const net::NetworkTrafficAnnotationTag kRpIdCheckTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("webauthn_rp_id_check", R"(
        semantics {
          sender: "Web Authentication"
          description:
            "WebAuthn credentials are bound to domain names. If a web site "
            "attempts to use a credential owned by a different domain then a "
            "network request is made to the owning domain to see whether the "
            "calling origin is authorized."
          trigger:
            "A web-site initiates a WebAuthn request and the requested RP ID "
            "cannot be trivially validated."
          user_data {
            type: WEB_CONTENT
          }
          data: "None sent. Response is public information from the target "
                "domain, or an error."
          internal {
            contacts {
              email: "chrome-webauthn@google.com"
            }
          }
          destination: WEBSITE
          last_reviewed: "2023-10-31"
        }
        policy {
          cookies_allowed: NO
          setting: "Not controlled by a setting because the operation is "
            "triggered by web sites and is needed to implement the "
            "WebAuthn API."
          policy_exception_justification:
            "No policy provided because the operation is triggered by "
            "websites to fetch public information. No background activity "
            "occurs."
        })");

// kRpIdMaxBodyBytes is the maximum number of bytes that we'll download in order
// to validate an RP ID.
constexpr size_t kRpIdMaxBodyBytes = 1u << 18;

RemoteValidation::~RemoteValidation() = default;

// static
std::unique_ptr<RemoteValidation> RemoteValidation::Create(
    const url::Origin& caller_origin,
    const std::string& relying_party_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::vector<network::mojom::ContentSecurityPolicyPtr>
        content_security_policies,
    base::OnceClosure log_use_counter_callback,
    base::OnceCallback<void(ValidationStatus)> callback) {
  if (!url_loader_factory) {
    std::move(callback).Run(ValidationStatus::kBadRelyingPartyId);
    return nullptr;
  }

  std::optional<GURL> well_known_url =
      webauthn::GetRemoteValidationUrl(relying_party_id);
  if (!well_known_url.has_value()) {
    std::move(callback).Run(ValidationStatus::kBadRelyingPartyId);
    return nullptr;
  }

  auto network_request = std::make_unique<network::ResourceRequest>();
  network_request->url = *well_known_url;
  network_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  std::unique_ptr<RemoteValidation> validation(new RemoteValidation(
      caller_origin, std::move(content_security_policies),
      std::move(log_use_counter_callback), std::move(callback)));

  validation->CheckCsp(*well_known_url, /*has_followed_redirect=*/false);

  validation->loader_ = network::SimpleURLLoader::Create(
      std::move(network_request), kRpIdCheckTrafficAnnotation);
  validation->loader_->SetTimeoutDuration(base::Seconds(10));
  validation->loader_->SetURLLoaderFactoryOptions(
      network::mojom::kURLLoadOptionBlockAllCookies);
  // Use of base::Unretained here is safe because the RemoteValidation object
  // owns the loader which holds the callback.
  validation->loader_->SetOnRedirectCallback(base::BindRepeating(
      &RemoteValidation::OnRedirect, base::Unretained(validation.get())));
  validation->loader_->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(&RemoteValidation::OnFetchComplete,
                     // `validation` owns the `SimpleURLLoader` so if it's
                     // deleted, the loader will be too.
                     base::Unretained(validation.get())),
      kRpIdMaxBodyBytes);

  return validation;
}

// static
ValidationStatus RemoteValidation::ValidateWellKnownJSON(
    const url::Origin& caller_origin,
    const std::string_view json) {
  // This code processes a .well-known/webauthn JSON. See
  // https://github.com/w3c/webauthn/wiki/Explainer:-Related-origin-requests

  auto result = base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);

  if (!result.has_value()) {
    return ValidationStatus::kJsonParseError;
  }

  const base::ListValue* origins = result->FindList("origins");
  if (!origins) {
    return ValidationStatus::kJsonParseError;
  }

  constexpr size_t kMaxLabels = 5;
  bool hit_limits = false;
  base::flat_set<std::string> labels_seen;
  for (const base::Value& origin_str : *origins) {
    if (!origin_str.is_string()) {
      return ValidationStatus::kJsonParseError;
    }

    const GURL url(origin_str.GetString());
    if (!url.is_valid()) {
      continue;
    }

    const std::string domain =
        net::registry_controlled_domains::GetDomainAndRegistry(
            url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    if (domain.empty()) {
      continue;
    }

    const std::string::size_type dot_index = domain.find('.');
    if (dot_index == std::string::npos) {
      continue;
    }

    const std::string etld_plus_1_label = domain.substr(0, dot_index);
    if (!labels_seen.contains(etld_plus_1_label)) {
      if (labels_seen.size() >= kMaxLabels) {
        hit_limits = true;
        continue;
      }
      labels_seen.insert(etld_plus_1_label);
    }

    const auto origin = url::Origin::Create(url);
    if (origin.IsSameOriginWith(caller_origin)) {
      return ValidationStatus::kSuccess;
    }
  }

  if (hit_limits) {
    return ValidationStatus::kNoJsonMatchHitLimits;
  }
  return ValidationStatus::kNoJsonMatch;
}

RemoteValidation::RemoteValidation(
    const url::Origin& caller_origin,
    std::vector<network::mojom::ContentSecurityPolicyPtr>
        content_security_policies,
    base::OnceClosure log_use_counter_callback,
    base::OnceCallback<void(ValidationStatus)> callback)
    : caller_origin_(caller_origin),
      content_security_policies_(std::move(content_security_policies)),
      log_use_counter_callback_(std::move(log_use_counter_callback)),
      callback_(std::move(callback)) {}

// OnFetchComplete is called when the `.well-known/webauthn` for an
// RP ID has finished downloading.
void RemoteValidation::OnFetchComplete(std::optional<std::string> body) {
  base::UmaHistogramBoolean("WebAuthentication.CspAllow.Remote",
                            !was_disallowed_by_csp_);
  if (log_use_counter_callback_ && was_disallowed_by_csp_) {
    std::move(log_use_counter_callback_).Run();
  }

  if (!body) {
    std::move(callback_).Run(ValidationStatus::kAttemptedFetch);
    return;
  }

  if (loader_->ResponseInfo()->mime_type != "application/json") {
    std::move(callback_).Run(ValidationStatus::kWrongContentType);
    return;
  }

  std::move(callback_).Run(ValidateWellKnownJSON(caller_origin_, *body));
}

void RemoteValidation::OnRedirect(
    const GURL& url_before_redirects,
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* removed_headers) {
  CheckCsp(redirect_info.new_url, /*has_followed_redirect=*/true);
}

void RemoteValidation::CheckCsp(const GURL& url, bool has_followed_redirect) {
  network::CSPContext csp_context;
  bool allowed =
      csp_context
          .IsAllowedByCsp(content_security_policies_,
                          network::mojom::CSPDirectiveName::ConnectSrc, url,
                          url,  // Note: This is supposed to be
                                // `url_before_redirects`, but it doesn't
                                // matter for the purpose of checking
                                // connect-src.
                          has_followed_redirect,
                          /*source_location=*/nullptr,
                          network::CSPContext::CHECK_ENFORCED_CSP,
                          /*is_opaque_fenced_frame=*/false)
          .IsAllowed();
  if (!allowed) {
    was_disallowed_by_csp_ = true;
  }
}

}  // namespace webauthn

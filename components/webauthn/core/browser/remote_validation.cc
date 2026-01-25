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
#include "base/strings/string_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/url_canon.h"

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
    base::OnceCallback<void(ValidationStatus)> callback) {
  if (!url_loader_factory) {
    std::move(callback).Run(ValidationStatus::kBadRelyingPartyId);
    return nullptr;
  }

  // The relying party may allow other origins to use its RP ID based on the
  // contents of a .well-known file.
  std::string canonicalized_domain_storage;
  url::StdStringCanonOutput canon_output(&canonicalized_domain_storage);
  url::CanonHostInfo host_info;
  url::CanonicalizeHostVerbose(relying_party_id,
                               url::Component(0, relying_party_id.size()),
                               &canon_output, &host_info);
  const std::string_view canonicalized_domain = canon_output.view();
  if (host_info.family != url::CanonHostInfo::Family::NEUTRAL ||
      !net::IsCanonicalizedHostCompliant(canonicalized_domain)) {
    // The RP ID must look like a hostname, e.g. not an IP address.
    std::move(callback).Run(ValidationStatus::kBadRelyingPartyId);
    return nullptr;
  }

  constexpr char well_known_url_template[] =
      "https://domain.com/.well-known/webauthn";
  GURL well_known_url(well_known_url_template);
  CHECK(well_known_url.is_valid());

  GURL::Replacements replace_host;
  replace_host.SetHostStr(canonicalized_domain);
  well_known_url = well_known_url.ReplaceComponents(replace_host);

  auto network_request = std::make_unique<network::ResourceRequest>();
  network_request->url = well_known_url;
  network_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  std::unique_ptr<RemoteValidation> validation(
      new RemoteValidation(caller_origin, std::move(callback)));

  validation->loader_ = network::SimpleURLLoader::Create(
      std::move(network_request), kRpIdCheckTrafficAnnotation);
  validation->loader_->SetTimeoutDuration(base::Seconds(10));
  validation->loader_->SetURLLoaderFactoryOptions(
      network::mojom::kURLLoadOptionBlockAllCookies);
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
    base::OnceCallback<void(ValidationStatus)> callback)
    : caller_origin_(caller_origin), callback_(std::move(callback)) {}

// OnFetchComplete is called when the `.well-known/webauthn` for an
// RP ID has finished downloading.
void RemoteValidation::OnFetchComplete(std::optional<std::string> body) {
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

}  // namespace webauthn

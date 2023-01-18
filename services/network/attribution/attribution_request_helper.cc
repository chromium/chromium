// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/attribution_request_helper.h"

#include <functional>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/attribution/attribution_attestation_mediator.h"
#include "services/network/attribution/boringssl_attestation_cryptographer.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/trust_tokens/trust_token_key_commitment_getter.h"
#include "url/origin.h"

namespace network {

namespace {

// Same as attribution_reporting::SuitableOrigin
// TODO(crbug.com/1408181): unify logic across browser and network service
bool IsSuitable(const url::Origin& origin) {
  const std::string& scheme = origin.scheme();
  return (scheme == url::kHttpsScheme || scheme == url::kHttpScheme) &&
         network::IsOriginPotentiallyTrustworthy(origin);
}

bool IsNeededForRequest(const net::HttpRequestHeaders& request_headers) {
  std::string attribution_header;
  bool is_trigger_ping =
      request_headers.GetHeader("Attribution-Reporting-Eligible",
                                &attribution_header) &&
      base::Contains(attribution_header, "trigger");
  return is_trigger_ping;
}

}  // namespace

std::unique_ptr<AttributionRequestHelper>
AttributionRequestHelper::CreateIfNeeded(
    const net::HttpRequestHeaders& request_headers,
    const TrustTokenKeyCommitmentGetter* key_commitment_getter) {
  if (!IsNeededForRequest(request_headers)) {
    return nullptr;
  }

  auto cryptographer = std::make_unique<BoringsslAttestationCryptographer>();

  auto mediator = std::make_unique<AttributionAttestationMediator>(
      key_commitment_getter, std::move(cryptographer));

  return absl::WrapUnique(new AttributionRequestHelper(std::move(mediator)));
}

std::unique_ptr<AttributionRequestHelper>
AttributionRequestHelper::CreateForTesting(
    const net::HttpRequestHeaders& request_headers,
    std::unique_ptr<AttributionAttestationMediator> mediator) {
  if (!IsNeededForRequest(request_headers)) {
    return nullptr;
  }

  return absl::WrapUnique(new AttributionRequestHelper(std::move(mediator)));
}

AttributionRequestHelper::AttributionRequestHelper(
    std::unique_ptr<AttributionAttestationMediator> mediator)
    : aggregatable_report_id_(base::GUID::GenerateRandomV4()),
      mediator_(std::move(mediator)) {
  DCHECK(mediator_);
}

AttributionRequestHelper::~AttributionRequestHelper() = default;

void AttributionRequestHelper::Begin(net::URLRequest& url_request,
                                     base::OnceClosure done) {
  // TODO(crbug.com/1406643): investigate the situations in which
  // `url_request->isolation_info().top_frame_origin()` would not be defined and
  // confirm that it can be relied upon here.
  absl::optional<url::Origin> destination_origin =
      url_request.isolation_info().top_frame_origin();
  if (!destination_origin.has_value() ||
      !IsSuitable(destination_origin.value())) {
    std::move(done).Run();
    return;
  }

  mediator_->GetHeadersForAttestation(
      url_request.url(),
      GenerateTriggerAttestationMessage(destination_origin.value()),
      base::BindOnce(&AttributionRequestHelper::OnDoneGettingHeaders,
                     weak_ptr_factory_.GetWeakPtr(), std::ref(url_request),
                     std::move(done)));
}

void AttributionRequestHelper::OnDoneGettingHeaders(
    net::URLRequest& url_request,
    base::OnceClosure done,
    net::HttpRequestHeaders headers) {
  if (headers.IsEmpty()) {
    std::move(done).Run();
    return;
  }

  set_attestation_headers_ = true;

  for (const auto& header_pair : headers.GetHeaderVector()) {
    url_request.SetExtraRequestHeaderByName(header_pair.key, header_pair.value,
                                            /*overwrite=*/true);
  }

  std::move(done).Run();
}

void AttributionRequestHelper::Finalize(mojom::URLResponseHead& response,
                                        base::OnceClosure done) {
  if (!set_attestation_headers_) {
    std::move(done).Run();
    return;
  }

  mediator_->ProcessAttestationToGetToken(
      *response.headers,
      base::BindOnce(
          &AttributionRequestHelper::OnDoneProcessingAttestationResponse,
          weak_ptr_factory_.GetWeakPtr(), std::ref(response), std::move(done)));
}

void AttributionRequestHelper::OnDoneProcessingAttestationResponse(
    mojom::URLResponseHead& response,
    base::OnceClosure done,
    absl::optional<std::string> maybe_attestation_header) {
  if (!maybe_attestation_header.has_value()) {
    std::move(done).Run();
    return;
  }

  // TODO(crbug.com/1405832) Add attestation header to the response as it gets
  // updated with a trigger attestation property.

  std::move(done).Run();
}

std::string AttributionRequestHelper::GenerateTriggerAttestationMessage(
    const url::Origin& destination_origin) {
  net::SchemefulSite destination_site(destination_origin);

  return base::StrCat({aggregatable_report_id_.AsLowercaseString(),
                       destination_site.Serialize()});
}

}  // namespace network

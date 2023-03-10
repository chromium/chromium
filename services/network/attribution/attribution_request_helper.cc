// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/attribution_request_helper.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "net/base/isolation_info.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "services/network/attribution/attribution_attestation_mediator.h"
#include "services/network/attribution/attribution_attestation_mediator_metrics_recorder.h"
#include "services/network/attribution/boringssl_attestation_cryptographer.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/trigger_attestation.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/trust_tokens/trust_token_key_commitment_getter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {

void RecordDestinationOriginStatus(
    AttributionRequestHelper::DestinationOriginStatus status) {
  base::UmaHistogramEnumeration(
      "Conversions.TriggerAttestation.DestinationOriginStatus", status);
}

// Same as `attribution_reporting::SuitableOrigin`
// TODO(https://crbug.com/1408181): unify logic across browser and network
// service.
bool IsSuitableDestinationOrigin(const url::Origin& origin) {
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

struct AttributionRequestHelper::AttestationOperation {
  explicit AttestationOperation(
      const base::RepeatingCallback<AttributionAttestationMediator()>&
          create_mediator)
      : aggregatable_report_id(base::GUID::GenerateRandomV4()),
        mediator(create_mediator.Run()) {}

  // Returns the message associated to this atttestation operation. It is
  // represented by concatenating a trigger`s `destination_origin` and the
  // `aggregatable_report_id`.
  std::string Message(const url::Origin& destination_origin);

  // TODO(https://crbug.com/1406645): use explicitly spec compliant structure
  base::GUID aggregatable_report_id;

  AttributionAttestationMediator mediator;
};

std::string AttributionRequestHelper::AttestationOperation::Message(
    const url::Origin& destination_origin) {
  net::SchemefulSite destination_site(destination_origin);

  return base::StrCat({aggregatable_report_id.AsLowercaseString(),
                       destination_site.Serialize()});
}

std::unique_ptr<AttributionRequestHelper>
AttributionRequestHelper::CreateIfNeeded(
    const net::HttpRequestHeaders& request_headers,
    const TrustTokenKeyCommitmentGetter* key_commitment_getter) {
  DCHECK(key_commitment_getter);

  if (!base::FeatureList::IsEnabled(
          network::features::kAttributionReportingTriggerAttestation) ||
      !IsNeededForRequest(request_headers)) {
    return nullptr;
  }

  auto create_mediator = base::BindRepeating(
      [](const TrustTokenKeyCommitmentGetter* t) {
        // The key_commitment_getter instance  (`t`) is a singleton owned by
        // NetworkService, it will always outlive this.
        return AttributionAttestationMediator(
            t, std::make_unique<BoringsslAttestationCryptographer>(),
            std::make_unique<AttributionAttestationMediatorMetricsRecorder>());
      },
      key_commitment_getter);
  return base::WrapUnique(
      new AttributionRequestHelper(std::move(create_mediator)));
}

std::unique_ptr<AttributionRequestHelper>
AttributionRequestHelper::CreateForTesting(
    const net::HttpRequestHeaders& request_headers,
    base::RepeatingCallback<AttributionAttestationMediator()> create_mediator) {
  if (!IsNeededForRequest(request_headers)) {
    return nullptr;
  }

  return base::WrapUnique(
      new AttributionRequestHelper(std::move(create_mediator)));
}

AttributionRequestHelper::AttributionRequestHelper(
    base::RepeatingCallback<AttributionAttestationMediator()> create_mediator)
    : create_mediator_(std::move(create_mediator)) {}

AttributionRequestHelper::~AttributionRequestHelper() = default;

void AttributionRequestHelper::Begin(net::URLRequest& request,
                                     base::OnceClosure done) {
  DCHECK(!attestation_operation_);

  // TODO(https://crbug.com/1406643): investigate the situations in which
  // `url_request->isolation_info().top_frame_origin()` would not be defined and
  // confirm that it can be relied upon here.
  if (!request.isolation_info().top_frame_origin().has_value()) {
    RecordDestinationOriginStatus(
        AttributionRequestHelper::DestinationOriginStatus::kMissing);
    std::move(done).Run();
    return;
  }
  has_suitable_destination_origin_ = IsSuitableDestinationOrigin(
      request.isolation_info().top_frame_origin().value());
  RecordDestinationOriginStatus(
      has_suitable_destination_origin_
          ? AttributionRequestHelper::DestinationOriginStatus::kValid
          : AttributionRequestHelper::DestinationOriginStatus::kNonSuitable);
  if (!has_suitable_destination_origin_) {
    std::move(done).Run();
    return;
  }

  attestation_operation_ =
      std::make_unique<AttestationOperation>(create_mediator_);

  attestation_operation_->mediator.GetHeadersForAttestation(
      request.url(),
      attestation_operation_->Message(
          /*destination_origin=*/request.isolation_info()
              .top_frame_origin()
              .value()),
      base::BindOnce(&AttributionRequestHelper::OnDoneGettingHeaders,
                     weak_ptr_factory_.GetWeakPtr(), std::ref(request),
                     std::move(done)));
}

void AttributionRequestHelper::OnDoneGettingHeaders(
    net::URLRequest& request,
    base::OnceClosure done,
    net::HttpRequestHeaders headers) {
  if (headers.IsEmpty()) {
    attestation_operation_ = nullptr;
    std::move(done).Run();
    return;
  }

  for (const auto& header_pair : headers.GetHeaderVector()) {
    request.SetExtraRequestHeaderByName(header_pair.key, header_pair.value,
                                        /*overwrite=*/true);
  }

  std::move(done).Run();
}

void AttributionRequestHelper::OnReceiveRedirect(
    net::URLRequest& request,
    mojom::URLResponseHeadPtr response,
    const net::RedirectInfo& redirect_info,
    base::OnceCallback<void(mojom::URLResponseHeadPtr response)> done) {
  // No operation was started and none will start for the redirect request as
  // the request's destination origin is not suitable. We can return early.
  if (!has_suitable_destination_origin_) {
    std::move(done).Run(std::move(response));
    return;
  }

  mojom::URLResponseHead* raw_response = response.get();
  Finalize(*raw_response,
           base::BindOnce(
               &AttributionRequestHelper::OnDoneFinalizingResponseFromRedirect,
               weak_ptr_factory_.GetWeakPtr(), std::ref(request),
               redirect_info.new_url,
               base::BindOnce(std::move(done), std::move(response))));
}

void AttributionRequestHelper::OnDoneFinalizingResponseFromRedirect(
    net::URLRequest& request,
    const GURL& new_url,
    base::OnceClosure done) {
  // If attribution headers were previously added on the request, we clear them.
  // This avoids leaking headers in a situation where the first request needed
  // attribution headers but the subsequent one does not.
  request.RemoveRequestHeaderByName(
      AttributionAttestationMediator::kTriggerAttestationHeader);
  request.RemoveRequestHeaderByName(kTrustTokensSecTrustTokenVersionHeader);

  // Now that we've finalized the previous operation, we create a new one for
  // the redirect.
  attestation_operation_ =
      std::make_unique<AttestationOperation>(create_mediator_);

  attestation_operation_->mediator.GetHeadersForAttestation(
      new_url,
      attestation_operation_->Message(
          /*destination_origin=*/request.isolation_info()
              .top_frame_origin()
              .value()),
      base::BindOnce(&AttributionRequestHelper::OnDoneGettingHeaders,
                     weak_ptr_factory_.GetWeakPtr(), std::ref(request),
                     std::move(done)));
}

void AttributionRequestHelper::Finalize(mojom::URLResponseHead& response,
                                        base::OnceClosure done) {
  if (!attestation_operation_) {
    std::move(done).Run();
    return;
  }

  attestation_operation_->mediator.ProcessAttestationToGetToken(
      *response.headers,
      base::BindOnce(
          &AttributionRequestHelper::OnDoneProcessingAttestationResponse,
          weak_ptr_factory_.GetWeakPtr(), std::ref(response), std::move(done)));
}

void AttributionRequestHelper::OnDoneProcessingAttestationResponse(
    mojom::URLResponseHead& response,
    base::OnceClosure done,
    absl::optional<std::string> maybe_attestation_header) {
  DCHECK(attestation_operation_);
  std::unique_ptr<AttestationOperation> attestation_operation(
      std::move(attestation_operation_));

  if (!maybe_attestation_header.has_value()) {
    std::move(done).Run();
    return;
  }

  response.trigger_attestation = TriggerAttestation::Create(
      /*token=*/*std::move(maybe_attestation_header),
      attestation_operation->aggregatable_report_id.AsLowercaseString());
  std::move(done).Run();
}

}  // namespace network

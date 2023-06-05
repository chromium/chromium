// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/attribution_request_helper.h"

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/uuid.h"
#include "net/base/isolation_info.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "services/network/attribution/attribution_verification_mediator.h"
#include "services/network/attribution/attribution_verification_mediator_metrics_recorder.h"
#include "services/network/attribution/boringssl_verification_cryptographer.h"
#include "services/network/attribution/request_headers_internal.h"
#include "services/network/public/cpp/attribution_reporting_runtime_features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/trigger_verification.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/trust_tokens/trust_token_key_commitment_getter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {

using ::network::mojom::AttributionReportingEligibility;

void RecordDestinationOriginStatus(
    AttributionRequestHelper::DestinationOriginStatus status) {
  base::UmaHistogramEnumeration(
      "Conversions.ReportVerification.DestinationOriginStatus", status);
}

// Same as `attribution_reporting::SuitableOrigin`
// TODO(https://crbug.com/1408181): unify logic across browser and network
// service.
bool IsSuitableDestinationOrigin(const url::Origin& origin) {
  const std::string& scheme = origin.scheme();
  return (scheme == url::kHttpsScheme || scheme == url::kHttpScheme) &&
         network::IsOriginPotentiallyTrustworthy(origin);
}

bool IsNeededForRequest(AttributionReportingEligibility eligibility) {
  switch (eligibility) {
    case AttributionReportingEligibility::kUnset:
    case AttributionReportingEligibility::kEmpty:
    case AttributionReportingEligibility::kEventSource:
    case AttributionReportingEligibility::kNavigationSource:
      return false;
    case AttributionReportingEligibility::kTrigger:
    case AttributionReportingEligibility::kEventSourceOrTrigger:
      return true;
  }
}

AttributionVerificationMediator CreateDefaultMediator(
    const TrustTokenKeyCommitmentGetter* key_commitment_getter) {
  // The TrustTokenKeyCommitmentGetter instance  (`key_commitment_getter`) is a
  // singleton owned by NetworkService, it will always outlive this.
  std::vector<std::unique_ptr<AttributionVerificationMediator::Cryptographer>>
      cryptographers;
  cryptographers.reserve(
      AttributionRequestHelper::kVerificationTokensPerTrigger);
  for (size_t i = 0;
       i < AttributionRequestHelper::kVerificationTokensPerTrigger; ++i) {
    cryptographers.push_back(
        std::make_unique<BoringsslVerificationCryptographer>());
  }

  return AttributionVerificationMediator(
      key_commitment_getter, std::move(cryptographers),
      std::make_unique<AttributionVerificationMediatorMetricsRecorder>());
}

}  // namespace

struct AttributionRequestHelper::VerificationOperation {
  explicit VerificationOperation(
      const base::RepeatingCallback<AttributionVerificationMediator()>&
          create_mediator)
      : mediator(create_mediator.Run()) {
    aggregatable_report_ids.reserve(kVerificationTokensPerTrigger);
    for (size_t i = 0; i < kVerificationTokensPerTrigger; ++i) {
      aggregatable_report_ids.push_back(base::Uuid::GenerateRandomV4());
    }
  }

  // Returns `kVerificationTokensPerTrigger` messages associated to this
  // verification operation. Each message is represented by concatenating the
  // trigger`s `destination_origin` and a generated aggregatable report ID.
  std::vector<std::string> Messages(const url::Origin& destination_origin);

  // A verification operation requests multiple tokens. Each token is signed
  // over a message that includes a different id as we need one id per report
  // that we want to create & verify. With N (id,token) pairs, we can create N
  // verified reports.
  // TODO(https://crbug.com/1406645): use explicitly spec compliant structure.
  std::vector<base::Uuid> aggregatable_report_ids;

  AttributionVerificationMediator mediator;
};

std::vector<std::string>
AttributionRequestHelper::VerificationOperation::Messages(
    const url::Origin& destination_origin) {
  std::string destination_site =
      net::SchemefulSite(destination_origin).Serialize();
  std::vector<std::string> messages;
  messages.reserve(aggregatable_report_ids.size());

  for (const base::Uuid& id : aggregatable_report_ids) {
    messages.push_back(
        base::StrCat({id.AsLowercaseString(), destination_site}));
  }
  return messages;
}

std::unique_ptr<AttributionRequestHelper>
AttributionRequestHelper::CreateIfNeeded(
    AttributionReportingEligibility eligibility,
    const TrustTokenKeyCommitmentGetter* key_commitment_getter) {
  if (!base::FeatureList::IsEnabled(
          network::features::kAttributionReportingReportVerification) ||
      !IsNeededForRequest(eligibility)) {
    return nullptr;
  }

  CHECK(key_commitment_getter);
  auto create_mediator =
      base::BindRepeating(&CreateDefaultMediator, key_commitment_getter);
  return base::WrapUnique(
      new AttributionRequestHelper(std::move(create_mediator)));
}

std::unique_ptr<AttributionRequestHelper>
AttributionRequestHelper::CreateForTesting(
    AttributionReportingEligibility eligibility,
    base::RepeatingCallback<AttributionVerificationMediator()>
        create_mediator) {
  if (!IsNeededForRequest(eligibility)) {
    return nullptr;
  }

  return base::WrapUnique(
      new AttributionRequestHelper(std::move(create_mediator)));
}

AttributionRequestHelper::AttributionRequestHelper(
    base::RepeatingCallback<AttributionVerificationMediator()> create_mediator)
    : create_mediator_(std::move(create_mediator)) {}

AttributionRequestHelper::~AttributionRequestHelper() = default;

void AttributionRequestHelper::Begin(net::URLRequest& request,
                                     base::OnceClosure done) {
  CHECK(!verification_operation_);

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

  verification_operation_ =
      std::make_unique<VerificationOperation>(create_mediator_);

  verification_operation_->mediator.GetHeadersForVerification(
      request.url(),
      verification_operation_->Messages(
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
    verification_operation_ = nullptr;
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
      AttributionVerificationMediator::kReportVerificationHeader);
  request.RemoveRequestHeaderByName(kTrustTokensSecTrustTokenVersionHeader);

  // Now that we've finalized the previous operation, we create a new one for
  // the redirect.
  verification_operation_ =
      std::make_unique<VerificationOperation>(create_mediator_);

  verification_operation_->mediator.GetHeadersForVerification(
      new_url,
      verification_operation_->Messages(
          /*destination_origin=*/request.isolation_info()
              .top_frame_origin()
              .value()),
      base::BindOnce(&AttributionRequestHelper::OnDoneGettingHeaders,
                     weak_ptr_factory_.GetWeakPtr(), std::ref(request),
                     std::move(done)));
}

void AttributionRequestHelper::Finalize(mojom::URLResponseHead& response,
                                        base::OnceClosure done) {
  if (!verification_operation_) {
    std::move(done).Run();
    return;
  }

  verification_operation_->mediator.ProcessVerificationToGetTokens(
      *response.headers,
      base::BindOnce(
          &AttributionRequestHelper::OnDoneProcessingVerificationResponse,
          weak_ptr_factory_.GetWeakPtr(), std::ref(response), std::move(done)));
}

void AttributionRequestHelper::OnDoneProcessingVerificationResponse(
    mojom::URLResponseHead& response,
    base::OnceClosure done,
    std::vector<std::string> redemption_tokens) {
  CHECK(verification_operation_);
  std::unique_ptr<VerificationOperation> verification_operation(
      std::move(verification_operation_));

  if (redemption_tokens.empty()) {
    std::move(done).Run();
    return;
  }

  CHECK_GE(kVerificationTokensPerTrigger, redemption_tokens.size());
  CHECK_GE(verification_operation->aggregatable_report_ids.size(),
           redemption_tokens.size());

  std::vector<TriggerVerification> verifications;
  verifications.reserve(redemption_tokens.size());
  for (size_t i = 0; i < redemption_tokens.size(); ++i) {
    auto verification = TriggerVerification::Create(
        /*token=*/std::move(redemption_tokens.at(i)),
        /*aggregatable_report_id=*/verification_operation
            ->aggregatable_report_ids.at(i)
            .AsLowercaseString());
    CHECK(verification.has_value());
    verifications.push_back(std::move(verification.value()));
  }
  response.trigger_verifications = std::move(verifications);

  std::move(done).Run();
}

// https://wicg.github.io/attribution-reporting-api/#mark-a-request-for-attribution-reporting-eligibility
void SetAttributionReportingHeaders(net::URLRequest& url_request,
                                    const ResourceRequest& request) {
  if (request.attribution_reporting_eligibility ==
      AttributionReportingEligibility::kUnset) {
    return;
  }

  uint64_t grease_bits = base::RandUint64();

  std::string eligible_header = SerializeAttributionReportingEligibleHeader(
      request.attribution_reporting_eligibility,
      AttributionReportingHeaderGreaseOptions::FromBits(grease_bits & 0xff));
  grease_bits >>= 8;

  url_request.SetExtraRequestHeaderByName("Attribution-Reporting-Eligible",
                                          std::move(eligible_header),
                                          /*overwrite=*/true);

  // Note that it's important that the network process check both the
  // base::Feature (which is set from the browser, so trustworthy) and the
  // runtime feature (which can be spoofed in a compromised renderer, so is
  // best-effort).
  if (request.attribution_reporting_runtime_features.Has(
          AttributionReportingRuntimeFeature::kCrossAppWeb) &&
      base::FeatureList::IsEnabled(
          features::kAttributionReportingCrossAppWeb)) {
    url_request.SetExtraRequestHeaderByName(
        "Attribution-Reporting-Support",
        GetAttributionSupportHeader(
            request.attribution_reporting_support,
            AttributionReportingHeaderGreaseOptions::FromBits(grease_bits &
                                                              0xff)),
        /*overwrite=*/true);
    grease_bits >>= 8;
  }
}

}  // namespace network

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_REQUEST_HELPER_H_
#define SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_REQUEST_HELPER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "services/network/public/mojom/attribution.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

class GURL;

namespace net {

class HttpRequestHeaders;
class URLRequest;
struct RedirectInfo;

}  // namespace net

namespace network {

class AttributionVerificationMediator;
class TrustTokenKeyCommitmentGetter;

struct ResourceRequest;

// Class AttributionRequestHelper handles attribution-reporting-api related
// operations (https://github.com/WICG/attribution-reporting-api) that must
// happen in the network service. It is meant to be optionally hooked to a
// url_loader instance.
class AttributionRequestHelper {
 public:
  // In the context of an attribution trigger registration request. The
  // destination origin corresponds to the top_frame origin where the trigger is
  // registered. We use this enum to log the status of this value. We can only
  // proceed with report verification with a valid destination origin.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class DestinationOriginStatus {
    kValid = 0,
    kMissing = 1,
    kNonSuitable = 2,
    kMaxValue = kNonSuitable,
  };

  // The higher the number, the more work and bandwidth is needed to generate
  // report verification tokens. The lower the number, the more likely it is
  // that some reports will be sent without a verification token. Using 3, the
  // probability of failure is ~.04%. See. https://crbug.com/1440744#c1
  static constexpr size_t kVerificationTokensPerTrigger = 3;

  // Creates an AttributionRequestHelper instance if needed.
  //
  // It is needed when it's to be hooked to a request related to attribution;
  // for now only trigger registration ping.
  static std::unique_ptr<AttributionRequestHelper> CreateIfNeeded(
      mojom::AttributionReportingEligibility,
      const TrustTokenKeyCommitmentGetter* key_commitment_getter);

  // Test method which allows to instantiate an AttributionRequestHelper with
  // dependency injection (i.e. `CreateIfNeeded` builds `create_mediator`, this
  // method receives it).
  static std::unique_ptr<AttributionRequestHelper> CreateForTesting(
      mojom::AttributionReportingEligibility,
      base::RepeatingCallback<AttributionVerificationMediator()>
          create_mediator);

  ~AttributionRequestHelper();
  AttributionRequestHelper(const AttributionRequestHelper&) = delete;
  AttributionRequestHelper& operator=(const AttributionRequestHelper&) = delete;

  // Orchestrates report verification by calling the attribution verification
  // mediator and optionally adding headers on the `request`. Externally, it
  // will be called once per request. Internally, on redirection, it will be
  // called by `OnReceivedRedirect`.
  void Begin(net::URLRequest& request, base::OnceClosure done);

  // Orchestrates report verification on a redirection request by `Finalize`-ing
  // an initial request and `Begin`-ing the verification process on the
  // redirection request. A trigger_verification property might be added to the
  // `response`. Verification headers will potentially be added to or removed
  // from the `request`.
  void OnReceiveRedirect(
      net::URLRequest& request,
      mojom::URLResponseHeadPtr response,
      const net::RedirectInfo& redirect_info,
      base::OnceCallback<void(mojom::URLResponseHeadPtr response)> done);

  // Orchestrates report verification by calling the attribution verification
  // mediator with the `response`'s headers. If a verification header is
  // present, it will be processed and removed from the `response`. A
  // trigger_verification property might be added to the `response`. Externally,
  // it will be called at most once per request. Internally, it might be called
  // on redirection by `OnReceivedRedirect`.
  void Finalize(mojom::URLResponseHead& response, base::OnceClosure done);

 private:
  struct VerificationOperation;

  explicit AttributionRequestHelper(
      base::RepeatingCallback<AttributionVerificationMediator()>
          create_mediator);

  // Continuation of `Begin` after asynchronous
  // mediator_::GetHeadersForVerification concludes.
  //
  // `request` and `done` are `Begin`'s parameters, passed on to the
  // continuation. `headers` are headers optionally returned by the
  // attribution verification mediator that wil be added to the request.
  void OnDoneGettingHeaders(net::URLRequest& request,
                            base::OnceClosure done,
                            net::HttpRequestHeaders headers);

  // Continuation of `Redirect` after asynchronous call to `Finalize`. `request`
  // and `done` are `Redirect`'s parameters, passed on to the continuation.
  void OnDoneFinalizingResponseFromRedirect(net::URLRequest& request,
                                            const GURL& new_url,
                                            base::OnceClosure done);

  // Continuation of `Finalize` after asynchronous
  // mediator_::ProcessVerificationToGetTokens concludes.
  //
  // `response` and `done` are `Finalize`'s parameters, passed on to the
  // continuation. `redemption_tokens` is the result from the
  // attribution verification mediator.
  void OnDoneProcessingVerificationResponse(
      mojom::URLResponseHead& response,
      base::OnceClosure done,
      std::vector<std::string> redemption_tokens);

  // A mediator can perform a single verification operation. Each redirect does
  // a verification. We use this callback to generate a new mediator instance
  // per verification operation.
  base::RepeatingCallback<AttributionVerificationMediator()> create_mediator_;

  // One request can lead to multiple report verification operations as each
  // redirect requires a distinct operation. `verification_operation_`
  // will be non-null when an operation is undergoing.
  std::unique_ptr<VerificationOperation> verification_operation_;

  // The destination origin is needed to complete the  verification. On
  // `Request`, we check that it is suitable and update
  // `has_suitable_destination_origin_` accordingly. On `finalize` we check that
  // it is true before proceeding.
  bool has_suitable_destination_origin_ = false;

  base::WeakPtrFactory<AttributionRequestHelper> weak_ptr_factory_{this};
};

// Sets the Attribution Reporting request headers on attribution eligible
// requests. See https://github.com/WICG/attribution-reporting-api.
void SetAttributionReportingHeaders(net::URLRequest&, const ResourceRequest&);

}  // namespace network

#endif  // SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_REQUEST_HELPER_H_

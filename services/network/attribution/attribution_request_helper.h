// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_REQUEST_HELPER_H_
#define SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_REQUEST_HELPER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace net {

class HttpRequestHeaders;
class URLRequest;
struct RedirectInfo;

}  // namespace net

namespace network {

class AttributionAttestationMediator;
class TrustTokenKeyCommitmentGetter;

// Class AttributionRequestHelper handles attribution-reporting-api related
// operations (https://github.com/WICG/attribution-reporting-api) that must
// happen in the network service. It is meant to be optionally hooked to a
// url_loader instance.
class AttributionRequestHelper {
 public:
  // In the context of an attribution trigger registration request. The
  // destination origin corresponds to the top_frame origin where the trigger is
  // registered. We use this enum to log the status of this value. We can only
  // proceed with attestation with a valid destination origin.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class DestinationOriginStatus {
    kValid = 0,
    kMissing = 1,
    kNonSuitable = 2,
    kMaxValue = kNonSuitable,
  };

  // Creates an AttributionRequestHelper instance if needed.
  //
  // It is needed when it's to be hooked to a request related to attribution;
  // for now only trigger registration ping (i.e. has an
  // "Attribution-Reporting-Eligible" header which includes "trigger").
  // `request_headers` should contain the headers associated to the request to
  // which the helper would be hooked.
  static std::unique_ptr<AttributionRequestHelper> CreateIfNeeded(
      const net::HttpRequestHeaders& request_headers,
      const TrustTokenKeyCommitmentGetter* key_commitment_getter);

  // Test method which allows to instantiate an AttributionRequestHelper with
  // dependency injection (i.e. `CreateIfNeeded` builds `create_mediator`, this
  // method receives it).
  static std::unique_ptr<AttributionRequestHelper> CreateForTesting(
      const net::HttpRequestHeaders& request_headers,
      base::RepeatingCallback<AttributionAttestationMediator()>
          create_mediator);

  ~AttributionRequestHelper();
  AttributionRequestHelper(const AttributionRequestHelper&) = delete;
  AttributionRequestHelper& operator=(const AttributionRequestHelper&) = delete;

  // Orchestrates trigger attestation by calling the attribution attestation
  // mediator and optionally adding headers on the `request`. Externally, it
  // will be called once per request. Internally, on redirection, it will be
  // called by `OnReceivedRedirect`.
  void Begin(net::URLRequest& request, base::OnceClosure done);

  // Orchestrates attestation on a redirection request by `Finalize`-ing an
  // initial request and `Begin`-ing the attestation process on the redirection
  // request. A trigger_attestation property might be added to the `response`.
  // Attestation headers will potentially be added to or removed from the
  // `request`.
  void OnReceiveRedirect(
      net::URLRequest& request,
      mojom::URLResponseHeadPtr response,
      const net::RedirectInfo& redirect_info,
      base::OnceCallback<void(mojom::URLResponseHeadPtr response)> done);

  // Orchestrates attestation by calling the attribution attestation mediator
  // with the `response`'s headers. If an attestation header is present, it will
  // be processed and removed from the `response`. A trigger_attestation
  // property might be added to the `response`. Externally, it will be called at
  // most once per request. Internally, it might be called on redirection by
  // `OnReceivedRedirect`.
  void Finalize(mojom::URLResponseHead& response, base::OnceClosure done);

 private:
  struct AttestationOperation;

  explicit AttributionRequestHelper(
      base::RepeatingCallback<AttributionAttestationMediator()>
          create_mediator);

  // Continuation of `Begin` after asynchronous
  // mediator_::GetHeadersForAttestation concludes.
  //
  // `request` and `done` are `Begin`'s parameters, passed on to the
  // continuation. `headers` are headers optionally returned by the
  // attribution attestation mediator that wil be added to the request.
  void OnDoneGettingHeaders(net::URLRequest& request,
                            base::OnceClosure done,
                            net::HttpRequestHeaders headers);

  // Continuation of `Redirect` after asynchronous call to `Finalize`. `request`
  // and `done` are `Redirect`'s parameters, passed on to the continuation.
  void OnDoneFinalizingResponseFromRedirect(net::URLRequest& request,
                                            const GURL& new_url,
                                            base::OnceClosure done);

  // Continuation of `Finalize` after asynchronous
  // mediator_::ProcessAttestationToGetToken concludes.
  //
  // `response` and `done` are `Finalize`'s parameters, passed on to the
  // continuation. `maybe_redemption_token` is the result from the
  // attribution attestation mediator.
  void OnDoneProcessingAttestationResponse(
      mojom::URLResponseHead& response,
      base::OnceClosure done,
      absl::optional<std::string> maybe_redemption_token);

  // A mediator can perform a single attesation operation. Each redirect does an
  // attestation. We use this callback to generate a new mediator instance per
  // attestation operation.
  base::RepeatingCallback<AttributionAttestationMediator()> create_mediator_;

  // One request can lead to multiple attestation operations as each redirect
  // requires a distinct operation. `attestation_operation_` will be non-null
  // when an operation is undergoing.
  std::unique_ptr<AttestationOperation> attestation_operation_;

  // The destination origin is needed to complete the attestation. On `Request`,
  // we check that it is suitable and update `has_suitable_destination_origin_`
  // accordingly. On `finalize` we check that it is true before proceeding.
  bool has_suitable_destination_origin_ = false;

  base::WeakPtrFactory<AttributionRequestHelper> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_REQUEST_HELPER_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_REQUEST_HELPER_H_
#define SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_REQUEST_HELPER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/guid.h"
#include "base/memory/weak_ptr.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/origin.h"

namespace net {

class URLRequest;
class HttpRequestHeaders;

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

  // Test methods which allows to instantiate an AttributionRequestHelper with
  // dependency injection (i.e. `CreateIfNeeded` instantiates an
  // `AttributionAttestationMediator`, this method receives it).
  static std::unique_ptr<AttributionRequestHelper> CreateForTesting(
      const net::HttpRequestHeaders& request_headers,
      std::unique_ptr<AttributionAttestationMediator>);

  ~AttributionRequestHelper();
  AttributionRequestHelper(const AttributionRequestHelper&) = delete;
  AttributionRequestHelper& operator=(const AttributionRequestHelper&) = delete;

  // Orchestrates trigger attestation by calling the attribution attestation
  // mediator and optionally adding headers on the `request`.
  void Begin(net::URLRequest& request, base::OnceClosure done);

  // Orchestrates attestation by calling the attribution attestation mediator
  // with the `response`'s headers. If an attestation header is present, it will
  // be processed and removed from the response. A trigger_attestation property
  // might be added to the response.
  void Finalize(mojom::URLResponseHead& response, base::OnceClosure done);

 private:
  explicit AttributionRequestHelper(
      std::unique_ptr<AttributionAttestationMediator> mediator);

  // Generates a message by concatenating a trigger`s `destination_origin` and
  // the `aggregatable_report_id_`.
  std::string GenerateTriggerAttestationMessage(
      const url::Origin& destination_origin);

  // Continuation of `Begin` after asynchronous
  // mediator_::GetHeadersForAttestation concludes.
  //
  // `url_requests` and `done` are `Begin`'s parameters, passed on to the
  // continuation. `maybe_headers` are headers optionally returned by the
  // attribution attestation mediator that wil be added to the request.
  void OnDoneGettingHeaders(net::URLRequest& url_request,
                            base::OnceClosure done,
                            net::HttpRequestHeaders headers);

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

  // The id for a potential future aggregatable report. It is eagerly generated
  // in this class to be embedded in the attestation message.
  // TODO(1406645): use explicitly spec compliant structure
  base::GUID aggregatable_report_id_;

  std::unique_ptr<AttributionAttestationMediator> mediator_;

  // Set to true when headers are added as part of `Begin`. This indicates that
  // the response is to be parsed on `Finalize`. If still false when `Finalize`
  // is called, we can return early.
  bool set_attestation_headers_ = false;

  base::WeakPtrFactory<AttributionRequestHelper> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_REQUEST_HELPER_H_

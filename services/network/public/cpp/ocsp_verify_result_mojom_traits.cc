// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ocsp_verify_result_mojom_traits.h"

#include "base/notreached.h"
#include "services/network/public/mojom/ocsp_verify_result.mojom-shared.h"
#include "third_party/boringssl/src/include/openssl/pki/ocsp.h"

namespace mojo {

// static
network::mojom::OCSPVerifyResultResponseStatus
EnumTraits<network::mojom::OCSPVerifyResultResponseStatus,
           bssl::OCSPVerifyResult::ResponseStatus>::
    ToMojom(bssl::OCSPVerifyResult::ResponseStatus status) {
  switch (status) {
    case bssl::OCSPVerifyResult::NOT_CHECKED:
      return network::mojom::OCSPVerifyResultResponseStatus::kNotChecked;
    case bssl::OCSPVerifyResult::MISSING:
      return network::mojom::OCSPVerifyResultResponseStatus::kMissing;
    case bssl::OCSPVerifyResult::PROVIDED:
      return network::mojom::OCSPVerifyResultResponseStatus::kProvided;
    case bssl::OCSPVerifyResult::ERROR_RESPONSE:
      return network::mojom::OCSPVerifyResultResponseStatus::kErrorResponse;
    case bssl::OCSPVerifyResult::BAD_PRODUCED_AT:
      return network::mojom::OCSPVerifyResultResponseStatus::kBadProducedAt;
    case bssl::OCSPVerifyResult::NO_MATCHING_RESPONSE:
      return network::mojom::OCSPVerifyResultResponseStatus::
          kNoMatchingResponse;
    case bssl::OCSPVerifyResult::INVALID_DATE:
      return network::mojom::OCSPVerifyResultResponseStatus::kInvalidDate;
    case bssl::OCSPVerifyResult::PARSE_RESPONSE_ERROR:
      return network::mojom::OCSPVerifyResultResponseStatus::
          kParseResponseError;
    case bssl::OCSPVerifyResult::PARSE_RESPONSE_DATA_ERROR:
      return network::mojom::OCSPVerifyResultResponseStatus::
          kParseResponseDataError;
    case bssl::OCSPVerifyResult::UNHANDLED_CRITICAL_EXTENSION:
      return network::mojom::OCSPVerifyResultResponseStatus::
          kUnhandledCriticalExtension;
  }
  NOTREACHED();
}

// static
bssl::OCSPVerifyResult::ResponseStatus
EnumTraits<network::mojom::OCSPVerifyResultResponseStatus,
           bssl::OCSPVerifyResult::ResponseStatus>::
    FromMojom(network::mojom::OCSPVerifyResultResponseStatus input) {
  switch (input) {
    case network::mojom::OCSPVerifyResultResponseStatus::kNotChecked:
      return bssl::OCSPVerifyResult::NOT_CHECKED;
    case network::mojom::OCSPVerifyResultResponseStatus::kMissing:
      return bssl::OCSPVerifyResult::MISSING;
    case network::mojom::OCSPVerifyResultResponseStatus::kProvided:
      return bssl::OCSPVerifyResult::PROVIDED;
    case network::mojom::OCSPVerifyResultResponseStatus::kErrorResponse:
      return bssl::OCSPVerifyResult::ERROR_RESPONSE;
    case network::mojom::OCSPVerifyResultResponseStatus::kBadProducedAt:
      return bssl::OCSPVerifyResult::BAD_PRODUCED_AT;
    case network::mojom::OCSPVerifyResultResponseStatus::kNoMatchingResponse:
      return bssl::OCSPVerifyResult::NO_MATCHING_RESPONSE;
    case network::mojom::OCSPVerifyResultResponseStatus::kInvalidDate:
      return bssl::OCSPVerifyResult::INVALID_DATE;
    case network::mojom::OCSPVerifyResultResponseStatus::kParseResponseError:
      return bssl::OCSPVerifyResult::PARSE_RESPONSE_ERROR;
    case network::mojom::OCSPVerifyResultResponseStatus::
        kParseResponseDataError:
      return bssl::OCSPVerifyResult::PARSE_RESPONSE_DATA_ERROR;
    case network::mojom::OCSPVerifyResultResponseStatus::
        kUnhandledCriticalExtension:
      return bssl::OCSPVerifyResult::UNHANDLED_CRITICAL_EXTENSION;
  }
  NOTREACHED();
}

// static
network::mojom::OCSPRevocationStatus EnumTraits<
    network::mojom::OCSPRevocationStatus,
    bssl::OCSPRevocationStatus>::ToMojom(bssl::OCSPRevocationStatus status) {
  switch (status) {
    case bssl::OCSPRevocationStatus::GOOD:
      return network::mojom::OCSPRevocationStatus::kGood;
    case bssl::OCSPRevocationStatus::REVOKED:
      return network::mojom::OCSPRevocationStatus::kRevoked;
    case bssl::OCSPRevocationStatus::UNKNOWN:
      return network::mojom::OCSPRevocationStatus::kUnknown;
  }
  NOTREACHED();
}

// static
bssl::OCSPRevocationStatus
EnumTraits<network::mojom::OCSPRevocationStatus, bssl::OCSPRevocationStatus>::
    FromMojom(network::mojom::OCSPRevocationStatus input) {
  switch (input) {
    case network::mojom::OCSPRevocationStatus::kGood:
      return bssl::OCSPRevocationStatus::GOOD;
    case network::mojom::OCSPRevocationStatus::kRevoked:
      return bssl::OCSPRevocationStatus::REVOKED;
    case network::mojom::OCSPRevocationStatus::kUnknown:
      return bssl::OCSPRevocationStatus::UNKNOWN;
  }
  NOTREACHED();
}

// static
bool StructTraits<
    network::mojom::OCSPVerifyResultDataView,
    bssl::OCSPVerifyResult>::Read(network::mojom::OCSPVerifyResultDataView data,
                                  bssl::OCSPVerifyResult* out) {
  return data.ReadResponseStatus(&out->response_status) &&
         data.ReadRevocationStatus(&out->revocation_status);
}

}  // namespace mojo

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/test_utils.h"

#include "base/time/time.h"
#include "device/fido/enclave/verify/claim.h"

namespace device::enclave {

EndorsementStatement MakeEndorsementStatement(
    std::string_view statement_type,
    std::string_view predicate_type,
    base::Time issued_on,
    base::Time not_before,
    base::Time not_after,
    std::string_view endorsement_type) {
  EndorsementStatement endorsement_statement;
  endorsement_statement.predicate.issued_on = issued_on;
  endorsement_statement.type = statement_type;
  endorsement_statement.predicate_type = predicate_type;
  ClaimValidity claim_validity;
  claim_validity.not_after = not_after;
  claim_validity.not_before = not_before;
  endorsement_statement.predicate.validity = claim_validity;
  endorsement_statement.predicate.claim_type = endorsement_type;
  return endorsement_statement;
}

EndorsementStatement MakeValidEndorsementStatement() {
  return MakeEndorsementStatement(
      kStatementV1, kPredicateV2, base::Time::FromTimeT(10),
      base::Time::FromTimeT(15), base::Time::FromTimeT(20));
}

}  // namespace device::enclave

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/claim.h"

namespace device::enclave {

Subject::Subject() = default;
Subject::Subject(std::string name, std::map<std::string, std::string> digest)
    : name(std::move(name)), digest(std::move(digest)) {}
Subject::~Subject() = default;

ClaimEvidence::ClaimEvidence() = default;
ClaimEvidence::ClaimEvidence(std::optional<std::string> role,
                             std::string uri,
                             std::map<std::string, std::string> digest)
    : role(std::move(role)), uri(std::move(uri)), digest(std::move(digest)) {}
ClaimEvidence::~ClaimEvidence() = default;

template <typename T>
bool ValidateClaim(const Statement<ClaimPredicate<T>>& claim) {
  if (claim.type != kStatementV1) {
    return false;
  }
  if (claim.predicate_type != kPredicateV1 &&
      claim.predicate_type != kPredicateV2) {
    return false;
  }
  if (claim.predicate.validity.has_value()) {
    if (claim.predicate.validity->not_before < claim.predicate.issued_on) {
      return false;
    }
    if (claim.predicate.validity->not_before >
        claim.predicate.validity->not_after) {
      return false;
    }
  }
  return true;
}

bool ValidateEndorsement(const EndorsementStatement& claim) {
  if (!ValidateClaim(claim) || claim.predicate.claim_type != kEndorsementV2) {
    return false;
  }
  return true;
}

}  // namespace device::enclave

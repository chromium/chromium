// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_VERIFY_CLAIM_H_
#define DEVICE_FIDO_ENCLAVE_VERIFY_CLAIM_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "device/fido/enclave/verify/proto/digest.pb.h"

namespace device::enclave {

// Admissible predicate type of in-toto endorsement statements.
extern const inline std::string_view kPredicateV1 =
    "https://github.com/project-oak/transparent-release/claim/v1";
extern const inline std::string_view kPredicateV2 =
    "https://github.com/project-oak/transparent-release/claim/v2";

// ClaimType for endorsements. Expected to be used together with `ClaimV1` as
// the predicate type in an in-toto statement.
extern const inline std::string_view kEndorsementV2 =
    "https://github.com/project-oak/transparent-release/endorsement/v2";

// URI representing in-toto statements. We only use V1.
extern const inline std::string_view kStatementV1 =
    "https://in-toto.io/Statement/v1";

// A software artifact identified by its name and a set of artifacts.
struct COMPONENT_EXPORT(DEVICE_FIDO) Subject {
  Subject(std::string name, HexDigest digest);
  Subject();
  ~Subject();

  std::string name;
  HexDigest digest;
};

// Represents a generic statement that binds a predicate to a subject.
template <typename T>
struct Statement {
  std::string type;
  std::string predicate_type;
  std::vector<Subject> subject;
  T predicate;
};

// Metadata about an artifact that serves as the evidence for the truth of a
// claim.
struct COMPONENT_EXPORT(DEVICE_FIDO) ClaimEvidence {
  ClaimEvidence(std::optional<std::string> role,
                std::string uri,
                HexDigest digest);
  ClaimEvidence();
  ~ClaimEvidence();
  ClaimEvidence(const ClaimEvidence& claim_evidence);

  // Optional field specifying the role of this evidence within the claim.
  std::optional<std::string> role;
  // URI uniquely identifies this evidence.
  std::string uri;
  // Collection of cryptographic digests for the contents of this artifact.
  HexDigest digest;
};

// Validity time range of an issued claim.
struct ClaimValidity {
  // The timestamp from which the claim is effective.
  base::Time not_before;
  // The timestamp from which the claim no longer applies to the artifact.
  base::Time not_after;
};

// Detailed content of a claim.
template <typename T>
struct COMPONENT_EXPORT(DEVICE_FIDO) ClaimPredicate {
  // URI indicating the type of the claim. It determines the meaning of
  // `claimSpec` and `evidence`.
  std::string claim_type;
  // A detailed description of the claim, as an optional arbitrary object.
  std::optional<T> claim_spec;
  // Specifies which evidence field the endorsement targets.
  std::string usage;
  // The timestamp (encoded as an Epoch time) when the claim was issued.
  base::Time issued_on;
  // Validity duration of this claim.
  std::optional<ClaimValidity> validity;
  // A collection of artifacts that support the truth of the claim.
  std::vector<ClaimEvidence> evidence;
};

// Inner type for a simple claim with no further fields.
struct COMPONENT_EXPORT(DEVICE_FIDO) Claimless {};

typedef Statement<ClaimPredicate<Claimless>> EndorsementStatement;

// Converts the given byte array into an endorsement statement.
base::expected<EndorsementStatement, std::string> COMPONENT_EXPORT(DEVICE_FIDO)
    ParseEndorsementStatement(base::span<const uint8_t> bytes);

// Checks that the given statement is a valid claim:
// - has valid Statement and Predicate types, and
// - has a valid validity duration.
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

// Checks that the input claim has a validity duration, and that the specified
// time is inside the validity period.
template <typename T>
bool VerifyValidityDuration(base::Time now,
                            const Statement<ClaimPredicate<T>>& claim) {
  if (!claim.predicate.validity.has_value() ||
      claim.predicate.validity->not_before > now ||
      claim.predicate.validity->not_after < now) {
    return false;
  }
  return true;
}

// Checks that the given endorsement statement is valid and has the correct
// claim type.
bool COMPONENT_EXPORT(DEVICE_FIDO)
    ValidateEndorsement(const EndorsementStatement& claim);

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_VERIFY_CLAIM_H_

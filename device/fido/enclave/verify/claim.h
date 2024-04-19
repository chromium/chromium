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
  Subject(std::string name, std::map<std::string, std::string> digest);
  Subject();
  ~Subject();

  std::string name;
  std::map<std::string, std::string> digest;
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
                std::map<std::string, std::string> digest);
  ClaimEvidence();
  ~ClaimEvidence();

  std::optional<std::string> role;
  std::string uri;
  std::map<std::string, std::string> digest;
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
  std::string claim_type;
  std::optional<T> claim_spec;
  std::string usage;
  // The timestamp (encoded as an Epoch time) when the claim was issued.
  base::Time issued_on;
  std::optional<ClaimValidity> validity;
  std::vector<ClaimEvidence> evidence;
};

// Inner type for a simple claim with no further fields.
struct COMPONENT_EXPORT(DEVICE_FIDO) Claimless {};

typedef Statement<ClaimPredicate<Claimless>> EndorsementStatement;

// Converts the given byte array into an endorsement statement.
base::expected<EndorsementStatement, std::string> ParseEndorsementStatement(
    base::span<const uint8_t> bytes);

// Checks that the given statement is a valid claim:
// - has valid Statement and Predicate types, and
// - has a valid validity duration.
template <typename T>
bool COMPONENT_EXPORT(DEVICE_FIDO)
    ValidateClaim(const Statement<ClaimPredicate<T>>& claim);

// Checks that the input claim has a validity duration, and that the specified
// time is inside the validity period.
template <typename T>
bool VerifyValidityDuration(base::Time now,
                            const Statement<ClaimPredicate<T>>& claim);

// Checks that the given endorsement statement is a valid and has the correct
// claim type.
bool COMPONENT_EXPORT(DEVICE_FIDO)
    ValidateEndorsement(const EndorsementStatement& claim);

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_VERIFY_CLAIM_H_

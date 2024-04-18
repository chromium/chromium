// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_VERIFY_CLAIM_H_
#define DEVICE_FIDO_ENCLAVE_VERIFY_CLAIM_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "base/types/expected.h"

namespace device::enclave {

// A software artifact identified by its name and a set of artifacts.
struct Subject {
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
struct ClaimEvidence {
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
  base::Time not_before;
  base::Time not_after;
};

// Detailed content of a claim.
template <typename T>
struct ClaimPredicate {
  std::string claim_type;
  std::optional<T> claim_spec;
  std::string usage;
  base::Time issued_on;
  std::optional<ClaimValidity> validity;
  std::vector<ClaimEvidence> evidence;
};

// Inner type for a simple claim with no further fields.
struct Claimless {};

typedef Statement<ClaimPredicate<Claimless>> EndorsementStatement;

// Converts the given byte array into an endorsement statement.
base::expected<EndorsementStatement, std::string> ParseEndorsementStatement(
    base::span<const uint8_t> bytes);

// Checks that the given statement is a valid claim:
// - has valid Statement and Predicate types, and
// - has a valid validity duration.
template <typename T>
bool ValidateClaim(const Statement<ClaimPredicate<T>>& claim);

// Checks that the input claim has a validity duration, and that the specified
// time is inside the validity period.
template <typename T>
bool VerifyValidityDuration(base::Time now,
                            const Statement<ClaimPredicate<T>>& claim);

// Checks that the given endorsement statement is a valid and has the correct
// claim type.
bool ValidateEndorsement(const EndorsementStatement& claim);

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_VERIFY_CLAIM_H_

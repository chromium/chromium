// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_VERIFY_CLAIM_H_
#define DEVICE_FIDO_ENCLAVE_VERIFY_CLAIM_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"

namespace device::enclave {

struct Subject {
  Subject(std::string name, std::map<std::string, std::string> digest);
  Subject();
  ~Subject();

  std::string name;
  std::map<std::string, std::string> digest;
};

template <typename T>
struct Statement {
  std::string type;
  std::string predicate_type;
  std::vector<Subject> subject;
  T predicate;
};

struct ClaimEvidence {
  ClaimEvidence(std::optional<std::string> role,
                std::string uri,
                std::vector<uint8_t> digest);
  ClaimEvidence();
  ~ClaimEvidence();

  std::optional<std::string> role;
  std::string uri;
  std::vector<uint8_t> digest;
};

struct ClaimValidity {
  base::Time not_before;
  base::Time not_after;
};

template <typename T>
struct ClaimPredicate {
  std::string claim_type;
  std::optional<T> claim_spec;
  std::string usage;
  base::Time issued_on;
  std::optional<ClaimValidity> validity;
  std::vector<ClaimEvidence> evidence;
};

struct Claimless {};

typedef Statement<ClaimPredicate<Claimless>> EndorsementStatement;

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_VERIFY_CLAIM_H_

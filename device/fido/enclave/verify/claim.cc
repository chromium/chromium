// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/claim.h"

#include <ctime>
#include <iomanip>
#include <sstream>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "device/fido/enclave/verify/proto/digest.pb.h"

namespace device::enclave {

Subject::Subject() = default;
Subject::Subject(std::string name, HexDigest digest)
    : name(std::move(name)), digest(std::move(digest)) {}
Subject::~Subject() = default;

ClaimEvidence::ClaimEvidence() = default;
ClaimEvidence::ClaimEvidence(std::optional<std::string> role,
                             std::string uri,
                             HexDigest digest)
    : role(std::move(role)), uri(std::move(uri)), digest(std::move(digest)) {}
ClaimEvidence::~ClaimEvidence() = default;
ClaimEvidence::ClaimEvidence(const ClaimEvidence& claim_evidence) = default;

bool ValidateEndorsement(const EndorsementStatement& claim) {
  if (!ValidateClaim(claim) || claim.predicate.claim_type != kEndorsementV2) {
    return false;
  }
  return true;
}

std::optional<base::Time> ConvertStringToTime(std::string timestamp) {
  std::tm time_result{0};
  std::istringstream ss(timestamp);
  ss >> std::get_time(&time_result, "%Y-%m-%dT%H:%M:%S");
  if (ss.fail()) {
    return std::nullopt;
  }
  return base::Time::FromTimeT(std::mktime(&time_result));
}

std::optional<ClaimValidity> CreateClaimValidity(
    const base::Value::Dict* predicate) {
  ClaimValidity validity_struct;
  const base::Value::Dict* validity_dict = predicate->FindDict("validity");
  if (!validity_dict) {
    return std::nullopt;
  }
  const std::string* not_before = validity_dict->FindString("notBefore");
  if (!not_before) {
    return std::nullopt;
  }
  std::optional<base::Time> not_before_time = ConvertStringToTime(*not_before);
  if (!not_before_time.has_value()) {
    return std::nullopt;
  }
  validity_struct.not_before = std::move(*not_before_time);
  const std::string* not_after = validity_dict->FindString("notAfter");
  if (!not_after) {
    return std::nullopt;
  }
  std::optional<base::Time> not_after_time = ConvertStringToTime(*not_after);
  if (!not_after_time.has_value()) {
    return std::nullopt;
  }
  validity_struct.not_after = std::move(*not_after_time);
  return validity_struct;
}

std::optional<ClaimPredicate<Claimless>> CreateClaimPredicate(
    const base::Value::Dict* predicate) {
  ClaimPredicate<Claimless> predicate_struct;
  const std::string* claim_type = predicate->FindString("claimType");
  if (!claim_type) {
    return std::nullopt;
  }
  predicate_struct.claim_type = *claim_type;
  const std::string* issued_on = predicate->FindString("issuedOn");
  if (!issued_on) {
    return std::nullopt;
  }
  std::optional<ClaimValidity> validity_struct = CreateClaimValidity(predicate);
  if (!validity_struct.has_value()) {
    return std::nullopt;
  }
  predicate_struct.validity = std::move(validity_struct);
  return predicate_struct;
}

std::optional<HexDigest> CreateDigest(const base::Value::Dict* subject) {
  const base::Value::Dict* digest_dict = subject->FindDict("digest");
  if (!digest_dict) {
    return std::nullopt;
  }
  const std::string* sha256 = digest_dict->FindString("sha256");
  if (!sha256) {
    return std::nullopt;
  }
  HexDigest digest;
  digest.set_sha2_256(*sha256);
  return digest;
}

std::optional<std::vector<Subject>> CreateSubjectVector(
    const base::Value::List* subjects) {
  std::vector<Subject> subject_vector;
  for (const auto& subject_val : *subjects) {
    const base::Value::Dict* subject = subject_val.GetIfDict();
    if (subject == nullptr) {
      return std::nullopt;
    }
    Subject subject_struct;
    const std::string* name = subject->FindString("name");
    if (!name) {
      return std::nullopt;
    }
    subject_struct.name = *name;
    std::optional<HexDigest> digest = CreateDigest(subject);
    if (!digest.has_value()) {
      return std::nullopt;
    }
    subject_struct.digest = std::move(*digest);
    subject_vector.emplace_back(std::move(subject_struct));
  }
  return subject_vector;
}

base::expected<EndorsementStatement, std::string> ParseEndorsementStatement(
    base::span<const uint8_t> bytes) {
  std::string_view json(reinterpret_cast<const char*>(bytes.data()),
                        bytes.size());
  std::optional<base::Value> endorsement_statement_json =
      base::JSONReader::Read(json);
  if (!endorsement_statement_json.has_value()) {
    return base::unexpected("invalid endorsement statement json.");
  }
  base::Value::Dict* dict = endorsement_statement_json->GetIfDict();
  if (!dict) {
    return base::unexpected("couldn't parse endorsement statement.");
  }
  EndorsementStatement endorsement_statement;
  std::string* type = dict->FindString("_type");
  if (!type) {
    return base::unexpected("_type isn't specified as a string.");
  }
  endorsement_statement.type = *type;
  std::string* predicate_type = dict->FindString("predicateType");
  if (!predicate_type) {
    return base::unexpected("predicateType isn't specified as a string.");
  }
  endorsement_statement.predicate_type = *predicate_type;
  base::Value::List* subjects = dict->FindList("subject");
  if (!subjects) {
    return base::unexpected("subject isn't specified as a list.");
  }
  auto subject = CreateSubjectVector(subjects);
  if (!subject.has_value()) {
    return base::unexpected("can't parse subject from endorsement.");
  }
  endorsement_statement.subject = std::move(*subject);
  base::Value::Dict* predicate = dict->FindDict("predicate");
  if (!predicate) {
    return base::unexpected(
        "predicate not formatted correctly in endorsement.");
  }
  std::optional<ClaimPredicate<Claimless>> predicate_struct =
      CreateClaimPredicate(predicate);
  if (!predicate_struct.has_value()) {
    return base::unexpected("can't parse predicate from endorsement.");
  }
  endorsement_statement.predicate = std::move(*predicate_struct);
  return endorsement_statement;
}

}  // namespace device::enclave

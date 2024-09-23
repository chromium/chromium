// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/test_utils.h"

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "device/fido/enclave/verify/claim.h"
#include "testing/gtest/include/gtest/gtest.h"

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

std::string GetContentsFromFile(std::string_view file_name) {
  std::string result;
  base::FilePath file_path;
  base::PathService::Get(base::BasePathKey::DIR_SRC_TEST_DATA_ROOT, &file_path);
  file_path = file_path.AppendASCII("device/fido/enclave/verify/testdata");
  file_path = file_path.AppendASCII(file_name);
  EXPECT_TRUE(base::ReadFileToString(file_path, &result));
  return result;
}

}  // namespace device::enclave

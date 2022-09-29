// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/install/crx_install_error.h"

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "extensions/browser/install/sandboxed_unpacker_failure_reason.h"

namespace extensions {

CrxInstallError::CrxInstallError(CrxInstallErrorType type,
                                 CrxInstallErrorDetail detail,
                                 const std::u16string& message)
    : type_(type), detail_(detail), message_(message) {
  DCHECK_NE(CrxInstallErrorType::NONE, type);
  DCHECK_NE(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE, type);
}

CrxInstallError::CrxInstallError(CrxInstallErrorType type,
                                 CrxInstallErrorDetail detail)
    : CrxInstallError(type, detail, std::u16string()) {}

CrxInstallError::CrxInstallError(SandboxedUnpackerFailureReason reason,
                                 const std::u16string& message)
    : type_(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE),
      detail_(CrxInstallErrorDetail::NONE),
      sandbox_failure_detail_(reason),
      message_(message) {}

CrxInstallError::CrxInstallError(const CrxInstallError& other) = default;
CrxInstallError::CrxInstallError(CrxInstallError&& other) = default;
CrxInstallError& CrxInstallError::operator=(const CrxInstallError& other) =
    default;
CrxInstallError& CrxInstallError::operator=(CrxInstallError&& other) = default;

CrxInstallError::~CrxInstallError() = default;

// For SANDBOXED_UNPACKER_FAILURE type, use sandbox_failure_detail().
CrxInstallErrorDetail CrxInstallError::detail() const {
  DCHECK_NE(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE, type_);
  return detail_;
}

// sandbox_failure_detail() only returns a value when the error type is
// SANDBOXED_UNPACKER_FAILURE.
SandboxedUnpackerFailureReason CrxInstallError::sandbox_failure_detail() const {
  DCHECK_EQ(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE, type_);
  DCHECK(sandbox_failure_detail_);
  return sandbox_failure_detail_.value();
}

// Returns true if the error occurred during crx file verification.
// Use this only if the error type is SANDBOXED_UNPACKER_FAILURE.
bool CrxInstallError::IsCrxVerificationFailedError() const {
  // List of unpack failure codes related to bad CRX file.
  constexpr SandboxedUnpackerFailureReason kVerificationFailureReasons[] = {
      SandboxedUnpackerFailureReason::CRX_FILE_NOT_READABLE,
      SandboxedUnpackerFailureReason::CRX_HEADER_INVALID,
      SandboxedUnpackerFailureReason::CRX_MAGIC_NUMBER_INVALID,
      SandboxedUnpackerFailureReason::CRX_VERSION_NUMBER_INVALID,
      SandboxedUnpackerFailureReason::CRX_EXCESSIVELY_LARGE_KEY_OR_SIGNATURE,
      SandboxedUnpackerFailureReason::CRX_ZERO_KEY_LENGTH,
      SandboxedUnpackerFailureReason::CRX_ZERO_SIGNATURE_LENGTH,
      SandboxedUnpackerFailureReason::CRX_PUBLIC_KEY_INVALID,
      SandboxedUnpackerFailureReason::CRX_SIGNATURE_INVALID,
      SandboxedUnpackerFailureReason::
          CRX_SIGNATURE_VERIFICATION_INITIALIZATION_FAILED,
      SandboxedUnpackerFailureReason::CRX_SIGNATURE_VERIFICATION_FAILED,
      SandboxedUnpackerFailureReason::CRX_HASH_VERIFICATION_FAILED,
      SandboxedUnpackerFailureReason::CRX_FILE_IS_DELTA_UPDATE,
      SandboxedUnpackerFailureReason::CRX_EXPECTED_HASH_INVALID,
      SandboxedUnpackerFailureReason::CRX_REQUIRED_PROOF_MISSING,
  };
  if (type() != CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE)
    return false;
  const SandboxedUnpackerFailureReason unpacker_failure_reason =
      sandbox_failure_detail();
  return base::Contains(kVerificationFailureReasons, unpacker_failure_reason);
}

// Returns true if the error occurred during crx installation due to mismatch in
// expectations from the manifest.
bool CrxInstallError::IsCrxExpectationsFailedError() const {
  if (type() != CrxInstallErrorType::OTHER)
    return false;
  const CrxInstallErrorDetail failure_reason = detail();
  return failure_reason == CrxInstallErrorDetail::UNEXPECTED_ID ||
         failure_reason == CrxInstallErrorDetail::MISMATCHED_VERSION;
}

}  // namespace extensions

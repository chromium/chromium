// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_INSTALL_CRX_INSTALL_ERROR_H_
#define EXTENSIONS_BROWSER_INSTALL_CRX_INSTALL_ERROR_H_

#include <optional>
#include <string>

namespace extensions {

enum class SandboxedUnpackerFailureReason;

// Typed errors that need to be handled specially by clients.
// Do not change the order of the entries or remove entries in this list.
enum class CrxInstallErrorType {
  NONE = 0,
  // DECLINED for situations when a .crx file seems to be OK, but there
  // are some policy restrictions or unmet dependencies that prevent it from
  // being installed.
  DECLINED = 1,
  // SANDBOXED_UNPACKER_FAILURE for sandboxed unpacker error.
  // CrxInstallErrorDetail will give more detail about the failure.
  SANDBOXED_UNPACKER_FAILURE = 2,
  OTHER = 3,
};

// Extended error code that may help explain the error type.
// Do not change the order of the entries or remove entries in this list.
// 1) Don't forget to update enums.xml when adding new entries.
// 2) Don't forget to update device_management_backend.proto (name:
// ExtensionInstallReportLogEvent::CrxInstallErrorDetail) when adding new
// entries.
// 3) Don't forget to update ConvertCrxInstallErrorDetailToProto method in
// ExtensionInstallEventLogCollector.
enum class CrxInstallErrorDetail {
  NONE,                                     // 0
  CONVERT_USER_SCRIPT_TO_EXTENSION_FAILED,  // 1
  UNEXPECTED_ID,                            // 2
  UNEXPECTED_VERSION,                       // 3
  MISMATCHED_VERSION,                       // 4
  MANIFEST_INVALID,                         // 5
  INSTALL_NOT_ENABLED,                      // 6
  OFFSTORE_INSTALL_DISALLOWED,              // 7
  INCORRECT_APP_CONTENT_TYPE,               // 8
  NOT_INSTALLED_FROM_GALLERY,               // 9
  INCORRECT_INSTALL_HOST,                   // 10
  DEPENDENCY_NOT_SHARED_MODULE,             // 11
  DEPENDENCY_OLD_VERSION,                   // 12
  DEPENDENCY_NOT_ALLOWLISTED,               // 13
  UNSUPPORTED_REQUIREMENTS,                 // 14
  EXTENSION_IS_BLOCKLISTED,                 // 15
  DISALLOWED_BY_POLICY,                     // 16
  KIOSK_MODE_ONLY,                          // 17
  OVERLAPPING_WEB_EXTENT,                   // 18
  CANT_DOWNGRADE_VERSION,                   // 19
  MOVE_DIRECTORY_TO_PROFILE_FAILED,         // 20
  CANT_LOAD_EXTENSION,                      // 21
  USER_CANCELED,                            // 22
  USER_ABORTED,                             // 23
  UPDATE_NON_EXISTING_EXTENSION,            // 24

  // Magic constant used by the histogram macros.
  // Always update it to the max value.
  kMaxValue = UPDATE_NON_EXISTING_EXTENSION,
};

// Simple error class for CrxInstaller.
class CrxInstallError {
 public:
  CrxInstallError(CrxInstallErrorType type,
                  CrxInstallErrorDetail detail,
                  const std::u16string& message);
  CrxInstallError(CrxInstallErrorType type, CrxInstallErrorDetail detail);
  CrxInstallError(SandboxedUnpackerFailureReason reason,
                  const std::u16string& message);
  ~CrxInstallError();

  CrxInstallError(const CrxInstallError& other);
  CrxInstallError(CrxInstallError&& other);
  CrxInstallError& operator=(const CrxInstallError& other);
  CrxInstallError& operator=(CrxInstallError&& other);

  CrxInstallErrorType type() const { return type_; }
  const std::u16string& message() const { return message_; }
  CrxInstallErrorDetail detail() const;
  SandboxedUnpackerFailureReason sandbox_failure_detail() const;
  bool IsCrxVerificationFailedError() const;
  bool IsCrxExpectationsFailedError() const;

 private:
  CrxInstallErrorType type_;
  CrxInstallErrorDetail detail_;
  std::optional<SandboxedUnpackerFailureReason> sandbox_failure_detail_;
  std::u16string message_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_INSTALL_CRX_INSTALL_ERROR_H_

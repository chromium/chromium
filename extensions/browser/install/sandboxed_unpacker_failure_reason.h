// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_INSTALL_SANDBOXED_UNPACKER_FAILURE_REASON_H_
#define EXTENSIONS_BROWSER_INSTALL_SANDBOXED_UNPACKER_FAILURE_REASON_H_

namespace extensions {

// Enumerate all the ways SandboxedUnpacker can fail.
// Note: enum used for UMA. Do NOT reorder or remove entries. Don't forget to
// update enums.xml (name: ExtensionUnpackFailureReason) when adding new
// entries.
// Don't forget to update device_management_backend.proto (name:
// ExtensionInstallReportLogEvent::SandboxedUnpackerFailureReason) when adding
// new entries. Don't forget to update ConvertUnpackerFailureReasonToProto
// method in ExtensionInstallEventLogCollector.
enum class SandboxedUnpackerFailureReason {
  // SandboxedUnpacker::CreateTempDirectory()
  COULD_NOT_GET_TEMP_DIRECTORY = 0,
  COULD_NOT_CREATE_TEMP_DIRECTORY = 1,

  // SandboxedUnpacker::Start()
  FAILED_TO_COPY_EXTENSION_FILE_TO_TEMP_DIRECTORY = 2,
  COULD_NOT_GET_SANDBOX_FRIENDLY_PATH = 3,

  // SandboxedUnpacker::UnpackExtensionSucceeded()
  COULD_NOT_LOCALIZE_EXTENSION = 4,
  INVALID_MANIFEST = 5,

  // SandboxedUnpacker::UnpackExtensionFailed()
  UNPACKER_CLIENT_FAILED = 6,

  // SandboxedUnpacker::UtilityProcessCrashed()
  UTILITY_PROCESS_CRASHED_WHILE_TRYING_TO_INSTALL = 7,

  // SandboxedUnpacker::ValidateSignature()
  CRX_FILE_NOT_READABLE = 8,
  CRX_HEADER_INVALID = 9,
  CRX_MAGIC_NUMBER_INVALID = 10,
  CRX_VERSION_NUMBER_INVALID = 11,
  CRX_EXCESSIVELY_LARGE_KEY_OR_SIGNATURE = 12,
  CRX_ZERO_KEY_LENGTH = 13,
  CRX_ZERO_SIGNATURE_LENGTH = 14,
  CRX_PUBLIC_KEY_INVALID = 15,
  CRX_SIGNATURE_INVALID = 16,
  CRX_SIGNATURE_VERIFICATION_INITIALIZATION_FAILED = 17,
  CRX_SIGNATURE_VERIFICATION_FAILED = 18,

  // SandboxedUnpacker::RewriteManifestFile()
  ERROR_SERIALIZING_MANIFEST_JSON = 19,
  ERROR_SAVING_MANIFEST_JSON = 20,

  // SandboxedUnpacker::RewriteImageFiles()
  COULD_NOT_READ_IMAGE_DATA_FROM_DISK_UNUSED = 21,
  DECODED_IMAGES_DO_NOT_MATCH_THE_MANIFEST_UNUSED = 22,
  INVALID_PATH_FOR_BROWSER_IMAGE = 23,
  ERROR_REMOVING_OLD_IMAGE_FILE = 24,
  INVALID_PATH_FOR_BITMAP_IMAGE = 25,
  ERROR_RE_ENCODING_THEME_IMAGE = 26,
  ERROR_SAVING_THEME_IMAGE = 27,
  DEPRECATED_ABORTED_DUE_TO_SHUTDOWN = 28,  // No longer used; kept for UMA.

  // SandboxedUnpacker::RewriteCatalogFiles()
  COULD_NOT_READ_CATALOG_DATA_FROM_DISK_UNUSED = 29,
  INVALID_CATALOG_DATA = 30,
  INVALID_PATH_FOR_CATALOG_UNUSED = 31,
  ERROR_SERIALIZING_CATALOG = 32,
  ERROR_SAVING_CATALOG = 33,

  // SandboxedUnpacker::ValidateSignature()
  CRX_HASH_VERIFICATION_FAILED = 34,

  UNZIP_FAILED = 35,
  DIRECTORY_MOVE_FAILED = 36,

  // SandboxedUnpacker::ValidateSignature()
  CRX_FILE_IS_DELTA_UPDATE = 37,
  CRX_EXPECTED_HASH_INVALID = 38,

  // SandboxedUnpacker::IndexAndPersistRulesIfNeeded()
  DEPRECATED_ERROR_PARSING_DNR_RULESET = 39,  // No longer used; kept for UMA.
  ERROR_INDEXING_DNR_RULESET = 40,

  // SandboxedUnpacker::ValidateSignature()
  CRX_REQUIRED_PROOF_MISSING = 41,

  // SandboxedUnpacker::OnVerifiedContentsUncompressed()
  CRX_HEADER_VERIFIED_CONTENTS_UNCOMPRESSING_FAILURE = 42,

  // SandboxedUnpacker::StoreVerifiedContentsInExtensionDir()
  MALFORMED_VERIFIED_CONTENTS = 43,
  COULD_NOT_CREATE_METADATA_DIRECTORY = 44,
  COULD_NOT_WRITE_VERIFIED_CONTENTS_INTO_FILE = 45,

  NUM_FAILURE_REASONS
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_INSTALL_SANDBOXED_UNPACKER_FAILURE_REASON_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_VERIFY_ATTESTATION_REPORT_H_
#define DEVICE_FIDO_ENCLAVE_VERIFY_ATTESTATION_REPORT_H_

#include <array>
#include <cstdint>
#include <string>

#include "base/component_export.h"

// AMD SEV-SNP data structures for attestation reports.

namespace device::enclave {

// The version of all the components in the Trusted Computing Base (TCB).
//
// See Table 3 in <https://www.amd.com/system/files/TechDocs/56860.pdf>.
struct COMPONENT_EXPORT(DEVICE_FIDO) TcbVersion {
  // The current security version number (SVN) of the secure processor (PSP)
  // bootloader.
  uint8_t boot_loader;

  // The current SVN of the PSP operating system.
  uint8_t tee;

  // The current SVN of the SNP firmware.
  uint8_t snp;

  // The lowest current patch level of all the CPU cores.
  uint8_t microcode;
};

// The required policy for a guest to run.
//
// See Table 9 in <https://www.amd.com/system/files/TechDocs/56860.pdf>
struct COMPONENT_EXPORT(DEVICE_FIDO) GuestPolicy {
  // The minimum ABI minor version required to launch the guest.
  uint8_t abi_minor;

  // The minimum ABI major version required to launch the guest.
  uint8_t abi_major;

  // The allowed settings for the guest.
  //
  // Use `GuestPolicy::get_flags` to try to convert this to a `PolicyFlags`
  // enum.
  uint16_t flags;
};

// The number of bytes of custom data that can be included in the attestation
// report.
//
// See Table 22 in <https://www.amd.com/system/files/TechDocs/56860.pdf>.
const size_t REPORT_DATA_SIZE = 64;

// The data contained in an attestation report.
//
// See Table 22 in <https://www.amd.com/system/files/TechDocs/56860.pdf>.
struct COMPONENT_EXPORT(DEVICE_FIDO) AttestationReportData {
  // The version of the attestation report format.
  //
  // This implementation is based on version 2.
  uint32_t version;

  // The guest security version number.
  uint32_t guest_svn;

  // The policy required by the guest VM to be launched.
  GuestPolicy policy;

  // The family ID provided at launch.
  std::array<uint8_t, 16> family_id;

  // The image ID provided at launch.
  std::array<uint8_t, 16> image_id;

  // The VMPL value that was passed in the request.
  uint32_t vmpl;

  // The algorithm used to sign the report.
  //
  // Use `AttestationReportData::get_signature_algo` to try to convert this
  // to a `SigningAlgorithm` enum.
  uint32_t signature_algo;

  // The current version of each of the components in the Trusted Computing
  // Base (TCB). This could be different from the committed value during
  // provisional execution when firmware is being updated.
  TcbVersion current_tcb;

  // Information about the platform.
  //
  // Use `AttestationReportData::get_platform_info` to try to convert this to
  // a `PlatformInfo` biflag representation.

  uint64_t platform_info;

  // The least significant bit indicates Whether the digest of the author key
  // is included in the report, all other bits are reserved and must be
  // zero.
  //
  // Use `AttestationReportData::get_author_key_en` to try to convert this to
  // an `AuthorKey` enum.
  uint64_t author_key_en;

  // Guest-provided data. The custom data provided in the attestation
  // request.
  std::array<uint8_t, REPORT_DATA_SIZE> report_data;

  // The measurement of the VM memory calculated at launch.
  std::array<uint8_t, 48> measurement;

  // Custom data provided by the hypervisor at launch.
  std::array<uint8_t, 32> host_data;

  // The SHA-384 digest of the ID public key used to sign the ID block that
  // was provided in SNP_LAUNCH_FINISH.
  std::array<uint8_t, 48> id_key_digest;

  // The SHA-384 digest of the author public key used to certify the ID key,
  // if the least significant bit of `author_key_en` is 1, or all zeroes
  // otherwise.
  std::array<uint8_t, 48> author_key_digest;

  // The report ID of this guest.
  std::array<uint8_t, 32> report_id;

  // The report ID of this guest's migration agent.
  std::array<uint8_t, 32> report_id_ma;

  // The reported TCB version that was used to generate the versioned chip
  // endorsement key (VCEK) used to sign this report.
  TcbVersion reported_tcb;

  // Identifier unique to the chip, unless the ID has been masked in
  // configuration in which case it is all zeroes.
  std::array<uint8_t, 64> chip_id;

  // The committed TCB version.
  TcbVersion committed_tcb;

  // The build number of the current secure firmware ABI version.
  uint8_t current_build;

  // The minor number of the current secure firmware ABI version.
  uint8_t current_minor;

  // The major number of the current secure firmware ABI version.
  uint8_t current_major;

  // The build number of the committed secure firmware ABI version.
  uint8_t committed_build;

  // The minor number of the committed secure firmware ABI version.
  uint8_t committed_minor;

  // The major number of the committed secure firmware ABI version.
  uint8_t committed_major;

  // The value of the current TCB version when the guest was launched or
  // imported.
  TcbVersion launch_tcb;
};

// An ECDSA signature.
//
// See Table 119 in <https://www.amd.com/system/files/TechDocs/56860.pdf>.
struct COMPONENT_EXPORT(DEVICE_FIDO) EcdsaSignature {
  // The R component of this signature. The value is zero-extended and
  // little-endian encoded.
  std::array<uint8_t, 72> r;

  // The S component of this signature. The value is zero-extended and
  // little-endian encoded.
  std::array<uint8_t, 72> s;
};

struct COMPONENT_EXPORT(DEVICE_FIDO) AttestationReport {
  explicit AttestationReport(std::array<uint8_t, REPORT_DATA_SIZE> report_data);
  AttestationReport(const AttestationReport& attestation_report);
  AttestationReport();
  ~AttestationReport();

  // The data contained in the report.
  AttestationReportData data;

  // The signature over the data.
  EcdsaSignature signature;
};

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_VERIFY_ATTESTATION_REPORT_H_

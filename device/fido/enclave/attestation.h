// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_ATTESTATION_H_
#define DEVICE_FIDO_ENCLAVE_ATTESTATION_H_

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/types/expected.h"

namespace device::enclave {

// The result of a successful attestation validation. This contains firmware
// versions of the CPU that's running the enclave.
// See
// https://www.amd.com/content/dam/amd/en/documents/epyc-technical-docs/specifications/56860.pdf
// (esp table three) for more details on these values.
struct AttestationResult {
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

COMPONENT_EXPORT(DEVICE_FIDO)
base::expected<AttestationResult, const char*> ProcessAttestation(
    base::span<const uint8_t> attestation_bytes,
    base::span<const uint8_t, 32> handshake_hash);

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_ATTESTATION_H_

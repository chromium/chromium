// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/attestation_report.h"

namespace device::enclave {

AttestationReport::AttestationReport(
    std::array<uint8_t, REPORT_DATA_SIZE> report_data) {
  data.report_data = std::move(report_data);
}
AttestationReport::AttestationReport(
    const AttestationReport& attestation_report) = default;
AttestationReport::AttestationReport() = default;
AttestationReport::~AttestationReport() = default;

}  // namespace device::enclave

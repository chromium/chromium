// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/platform_trust_store.h"

namespace net {

PlatformTrustStore::CertWithTrust::CertWithTrust(
    std::vector<uint8_t> cert_bytes,
    bssl::CertificateTrust trust)
    : cert_bytes(std::move(cert_bytes)), trust(trust) {}
PlatformTrustStore::CertWithTrust::~CertWithTrust() = default;

PlatformTrustStore::CertWithTrust::CertWithTrust(const CertWithTrust&) =
    default;
PlatformTrustStore::CertWithTrust& PlatformTrustStore::CertWithTrust::operator=(
    const CertWithTrust& other) = default;
PlatformTrustStore::CertWithTrust::CertWithTrust(CertWithTrust&&) = default;
PlatformTrustStore::CertWithTrust& PlatformTrustStore::CertWithTrust::operator=(
    CertWithTrust&& other) = default;

}  // namespace net

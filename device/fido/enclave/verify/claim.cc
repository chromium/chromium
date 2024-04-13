// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/claim.h"

namespace device::enclave {

Subject::Subject() = default;
Subject::Subject(std::string name, std::map<std::string, std::string> digest)
    : name(std::move(name)), digest(std::move(digest)) {}
Subject::~Subject() = default;

ClaimEvidence::ClaimEvidence() = default;
ClaimEvidence::ClaimEvidence(std::optional<std::string> role,
                             std::string uri,
                             std::vector<uint8_t> digest)
    : role(std::move(role)), uri(std::move(uri)), digest(std::move(digest)) {}
ClaimEvidence::~ClaimEvidence() = default;

}  // namespace device::enclave

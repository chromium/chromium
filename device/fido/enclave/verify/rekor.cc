// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/rekor.h"

namespace device::enclave {

RekorSignatureBundle::RekorSignatureBundle(std::vector<uint8_t> canonicalized,
                                           std::vector<uint8_t> signature)
    : canonicalized(std::move(canonicalized)),
      signature(std::move(signature)) {}
RekorSignatureBundle::RekorSignatureBundle() = default;
RekorSignatureBundle::~RekorSignatureBundle() = default;

}  // namespace device::enclave

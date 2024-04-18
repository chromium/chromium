// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_VERIFY_UTILS_H_
#define DEVICE_FIDO_ENCLAVE_VERIFY_UTILS_H_

#include <string>

#include "base/component_export.h"

namespace device::enclave {

// Makes a plausible guess whether the public key is in PEM format.
bool COMPONENT_EXPORT(DEVICE_FIDO) LooksLikePem(std::string_view maybe_pem);

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_VERIFY_UTILS_H_

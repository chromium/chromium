// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cmtg_key_response.h"

#include <utility>

namespace device {

CmtgKeyResponse::CmtgKeyResponse(std::vector<uint8_t> key,
                                 std::vector<uint8_t> signature)
    : key(std::move(key)), signature(std::move(signature)) {}
CmtgKeyResponse::~CmtgKeyResponse() = default;
CmtgKeyResponse::CmtgKeyResponse(CmtgKeyResponse&&) = default;
CmtgKeyResponse& CmtgKeyResponse::operator=(CmtgKeyResponse&&) = default;

}  // namespace device

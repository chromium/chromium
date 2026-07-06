// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CMTG_KEY_RESPONSE_H_
#define DEVICE_FIDO_CMTG_KEY_RESPONSE_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"

namespace device {

// The response to a CMTG key request, shared between make credential and get
// assertion responses.
struct COMPONENT_EXPORT(DEVICE_FIDO) CmtgKeyResponse {
  CmtgKeyResponse(std::vector<uint8_t> key, std::vector<uint8_t> signature);
  ~CmtgKeyResponse();

  CmtgKeyResponse(CmtgKeyResponse&&);
  CmtgKeyResponse& operator=(CmtgKeyResponse&&);
  CmtgKeyResponse(const CmtgKeyResponse&) = delete;
  CmtgKeyResponse& operator=(const CmtgKeyResponse&) = delete;

  // The CMTG public key, in the same format as the credential public key.
  std::vector<uint8_t> key;

  // The CMTG key signature over the WebAuthn signed data.
  std::vector<uint8_t> signature;
};

}  // namespace device

#endif  // DEVICE_FIDO_CMTG_KEY_RESPONSE_H_

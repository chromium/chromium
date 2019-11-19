// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "net/cert/internal/ocsp.h"
#include "net/der/input.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  net::der::Input single_response_der(data, size);
  net::OCSPSingleResponse single_response;
  net::ParseOCSPSingleResponse(single_response_der, &single_response);

  return 0;
}

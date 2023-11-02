// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "net/cert/pki/ocsp.h"
#include "net/der/input.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  net::der::Input response_data_der(data, size);
  net::OCSPResponseData response_data;
  net::ParseOCSPResponseData(response_data_der, &response_data);

  return 0;
}

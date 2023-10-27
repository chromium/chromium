// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "net/cert/pki/parse_certificate.h"
#include "net/der/input.h"
#include "third_party/boringssl/src/include/openssl/base.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::vector<net::ParsedDistributionPoint> distribution_points;

  bool success = ParseCrlDistributionPoints(net::der::Input(data, size),
                                            &distribution_points);

  if (success) {
    // A valid CRLDistributionPoints must have at least 1 element.
    BSSL_CHECK(!distribution_points.empty());
  }

  return 0;
}

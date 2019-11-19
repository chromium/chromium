// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "net/cert/internal/crl.h"
#include "net/der/input.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  net::der::Input idp_der(data, size);

  std::unique_ptr<net::GeneralNames> distribution_point_names;
  net::ContainedCertsType only_contains_cert_type;

  if (net::ParseIssuingDistributionPoint(idp_der, &distribution_point_names,
                                         &only_contains_cert_type)) {
    CHECK((distribution_point_names &&
           distribution_point_names->present_name_types !=
               net::GENERAL_NAME_NONE) ||
          only_contains_cert_type != net::ContainedCertsType::ANY_CERTS);
  }
  return 0;
}

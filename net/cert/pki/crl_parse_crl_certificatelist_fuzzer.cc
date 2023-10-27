// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <tuple>

#include "net/cert/pki/crl.h"
#include "net/der/input.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  net::der::Input crl_der(data, size);

  net::der::Input tbs_cert_list_tlv;
  net::der::Input signature_algorithm_tlv;
  net::der::BitString signature_value;

  std::ignore = net::ParseCrlCertificateList(
      crl_der, &tbs_cert_list_tlv, &signature_algorithm_tlv, &signature_value);

  return 0;
}

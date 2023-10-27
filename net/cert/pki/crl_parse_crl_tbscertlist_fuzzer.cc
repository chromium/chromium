// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <tuple>

#include "net/cert/pki/crl.h"
#include "net/der/input.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  net::der::Input input_der(data, size);

  net::ParsedCrlTbsCertList tbs_cert_list;
  std::ignore = net::ParseCrlTbsCertList(input_der, &tbs_cert_list);

  return 0;
}

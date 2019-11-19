// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "net/cert/internal/parse_certificate.h"
#include "net/der/input.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  net::der::Input der(data, size);

  net::ParsedAuthorityKeyIdentifier authority_key_identifier;

  ignore_result(
      net::ParseAuthorityKeyIdentifier(der, &authority_key_identifier));

  return 0;
}

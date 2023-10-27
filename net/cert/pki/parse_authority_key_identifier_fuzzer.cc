// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <tuple>

#include "net/cert/pki/parse_certificate.h"
#include "net/der/input.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  net::der::Input der(data, size);

  net::ParsedAuthorityKeyIdentifier authority_key_identifier;

  std::ignore =
      net::ParseAuthorityKeyIdentifier(der, &authority_key_identifier);

  return 0;
}

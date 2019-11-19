// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service_win.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/strings/string_piece.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size > 8 * 1024)
    return 0;

  base::StringPiece16 widestr(
      reinterpret_cast<const base::StringPiece16::value_type*>(data), size / 2);
  std::string result;

  if (net::internal::ParseDomainASCII(widestr, &result))
    // Call base::ToLowerASCII to get some additional code coverage signal.
    result = base::ToLowerASCII(result);

  return 0;
}

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service_win.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size > 8 * 1024)
    return 0;

  base::WStringPiece widestr(reinterpret_cast<const wchar_t*>(data), size / 2);
  std::string result = net::internal::ParseDomainASCII(widestr);

  if (!result.empty())
    // Call base::ToLowerASCII to get some additional code coverage signal.
    result = base::ToLowerASCII(result);

  return 0;
}

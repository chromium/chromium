// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>

#include "base/strings/string_util.h"
#include "net/dns/dns_config_service_win.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size > 8 * 1024)
    return 0;

  std::wstring_view widestr(reinterpret_cast<const wchar_t*>(data), size / 2);
  std::string result = net::internal::ParseDomainASCII(widestr);

  if (!result.empty())
    // Call base::ToLowerASCII to get some additional code coverage signal.
    result = base::ToLowerASCII(result);

  return 0;
}

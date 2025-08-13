// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>

#include "base/strings/string_util.h"
#include "net/dns/dns_config_service_win.h"

template <typename destination_string_view>
destination_string_view LaunderFuzzerDataAndSize(const uint8_t* data,
                                                 size_t size) {
  // SAFETY: libFuzzer guarantees `size` bytes behind `data`. Using
  // this calculated longer-stride view is calculably safe.
  //
  // Given `x = sizeof(destination_string_view::value_type)`, the return
  // value has byte-length `x * floor(size / x)`, which is less than or
  // equal to `size`.
  return destination_string_view(
      reinterpret_cast<const destination_string_view::value_type*>(data),
      size / sizeof(typename destination_string_view::value_type));
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size > 8 * 1024) {
    return 0;
  }

  auto widestr = LaunderFuzzerDataAndSize<std::wstring_view>(data, size);
  std::string result = net::internal::ParseDomainASCII(widestr);

  if (!result.empty()) {
    // Call base::ToLowerASCII to get some additional code coverage signal.
    result = base::ToLowerASCII(result);
  }

  return 0;
}

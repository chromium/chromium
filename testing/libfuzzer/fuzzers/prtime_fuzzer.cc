// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/third_party/nspr/prtime.h"

PRTime parsed_time;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1)
    return 0;

  const uint8_t selector = data[0];

  // Using std::string instead of a (potentially faster) fixed buffer to catch
  // accesses beyond the end of the string.
  std::string str(reinterpret_cast<const char*>(data+1), size - 1);
  PR_ParseTimeString(str.c_str(), selector & 1, &parsed_time);

  return 0;
}

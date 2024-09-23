// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "base/time/time.h"
#include "net/http/http_security_headers.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string input(data, data + size);
  base::TimeDelta max_age;
  bool include_subdomains = false;
  net::ParseHSTSHeader(input, &max_age, &include_subdomains);
  return 0;
}

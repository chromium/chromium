// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "base/time/time.h"
#include "net/http/http_security_headers.h"
#include "url/gurl.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string input(data, data + size);
  base::TimeDelta max_age;
  bool enforce;
  GURL report_uri;

  net::ParseExpectCTHeader(input, &max_age, &enforce, &report_uri);
  return 0;
}

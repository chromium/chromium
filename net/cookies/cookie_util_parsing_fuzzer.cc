// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <fuzzer/FuzzedDataProvider.h>

#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);

  // Generate a cookie line and parse it.
  const std::string cookie_line = data_provider.ConsumeRandomLengthString();
  net::cookie_util::ParsedRequestCookies parsed_cookies;
  net::cookie_util::ParseRequestCookieLine(cookie_line, &parsed_cookies);

  // If any non-empty cookies were parsed, the re-serialized cookie line
  // shouldn't be empty. The re-serialized cookie line may not match the
  // original line if the input was malformed.
  if (parsed_cookies.size() > 0) {
    CHECK_GT(
        net::cookie_util::SerializeRequestCookieLine(parsed_cookies).length(),
        0U);
  }

  return 0;
}

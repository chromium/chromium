// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/logging.h"
#include "net/cookies/parsed_cookie.h"

const std::string GetArbitraryString(FuzzedDataProvider* data_provider) {
  // Adding a fudge factor to kMaxCookieSize so that both branches of the bounds
  // detection code will be tested.
  return data_provider->ConsumeRandomLengthString(
      net::ParsedCookie::kMaxCookieSize + 10);
}

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);
  const std::string cookie_line = GetArbitraryString(&data_provider);
  net::ParsedCookie parsed_cookie(cookie_line);

  // Call zero or one of ParsedCookie's mutator methods.  Should not call
  // anything other than SetName/SetValue when !IsValid().
  const uint8_t action = data_provider.ConsumeIntegralInRange(0, 10);
  switch (action) {
    case 1:
      parsed_cookie.SetName(GetArbitraryString(&data_provider));
      break;
    case 2:
      parsed_cookie.SetValue(GetArbitraryString(&data_provider));
      break;
  }

  if (parsed_cookie.IsValid()) {
    switch (action) {
      case 3:
        if (parsed_cookie.IsValid())
          parsed_cookie.SetPath(GetArbitraryString(&data_provider));
        break;
      case 4:
        parsed_cookie.SetDomain(GetArbitraryString(&data_provider));
        break;
      case 5:
        parsed_cookie.SetExpires(GetArbitraryString(&data_provider));
        break;
      case 6:
        parsed_cookie.SetMaxAge(GetArbitraryString(&data_provider));
        break;
      case 7:
        parsed_cookie.SetIsSecure(data_provider.ConsumeBool());
        break;
      case 8:
        parsed_cookie.SetIsHttpOnly(data_provider.ConsumeBool());
        break;
      case 9:
        parsed_cookie.SetSameSite(GetArbitraryString(&data_provider));
        break;
      case 10:
        parsed_cookie.SetPriority(GetArbitraryString(&data_provider));
        break;
    }
  }

  // Check that serialize/deserialize inverse property holds for valid cookies.
  if (parsed_cookie.IsValid()) {
    const std::string serialized = parsed_cookie.ToCookieLine();
    net::ParsedCookie reparsed_cookie(serialized);
    const std::string reserialized = reparsed_cookie.ToCookieLine();

    // RFC6265 requires semicolons to be followed by spaces. Because our parser
    // permits this rule to be broken, but follows the rule in ToCookieLine(),
    // it's possible to serialize a string that's longer than the original
    // input. If the serialized string exceeds kMaxCookieSize, the parser will
    // reject it. For this fuzzer, we are considering this situation a false
    // positive.
    if (serialized.size() <= net::ParsedCookie::kMaxCookieSize) {
      CHECK(reparsed_cookie.IsValid());
      CHECK_EQ(serialized, reserialized);
    }
  }

  return 0;
}

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/check_op.h"
#include "net/cookies/parsed_cookie.h"

const std::string GetArbitraryNameValueString(
    FuzzedDataProvider* data_provider) {
  // There's no longer an upper bound on the size of a cookie line, but
  // in practice using double kMaxCookieNamePlusValueSize should allow
  // the majority of interesting cases to be covered.
  return data_provider->ConsumeRandomLengthString(
      net::ParsedCookie::kMaxCookieNamePlusValueSize * 2);
}

const std::string GetArbitraryAttributeValueString(
    FuzzedDataProvider* data_provider) {
  // Adding a fudge factor to kMaxCookieAttributeValueSize so that both branches
  // of the bounds detection code will be tested.
  return data_provider->ConsumeRandomLengthString(
      net::ParsedCookie::kMaxCookieAttributeValueSize + 10);
}

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);
  const std::string cookie_line = GetArbitraryNameValueString(&data_provider);
  net::ParsedCookie parsed_cookie(cookie_line);

  // Call zero or one of ParsedCookie's mutator methods.  Should not call
  // anything other than SetName/SetValue when !IsValid().
  const uint8_t action = data_provider.ConsumeIntegralInRange(0, 11);
  switch (action) {
    case 1:
      parsed_cookie.SetName(GetArbitraryNameValueString(&data_provider));
      break;
    case 2:
      parsed_cookie.SetValue(GetArbitraryNameValueString(&data_provider));
      break;
  }

  if (parsed_cookie.IsValid()) {
    switch (action) {
      case 3:
        if (parsed_cookie.IsValid())
          parsed_cookie.SetPath(
              GetArbitraryAttributeValueString(&data_provider));
        break;
      case 4:
        parsed_cookie.SetDomain(
            GetArbitraryAttributeValueString(&data_provider));
        break;
      case 5:
        parsed_cookie.SetExpires(
            GetArbitraryAttributeValueString(&data_provider));
        break;
      case 6:
        parsed_cookie.SetMaxAge(
            GetArbitraryAttributeValueString(&data_provider));
        break;
      case 7:
        parsed_cookie.SetIsSecure(data_provider.ConsumeBool());
        break;
      case 8:
        parsed_cookie.SetIsHttpOnly(data_provider.ConsumeBool());
        break;
      case 9:
        parsed_cookie.SetSameSite(
            GetArbitraryAttributeValueString(&data_provider));
        break;
      case 10:
        parsed_cookie.SetPriority(
            GetArbitraryAttributeValueString(&data_provider));
        break;
      case 11:
        parsed_cookie.SetIsPartitioned(data_provider.ConsumeBool());
        break;
    }
  }

  // Check that serialize/deserialize inverse property holds for valid cookies.
  if (parsed_cookie.IsValid()) {
    const std::string serialized = parsed_cookie.ToCookieLine();
    net::ParsedCookie reparsed_cookie(serialized);
    const std::string reserialized = reparsed_cookie.ToCookieLine();
    CHECK(reparsed_cookie.IsValid());
    CHECK_EQ(serialized, reserialized);
  }

  return 0;
}

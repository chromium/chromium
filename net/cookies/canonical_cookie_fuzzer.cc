// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <limits>
#include <memory>

#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"

namespace net {
const base::Time getRandomTime(FuzzedDataProvider* data_provider) {
  const uint64_t max = std::numeric_limits<uint64_t>::max();
  return base::Time::FromTimeT(
      data_provider->ConsumeIntegralInRange<uint64_t>(0, max));
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);

  const std::string name = data_provider.ConsumeRandomLengthString(800);
  const std::string value = data_provider.ConsumeRandomLengthString(800);
  const std::string domain = data_provider.ConsumeRandomLengthString(800);
  const std::string path = data_provider.ConsumeRandomLengthString(800);

  const GURL url(data_provider.ConsumeRandomLengthString(800));
  if (!url.is_valid())
    return 0;

  const base::Time creation = getRandomTime(&data_provider);
  const base::Time expiration = getRandomTime(&data_provider);
  const base::Time last_access = getRandomTime(&data_provider);

  const CookieSameSite same_site =
      data_provider.PickValueInArray<CookieSameSite>({
          CookieSameSite::UNSPECIFIED,
          CookieSameSite::NO_RESTRICTION,
          CookieSameSite::LAX_MODE,
          CookieSameSite::STRICT_MODE,
      });

  const CookiePriority priority =
      data_provider.PickValueInArray<CookiePriority>({
          CookiePriority::COOKIE_PRIORITY_LOW,
          CookiePriority::COOKIE_PRIORITY_MEDIUM,
          CookiePriority::COOKIE_PRIORITY_HIGH,
      });

  const std::unique_ptr<const CanonicalCookie> sanitized_cookie =
      CanonicalCookie::CreateSanitizedCookie(
          url, name, value, domain, path, creation, expiration, last_access,
          data_provider.ConsumeBool(), data_provider.ConsumeBool(), same_site,
          priority);

  if (sanitized_cookie) {
    CHECK(sanitized_cookie->IsCanonical());

    // Check identity property of various comparison functions
    const CanonicalCookie copied_cookie = *sanitized_cookie;
    CHECK(sanitized_cookie->IsEquivalent(copied_cookie));
    CHECK(sanitized_cookie->IsEquivalentForSecureCookieMatching(copied_cookie));
    CHECK(!sanitized_cookie->PartialCompare(copied_cookie));
  }

  return 0;
}
}  // namespace net

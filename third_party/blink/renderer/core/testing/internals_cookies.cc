// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/internals_cookies.h"
#include "base/time/time.h"

namespace blink {

InternalCookie* CookieMojomToInternalCookie(
    const network::mojom::blink::CookieWithAccessResultPtr& cookie,
    v8::Isolate* isolate) {
  InternalCookie* result = InternalCookie::Create(isolate);
  result->setName(cookie->cookie.Name().c_str());
  result->setValue(cookie->cookie.Value().c_str());
  result->setPath(cookie->cookie.Path().c_str());
  result->setDomain(cookie->cookie.Domain().c_str());
  result->setSecure(cookie->cookie.SecureAttribute());
  result->setHttpOnly(cookie->cookie.IsHttpOnly());
  if (!cookie->cookie.ExpiryDate().is_null()) {
    // Expiry is omitted if unspecified.
    result->setExpiry(
        (cookie->cookie.ExpiryDate() - base::Time::UnixEpoch()).InSeconds());
  }
  switch (cookie->cookie.SameSite()) {
    case net::CookieSameSite::NO_RESTRICTION:
      result->setSameSite(V8InternalCookieSameSite::Enum::kNone);
      break;
    case net::CookieSameSite::UNSPECIFIED:
    case net::CookieSameSite::LAX_MODE:
      result->setSameSite(V8InternalCookieSameSite::Enum::kLax);
      break;
    case net::CookieSameSite::STRICT_MODE:
      result->setSameSite(V8InternalCookieSameSite::Enum::kStrict);
      break;
  }
  return result;
}

}  // namespace blink

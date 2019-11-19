// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/cookie/canonical_cookie_mojom_traits.h"

#include <utility>

#include "base/optional.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/string_traits_wtf.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace mojo {

// static
WTF::String
StructTraits<network::mojom::CanonicalCookieDataView,
             blink::CanonicalCookie>::name(const blink::CanonicalCookie& c) {
  return c.Name();
}

// static
WTF::String
StructTraits<network::mojom::CanonicalCookieDataView,
             blink::CanonicalCookie>::value(const blink::CanonicalCookie& c) {
  return c.Value();
}

// static
WTF::String
StructTraits<network::mojom::CanonicalCookieDataView,
             blink::CanonicalCookie>::domain(const blink::CanonicalCookie& c) {
  return c.Domain();
}

// static
WTF::String
StructTraits<network::mojom::CanonicalCookieDataView,
             blink::CanonicalCookie>::path(const blink::CanonicalCookie& c) {
  return c.Path();
}

// static
base::Time StructTraits<
    network::mojom::CanonicalCookieDataView,
    blink::CanonicalCookie>::creation(const blink::CanonicalCookie& c) {
  return c.CreationDate();
}

// static
base::Time
StructTraits<network::mojom::CanonicalCookieDataView,
             blink::CanonicalCookie>::expiry(const blink::CanonicalCookie& c) {
  return c.ExpiryDate();
}

// static
base::Time StructTraits<
    network::mojom::CanonicalCookieDataView,
    blink::CanonicalCookie>::last_access(const blink::CanonicalCookie& c) {
  return c.LastAccessDate();
}

// static
bool StructTraits<network::mojom::CanonicalCookieDataView,
                  blink::CanonicalCookie>::secure(const blink::CanonicalCookie&
                                                      c) {
  return c.IsSecure();
}
// static
bool StructTraits<
    network::mojom::CanonicalCookieDataView,
    blink::CanonicalCookie>::httponly(const blink::CanonicalCookie& c) {
  return c.IsHttpOnly();
}

// static
network::mojom::CookieSameSite
StructTraits<network::mojom::CanonicalCookieDataView, blink::CanonicalCookie>::
    site_restrictions(const blink::CanonicalCookie& c) {
  return c.SameSite();
}

// static
network::mojom::CookiePriority StructTraits<
    network::mojom::CanonicalCookieDataView,
    blink::CanonicalCookie>::priority(const blink::CanonicalCookie& c) {
  return c.Priority();
}

// static
bool StructTraits<network::mojom::CanonicalCookieDataView,
                  blink::CanonicalCookie>::
    Read(network::mojom::CanonicalCookieDataView cookie,
         blink::CanonicalCookie* out) {
  WTF::String name;
  if (!cookie.ReadName(&name))
    return false;

  WTF::String value;
  if (!cookie.ReadValue(&value))
    return false;

  WTF::String domain;
  if (!cookie.ReadDomain(&domain))
    return false;

  WTF::String path;
  if (!cookie.ReadPath(&path))
    return false;

  base::Time creation;
  if (!cookie.ReadCreation(&creation))
    return false;

  base::Time expiry;
  if (!cookie.ReadExpiry(&expiry))
    return false;

  base::Time last_access;
  if (!cookie.ReadLastAccess(&last_access))
    return false;

  network::mojom::CookieSameSite site_restrictions;
  if (!cookie.ReadSiteRestrictions(&site_restrictions))
    return false;

  network::mojom::CookiePriority priority;
  if (!cookie.ReadPriority(&priority))
    return false;

  base::Optional<blink::CanonicalCookie> created =
      blink::CanonicalCookie::Create(
          name, value, domain, path, creation, expiry, last_access,
          cookie.secure(), cookie.httponly(), site_restrictions, priority);
  if (!created)
    return false;
  *out = std::move(created).value();
  return true;
}

}  // namespace mojo

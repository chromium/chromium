// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_COOKIE_CANONICAL_COOKIE_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_COOKIE_CANONICAL_COOKIE_MOJOM_TRAITS_H_

#include "base/time/time.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/network/public/mojom/cookie_manager.mojom-blink-forward.h"
#include "services/network/public/mojom/cookie_manager.mojom-shared.h"
#include "third_party/blink/renderer/platform/cookie/canonical_cookie.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace mojo {

template <>
struct PLATFORM_EXPORT StructTraits<network::mojom::CanonicalCookieDataView,
                                    blink::CanonicalCookie> {
  static WTF::String name(const blink::CanonicalCookie& c);
  static WTF::String value(const blink::CanonicalCookie& c);
  static WTF::String domain(const blink::CanonicalCookie& c);
  static WTF::String path(const blink::CanonicalCookie& c);
  static base::Time creation(const blink::CanonicalCookie& c);
  static base::Time expiry(const blink::CanonicalCookie& c);
  static base::Time last_access(const blink::CanonicalCookie& c);
  static bool secure(const blink::CanonicalCookie& c);
  static bool httponly(const blink::CanonicalCookie& c);
  static network::mojom::CookieSameSite site_restrictions(
      const blink::CanonicalCookie& c);
  static network::mojom::CookiePriority priority(
      const blink::CanonicalCookie& c);

  static bool Read(network::mojom::CanonicalCookieDataView cookie,
                   blink::CanonicalCookie* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_COOKIE_CANONICAL_COOKIE_MOJOM_TRAITS_H_

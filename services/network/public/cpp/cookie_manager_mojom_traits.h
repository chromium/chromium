// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_COOKIE_MANAGER_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_COOKIE_MANAGER_MOJOM_TRAITS_H_

#include "ipc/ipc_message_utils.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace mojo {

template <>
struct EnumTraits<network::mojom::CookiePriority, net::CookiePriority> {
  static network::mojom::CookiePriority ToMojom(net::CookiePriority input);
  static bool FromMojom(network::mojom::CookiePriority input,
                        net::CookiePriority* output);
};

template <>
struct EnumTraits<network::mojom::CookieSameSite, net::CookieSameSite> {
  static network::mojom::CookieSameSite ToMojom(net::CookieSameSite input);
  static bool FromMojom(network::mojom::CookieSameSite input,
                        net::CookieSameSite* output);
};

template <>
struct EnumTraits<network::mojom::CookieAccessSemantics,
                  net::CookieAccessSemantics> {
  static network::mojom::CookieAccessSemantics ToMojom(
      net::CookieAccessSemantics input);
  static bool FromMojom(network::mojom::CookieAccessSemantics input,
                        net::CookieAccessSemantics* output);
};

template <>
struct EnumTraits<network::mojom::CookieInclusionStatusWarningReason,
                  net::CanonicalCookie::CookieInclusionStatus::WarningReason> {
  static network::mojom::CookieInclusionStatusWarningReason ToMojom(
      net::CanonicalCookie::CookieInclusionStatus::WarningReason input);
  static bool FromMojom(
      network::mojom::CookieInclusionStatusWarningReason input,
      net::CanonicalCookie::CookieInclusionStatus::WarningReason* output);
};

template <>
struct EnumTraits<network::mojom::CookieSameSiteContext,
                  net::CookieOptions::SameSiteCookieContext> {
  static network::mojom::CookieSameSiteContext ToMojom(
      net::CookieOptions::SameSiteCookieContext input);

  static bool FromMojom(network::mojom::CookieSameSiteContext input,
                        net::CookieOptions::SameSiteCookieContext* output);
};

template <>
struct EnumTraits<network::mojom::CookieChangeCause, net::CookieChangeCause> {
  static network::mojom::CookieChangeCause ToMojom(
      net::CookieChangeCause input);

  static bool FromMojom(network::mojom::CookieChangeCause input,
                        net::CookieChangeCause* output);
};

template <>
struct StructTraits<network::mojom::CookieOptionsDataView, net::CookieOptions> {
  static bool exclude_httponly(const net::CookieOptions& o) {
    return o.exclude_httponly();
  }
  static net::CookieOptions::SameSiteCookieContext same_site_cookie_context(
      const net::CookieOptions& o) {
    return o.same_site_cookie_context();
  }
  static bool update_access_time(const net::CookieOptions& o) {
    return o.update_access_time();
  }
  static bool return_excluded_cookies(const net::CookieOptions& o) {
    return o.return_excluded_cookies();
  }

  static bool Read(network::mojom::CookieOptionsDataView mojo_options,
                   net::CookieOptions* cookie_options);
};

template <>
struct StructTraits<network::mojom::CanonicalCookieDataView,
                    net::CanonicalCookie> {
  static const std::string& name(const net::CanonicalCookie& c) {
    return c.Name();
  }
  static const std::string& value(const net::CanonicalCookie& c) {
    return c.Value();
  }
  static const std::string& domain(const net::CanonicalCookie& c) {
    return c.Domain();
  }
  static const std::string& path(const net::CanonicalCookie& c) {
    return c.Path();
  }
  static base::Time creation(const net::CanonicalCookie& c) {
    return c.CreationDate();
  }
  static base::Time expiry(const net::CanonicalCookie& c) {
    return c.ExpiryDate();
  }
  static base::Time last_access(const net::CanonicalCookie& c) {
    return c.LastAccessDate();
  }
  static bool secure(const net::CanonicalCookie& c) { return c.IsSecure(); }
  static bool httponly(const net::CanonicalCookie& c) { return c.IsHttpOnly(); }
  static net::CookieSameSite site_restrictions(const net::CanonicalCookie& c) {
    return c.SameSite();
  }
  static net::CookiePriority priority(const net::CanonicalCookie& c) {
    return c.Priority();
  }

  static bool Read(network::mojom::CanonicalCookieDataView cookie,
                   net::CanonicalCookie* out);
};

template <>
struct StructTraits<network::mojom::CookieInclusionStatusDataView,
                    net::CanonicalCookie::CookieInclusionStatus> {
  static uint32_t exclusion_reasons(
      const net::CanonicalCookie::CookieInclusionStatus& s) {
    return s.exclusion_reasons();
  }
  static net::CanonicalCookie::CookieInclusionStatus::WarningReason warning(
      const net::CanonicalCookie::CookieInclusionStatus& s) {
    return s.warning();
  }
  static bool Read(network::mojom::CookieInclusionStatusDataView status,
                   net::CanonicalCookie::CookieInclusionStatus* out);
};

template <>
struct StructTraits<network::mojom::CookieWithStatusDataView,
                    net::CookieWithStatus> {
  static const net::CanonicalCookie& cookie(const net::CookieWithStatus& c) {
    return c.cookie;
  }
  static const net::CanonicalCookie::CookieInclusionStatus& status(
      const net::CookieWithStatus& c) {
    return c.status;
  }
  static bool Read(network::mojom::CookieWithStatusDataView cookie,
                   net::CookieWithStatus* out);
};

template <>
struct StructTraits<network::mojom::CookieAndLineWithStatusDataView,
                    net::CookieAndLineWithStatus> {
  static const base::Optional<net::CanonicalCookie>& cookie(
      const net::CookieAndLineWithStatus& c) {
    return c.cookie;
  }
  static const std::string& cookie_string(
      const net::CookieAndLineWithStatus& c) {
    return c.cookie_string;
  }
  static const net::CanonicalCookie::CookieInclusionStatus& status(
      const net::CookieAndLineWithStatus& c) {
    return c.status;
  }
  static bool Read(network::mojom::CookieAndLineWithStatusDataView cookie,
                   net::CookieAndLineWithStatus* out);
};

template <>
struct StructTraits<network::mojom::CookieChangeInfoDataView,
                    net::CookieChangeInfo> {
  static const net::CanonicalCookie& cookie(const net::CookieChangeInfo& c) {
    return c.cookie;
  }
  static net::CookieAccessSemantics access_semantics(
      const net::CookieChangeInfo& c) {
    return c.access_semantics;
  }
  static net::CookieChangeCause cause(const net::CookieChangeInfo& c) {
    return c.cause;
  }
  static bool Read(network::mojom::CookieChangeInfoDataView info,
                   net::CookieChangeInfo* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_COOKIE_MANAGER_MOJOM_TRAITS_H_

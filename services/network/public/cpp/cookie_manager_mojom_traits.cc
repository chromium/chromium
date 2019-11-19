// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cookie_manager_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

network::mojom::CookiePriority
EnumTraits<network::mojom::CookiePriority, net::CookiePriority>::ToMojom(
    net::CookiePriority input) {
  switch (input) {
    case net::COOKIE_PRIORITY_LOW:
      return network::mojom::CookiePriority::LOW;
    case net::COOKIE_PRIORITY_MEDIUM:
      return network::mojom::CookiePriority::MEDIUM;
    case net::COOKIE_PRIORITY_HIGH:
      return network::mojom::CookiePriority::HIGH;
  }
  NOTREACHED();
  return static_cast<network::mojom::CookiePriority>(input);
}

bool EnumTraits<network::mojom::CookiePriority, net::CookiePriority>::FromMojom(
    network::mojom::CookiePriority input,
    net::CookiePriority* output) {
  switch (input) {
    case network::mojom::CookiePriority::LOW:
      *output = net::CookiePriority::COOKIE_PRIORITY_LOW;
      return true;
    case network::mojom::CookiePriority::MEDIUM:
      *output = net::CookiePriority::COOKIE_PRIORITY_MEDIUM;
      return true;
    case network::mojom::CookiePriority::HIGH:
      *output = net::CookiePriority::COOKIE_PRIORITY_HIGH;
      return true;
  }
  return false;
}

network::mojom::CookieSameSite
EnumTraits<network::mojom::CookieSameSite, net::CookieSameSite>::ToMojom(
    net::CookieSameSite input) {
  switch (input) {
    case net::CookieSameSite::UNSPECIFIED:
      return network::mojom::CookieSameSite::UNSPECIFIED;
    case net::CookieSameSite::NO_RESTRICTION:
      return network::mojom::CookieSameSite::NO_RESTRICTION;
    case net::CookieSameSite::LAX_MODE:
      return network::mojom::CookieSameSite::LAX_MODE;
    case net::CookieSameSite::STRICT_MODE:
      return network::mojom::CookieSameSite::STRICT_MODE;
    default:
      break;
  }
  NOTREACHED();
  return static_cast<network::mojom::CookieSameSite>(input);
}

bool EnumTraits<network::mojom::CookieSameSite, net::CookieSameSite>::FromMojom(
    network::mojom::CookieSameSite input,
    net::CookieSameSite* output) {
  switch (input) {
    case network::mojom::CookieSameSite::UNSPECIFIED:
      *output = net::CookieSameSite::UNSPECIFIED;
      return true;
    case network::mojom::CookieSameSite::NO_RESTRICTION:
      *output = net::CookieSameSite::NO_RESTRICTION;
      return true;
    case network::mojom::CookieSameSite::LAX_MODE:
      *output = net::CookieSameSite::LAX_MODE;
      return true;
    case network::mojom::CookieSameSite::STRICT_MODE:
      *output = net::CookieSameSite::STRICT_MODE;
      return true;
    default:
      break;
  }
  return false;
}

network::mojom::CookieAccessSemantics EnumTraits<
    network::mojom::CookieAccessSemantics,
    net::CookieAccessSemantics>::ToMojom(net::CookieAccessSemantics input) {
  switch (input) {
    case net::CookieAccessSemantics::UNKNOWN:
      return network::mojom::CookieAccessSemantics::UNKNOWN;
    case net::CookieAccessSemantics::NONLEGACY:
      return network::mojom::CookieAccessSemantics::NONLEGACY;
    case net::CookieAccessSemantics::LEGACY:
      return network::mojom::CookieAccessSemantics::LEGACY;
    default:
      break;
  }
  NOTREACHED();
  return static_cast<network::mojom::CookieAccessSemantics>(input);
}

bool EnumTraits<network::mojom::CookieAccessSemantics,
                net::CookieAccessSemantics>::
    FromMojom(network::mojom::CookieAccessSemantics input,
              net::CookieAccessSemantics* output) {
  switch (input) {
    case network::mojom::CookieAccessSemantics::UNKNOWN:
      *output = net::CookieAccessSemantics::UNKNOWN;
      return true;
    case network::mojom::CookieAccessSemantics::NONLEGACY:
      *output = net::CookieAccessSemantics::NONLEGACY;
      return true;
    case network::mojom::CookieAccessSemantics::LEGACY:
      *output = net::CookieAccessSemantics::LEGACY;
      return true;
    default:
      break;
  }
  return false;
}

network::mojom::CookieInclusionStatusWarningReason
EnumTraits<network::mojom::CookieInclusionStatusWarningReason,
           net::CanonicalCookie::CookieInclusionStatus::WarningReason>::
    ToMojom(net::CanonicalCookie::CookieInclusionStatus::WarningReason input) {
  switch (input) {
    case net::CanonicalCookie::CookieInclusionStatus::WarningReason::
        DO_NOT_WARN:
      return network::mojom::CookieInclusionStatusWarningReason::DO_NOT_WARN;
    case net::CanonicalCookie::CookieInclusionStatus::WarningReason::
        WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT:
      return network::mojom::CookieInclusionStatusWarningReason::
          WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT;
    case net::CanonicalCookie::CookieInclusionStatus::WarningReason::
        WARN_SAMESITE_NONE_INSECURE:
      return network::mojom::CookieInclusionStatusWarningReason::
          WARN_SAMESITE_NONE_INSECURE;
    case net::CanonicalCookie::CookieInclusionStatus::WarningReason::
        WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE:
      return network::mojom::CookieInclusionStatusWarningReason::
          WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE;
  }
  NOTREACHED();
  return network::mojom::CookieInclusionStatusWarningReason::DO_NOT_WARN;
}

bool EnumTraits<network::mojom::CookieInclusionStatusWarningReason,
                net::CanonicalCookie::CookieInclusionStatus::WarningReason>::
    FromMojom(
        network::mojom::CookieInclusionStatusWarningReason input,
        net::CanonicalCookie::CookieInclusionStatus::WarningReason* output) {
  switch (input) {
    case network::mojom::CookieInclusionStatusWarningReason::DO_NOT_WARN:
      *output = net::CanonicalCookie::CookieInclusionStatus::WarningReason::
          DO_NOT_WARN;
      return true;
    case network::mojom::CookieInclusionStatusWarningReason::
        WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT:
      *output = net::CanonicalCookie::CookieInclusionStatus::WarningReason::
          WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT;
      return true;
    case network::mojom::CookieInclusionStatusWarningReason::
        WARN_SAMESITE_NONE_INSECURE:
      *output = net::CanonicalCookie::CookieInclusionStatus::WarningReason::
          WARN_SAMESITE_NONE_INSECURE;
      return true;
    case network::mojom::CookieInclusionStatusWarningReason::
        WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE:
      *output = net::CanonicalCookie::CookieInclusionStatus::WarningReason::
          WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE;
      return true;
  }
  NOTREACHED();
  return false;
}

network::mojom::CookieSameSiteContext
EnumTraits<network::mojom::CookieSameSiteContext,
           net::CookieOptions::SameSiteCookieContext>::
    ToMojom(net::CookieOptions::SameSiteCookieContext input) {
  switch (input) {
    case net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT:
      return network::mojom::CookieSameSiteContext::SAME_SITE_STRICT;
    case net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX:
      return network::mojom::CookieSameSiteContext::SAME_SITE_LAX;
    case net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX_METHOD_UNSAFE:
      return network::mojom::CookieSameSiteContext::SAME_SITE_LAX_METHOD_UNSAFE;
    case net::CookieOptions::SameSiteCookieContext::CROSS_SITE:
      return network::mojom::CookieSameSiteContext::CROSS_SITE;
    case net::CookieOptions::SameSiteCookieContext::
        SAME_SITE_LAX_METHOD_UNSAFE_CROSS_SCHEME_SECURE_URL:
      return network::mojom::CookieSameSiteContext::
          SAME_SITE_LAX_METHOD_UNSAFE_CROSS_SCHEME_SECURE_URL;
    case net::CookieOptions::SameSiteCookieContext::
        SAME_SITE_LAX_CROSS_SCHEME_SECURE_URL:
      return network::mojom::CookieSameSiteContext::
          SAME_SITE_LAX_CROSS_SCHEME_SECURE_URL;
    case net::CookieOptions::SameSiteCookieContext::
        SAME_SITE_STRICT_CROSS_SCHEME_SECURE_URL:
      return network::mojom::CookieSameSiteContext::
          SAME_SITE_STRICT_CROSS_SCHEME_SECURE_URL;
    case net::CookieOptions::SameSiteCookieContext::
        SAME_SITE_LAX_METHOD_UNSAFE_CROSS_SCHEME_INSECURE_URL:
      return network::mojom::CookieSameSiteContext::
          SAME_SITE_LAX_METHOD_UNSAFE_CROSS_SCHEME_INSECURE_URL;
    case net::CookieOptions::SameSiteCookieContext::
        SAME_SITE_LAX_CROSS_SCHEME_INSECURE_URL:
      return network::mojom::CookieSameSiteContext::
          SAME_SITE_LAX_CROSS_SCHEME_INSECURE_URL;
    case net::CookieOptions::SameSiteCookieContext::
        SAME_SITE_STRICT_CROSS_SCHEME_INSECURE_URL:
      return network::mojom::CookieSameSiteContext::
          SAME_SITE_STRICT_CROSS_SCHEME_INSECURE_URL;
    default:
      NOTREACHED();
      return network::mojom::CookieSameSiteContext::CROSS_SITE;
  }
}

bool EnumTraits<network::mojom::CookieSameSiteContext,
                net::CookieOptions::SameSiteCookieContext>::
    FromMojom(network::mojom::CookieSameSiteContext input,
              net::CookieOptions::SameSiteCookieContext* output) {
  switch (input) {
    case network::mojom::CookieSameSiteContext::SAME_SITE_STRICT:
      *output = net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT;
      return true;
    case network::mojom::CookieSameSiteContext::SAME_SITE_LAX:
      *output = net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX;
      return true;
    case network::mojom::CookieSameSiteContext::SAME_SITE_LAX_METHOD_UNSAFE:
      *output = net::CookieOptions::SameSiteCookieContext::
          SAME_SITE_LAX_METHOD_UNSAFE;
      return true;
    case network::mojom::CookieSameSiteContext::CROSS_SITE:
      *output = net::CookieOptions::SameSiteCookieContext::CROSS_SITE;
      return true;
    case network::mojom::CookieSameSiteContext::
        SAME_SITE_LAX_METHOD_UNSAFE_CROSS_SCHEME_SECURE_URL:
      *output = net::CookieOptions::SameSiteCookieContext::
          SAME_SITE_LAX_METHOD_UNSAFE_CROSS_SCHEME_SECURE_URL;
      return true;
    case network::mojom::CookieSameSiteContext::
        SAME_SITE_LAX_CROSS_SCHEME_SECURE_URL:
      *output = net::CookieOptions::SameSiteCookieContext::
          SAME_SITE_LAX_CROSS_SCHEME_SECURE_URL;
      return true;
    case network::mojom::CookieSameSiteContext::
        SAME_SITE_STRICT_CROSS_SCHEME_SECURE_URL:
      *output = net::CookieOptions::SameSiteCookieContext::
          SAME_SITE_STRICT_CROSS_SCHEME_SECURE_URL;
      return true;
    case network::mojom::CookieSameSiteContext::
        SAME_SITE_LAX_METHOD_UNSAFE_CROSS_SCHEME_INSECURE_URL:
      *output = net::CookieOptions::SameSiteCookieContext::
          SAME_SITE_LAX_METHOD_UNSAFE_CROSS_SCHEME_INSECURE_URL;
      return true;
    case network::mojom::CookieSameSiteContext::
        SAME_SITE_LAX_CROSS_SCHEME_INSECURE_URL:
      *output = net::CookieOptions::SameSiteCookieContext::
          SAME_SITE_LAX_CROSS_SCHEME_INSECURE_URL;
      return true;
    case network::mojom::CookieSameSiteContext::
        SAME_SITE_STRICT_CROSS_SCHEME_INSECURE_URL:
      *output = net::CookieOptions::SameSiteCookieContext::
          SAME_SITE_STRICT_CROSS_SCHEME_INSECURE_URL;
      return true;
  }
  return false;
}

network::mojom::CookieChangeCause
EnumTraits<network::mojom::CookieChangeCause, net::CookieChangeCause>::ToMojom(
    net::CookieChangeCause input) {
  switch (input) {
    case net::CookieChangeCause::INSERTED:
      return network::mojom::CookieChangeCause::INSERTED;
    case net::CookieChangeCause::EXPLICIT:
      return network::mojom::CookieChangeCause::EXPLICIT;
    case net::CookieChangeCause::UNKNOWN_DELETION:
      return network::mojom::CookieChangeCause::UNKNOWN_DELETION;
    case net::CookieChangeCause::OVERWRITE:
      return network::mojom::CookieChangeCause::OVERWRITE;
    case net::CookieChangeCause::EXPIRED:
      return network::mojom::CookieChangeCause::EXPIRED;
    case net::CookieChangeCause::EVICTED:
      return network::mojom::CookieChangeCause::EVICTED;
    case net::CookieChangeCause::EXPIRED_OVERWRITE:
      return network::mojom::CookieChangeCause::EXPIRED_OVERWRITE;
    default:
      break;
  }
  NOTREACHED();
  return static_cast<network::mojom::CookieChangeCause>(input);
}

bool EnumTraits<network::mojom::CookieChangeCause, net::CookieChangeCause>::
    FromMojom(network::mojom::CookieChangeCause input,
              net::CookieChangeCause* output) {
  switch (input) {
    case network::mojom::CookieChangeCause::INSERTED:
      *output = net::CookieChangeCause::INSERTED;
      return true;
    case network::mojom::CookieChangeCause::EXPLICIT:
      *output = net::CookieChangeCause::EXPLICIT;
      return true;
    case network::mojom::CookieChangeCause::UNKNOWN_DELETION:
      *output = net::CookieChangeCause::UNKNOWN_DELETION;
      return true;
    case network::mojom::CookieChangeCause::OVERWRITE:
      *output = net::CookieChangeCause::OVERWRITE;
      return true;
    case network::mojom::CookieChangeCause::EXPIRED:
      *output = net::CookieChangeCause::EXPIRED;
      return true;
    case network::mojom::CookieChangeCause::EVICTED:
      *output = net::CookieChangeCause::EVICTED;
      return true;
    case network::mojom::CookieChangeCause::EXPIRED_OVERWRITE:
      *output = net::CookieChangeCause::EXPIRED_OVERWRITE;
      return true;
    default:
      break;
  }
  return false;
}

bool StructTraits<network::mojom::CookieOptionsDataView, net::CookieOptions>::
    Read(network::mojom::CookieOptionsDataView mojo_options,
         net::CookieOptions* cookie_options) {
  if (mojo_options.exclude_httponly())
    cookie_options->set_exclude_httponly();
  else
    cookie_options->set_include_httponly();

  net::CookieOptions::SameSiteCookieContext same_site_cookie_context;
  if (!mojo_options.ReadSameSiteCookieContext(&same_site_cookie_context))
    return false;
  cookie_options->set_same_site_cookie_context(same_site_cookie_context);

  if (mojo_options.update_access_time())
    cookie_options->set_update_access_time();
  else
    cookie_options->set_do_not_update_access_time();

  if (mojo_options.return_excluded_cookies())
    cookie_options->set_return_excluded_cookies();
  else
    cookie_options->unset_return_excluded_cookies();

  return true;
}

bool StructTraits<
    network::mojom::CanonicalCookieDataView,
    net::CanonicalCookie>::Read(network::mojom::CanonicalCookieDataView cookie,
                                net::CanonicalCookie* out) {
  std::string name;
  if (!cookie.ReadName(&name))
    return false;

  std::string value;
  if (!cookie.ReadValue(&value))
    return false;

  std::string domain;
  if (!cookie.ReadDomain(&domain))
    return false;

  std::string path;
  if (!cookie.ReadPath(&path))
    return false;

  base::Time creation_time;
  base::Time expiry_time;
  base::Time last_access_time;
  if (!cookie.ReadCreation(&creation_time))
    return false;

  if (!cookie.ReadExpiry(&expiry_time))
    return false;

  if (!cookie.ReadLastAccess(&last_access_time))
    return false;

  net::CookieSameSite site_restrictions;
  if (!cookie.ReadSiteRestrictions(&site_restrictions))
    return false;

  net::CookiePriority priority;
  if (!cookie.ReadPriority(&priority))
    return false;

  *out = net::CanonicalCookie(name, value, domain, path, creation_time,
                              expiry_time, last_access_time, cookie.secure(),
                              cookie.httponly(), site_restrictions, priority);
  return out->IsCanonical();
}

bool StructTraits<network::mojom::CookieInclusionStatusDataView,
                  net::CanonicalCookie::CookieInclusionStatus>::
    Read(network::mojom::CookieInclusionStatusDataView status,
         net::CanonicalCookie::CookieInclusionStatus* out) {
  *out = net::CanonicalCookie::CookieInclusionStatus();
  out->set_exclusion_reasons(status.exclusion_reasons());

  net::CanonicalCookie::CookieInclusionStatus::WarningReason warning;
  if (!status.ReadWarning(&warning))
    return false;
  out->set_warning(warning);

  return out->IsValid();
}

bool StructTraits<
    network::mojom::CookieWithStatusDataView,
    net::CookieWithStatus>::Read(network::mojom::CookieWithStatusDataView c,
                                 net::CookieWithStatus* out) {
  net::CanonicalCookie cookie;
  net::CanonicalCookie::CookieInclusionStatus status;
  if (!c.ReadCookie(&cookie))
    return false;
  if (!c.ReadStatus(&status))
    return false;

  *out = {cookie, status};

  return true;
}

bool StructTraits<network::mojom::CookieAndLineWithStatusDataView,
                  net::CookieAndLineWithStatus>::
    Read(network::mojom::CookieAndLineWithStatusDataView c,
         net::CookieAndLineWithStatus* out) {
  base::Optional<net::CanonicalCookie> cookie;
  std::string cookie_string;
  net::CanonicalCookie::CookieInclusionStatus status;
  if (!c.ReadCookie(&cookie))
    return false;
  if (!c.ReadCookieString(&cookie_string))
    return false;
  if (!c.ReadStatus(&status))
    return false;

  *out = {cookie, cookie_string, status};

  return true;
}

bool StructTraits<
    network::mojom::CookieChangeInfoDataView,
    net::CookieChangeInfo>::Read(network::mojom::CookieChangeInfoDataView info,
                                 net::CookieChangeInfo* out) {
  net::CanonicalCookie cookie;
  net::CookieAccessSemantics access_semantics =
      net::CookieAccessSemantics::UNKNOWN;
  net::CookieChangeCause cause = net::CookieChangeCause::EXPLICIT;
  if (!info.ReadCookie(&cookie))
    return false;
  if (!info.ReadAccessSemantics(&access_semantics))
    return false;
  if (!info.ReadCause(&cause))
    return false;

  *out = net::CookieChangeInfo(cookie, access_semantics, cause);
  return true;
}

}  // namespace mojo

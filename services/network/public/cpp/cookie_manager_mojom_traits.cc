// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cookie_manager_mojom_traits.h"

#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/string_data_view.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_partition_key.h"
#include "services/network/public/mojom/cookie_manager.mojom-shared.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/schemeful_site.mojom-shared.h"
#include "url/mojom/origin.mojom-shared.h"

namespace mojo {

network::mojom::CookieSourceType
EnumTraits<network::mojom::CookieSourceType, net::CookieSourceType>::ToMojom(
    net::CookieSourceType input) {
  switch (input) {
    case net::CookieSourceType::kHTTP:
      return network::mojom::CookieSourceType::kHTTP;
    case net::CookieSourceType::kScript:
      return network::mojom::CookieSourceType::kScript;
    case net::CookieSourceType::kOther:
      return network::mojom::CookieSourceType::kOther;
  }
}

net::CookieSourceType
EnumTraits<network::mojom::CookieSourceType, net::CookieSourceType>::FromMojom(
    network::mojom::CookieSourceType input) {
  switch (input) {
    case network::mojom::CookieSourceType::kHTTP:
      return net::CookieSourceType::kHTTP;
    case network::mojom::CookieSourceType::kScript:
      return net::CookieSourceType::kScript;
    case network::mojom::CookieSourceType::kOther:
      return net::CookieSourceType::kOther;
  }
  NOTREACHED();
}

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
}

net::CookiePriority
EnumTraits<network::mojom::CookiePriority, net::CookiePriority>::FromMojom(
    network::mojom::CookiePriority input) {
  switch (input) {
    case network::mojom::CookiePriority::LOW:
      return net::CookiePriority::COOKIE_PRIORITY_LOW;
    case network::mojom::CookiePriority::MEDIUM:
      return net::CookiePriority::COOKIE_PRIORITY_MEDIUM;
    case network::mojom::CookiePriority::HIGH:
      return net::CookiePriority::COOKIE_PRIORITY_HIGH;
  }
  NOTREACHED();
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
}

net::CookieSameSite
EnumTraits<network::mojom::CookieSameSite, net::CookieSameSite>::FromMojom(
    network::mojom::CookieSameSite input) {
  switch (input) {
    case network::mojom::CookieSameSite::UNSPECIFIED:
      return net::CookieSameSite::UNSPECIFIED;
    case network::mojom::CookieSameSite::NO_RESTRICTION:
      return net::CookieSameSite::NO_RESTRICTION;
    case network::mojom::CookieSameSite::LAX_MODE:
      return net::CookieSameSite::LAX_MODE;
    case network::mojom::CookieSameSite::STRICT_MODE:
      return net::CookieSameSite::STRICT_MODE;
    default:
      break;
  }
  NOTREACHED();
}

network::mojom::CookieEffectiveSameSite EnumTraits<
    network::mojom::CookieEffectiveSameSite,
    net::CookieEffectiveSameSite>::ToMojom(net::CookieEffectiveSameSite input) {
  switch (input) {
    case net::CookieEffectiveSameSite::NO_RESTRICTION:
      return network::mojom::CookieEffectiveSameSite::kNoRestriction;
    case net::CookieEffectiveSameSite::LAX_MODE:
      return network::mojom::CookieEffectiveSameSite::kLaxMode;
    case net::CookieEffectiveSameSite::STRICT_MODE:
      return network::mojom::CookieEffectiveSameSite::kStrictMode;
    case net::CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE:
      return network::mojom::CookieEffectiveSameSite::kLaxModeAllowUnsafe;
    case net::CookieEffectiveSameSite::UNDEFINED:
      return network::mojom::CookieEffectiveSameSite::kUndefined;
    default:
      break;
  }
  NOTREACHED();
}

net::CookieEffectiveSameSite EnumTraits<network::mojom::CookieEffectiveSameSite,
                                        net::CookieEffectiveSameSite>::
    FromMojom(network::mojom::CookieEffectiveSameSite input) {
  switch (input) {
    case network::mojom::CookieEffectiveSameSite::kNoRestriction:
      return net::CookieEffectiveSameSite::NO_RESTRICTION;
    case network::mojom::CookieEffectiveSameSite::kLaxMode:
      return net::CookieEffectiveSameSite::LAX_MODE;
    case network::mojom::CookieEffectiveSameSite::kStrictMode:
      return net::CookieEffectiveSameSite::STRICT_MODE;
    case network::mojom::CookieEffectiveSameSite::kLaxModeAllowUnsafe:
      return net::CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE;
    case network::mojom::CookieEffectiveSameSite::kUndefined:
      return net::CookieEffectiveSameSite::UNDEFINED;
    default:
      break;
  }
  NOTREACHED();
}

network::mojom::CookieSourceScheme
EnumTraits<network::mojom::CookieSourceScheme,
           net::CookieSourceScheme>::ToMojom(net::CookieSourceScheme input) {
  switch (input) {
    case net::CookieSourceScheme::kUnset:
      return network::mojom::CookieSourceScheme::kUnset;
    case net::CookieSourceScheme::kNonSecure:
      return network::mojom::CookieSourceScheme::kNonSecure;
    case net::CookieSourceScheme::kSecure:
      return network::mojom::CookieSourceScheme::kSecure;
  }
  NOTREACHED();
}

net::CookieSourceScheme
EnumTraits<network::mojom::CookieSourceScheme, net::CookieSourceScheme>::
    FromMojom(network::mojom::CookieSourceScheme input) {
  switch (input) {
    case network::mojom::CookieSourceScheme::kUnset:
      return net::CookieSourceScheme::kUnset;
    case network::mojom::CookieSourceScheme::kNonSecure:
      return net::CookieSourceScheme::kNonSecure;
    case network::mojom::CookieSourceScheme::kSecure:
      return net::CookieSourceScheme::kSecure;
  }
  NOTREACHED();
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
}

net::CookieAccessSemantics
EnumTraits<network::mojom::CookieAccessSemantics, net::CookieAccessSemantics>::
    FromMojom(network::mojom::CookieAccessSemantics input) {
  switch (input) {
    case network::mojom::CookieAccessSemantics::UNKNOWN:
      return net::CookieAccessSemantics::UNKNOWN;
    case network::mojom::CookieAccessSemantics::NONLEGACY:
      return net::CookieAccessSemantics::NONLEGACY;
    case network::mojom::CookieAccessSemantics::LEGACY:
      return net::CookieAccessSemantics::LEGACY;
    default:
      break;
  }
  NOTREACHED();
}

network::mojom::CookieScopeSemantics EnumTraits<
    network::mojom::CookieScopeSemantics,
    net::CookieScopeSemantics>::ToMojom(net::CookieScopeSemantics input) {
  switch (input) {
    case net::CookieScopeSemantics::UNKNOWN:
      return network::mojom::CookieScopeSemantics::UNKNOWN;
    case net::CookieScopeSemantics::NONLEGACY:
      return network::mojom::CookieScopeSemantics::NONLEGACY;
    case net::CookieScopeSemantics::LEGACY:
      return network::mojom::CookieScopeSemantics::LEGACY;
  }
}

net::CookieScopeSemantics
EnumTraits<network::mojom::CookieScopeSemantics, net::CookieScopeSemantics>::
    FromMojom(network::mojom::CookieScopeSemantics input) {
  switch (input) {
    case network::mojom::CookieScopeSemantics::UNKNOWN:
      return net::CookieScopeSemantics::UNKNOWN;
    case network::mojom::CookieScopeSemantics::NONLEGACY:
      return net::CookieScopeSemantics::NONLEGACY;
    case network::mojom::CookieScopeSemantics::LEGACY:
      return net::CookieScopeSemantics::LEGACY;
    default:
      break;
  }
  NOTREACHED();
}

network::mojom::ContextType
EnumTraits<network::mojom::ContextType,
           net::CookieOptions::SameSiteCookieContext::ContextType>::
    ToMojom(net::CookieOptions::SameSiteCookieContext::ContextType input) {
  switch (input) {
    case net::CookieOptions::SameSiteCookieContext::ContextType::
        SAME_SITE_STRICT:
      return network::mojom::ContextType::SAME_SITE_STRICT;
    case net::CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX:
      return network::mojom::ContextType::SAME_SITE_LAX;
    case net::CookieOptions::SameSiteCookieContext::ContextType::
        SAME_SITE_LAX_METHOD_UNSAFE:
      return network::mojom::ContextType::SAME_SITE_LAX_METHOD_UNSAFE;
    case net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE:
      return network::mojom::ContextType::CROSS_SITE;
    default:
      NOTREACHED();
  }
}

net::CookieOptions::SameSiteCookieContext::ContextType
EnumTraits<network::mojom::ContextType,
           net::CookieOptions::SameSiteCookieContext::ContextType>::
    FromMojom(network::mojom::ContextType input) {
  switch (input) {
    case network::mojom::ContextType::SAME_SITE_STRICT:
      return net::CookieOptions::SameSiteCookieContext::ContextType::
          SAME_SITE_STRICT;
    case network::mojom::ContextType::SAME_SITE_LAX:
      return net::CookieOptions::SameSiteCookieContext::ContextType::
          SAME_SITE_LAX;
    case network::mojom::ContextType::SAME_SITE_LAX_METHOD_UNSAFE:
      return net::CookieOptions::SameSiteCookieContext::ContextType::
          SAME_SITE_LAX_METHOD_UNSAFE;
    case network::mojom::ContextType::CROSS_SITE:
      return net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE;
  }
  NOTREACHED();
}

network::mojom::CookieSameSiteContextMetadataDowngradeType
EnumTraits<network::mojom::CookieSameSiteContextMetadataDowngradeType,
           net::CookieOptions::SameSiteCookieContext::ContextMetadata::
               ContextDowngradeType>::
    ToMojom(net::CookieOptions::SameSiteCookieContext::ContextMetadata::
                ContextDowngradeType input) {
  switch (input) {
    case net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        ContextDowngradeType::kNoDowngrade:
      return network::mojom::CookieSameSiteContextMetadataDowngradeType::
          kNoDowngrade;
    case net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        ContextDowngradeType::kStrictToLax:
      return network::mojom::CookieSameSiteContextMetadataDowngradeType::
          kStrictToLax;
    case net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        ContextDowngradeType::kStrictToCross:
      return network::mojom::CookieSameSiteContextMetadataDowngradeType::
          kStrictToCross;
    case net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        ContextDowngradeType::kLaxToCross:
      return network::mojom::CookieSameSiteContextMetadataDowngradeType::
          kLaxToCross;
  }
}

net::CookieOptions::SameSiteCookieContext::ContextMetadata::ContextDowngradeType
EnumTraits<network::mojom::CookieSameSiteContextMetadataDowngradeType,
           net::CookieOptions::SameSiteCookieContext::ContextMetadata::
               ContextDowngradeType>::
    FromMojom(
        network::mojom::CookieSameSiteContextMetadataDowngradeType input) {
  switch (input) {
    case network::mojom::CookieSameSiteContextMetadataDowngradeType::
        kNoDowngrade:
      return net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          ContextDowngradeType::kNoDowngrade;
    case network::mojom::CookieSameSiteContextMetadataDowngradeType::
        kStrictToLax:
      return net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          ContextDowngradeType::kStrictToLax;
    case network::mojom::CookieSameSiteContextMetadataDowngradeType::
        kStrictToCross:
      return net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          ContextDowngradeType::kStrictToCross;
    case network::mojom::CookieSameSiteContextMetadataDowngradeType::
        kLaxToCross:
      return net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          ContextDowngradeType::kLaxToCross;
  }
  NOTREACHED();
}

network::mojom::ContextRedirectTypeBug1221316
EnumTraits<network::mojom::ContextRedirectTypeBug1221316,
           net::CookieOptions::SameSiteCookieContext::ContextMetadata::
               ContextRedirectTypeBug1221316>::
    ToMojom(net::CookieOptions::SameSiteCookieContext::ContextMetadata::
                ContextRedirectTypeBug1221316 input) {
  switch (input) {
    case net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        ContextRedirectTypeBug1221316::kUnset:
      return network::mojom::ContextRedirectTypeBug1221316::kUnset;
    case net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        ContextRedirectTypeBug1221316::kNoRedirect:
      return network::mojom::ContextRedirectTypeBug1221316::kNoRedirect;
    case net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        ContextRedirectTypeBug1221316::kCrossSiteRedirect:
      return network::mojom::ContextRedirectTypeBug1221316::kCrossSiteRedirect;
    case net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        ContextRedirectTypeBug1221316::kPartialSameSiteRedirect:
      return network::mojom::ContextRedirectTypeBug1221316::
          kPartialSameSiteRedirect;
    case net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        ContextRedirectTypeBug1221316::kAllSameSiteRedirect:
      return network::mojom::ContextRedirectTypeBug1221316::
          kAllSameSiteRedirect;
  }
}

net::CookieOptions::SameSiteCookieContext::ContextMetadata::
    ContextRedirectTypeBug1221316
    EnumTraits<network::mojom::ContextRedirectTypeBug1221316,
               net::CookieOptions::SameSiteCookieContext::ContextMetadata::
                   ContextRedirectTypeBug1221316>::
        FromMojom(network::mojom::ContextRedirectTypeBug1221316 input) {
  switch (input) {
    case network::mojom::ContextRedirectTypeBug1221316::kUnset:
      return net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          ContextRedirectTypeBug1221316::kUnset;
    case network::mojom::ContextRedirectTypeBug1221316::kNoRedirect:
      return net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          ContextRedirectTypeBug1221316::kNoRedirect;
    case network::mojom::ContextRedirectTypeBug1221316::kCrossSiteRedirect:
      return net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          ContextRedirectTypeBug1221316::kCrossSiteRedirect;
    case network::mojom::ContextRedirectTypeBug1221316::
        kPartialSameSiteRedirect:
      return net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          ContextRedirectTypeBug1221316::kPartialSameSiteRedirect;
    case network::mojom::ContextRedirectTypeBug1221316::kAllSameSiteRedirect:
      return net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          ContextRedirectTypeBug1221316::kAllSameSiteRedirect;
  }
  NOTREACHED();
}

network::mojom::HttpMethod EnumTraits<
    network::mojom::HttpMethod,
    net::CookieOptions::SameSiteCookieContext::ContextMetadata::HttpMethod>::
    ToMojom(
        net::CookieOptions::SameSiteCookieContext::ContextMetadata::HttpMethod
            input) {
  switch (input) {
    case net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        HttpMethod::kUnset:
      return network::mojom::HttpMethod::kUnset;
    case net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        HttpMethod::kUnknown:
      return network::mojom::HttpMethod::kUnknown;
    case net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        HttpMethod::kGet:
      return network::mojom::HttpMethod::kGet;
    case net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        HttpMethod::kHead:
      return network::mojom::HttpMethod::kHead;
    case net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        HttpMethod::kPost:
      return network::mojom::HttpMethod::kPost;
    case net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        HttpMethod::KPut:
      return network::mojom::HttpMethod::KPut;
    case net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        HttpMethod::kDelete:
      return network::mojom::HttpMethod::kDelete;
    case net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        HttpMethod::kConnect:
      return network::mojom::HttpMethod::kConnect;
    case net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        HttpMethod::kOptions:
      return network::mojom::HttpMethod::kOptions;
    case net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        HttpMethod::kTrace:
      return network::mojom::HttpMethod::kTrace;
    case net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        HttpMethod::kPatch:
      return network::mojom::HttpMethod::kPatch;
  }
}

net::CookieOptions::SameSiteCookieContext::ContextMetadata::HttpMethod
EnumTraits<network::mojom::HttpMethod,
           net::CookieOptions::SameSiteCookieContext::ContextMetadata::
               HttpMethod>::FromMojom(network::mojom::HttpMethod input) {
  switch (input) {
    case network::mojom::HttpMethod::kUnset:
      return net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          HttpMethod::kUnset;
    case network::mojom::HttpMethod::kUnknown:
      return net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          HttpMethod::kUnknown;
    case network::mojom::HttpMethod::kGet:
      return net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          HttpMethod::kGet;
    case network::mojom::HttpMethod::kHead:
      return net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          HttpMethod::kHead;
    case network::mojom::HttpMethod::kPost:
      return net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          HttpMethod::kPost;
    case network::mojom::HttpMethod::KPut:
      return net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          HttpMethod::KPut;
    case network::mojom::HttpMethod::kDelete:
      return net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          HttpMethod::kDelete;
    case network::mojom::HttpMethod::kConnect:
      return net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          HttpMethod::kConnect;
    case network::mojom::HttpMethod::kOptions:
      return net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          HttpMethod::kOptions;
    case network::mojom::HttpMethod::kTrace:
      return net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          HttpMethod::kTrace;
    case network::mojom::HttpMethod::kPatch:
      return net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          HttpMethod::kPatch;
  }
  NOTREACHED();
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
    case net::CookieChangeCause::INSERTED_NO_CHANGE_OVERWRITE:
      return network::mojom::CookieChangeCause::INSERTED_NO_CHANGE_OVERWRITE;
    case net::CookieChangeCause::INSERTED_NO_VALUE_CHANGE_OVERWRITE:
      return network::mojom::CookieChangeCause::
          INSERTED_NO_VALUE_CHANGE_OVERWRITE;
    default:
      break;
  }
  NOTREACHED();
}

net::CookieChangeCause
EnumTraits<network::mojom::CookieChangeCause, net::CookieChangeCause>::
    FromMojom(network::mojom::CookieChangeCause input) {
  switch (input) {
    case network::mojom::CookieChangeCause::INSERTED:
      return net::CookieChangeCause::INSERTED;
    case network::mojom::CookieChangeCause::EXPLICIT:
      return net::CookieChangeCause::EXPLICIT;
    case network::mojom::CookieChangeCause::UNKNOWN_DELETION:
      return net::CookieChangeCause::UNKNOWN_DELETION;
    case network::mojom::CookieChangeCause::OVERWRITE:
      return net::CookieChangeCause::OVERWRITE;
    case network::mojom::CookieChangeCause::EXPIRED:
      return net::CookieChangeCause::EXPIRED;
    case network::mojom::CookieChangeCause::EVICTED:
      return net::CookieChangeCause::EVICTED;
    case network::mojom::CookieChangeCause::EXPIRED_OVERWRITE:
      return net::CookieChangeCause::EXPIRED_OVERWRITE;
    case network::mojom::CookieChangeCause::INSERTED_NO_CHANGE_OVERWRITE:
      return net::CookieChangeCause::INSERTED_NO_CHANGE_OVERWRITE;
    case network::mojom::CookieChangeCause::INSERTED_NO_VALUE_CHANGE_OVERWRITE:
      return net::CookieChangeCause::INSERTED_NO_VALUE_CHANGE_OVERWRITE;
    default:
      break;
  }
  NOTREACHED();
}

bool StructTraits<network::mojom::CookieSameSiteContextMetadataDataView,
                  net::CookieOptions::SameSiteCookieContext::ContextMetadata>::
    Read(network::mojom::CookieSameSiteContextMetadataDataView data,
         net::CookieOptions::SameSiteCookieContext::ContextMetadata* out) {
  if (!data.ReadCrossSiteRedirectDowngrade(
          &out->cross_site_redirect_downgrade)) {
    return false;
  }
  if (!data.ReadRedirectTypeBug1221316(&out->redirect_type_bug_1221316)) {
    return false;
  }

  return true;
}

bool StructTraits<network::mojom::CookieSameSiteContextDataView,
                  net::CookieOptions::SameSiteCookieContext>::
    Read(network::mojom::CookieSameSiteContextDataView mojo_context,
         net::CookieOptions::SameSiteCookieContext* context) {
  net::CookieOptions::SameSiteCookieContext::ContextType context_type;
  if (!mojo_context.ReadContext(&context_type)) {
    return false;
  }

  net::CookieOptions::SameSiteCookieContext::ContextType schemeful_context;
  if (!mojo_context.ReadSchemefulContext(&schemeful_context)) {
    return false;
  }

  // schemeful_context must be <= context.
  if (schemeful_context > context_type) {
    return false;
  }

  net::CookieOptions::SameSiteCookieContext::ContextMetadata metadata;
  if (!mojo_context.ReadMetadata(&metadata)) {
    return false;
  }

  net::CookieOptions::SameSiteCookieContext::ContextMetadata schemeful_metadata;
  if (!mojo_context.ReadSchemefulMetadata(&schemeful_metadata)) {
    return false;
  }

  *context = net::CookieOptions::SameSiteCookieContext(
      context_type, schemeful_context, metadata, schemeful_metadata);
  return true;
}

bool StructTraits<network::mojom::CookieOptionsDataView, net::CookieOptions>::
    Read(network::mojom::CookieOptionsDataView mojo_options,
         net::CookieOptions* cookie_options) {
  if (mojo_options.exclude_httponly()) {
    cookie_options->set_exclude_httponly();
  } else {
    cookie_options->set_include_httponly();
  }

  net::CookieOptions::SameSiteCookieContext same_site_cookie_context;
  if (!mojo_options.ReadSameSiteCookieContext(&same_site_cookie_context)) {
    return false;
  }
  cookie_options->set_same_site_cookie_context(same_site_cookie_context);

  if (mojo_options.update_access_time()) {
    cookie_options->set_update_access_time();
  } else {
    cookie_options->set_do_not_update_access_time();
  }

  if (mojo_options.return_excluded_cookies()) {
    cookie_options->set_return_excluded_cookies();
  } else {
    cookie_options->unset_return_excluded_cookies();
  }

  return true;
}

net::CookiePartitionKey::AncestorChainBit
EnumTraits<network::mojom::AncestorChainBit,
           net::CookiePartitionKey::AncestorChainBit>::
    FromMojom(network::mojom::AncestorChainBit input) {
  switch (input) {
    case network::mojom::AncestorChainBit::kSameSite:
      return net::CookiePartitionKey::AncestorChainBit::kSameSite;
    case network::mojom::AncestorChainBit::kCrossSite:
      return net::CookiePartitionKey::AncestorChainBit::kCrossSite;
  }
  NOTREACHED();
}

// static
network::mojom::AncestorChainBit
EnumTraits<network::mojom::AncestorChainBit,
           net::CookiePartitionKey::AncestorChainBit>::
    ToMojom(net::CookiePartitionKey::AncestorChainBit input) {
  switch (input) {
    case net::CookiePartitionKey::AncestorChainBit::kSameSite:
      return network::mojom::AncestorChainBit::kSameSite;
    case net::CookiePartitionKey::AncestorChainBit::kCrossSite:
      return network::mojom::AncestorChainBit::kCrossSite;
  }
  NOTREACHED();
}

bool StructTraits<network::mojom::CookiePartitionKeyDataView,
                  net::CookiePartitionKey>::
    Read(network::mojom::CookiePartitionKeyDataView partition_key,
         net::CookiePartitionKey* out) {
  net::CookiePartitionKey::AncestorChainBit ancestor_chain_bit;
  if (!partition_key.ReadAncestorChainBit(&ancestor_chain_bit)) {
    return false;
  }

  net::SchemefulSite site;
  if (!partition_key.ReadSite(&site)) {
    return false;
  }

  std::optional<base::UnguessableToken> nonce;
  if (!partition_key.ReadNonce(&nonce)) {
    return false;
  }

  *out = net::CookiePartitionKey::FromWire(site, ancestor_chain_bit, nonce);
  return true;
}

const std::vector<net::CookiePartitionKey>
StructTraits<network::mojom::CookiePartitionKeyCollectionDataView,
             net::CookiePartitionKeyCollection>::
    keys(const net::CookiePartitionKeyCollection& key_collection) {
  std::vector<net::CookiePartitionKey> result;
  if (key_collection.ContainsAllKeys() || key_collection.IsEmpty()) {
    return result;
  }
  result.insert(result.begin(), key_collection.PartitionKeys().begin(),
                key_collection.PartitionKeys().end());
  return result;
}

bool StructTraits<network::mojom::CookiePartitionKeyCollectionDataView,
                  net::CookiePartitionKeyCollection>::
    Read(network::mojom::CookiePartitionKeyCollectionDataView
             key_collection_data_view,
         net::CookiePartitionKeyCollection* out) {
  if (key_collection_data_view.contains_all_partitions()) {
    *out = net::CookiePartitionKeyCollection::ContainsAll();
    return true;
  }
  std::vector<net::CookiePartitionKey> keys;
  if (!key_collection_data_view.ReadKeys(&keys)) {
    return false;
  }
  *out = net::CookiePartitionKeyCollection(keys);
  return true;
}

bool StructTraits<
    network::mojom::CanonicalCookieDataView,
    net::CanonicalCookie>::Read(network::mojom::CanonicalCookieDataView cookie,
                                net::CanonicalCookie* out) {
  std::string name;
  if (!cookie.ReadName(&name)) {
    return false;
  }

  std::string value;
  if (!cookie.ReadValue(&value)) {
    return false;
  }

  std::string domain;
  if (!cookie.ReadDomain(&domain)) {
    return false;
  }

  std::string path;
  if (!cookie.ReadPath(&path)) {
    return false;
  }

  base::Time creation_time;
  base::Time expiry_time;
  base::Time last_access_time;
  base::Time last_update_time;
  if (!cookie.ReadCreation(&creation_time)) {
    return false;
  }

  if (!cookie.ReadExpiry(&expiry_time)) {
    return false;
  }

  if (!cookie.ReadLastAccess(&last_access_time)) {
    return false;
  }

  if (!cookie.ReadLastUpdate(&last_update_time)) {
    return false;
  }

  net::CookieSameSite site_restrictions;
  if (!cookie.ReadSiteRestrictions(&site_restrictions)) {
    return false;
  }

  net::CookiePriority priority;
  if (!cookie.ReadPriority(&priority)) {
    return false;
  }

  std::optional<net::CookiePartitionKey> partition_key;
  if (!cookie.ReadPartitionKey(&partition_key)) {
    return false;
  }

  net::CookieSourceScheme source_scheme;
  if (!cookie.ReadSourceScheme(&source_scheme)) {
    return false;
  }

  net::CookieSourceType source_type;
  if (!cookie.ReadSourceType(&source_type)) {
    return false;
  }

  auto cc = net::CanonicalCookie::FromStorage(
      std::move(name), std::move(value), std::move(domain), std::move(path),
      std::move(creation_time), std::move(expiry_time),
      std::move(last_access_time), std::move(last_update_time), cookie.secure(),
      cookie.httponly(), site_restrictions, priority, partition_key,
      source_scheme, cookie.source_port(), source_type,
      net::CanonicalCookieFromStorageCallSite::kCookieManagerMojomTraits);
  if (!cc) {
    return false;
  }
  *out = *cc;
  return true;
}

bool StructTraits<network::mojom::CookieAndLineWithAccessResultDataView,
                  net::CookieAndLineWithAccessResult>::
    Read(network::mojom::CookieAndLineWithAccessResultDataView c,
         net::CookieAndLineWithAccessResult* out) {
  std::optional<net::CanonicalCookie> cookie;
  std::string cookie_string;
  net::CookieAccessResult access_result;
  if (!c.ReadCookie(&cookie)) {
    return false;
  }
  if (!c.ReadCookieString(&cookie_string)) {
    return false;
  }
  if (!c.ReadAccessResult(&access_result)) {
    return false;
  }

  *out = {cookie, cookie_string, access_result};

  return true;
}

bool StructTraits<
    network::mojom::CookieAccessResultDataView,
    net::CookieAccessResult>::Read(network::mojom::CookieAccessResultDataView c,
                                   net::CookieAccessResult* out) {
  net::CookieEffectiveSameSite effective_same_site;
  net::CookieInclusionStatus status;
  net::CookieAccessSemantics access_semantics;
  net::CookieScopeSemantics scope_semantics;

  if (!c.ReadEffectiveSameSite(&effective_same_site)) {
    return false;
  }
  if (!c.ReadStatus(&status)) {
    return false;
  }
  if (!c.ReadAccessSemantics(&access_semantics)) {
    return false;
  }
  if (!c.ReadScopeSemantics(&scope_semantics)) {
    return false;
  }

  *out = {effective_same_site, status, access_semantics, scope_semantics,
          c.is_allowed_to_access_secure_cookies()};

  return true;
}

bool StructTraits<network::mojom::CookieWithAccessResultDataView,
                  net::CookieWithAccessResult>::
    Read(network::mojom::CookieWithAccessResultDataView c,
         net::CookieWithAccessResult* out) {
  net::CanonicalCookie cookie;
  net::CookieAccessResult access_result;
  if (!c.ReadCookie(&cookie)) {
    return false;
  }
  if (!c.ReadAccessResult(&access_result)) {
    return false;
  }

  *out = {cookie, access_result};

  return true;
}

bool StructTraits<
    network::mojom::CookieChangeInfoDataView,
    net::CookieChangeInfo>::Read(network::mojom::CookieChangeInfoDataView info,
                                 net::CookieChangeInfo* out) {
  net::CanonicalCookie cookie;
  net::CookieAccessResult access_result;
  net::CookieChangeCause cause = net::CookieChangeCause::EXPLICIT;
  if (!info.ReadCookie(&cookie)) {
    return false;
  }
  if (!info.ReadAccessResult(&access_result)) {
    return false;
  }
  if (!info.ReadCause(&cause)) {
    return false;
  }

  *out = net::CookieChangeInfo(cookie, access_result, cause);
  return true;
}

}  // namespace mojo

/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Google, Inc. ("Google") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

#include <memory>

#include "base/strings/pattern.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"
#include "url/gurl.h"

namespace blink {

static Mutex& GetMutex() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, mutex, ());
  return mutex;
}

static network::cors::OriginAccessList& GetOriginAccessList() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(network::cors::OriginAccessList,
                                  origin_access_list, ());
  return origin_access_list;
}

using OriginSet = HashSet<String>;

static OriginSet& TrustworthyOriginSafelist() {
  DEFINE_STATIC_LOCAL(OriginSet, safelist, ());
  return safelist;
}

network::mojom::ReferrerPolicy ReferrerPolicyResolveDefault(
    network::mojom::ReferrerPolicy referrer_policy) {
  if (referrer_policy == network::mojom::ReferrerPolicy::kDefault) {
    if (RuntimeEnabledFeatures::ReducedReferrerGranularityEnabled()) {
      return network::mojom::ReferrerPolicy::
          kNoReferrerWhenDowngradeOriginWhenCrossOrigin;
    } else {
      return network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade;
    }
  }

  return referrer_policy;
}

void SecurityPolicy::Init() {
  TrustworthyOriginSafelist();
}

bool SecurityPolicy::ShouldHideReferrer(const KURL& url, const KURL& referrer) {
  bool referrer_is_secure_url = referrer.ProtocolIs("https");
  bool scheme_is_allowed =
      SchemeRegistry::ShouldTreatURLSchemeAsAllowedForReferrer(
          referrer.Protocol());

  if (!scheme_is_allowed)
    return true;

  if (!referrer_is_secure_url)
    return false;

  bool url_is_secure_url = url.ProtocolIs("https");

  return !url_is_secure_url;
}

// When making changes to this method that affect the return Referrer, also
// update net::URLRequestJob::ComputeReferrerForPolicy accordingly.
Referrer SecurityPolicy::GenerateReferrer(
    network::mojom::ReferrerPolicy referrer_policy,
    scoped_refptr<const SecurityOrigin> origin,
    const KURL& url,
    const String& referrer) {
  network::mojom::ReferrerPolicy referrer_policy_no_default =
      ReferrerPolicyResolveDefault(referrer_policy);
  if (referrer == Referrer::NoReferrer())
    return Referrer(Referrer::NoReferrer(), referrer_policy_no_default);
  DCHECK(!referrer.IsEmpty());

  KURL referrer_url = KURL(NullURL(), referrer);
  String scheme = referrer_url.Protocol();
  if (!SchemeRegistry::ShouldTreatURLSchemeAsAllowedForReferrer(scheme))
    return Referrer(Referrer::NoReferrer(), referrer_policy_no_default);

  if (SecurityOrigin::ShouldUseInnerURL(url))
    return Referrer(Referrer::NoReferrer(), referrer_policy_no_default);

  switch (referrer_policy_no_default) {
    case network::mojom::ReferrerPolicy::kNever:
      return Referrer(Referrer::NoReferrer(), referrer_policy_no_default);
    case network::mojom::ReferrerPolicy::kAlways:
      return Referrer(referrer, referrer_policy_no_default);
    case network::mojom::ReferrerPolicy::kOrigin: {
      String referrer_origin_string =
          SecurityOrigin::Create(referrer_url)->ToString();
      // A security origin is not a canonical URL as it lacks a path. Add /
      // to turn it into a canonical URL we can use as referrer.
      return Referrer(referrer_origin_string + "/", referrer_policy_no_default);
    }
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin: {
      String referrer_origin_string =
          SecurityOrigin::Create(referrer_url)->ToString();
      scoped_refptr<const SecurityOrigin> url_origin =
          SecurityOrigin::Create(url);
      if (!url_origin->IsSameSchemeHostPort(origin.get())) {
        return Referrer(referrer_origin_string + "/",
                        referrer_policy_no_default);
      }
      break;
    }
    case network::mojom::ReferrerPolicy::kSameOrigin: {
      scoped_refptr<const SecurityOrigin> url_origin =
          SecurityOrigin::Create(url);
      if (!url_origin->IsSameSchemeHostPort(origin.get())) {
        return Referrer(Referrer::NoReferrer(), referrer_policy_no_default);
      }
      return Referrer(referrer, referrer_policy_no_default);
    }
    case network::mojom::ReferrerPolicy::kStrictOrigin: {
      String referrer_origin_string =
          SecurityOrigin::Create(referrer_url)->ToString();
      return Referrer(ShouldHideReferrer(url, referrer_url)
                          ? Referrer::NoReferrer()
                          : referrer_origin_string + "/",
                      referrer_policy_no_default);
    }
    case network::mojom::ReferrerPolicy::
        kNoReferrerWhenDowngradeOriginWhenCrossOrigin: {
      String referrer_origin_string =
          SecurityOrigin::Create(referrer_url)->ToString();
      scoped_refptr<const SecurityOrigin> url_origin =
          SecurityOrigin::Create(url);
      if (!url_origin->IsSameSchemeHostPort(origin.get())) {
        return Referrer(ShouldHideReferrer(url, referrer_url)
                            ? Referrer::NoReferrer()
                            : referrer_origin_string + "/",
                        referrer_policy_no_default);
      }
      break;
    }
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
      break;
    case network::mojom::ReferrerPolicy::kDefault:
      NOTREACHED();
      break;
  }

  return Referrer(
      ShouldHideReferrer(url, referrer_url) ? Referrer::NoReferrer() : referrer,
      referrer_policy_no_default);
}

void SecurityPolicy::AddOriginToTrustworthySafelist(
    const String& origin_or_pattern) {
#if DCHECK_IS_ON()
  // Must be called before we start other threads.
  DCHECK(WTF::IsBeforeThreadCreated());
#endif
  // Origins and hostname patterns must be canonicalized (including
  // canonicalization to 8-bit strings) before being inserted into
  // TrustworthyOriginSafelist().
  CHECK(origin_or_pattern.Is8Bit());
  TrustworthyOriginSafelist().insert(origin_or_pattern);
}

bool SecurityPolicy::IsOriginTrustworthySafelisted(
    const SecurityOrigin& origin) {
  // Early return if |origin| cannot possibly be matched.
  if (origin.IsOpaque() || TrustworthyOriginSafelist().IsEmpty())
    return false;

  if (TrustworthyOriginSafelist().Contains(origin.ToRawString()))
    return true;

  // KURL and SecurityOrigin hosts should be canonicalized to 8-bit strings.
  CHECK(origin.Host().Is8Bit());
  StringUTF8Adaptor host_adaptor(origin.Host());
  for (const auto& origin_or_pattern : TrustworthyOriginSafelist()) {
    StringUTF8Adaptor origin_or_pattern_adaptor(origin_or_pattern);
    if (base::MatchPattern(host_adaptor.AsStringPiece(),
                           origin_or_pattern_adaptor.AsStringPiece())) {
      return true;
    }
  }

  return false;
}

bool SecurityPolicy::IsUrlTrustworthySafelisted(const KURL& url) {
  // Early return to avoid initializing the SecurityOrigin.
  if (TrustworthyOriginSafelist().IsEmpty())
    return false;
  return IsOriginTrustworthySafelisted(*SecurityOrigin::Create(url).get());
}

bool SecurityPolicy::IsOriginAccessAllowed(
    const SecurityOrigin* active_origin,
    const SecurityOrigin* target_origin) {
  MutexLocker lock(GetMutex());
  return GetOriginAccessList().CheckAccessState(
             active_origin->ToUrlOrigin(),
             target_origin->ToUrlOrigin().GetURL()) ==
         network::cors::OriginAccessList::AccessState::kAllowed;
}

bool SecurityPolicy::IsOriginAccessToURLAllowed(
    const SecurityOrigin* active_origin,
    const KURL& url) {
  MutexLocker lock(GetMutex());
  return GetOriginAccessList().CheckAccessState(active_origin->ToUrlOrigin(),
                                                url) ==
         network::cors::OriginAccessList::AccessState::kAllowed;
}

void SecurityPolicy::AddOriginAccessAllowListEntry(
    const SecurityOrigin& source_origin,
    const String& destination_protocol,
    const String& destination_domain,
    const uint16_t port,
    const network::mojom::CorsDomainMatchMode domain_match_mode,
    const network::mojom::CorsPortMatchMode port_match_mode,
    const network::mojom::CorsOriginAccessMatchPriority priority) {
  MutexLocker lock(GetMutex());
  GetOriginAccessList().AddAllowListEntryForOrigin(
      source_origin.ToUrlOrigin(), destination_protocol.Utf8(),
      destination_domain.Utf8(), port, domain_match_mode, port_match_mode,
      priority);
}

void SecurityPolicy::AddOriginAccessBlockListEntry(
    const SecurityOrigin& source_origin,
    const String& destination_protocol,
    const String& destination_domain,
    const uint16_t port,
    const network::mojom::CorsDomainMatchMode domain_match_mode,
    const network::mojom::CorsPortMatchMode port_match_mode,
    const network::mojom::CorsOriginAccessMatchPriority priority) {
  MutexLocker lock(GetMutex());
  GetOriginAccessList().AddBlockListEntryForOrigin(
      source_origin.ToUrlOrigin(), destination_protocol.Utf8(),
      destination_domain.Utf8(), port, domain_match_mode, port_match_mode,
      priority);
}

void SecurityPolicy::ClearOriginAccessListForOrigin(
    const SecurityOrigin& source_origin) {
  MutexLocker lock(GetMutex());
  GetOriginAccessList().ClearForOrigin(source_origin.ToUrlOrigin());
}

void SecurityPolicy::ClearOriginAccessList() {
  MutexLocker lock(GetMutex());
  GetOriginAccessList().Clear();
}

bool SecurityPolicy::ReferrerPolicyFromString(
    const String& policy,
    ReferrerPolicyLegacyKeywordsSupport legacy_keywords_support,
    network::mojom::ReferrerPolicy* result) {
  DCHECK(!policy.IsNull());
  bool support_legacy_keywords =
      (legacy_keywords_support == kSupportReferrerPolicyLegacyKeywords);

  if (EqualIgnoringASCIICase(policy, "no-referrer") ||
      (support_legacy_keywords && (EqualIgnoringASCIICase(policy, "never") ||
                                   EqualIgnoringASCIICase(policy, "none")))) {
    *result = network::mojom::ReferrerPolicy::kNever;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "unsafe-url") ||
      (support_legacy_keywords && EqualIgnoringASCIICase(policy, "always"))) {
    *result = network::mojom::ReferrerPolicy::kAlways;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "origin")) {
    *result = network::mojom::ReferrerPolicy::kOrigin;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "origin-when-cross-origin") ||
      (support_legacy_keywords &&
       EqualIgnoringASCIICase(policy, "origin-when-crossorigin"))) {
    *result = network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "same-origin")) {
    *result = network::mojom::ReferrerPolicy::kSameOrigin;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "strict-origin")) {
    *result = network::mojom::ReferrerPolicy::kStrictOrigin;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "strict-origin-when-cross-origin")) {
    *result = network::mojom::ReferrerPolicy::
        kNoReferrerWhenDowngradeOriginWhenCrossOrigin;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "no-referrer-when-downgrade") ||
      (support_legacy_keywords && EqualIgnoringASCIICase(policy, "default"))) {
    *result = network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade;
    return true;
  }
  return false;
}

namespace {

template <typename CharType>
inline bool IsASCIIAlphaOrHyphen(CharType c) {
  return IsASCIIAlpha(c) || c == '-';
}

}  // namespace

bool SecurityPolicy::ReferrerPolicyFromHeaderValue(
    const String& header_value,
    ReferrerPolicyLegacyKeywordsSupport legacy_keywords_support,
    network::mojom::ReferrerPolicy* result) {
  network::mojom::ReferrerPolicy referrer_policy =
      network::mojom::ReferrerPolicy::kDefault;

  Vector<String> tokens;
  header_value.Split(',', true, tokens);
  for (const auto& token : tokens) {
    network::mojom::ReferrerPolicy current_result;
    auto stripped_token = token.StripWhiteSpace();
    if (SecurityPolicy::ReferrerPolicyFromString(token.StripWhiteSpace(),
                                                 legacy_keywords_support,
                                                 &current_result)) {
      referrer_policy = current_result;
    } else {
      Vector<UChar> characters;
      stripped_token.AppendTo(characters);
      const UChar* position = characters.data();
      UChar* end = characters.data() + characters.size();
      SkipWhile<UChar, IsASCIIAlphaOrHyphen>(position, end);
      if (position != end)
        return false;
    }
  }

  if (referrer_policy == network::mojom::ReferrerPolicy::kDefault)
    return false;

  *result = referrer_policy;
  return true;
}

}  // namespace blink

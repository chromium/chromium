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
#include "services/network/public/mojom/cors_origin_pattern.mojom-shared.h"
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

static OriginSet& TrustworthyOriginSet() {
  DEFINE_STATIC_LOCAL(OriginSet, trustworthy_origin_set, ());
  return trustworthy_origin_set;
}

void SecurityPolicy::Init() {
  TrustworthyOriginSet();
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

Referrer SecurityPolicy::GenerateReferrer(ReferrerPolicy referrer_policy,
                                          const KURL& url,
                                          const String& referrer) {
  ReferrerPolicy referrer_policy_no_default = referrer_policy;
  if (referrer_policy_no_default == kReferrerPolicyDefault) {
    if (RuntimeEnabledFeatures::ReducedReferrerGranularityEnabled()) {
      referrer_policy_no_default = kReferrerPolicyStrictOriginWhenCrossOrigin;
    } else {
      referrer_policy_no_default = kReferrerPolicyNoReferrerWhenDowngrade;
    }
  }
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
    case kReferrerPolicyNever:
      return Referrer(Referrer::NoReferrer(), referrer_policy_no_default);
    case kReferrerPolicyAlways:
      return Referrer(referrer, referrer_policy_no_default);
    case kReferrerPolicyOrigin: {
      String origin = SecurityOrigin::Create(referrer_url)->ToString();
      // A security origin is not a canonical URL as it lacks a path. Add /
      // to turn it into a canonical URL we can use as referrer.
      return Referrer(origin + "/", referrer_policy_no_default);
    }
    case kReferrerPolicyOriginWhenCrossOrigin: {
      scoped_refptr<const SecurityOrigin> referrer_origin =
          SecurityOrigin::Create(referrer_url);
      scoped_refptr<const SecurityOrigin> url_origin =
          SecurityOrigin::Create(url);
      if (!url_origin->IsSameSchemeHostPort(referrer_origin.get())) {
        String origin = referrer_origin->ToString();
        return Referrer(origin + "/", referrer_policy_no_default);
      }
      break;
    }
    case kReferrerPolicySameOrigin: {
      scoped_refptr<const SecurityOrigin> referrer_origin =
          SecurityOrigin::Create(referrer_url);
      scoped_refptr<const SecurityOrigin> url_origin =
          SecurityOrigin::Create(url);
      if (!url_origin->IsSameSchemeHostPort(referrer_origin.get())) {
        return Referrer(Referrer::NoReferrer(), referrer_policy_no_default);
      }
      return Referrer(referrer, referrer_policy_no_default);
    }
    case kReferrerPolicyStrictOrigin: {
      String origin = SecurityOrigin::Create(referrer_url)->ToString();
      return Referrer(ShouldHideReferrer(url, referrer_url)
                          ? Referrer::NoReferrer()
                          : origin + "/",
                      referrer_policy_no_default);
    }
    case kReferrerPolicyStrictOriginWhenCrossOrigin: {
      scoped_refptr<const SecurityOrigin> referrer_origin =
          SecurityOrigin::Create(referrer_url);
      scoped_refptr<const SecurityOrigin> url_origin =
          SecurityOrigin::Create(url);
      if (!url_origin->IsSameSchemeHostPort(referrer_origin.get())) {
        String origin = referrer_origin->ToString();
        return Referrer(ShouldHideReferrer(url, referrer_url)
                            ? Referrer::NoReferrer()
                            : origin + "/",
                        referrer_policy_no_default);
      }
      break;
    }
    case kReferrerPolicyNoReferrerWhenDowngrade:
      break;
    case kReferrerPolicyDefault:
      NOTREACHED();
      break;
  }

  return Referrer(
      ShouldHideReferrer(url, referrer_url) ? Referrer::NoReferrer() : referrer,
      referrer_policy_no_default);
}

void SecurityPolicy::AddOriginTrustworthyWhiteList(const String& origin) {
#if DCHECK_IS_ON()
  // Must be called before we start other threads.
  DCHECK(WTF::IsBeforeThreadCreated());
#endif
  TrustworthyOriginSet().insert(origin);
}

bool SecurityPolicy::IsOriginWhiteListedTrustworthy(
    const SecurityOrigin& origin) {
  // Early return if there are no whitelisted origins to avoid unnecessary
  // allocations, copies, and frees.
  if (origin.IsOpaque() || TrustworthyOriginSet().IsEmpty())
    return false;
  if (TrustworthyOriginSet().Contains(origin.ToRawString()))
    return true;

  // KURL and SecurityOrigin hosts should be canonicalized to 8-bit strings.
  CHECK(origin.Host().Is8Bit());
  StringUTF8Adaptor host_adaptor(origin.Host());
  for (const auto& origin_or_pattern : TrustworthyOriginSet()) {
    // Origins and hostname patterns are expected to be canonicalized (including
    // canonicalization to 8-bit strings) before being inserted into the
    // TrustworthyOriginSet().
    CHECK(origin_or_pattern.Is8Bit());
    StringUTF8Adaptor origin_or_pattern_adaptor(origin_or_pattern);
    if (base::MatchPattern(host_adaptor.AsStringPiece(),
                           origin_or_pattern_adaptor.AsStringPiece())) {
      return true;
    }
  }

  return false;
}

bool SecurityPolicy::IsUrlWhiteListedTrustworthy(const KURL& url) {
  // Early return to avoid initializing the SecurityOrigin.
  if (TrustworthyOriginSet().IsEmpty())
    return false;
  return IsOriginWhiteListedTrustworthy(*SecurityOrigin::Create(url).get());
}

bool SecurityPolicy::IsOriginAccessAllowed(
    const SecurityOrigin* active_origin,
    const SecurityOrigin* target_origin) {
  MutexLocker lock(GetMutex());
  return GetOriginAccessList().IsAllowed(active_origin->ToUrlOrigin(),
                                         target_origin->ToUrlOrigin().GetURL());
}

bool SecurityPolicy::IsOriginAccessToURLAllowed(
    const SecurityOrigin* active_origin,
    const KURL& url) {
  MutexLocker lock(GetMutex());
  return GetOriginAccessList().IsAllowed(active_origin->ToUrlOrigin(), url);
}

void SecurityPolicy::AddOriginAccessAllowListEntry(
    const SecurityOrigin& source_origin,
    const String& destination_protocol,
    const String& destination_domain,
    bool allow_destination_subdomains,
    const network::mojom::CORSOriginAccessMatchPriority priority) {
  MutexLocker lock(GetMutex());
  GetOriginAccessList().AddAllowListEntryForOrigin(
      source_origin.ToUrlOrigin(), WebString(destination_protocol).Utf8(),
      WebString(destination_domain).Utf8(), allow_destination_subdomains,
      priority);
}

void SecurityPolicy::ClearOriginAccessAllowListForOrigin(
    const SecurityOrigin& source_origin) {
  MutexLocker lock(GetMutex());
  GetOriginAccessList().SetAllowListForOrigin(
      source_origin.ToUrlOrigin(),
      std::vector<network::mojom::CorsOriginPatternPtr>());
}

void SecurityPolicy::ClearOriginAccessBlockListForOrigin(
    const SecurityOrigin& source_origin) {
  MutexLocker lock(GetMutex());
  GetOriginAccessList().SetBlockListForOrigin(
      source_origin.ToUrlOrigin(),
      std::vector<network::mojom::CorsOriginPatternPtr>());
}

void SecurityPolicy::ClearOriginAccessAllowList() {
  MutexLocker lock(GetMutex());
  GetOriginAccessList().ClearAllowList();
}

void SecurityPolicy::AddOriginAccessBlockListEntry(
    const SecurityOrigin& source_origin,
    const String& destination_protocol,
    const String& destination_domain,
    bool allow_destination_subdomains,
    const network::mojom::CORSOriginAccessMatchPriority priority) {
  MutexLocker lock(GetMutex());
  GetOriginAccessList().AddBlockListEntryForOrigin(
      source_origin.ToUrlOrigin(), WebString(destination_protocol).Utf8(),
      WebString(destination_domain).Utf8(), allow_destination_subdomains,
      priority);
}

void SecurityPolicy::ClearOriginAccessBlockList() {
  MutexLocker lock(GetMutex());
  GetOriginAccessList().ClearBlockList();
}

bool SecurityPolicy::ReferrerPolicyFromString(
    const String& policy,
    ReferrerPolicyLegacyKeywordsSupport legacy_keywords_support,
    ReferrerPolicy* result) {
  DCHECK(!policy.IsNull());
  bool support_legacy_keywords =
      (legacy_keywords_support == kSupportReferrerPolicyLegacyKeywords);

  if (EqualIgnoringASCIICase(policy, "no-referrer") ||
      (support_legacy_keywords && (EqualIgnoringASCIICase(policy, "never") ||
                                   EqualIgnoringASCIICase(policy, "none")))) {
    *result = kReferrerPolicyNever;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "unsafe-url") ||
      (support_legacy_keywords && EqualIgnoringASCIICase(policy, "always"))) {
    *result = kReferrerPolicyAlways;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "origin")) {
    *result = kReferrerPolicyOrigin;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "origin-when-cross-origin") ||
      (support_legacy_keywords &&
       EqualIgnoringASCIICase(policy, "origin-when-crossorigin"))) {
    *result = kReferrerPolicyOriginWhenCrossOrigin;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "same-origin")) {
    *result = kReferrerPolicySameOrigin;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "strict-origin")) {
    *result = kReferrerPolicyStrictOrigin;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "strict-origin-when-cross-origin")) {
    *result = kReferrerPolicyStrictOriginWhenCrossOrigin;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "no-referrer-when-downgrade") ||
      (support_legacy_keywords && EqualIgnoringASCIICase(policy, "default"))) {
    *result = kReferrerPolicyNoReferrerWhenDowngrade;
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
    ReferrerPolicy* result) {
  ReferrerPolicy referrer_policy = kReferrerPolicyDefault;

  Vector<String> tokens;
  header_value.Split(',', true, tokens);
  for (const auto& token : tokens) {
    ReferrerPolicy current_result;
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

  if (referrer_policy == kReferrerPolicyDefault)
    return false;

  *result = referrer_policy;
  return true;
}

STATIC_ASSERT_ENUM(network::mojom::ReferrerPolicy::kAlways,
                   kReferrerPolicyAlways);
STATIC_ASSERT_ENUM(network::mojom::ReferrerPolicy::kDefault,
                   kReferrerPolicyDefault);
STATIC_ASSERT_ENUM(network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade,
                   kReferrerPolicyNoReferrerWhenDowngrade);
STATIC_ASSERT_ENUM(network::mojom::ReferrerPolicy::kNever,
                   kReferrerPolicyNever);
STATIC_ASSERT_ENUM(network::mojom::ReferrerPolicy::kOrigin,
                   kReferrerPolicyOrigin);
STATIC_ASSERT_ENUM(network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin,
                   kReferrerPolicyOriginWhenCrossOrigin);
STATIC_ASSERT_ENUM(network::mojom::ReferrerPolicy::kSameOrigin,
                   kReferrerPolicySameOrigin);
STATIC_ASSERT_ENUM(network::mojom::ReferrerPolicy::kStrictOrigin,
                   kReferrerPolicyStrictOrigin);
STATIC_ASSERT_ENUM(network::mojom::ReferrerPolicy::
                       kNoReferrerWhenDowngradeOriginWhenCrossOrigin,
                   kReferrerPolicyStrictOriginWhenCrossOrigin);

}  // namespace blink

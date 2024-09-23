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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

#include <memory>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"
#include "url/gurl.h"

namespace blink {

static base::Lock& GetLock() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(base::Lock, lock, ());
  return lock;
}

static network::cors::OriginAccessList& GetOriginAccessList() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(network::cors::OriginAccessList,
                                  origin_access_list, ());
  return origin_access_list;
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

Referrer SecurityPolicy::GenerateReferrer(
    network::mojom::ReferrerPolicy referrer_policy,
    const KURL& url,
    const String& referrer) {
  network::mojom::ReferrerPolicy referrer_policy_no_default =
      ReferrerUtils::MojoReferrerPolicyResolveDefault(referrer_policy);
  // Empty (a possible input) and default (the value of `Referrer::NoReferrer`)
  // strings are not equivalent.
  if (referrer == Referrer::NoReferrer() || referrer.empty())
    return Referrer(Referrer::NoReferrer(), referrer_policy_no_default);

  KURL referrer_url = KURL(NullURL(), referrer).UrlStrippedForUseAsReferrer();

  if (!referrer_url.IsValid())
    return Referrer(Referrer::NoReferrer(), referrer_policy_no_default);

  // 5. Let referrerOrigin be the result of stripping referrerSource for use as
  // a referrer, with the origin-only flag set to true.
  KURL referrer_origin = referrer_url;
  referrer_origin.SetPath(String());
  referrer_origin.SetQuery(String());

  // 6. If the result of serializing referrerURL is a string whose length is
  // greater than 4096, set referrerURL to referrerOrigin.
  if (referrer_url.GetString().length() > 4096)
    referrer_url = referrer_origin;

  switch (referrer_policy_no_default) {
    case network::mojom::ReferrerPolicy::kNever:
      return Referrer(Referrer::NoReferrer(), referrer_policy_no_default);
    case network::mojom::ReferrerPolicy::kAlways:
      return Referrer(referrer_url, referrer_policy_no_default);
    case network::mojom::ReferrerPolicy::kOrigin: {
      return Referrer(referrer_origin, referrer_policy_no_default);
    }
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin: {
      if (!SecurityOrigin::AreSameOrigin(referrer_url, url)) {
        return Referrer(referrer_origin, referrer_policy_no_default);
      }
      break;
    }
    case network::mojom::ReferrerPolicy::kSameOrigin: {
      if (!SecurityOrigin::AreSameOrigin(referrer_url, url)) {
        return Referrer(Referrer::NoReferrer(), referrer_policy_no_default);
      }
      return Referrer(referrer_url, referrer_policy_no_default);
    }
    case network::mojom::ReferrerPolicy::kStrictOrigin: {
      return Referrer(ShouldHideReferrer(url, referrer_url)
                          ? Referrer::NoReferrer()
                          : referrer_origin,
                      referrer_policy_no_default);
    }
    case network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin: {
      if (!SecurityOrigin::AreSameOrigin(referrer_url, url)) {
        return Referrer(ShouldHideReferrer(url, referrer_url)
                            ? Referrer::NoReferrer()
                            : referrer_origin,
                        referrer_policy_no_default);
      }
      break;
    }
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
      break;
    case network::mojom::ReferrerPolicy::kDefault:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  return Referrer(ShouldHideReferrer(url, referrer_url) ? Referrer::NoReferrer()
                                                        : referrer_url,
                  referrer_policy_no_default);
}

bool SecurityPolicy::IsOriginAccessAllowed(
    const SecurityOrigin* active_origin,
    const SecurityOrigin* target_origin) {
  base::AutoLock locker(GetLock());
  return GetOriginAccessList().CheckAccessState(
             active_origin->ToUrlOrigin(),
             target_origin->ToUrlOrigin().GetURL()) ==
         network::cors::OriginAccessList::AccessState::kAllowed;
}

bool SecurityPolicy::IsOriginAccessToURLAllowed(
    const SecurityOrigin* active_origin,
    const KURL& url) {
  base::AutoLock locker(GetLock());
  return GetOriginAccessList().CheckAccessState(active_origin->ToUrlOrigin(),
                                                GURL(url)) ==
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
  base::AutoLock locker(GetLock());
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
  base::AutoLock locker(GetLock());
  GetOriginAccessList().AddBlockListEntryForOrigin(
      source_origin.ToUrlOrigin(), destination_protocol.Utf8(),
      destination_domain.Utf8(), port, domain_match_mode, port_match_mode,
      priority);
}

void SecurityPolicy::ClearOriginAccessListForOrigin(
    const SecurityOrigin& source_origin) {
  base::AutoLock locker(GetLock());
  GetOriginAccessList().ClearForOrigin(source_origin.ToUrlOrigin());
}

void SecurityPolicy::ClearOriginAccessList() {
  base::AutoLock locker(GetLock());
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
    *result = network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "no-referrer-when-downgrade")) {
    *result = network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade;
    return true;
  }
  if (support_legacy_keywords && EqualIgnoringASCIICase(policy, "default")) {
    *result = ReferrerUtils::NetToMojoReferrerPolicy(
        ReferrerUtils::GetDefaultNetReferrerPolicy());
    return true;
  }
  return false;
}

String SecurityPolicy::ReferrerPolicyAsString(
    network::mojom::ReferrerPolicy policy) {
  switch (policy) {
    case network::mojom::ReferrerPolicy::kAlways:
      return "unsafe-url";
    case network::mojom::ReferrerPolicy::kDefault:
      return "";
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
      return "no-referrer-when-downgrade";
    case network::mojom::ReferrerPolicy::kNever:
      return "no-referrer";
    case network::mojom::ReferrerPolicy::kOrigin:
      return "origin";
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
      return "origin-when-cross-origin";
    case network::mojom::ReferrerPolicy::kSameOrigin:
      return "same-origin";
    case network::mojom::ReferrerPolicy::kStrictOrigin:
      return "strict-origin";
    case network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin:
      return "strict-origin-when-cross-origin";
  }
  NOTREACHED_IN_MIGRATION();
  return String();
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

#if BUILDFLAG(IS_FUCHSIA)
namespace {
std::vector<url::Origin> GetSharedArrayBufferOrigins() {
  std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kSharedArrayBufferAllowedOrigins);
  std::vector<std::string> list =
      SplitString(switch_value, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
                  base::SplitResult::SPLIT_WANT_NONEMPTY);
  std::vector<url::Origin> result;
  for (auto& origin : list) {
    GURL url(origin);
    if (!url.is_valid() || url.scheme() != url::kHttpsScheme) {
      LOG(FATAL) << "Invalid --" << switches::kSharedArrayBufferAllowedOrigins
                 << " specified: " << switch_value;
    }
    result.push_back(url::Origin::Create(url));
  }
  return result;
}
}  // namespace
#endif  // BUILDFLAG(IS_FUCHSIA)

// static
bool SecurityPolicy::IsSharedArrayBufferAlwaysAllowedForOrigin(
    const SecurityOrigin* security_origin) {
#if BUILDFLAG(IS_FUCHSIA)
  static base::NoDestructor<std::vector<url::Origin>> allowed_origins(
      GetSharedArrayBufferOrigins());
  url::Origin origin = security_origin->ToUrlOrigin();
  for (const url::Origin& allowed_origin : *allowed_origins) {
    if (origin.scheme() == allowed_origin.scheme() &&
        origin.DomainIs(allowed_origin.host()) &&
        origin.port() == allowed_origin.port()) {
      return true;
    }
  }
#endif
  return false;
}

}  // namespace blink

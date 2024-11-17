// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_base.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "net/base/features.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_util.h"

namespace net {

namespace {

// Captures Strict -> Lax context downgrade with Strict cookie
bool IsBreakingStrictToLaxDowngrade(
    CookieOptions::SameSiteCookieContext::ContextType context,
    CookieOptions::SameSiteCookieContext::ContextType schemeful_context,
    CookieEffectiveSameSite effective_same_site,
    bool is_cookie_being_set) {
  if (context ==
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT &&
      schemeful_context ==
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX &&
      effective_same_site == CookieEffectiveSameSite::STRICT_MODE) {
    // This downgrade only applies when a SameSite=Strict cookie is being sent.
    // A Strict -> Lax downgrade will not affect a Strict cookie which is being
    // set because it will be set in either context.
    return !is_cookie_being_set;
  }

  return false;
}

// Captures Strict -> Cross-site context downgrade with {Strict, Lax} cookie
// Captures Strict -> Lax Unsafe context downgrade with {Strict, Lax} cookie.
// This is treated as a cross-site downgrade due to the Lax Unsafe context
// behaving like cross-site.
bool IsBreakingStrictToCrossDowngrade(
    CookieOptions::SameSiteCookieContext::ContextType context,
    CookieOptions::SameSiteCookieContext::ContextType schemeful_context,
    CookieEffectiveSameSite effective_same_site) {
  bool breaking_schemeful_context =
      schemeful_context ==
          CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE ||
      schemeful_context == CookieOptions::SameSiteCookieContext::ContextType::
                               SAME_SITE_LAX_METHOD_UNSAFE;

  bool strict_lax_enforcement =
      effective_same_site == CookieEffectiveSameSite::STRICT_MODE ||
      effective_same_site == CookieEffectiveSameSite::LAX_MODE ||
      // Treat LAX_MODE_ALLOW_UNSAFE the same as LAX_MODE for the purposes of
      // our SameSite enforcement check.
      effective_same_site == CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE;

  if (context ==
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT &&
      breaking_schemeful_context && strict_lax_enforcement) {
    return true;
  }

  return false;
}

// Captures Lax -> Cross context downgrade with {Strict, Lax} cookies.
// Ignores Lax Unsafe context.
bool IsBreakingLaxToCrossDowngrade(
    CookieOptions::SameSiteCookieContext::ContextType context,
    CookieOptions::SameSiteCookieContext::ContextType schemeful_context,
    CookieEffectiveSameSite effective_same_site,
    bool is_cookie_being_set) {
  bool lax_enforcement =
      effective_same_site == CookieEffectiveSameSite::LAX_MODE ||
      // Treat LAX_MODE_ALLOW_UNSAFE the same as LAX_MODE for the purposes of
      // our SameSite enforcement check.
      effective_same_site == CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE;

  if (context ==
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX &&
      schemeful_context ==
          CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE) {
    // For SameSite=Strict cookies this downgrade only applies when it is being
    // set. A Lax -> Cross downgrade will not affect a Strict cookie which is
    // being sent because it wouldn't be sent in either context.
    return effective_same_site == CookieEffectiveSameSite::STRICT_MODE
               ? is_cookie_being_set
               : lax_enforcement;
  }

  return false;
}

void ApplySameSiteCookieWarningToStatus(
    CookieSameSite samesite,
    CookieEffectiveSameSite effective_samesite,
    bool is_secure,
    const CookieOptions::SameSiteCookieContext& same_site_context,
    CookieInclusionStatus* status,
    bool is_cookie_being_set) {
  if (samesite == CookieSameSite::UNSPECIFIED &&
      same_site_context.GetContextForCookieInclusion() <
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX) {
    status->AddWarningReason(
        CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT);
  }
  if (effective_samesite == CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE &&
      same_site_context.GetContextForCookieInclusion() ==
          CookieOptions::SameSiteCookieContext::ContextType::
              SAME_SITE_LAX_METHOD_UNSAFE) {
    // This warning is more specific so remove the previous, more general,
    // warning.
    status->RemoveWarningReason(
        CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT);
    status->AddWarningReason(
        CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE);
  }
  if (samesite == CookieSameSite::NO_RESTRICTION && !is_secure) {
    status->AddWarningReason(
        CookieInclusionStatus::WARN_SAMESITE_NONE_INSECURE);
  }

  // Add a warning if the cookie would be accessible in
  // |same_site_context|::context but not in
  // |same_site_context|::schemeful_context.
  if (IsBreakingStrictToLaxDowngrade(same_site_context.context(),
                                     same_site_context.schemeful_context(),
                                     effective_samesite, is_cookie_being_set)) {
    status->AddWarningReason(
        CookieInclusionStatus::WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE);
  } else if (IsBreakingStrictToCrossDowngrade(
                 same_site_context.context(),
                 same_site_context.schemeful_context(), effective_samesite)) {
    // Which warning to apply depends on the SameSite value.
    if (effective_samesite == CookieEffectiveSameSite::STRICT_MODE) {
      status->AddWarningReason(
          CookieInclusionStatus::WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE);
    } else {
      // LAX_MODE or LAX_MODE_ALLOW_UNSAFE.
      status->AddWarningReason(
          CookieInclusionStatus::WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE);
    }

  } else if (IsBreakingLaxToCrossDowngrade(
                 same_site_context.context(),
                 same_site_context.schemeful_context(), effective_samesite,
                 is_cookie_being_set)) {
    // Which warning to apply depends on the SameSite value.
    if (effective_samesite == CookieEffectiveSameSite::STRICT_MODE) {
      status->AddWarningReason(
          CookieInclusionStatus::WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE);
    } else {
      // LAX_MODE or LAX_MODE_ALLOW_UNSAFE.
      // This warning applies to both set/send.
      status->AddWarningReason(
          CookieInclusionStatus::WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE);
    }
  }

  // Apply warning for whether inclusion was changed by considering redirects
  // for the SameSite context calculation. This does not look at the actual
  // inclusion or exclusion, but only at whether the inclusion differs between
  // considering redirects and not.
  using ContextDowngradeType = CookieOptions::SameSiteCookieContext::
      ContextMetadata::ContextDowngradeType;
  const auto& metadata = same_site_context.GetMetadataForCurrentSchemefulMode();
  bool apply_cross_site_redirect_downgrade_warning = false;
  switch (effective_samesite) {
    case CookieEffectiveSameSite::STRICT_MODE:
      // Strict contexts are all normalized to lax for cookie writes, so a
      // strict-to-{lax,cross} downgrade cannot occur for response cookies.
      apply_cross_site_redirect_downgrade_warning =
          is_cookie_being_set ? metadata.cross_site_redirect_downgrade ==
                                    ContextDowngradeType::kLaxToCross
                              : (metadata.cross_site_redirect_downgrade ==
                                     ContextDowngradeType::kStrictToLax ||
                                 metadata.cross_site_redirect_downgrade ==
                                     ContextDowngradeType::kStrictToCross);
      break;
    case CookieEffectiveSameSite::LAX_MODE:
    case CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE:
      // Note that a lax-to-cross downgrade can only happen for response
      // cookies, because a laxly same-site context only happens for a safe
      // top-level cross-site request, which cannot be downgraded due to a
      // cross-site redirect to a non-top-level or unsafe cross-site request.
      apply_cross_site_redirect_downgrade_warning =
          metadata.cross_site_redirect_downgrade ==
          (is_cookie_being_set ? ContextDowngradeType::kLaxToCross
                               : ContextDowngradeType::kStrictToCross);
      break;
    default:
      break;
  }
  if (apply_cross_site_redirect_downgrade_warning) {
    status->AddWarningReason(
        CookieInclusionStatus::
            WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION);
  }

  // If there are reasons to exclude the cookie other than SameSite, don't warn
  // about the cookie at all.
  status->MaybeClearSameSiteWarning();
}

}  // namespace

CookieAccessResult CookieBase::IncludeForRequestURL(
    const GURL& url,
    const CookieOptions& options,
    const CookieAccessParams& params) const {
  CookieInclusionStatus status;
  // Filter out HttpOnly cookies, per options.
  if (options.exclude_httponly() && IsHttpOnly()) {
    status.AddExclusionReason(CookieInclusionStatus::EXCLUDE_HTTP_ONLY);
  }
  // Secure cookies should not be included in requests for URLs with an
  // insecure scheme, unless it is a localhost url, or the CookieAccessDelegate
  // otherwise denotes them as trustworthy
  // (`delegate_treats_url_as_trustworthy`).
  bool is_allowed_to_access_secure_cookies = false;
  CookieAccessScheme cookie_access_scheme =
      cookie_util::ProvisionalAccessScheme(url);
  if (cookie_access_scheme == CookieAccessScheme::kNonCryptographic &&
      params.delegate_treats_url_as_trustworthy) {
    cookie_access_scheme = CookieAccessScheme::kTrustworthy;
  }
  switch (cookie_access_scheme) {
    case CookieAccessScheme::kNonCryptographic:
      if (SecureAttribute()) {
        status.AddExclusionReason(CookieInclusionStatus::EXCLUDE_SECURE_ONLY);
      }
      break;
    case CookieAccessScheme::kTrustworthy:
      is_allowed_to_access_secure_cookies = true;
      if (SecureAttribute() ||
          (cookie_util::IsSchemeBoundCookiesEnabled() &&
           source_scheme_ == CookieSourceScheme::kSecure)) {
        status.AddWarningReason(
            CookieInclusionStatus::
                WARN_SECURE_ACCESS_GRANTED_NON_CRYPTOGRAPHIC);
      }
      break;
    case CookieAccessScheme::kCryptographic:
      is_allowed_to_access_secure_cookies = true;
      break;
  }

  // For the following two sections we're checking to see if a cookie's
  // `source_scheme_` and `source_port_` match that of the url's. In most cases
  // this is a direct comparison but it does get a bit more complicated when
  // trustworthy origins are taken into accounts. Note that here, a kTrustworthy
  // url must have a non-secure scheme (http) because otherwise it'd be a
  // kCryptographic url.
  //
  // Trustworthy origins are allowed to both secure and non-secure cookies. This
  // means that we'll match source_scheme_ for both their usual kNonSecure as
  // well as KSecure. For source_port_ we'll match per usual as well as any 443
  // ports, since those are the default values for secure cookies and we still
  // want to be able to access them.

  // A cookie with a source scheme of kSecure shouldn't be accessible by
  // kNonCryptographic urls. But we can skip adding a status if the cookie is
  // already blocked due to the `Secure` attribute.
  if (source_scheme_ == CookieSourceScheme::kSecure &&
      cookie_access_scheme == CookieAccessScheme::kNonCryptographic &&
      !status.HasExclusionReason(CookieInclusionStatus::EXCLUDE_SECURE_ONLY)) {
    if (cookie_util::IsSchemeBoundCookiesEnabled()) {
      status.AddExclusionReason(CookieInclusionStatus::EXCLUDE_SCHEME_MISMATCH);
    } else {
      status.AddWarningReason(CookieInclusionStatus::WARN_SCHEME_MISMATCH);
    }
  }
  // A cookie with a source scheme of kNonSecure shouldn't be accessible by
  // kCryptographic urls.
  else if (source_scheme_ == CookieSourceScheme::kNonSecure &&
           cookie_access_scheme == CookieAccessScheme::kCryptographic) {
    if (cookie_util::IsSchemeBoundCookiesEnabled()) {
      status.AddExclusionReason(CookieInclusionStatus::EXCLUDE_SCHEME_MISMATCH);
    } else {
      status.AddWarningReason(CookieInclusionStatus::WARN_SCHEME_MISMATCH);
    }
  }
  // Else, the cookie has a source scheme of kUnset or the access scheme is
  // kTrustworthy. Neither of which will block the cookie.

  int url_port = url.EffectiveIntPort();
  CHECK(url_port != url::PORT_INVALID);
  // The cookie's source port either must match the url's port, be
  // PORT_UNSPECIFIED, or the cookie must be a domain cookie.
  bool port_matches = url_port == source_port_ ||
                      source_port_ == url::PORT_UNSPECIFIED || IsDomainCookie();

  // Or if the url is trustworthy, we'll also match 443 (in order to get secure
  // cookies).
  bool trustworthy_and_443 =
      cookie_access_scheme == CookieAccessScheme::kTrustworthy &&
      source_port_ == 443;
  if (!port_matches && !trustworthy_and_443) {
    if (cookie_util::IsPortBoundCookiesEnabled()) {
      status.AddExclusionReason(CookieInclusionStatus::EXCLUDE_PORT_MISMATCH);
    } else {
      status.AddWarningReason(CookieInclusionStatus::WARN_PORT_MISMATCH);
    }
  }

  // Don't include cookies for requests that don't apply to the cookie domain.
  if (!IsDomainMatch(url.host())) {
    status.AddExclusionReason(CookieInclusionStatus::EXCLUDE_DOMAIN_MISMATCH);
  }
  // Don't include cookies for requests with a url path that does not path
  // match the cookie-path.
  if (!IsOnPath(url.path())) {
    status.AddExclusionReason(CookieInclusionStatus::EXCLUDE_NOT_ON_PATH);
  }

  // For LEGACY cookies we should always return the schemeless context,
  // otherwise let GetContextForCookieInclusion() decide.
  const CookieOptions::SameSiteCookieContext::ContextType
      cookie_inclusion_context =
          params.access_semantics == CookieAccessSemantics::LEGACY
              ? options.same_site_cookie_context().context()
              : options.same_site_cookie_context()
                    .GetContextForCookieInclusion();

  // Don't include same-site cookies for cross-site requests.
  CookieEffectiveSameSite effective_same_site =
      GetEffectiveSameSite(params.access_semantics);
  DCHECK(effective_same_site != CookieEffectiveSameSite::UNDEFINED);

  switch (effective_same_site) {
    case CookieEffectiveSameSite::STRICT_MODE:
      if (cookie_inclusion_context <
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT) {
        status.AddExclusionReason(
            CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT);
      }
      break;
    case CookieEffectiveSameSite::LAX_MODE:
      if (cookie_inclusion_context <
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX) {
        status.AddExclusionReason(
            (SameSite() == CookieSameSite::UNSPECIFIED)
                ? CookieInclusionStatus::
                      EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX
                : CookieInclusionStatus::EXCLUDE_SAMESITE_LAX);
      }
      break;
    // TODO(crbug.com/40638805): Add a browsertest for this behavior.
    case CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE:
      DCHECK(SameSite() == CookieSameSite::UNSPECIFIED);
      if (cookie_inclusion_context <
          CookieOptions::SameSiteCookieContext::ContextType::
              SAME_SITE_LAX_METHOD_UNSAFE) {
        // TODO(chlily): Do we need a separate CookieInclusionStatus for this?
        status.AddExclusionReason(
            CookieInclusionStatus::EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX);
      }
      break;
    default:
      break;
  }

  // Unless legacy access semantics are in effect, SameSite=None cookies without
  // the Secure attribute should be ignored. This can apply to cookies which
  // were created before "SameSite=None requires Secure" was enabled (as
  // SameSite=None insecure cookies cannot be set while the options are on).
  if (params.access_semantics != CookieAccessSemantics::LEGACY &&
      SameSite() == CookieSameSite::NO_RESTRICTION && !SecureAttribute()) {
    status.AddExclusionReason(
        CookieInclusionStatus::EXCLUDE_SAMESITE_NONE_INSECURE);
  }

  ApplySameSiteCookieWarningToStatus(SameSite(), effective_same_site,
                                     SecureAttribute(),
                                     options.same_site_cookie_context(),
                                     &status, false /* is_cookie_being_set */);

  CookieAccessResult result{effective_same_site, status,
                            params.access_semantics,
                            is_allowed_to_access_secure_cookies};

  PostIncludeForRequestURL(result, options, cookie_inclusion_context);

  return result;
}

CookieAccessResult CookieBase::IsSetPermittedInContext(
    const GURL& source_url,
    const CookieOptions& options,
    const CookieAccessParams& params,
    const std::vector<std::string>& cookieable_schemes,
    const std::optional<CookieAccessResult>& cookie_access_result) const {
  CookieAccessResult access_result;
  if (cookie_access_result) {
    access_result = *cookie_access_result;
  }

  if (!base::Contains(cookieable_schemes, source_url.scheme())) {
    access_result.status.AddExclusionReason(
        CookieInclusionStatus::EXCLUDE_NONCOOKIEABLE_SCHEME);
  }

  if (!IsDomainMatch(source_url.host())) {
    access_result.status.AddExclusionReason(
        CookieInclusionStatus::EXCLUDE_DOMAIN_MISMATCH);
  }

  CookieAccessScheme access_scheme =
      cookie_util::ProvisionalAccessScheme(source_url);
  if (access_scheme == CookieAccessScheme::kNonCryptographic &&
      params.delegate_treats_url_as_trustworthy) {
    access_scheme = CookieAccessScheme::kTrustworthy;
  }

  switch (access_scheme) {
    case CookieAccessScheme::kNonCryptographic:
      access_result.is_allowed_to_access_secure_cookies = false;
      if (SecureAttribute()) {
        access_result.status.AddExclusionReason(
            CookieInclusionStatus::EXCLUDE_SECURE_ONLY);
      }
      break;

    case CookieAccessScheme::kCryptographic:
      // All cool!
      access_result.is_allowed_to_access_secure_cookies = true;
      break;

    case CookieAccessScheme::kTrustworthy:
      access_result.is_allowed_to_access_secure_cookies = true;
      if (SecureAttribute()) {
        // OK, but want people aware of this.
        // Note, we also want to apply this warning to cookies whose source
        // scheme is kSecure but are set by non-cryptographic (but trustworthy)
        // urls. Helpfully, since those cookies only get a kSecure source scheme
        // when they also specify "Secure" this if statement will already apply
        // to them.
        access_result.status.AddWarningReason(
            CookieInclusionStatus::
                WARN_SECURE_ACCESS_GRANTED_NON_CRYPTOGRAPHIC);
      }
      break;
  }

  access_result.access_semantics = params.access_semantics;
  if (options.exclude_httponly() && IsHttpOnly()) {
    DVLOG(net::cookie_util::kVlogSetCookies)
        << "HttpOnly cookie not permitted in script context.";
    access_result.status.AddExclusionReason(
        CookieInclusionStatus::EXCLUDE_HTTP_ONLY);
  }

  // Unless legacy access semantics are in effect, SameSite=None cookies without
  // the Secure attribute will be rejected.
  if (params.access_semantics != CookieAccessSemantics::LEGACY &&
      SameSite() == CookieSameSite::NO_RESTRICTION && !SecureAttribute()) {
    DVLOG(net::cookie_util::kVlogSetCookies)
        << "SetCookie() rejecting insecure cookie with SameSite=None.";
    access_result.status.AddExclusionReason(
        CookieInclusionStatus::EXCLUDE_SAMESITE_NONE_INSECURE);
  }

  // For LEGACY cookies we should always return the schemeless context,
  // otherwise let GetContextForCookieInclusion() decide.
  CookieOptions::SameSiteCookieContext::ContextType cookie_inclusion_context =
      params.access_semantics == CookieAccessSemantics::LEGACY
          ? options.same_site_cookie_context().context()
          : options.same_site_cookie_context().GetContextForCookieInclusion();

  access_result.effective_same_site =
      GetEffectiveSameSite(params.access_semantics);
  DCHECK(access_result.effective_same_site !=
         CookieEffectiveSameSite::UNDEFINED);
  switch (access_result.effective_same_site) {
    case CookieEffectiveSameSite::STRICT_MODE:
      // This intentionally checks for `< SAME_SITE_LAX`, as we allow
      // `SameSite=Strict` cookies to be set for top-level navigations that
      // qualify for receipt of `SameSite=Lax` cookies.
      if (cookie_inclusion_context <
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX) {
        DVLOG(net::cookie_util::kVlogSetCookies)
            << "Trying to set a `SameSite=Strict` cookie from a "
               "cross-site URL.";
        access_result.status.AddExclusionReason(
            CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT);
      }
      break;
    case CookieEffectiveSameSite::LAX_MODE:
    case CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE:
      if (cookie_inclusion_context <
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX) {
        if (SameSite() == CookieSameSite::UNSPECIFIED) {
          DVLOG(net::cookie_util::kVlogSetCookies)
              << "Cookies with no known SameSite attribute being treated as "
                 "lax; attempt to set from a cross-site URL denied.";
          access_result.status.AddExclusionReason(
              CookieInclusionStatus::
                  EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX);
        } else {
          DVLOG(net::cookie_util::kVlogSetCookies)
              << "Trying to set a `SameSite=Lax` cookie from a cross-site URL.";
          access_result.status.AddExclusionReason(
              CookieInclusionStatus::EXCLUDE_SAMESITE_LAX);
        }
      }
      break;
    default:
      break;
  }

  ApplySameSiteCookieWarningToStatus(
      SameSite(), access_result.effective_same_site, SecureAttribute(),
      options.same_site_cookie_context(), &access_result.status,
      true /* is_cookie_being_set */);

  PostIsSetPermittedInContext(access_result, options);

  return access_result;
}

bool CookieBase::IsOnPath(const std::string& url_path) const {
  return cookie_util::IsOnPath(path_, url_path);
}

bool CookieBase::IsDomainMatch(const std::string& host) const {
  return cookie_util::IsDomainMatch(domain_, host);
}

bool CookieBase::IsSecure() const {
  return SecureAttribute() || (cookie_util::IsSchemeBoundCookiesEnabled() &&
                               source_scheme_ == CookieSourceScheme::kSecure);
}

bool CookieBase::IsFirstPartyPartitioned() const {
  return IsPartitioned() && !CookiePartitionKey::HasNonce(partition_key_) &&
         SchemefulSite(GURL(
             base::StrCat({url::kHttpsScheme, url::kStandardSchemeSeparator,
                           DomainWithoutDot()}))) == partition_key_->site();
}

bool CookieBase::IsThirdPartyPartitioned() const {
  return IsPartitioned() && !IsFirstPartyPartitioned();
}

std::string CookieBase::DomainWithoutDot() const {
  return cookie_util::CookieDomainAsHost(domain_);
}

CookieBase::UniqueCookieKey CookieBase::UniqueKey() const {
  std::optional<CookieSourceScheme> source_scheme =
      cookie_util::IsSchemeBoundCookiesEnabled()
          ? std::make_optional(source_scheme_)
          : std::nullopt;
  std::optional<int> source_port = cookie_util::IsPortBoundCookiesEnabled()
                                       ? std::make_optional(source_port_)
                                       : std::nullopt;

  return std::make_tuple(partition_key_, name_, domain_, path_, source_scheme,
                         source_port);
}

CookieBase::UniqueDomainCookieKey CookieBase::UniqueDomainKey() const {
  std::optional<CookieSourceScheme> source_scheme =
      cookie_util::IsSchemeBoundCookiesEnabled()
          ? std::make_optional(source_scheme_)
          : std::nullopt;

  return std::make_tuple(partition_key_, name_, domain_, path_, source_scheme);
}

void CookieBase::SetSourcePort(int port) {
  source_port_ = ValidateAndAdjustSourcePort(port);
}

CookieBase::CookieBase() = default;

CookieBase::CookieBase(const CookieBase& other) = default;

CookieBase::CookieBase(CookieBase&& other) = default;

CookieBase& CookieBase::operator=(const CookieBase& other) = default;

CookieBase& CookieBase::operator=(CookieBase&& other) = default;

CookieBase::~CookieBase() = default;

CookieBase::CookieBase(std::string name,
                       std::string domain,
                       std::string path,
                       base::Time creation,
                       bool secure,
                       bool httponly,
                       CookieSameSite same_site,
                       std::optional<CookiePartitionKey> partition_key,
                       CookieSourceScheme source_scheme,
                       int source_port)
    : name_(std::move(name)),
      domain_(std::move(domain)),
      path_(std::move(path)),
      creation_date_(creation),
      secure_(secure),
      httponly_(httponly),
      same_site_(same_site),
      partition_key_(std::move(partition_key)),
      source_scheme_(source_scheme),
      source_port_(source_port) {}

CookieEffectiveSameSite CookieBase::GetEffectiveSameSite(
    CookieAccessSemantics access_semantics) const {
  base::TimeDelta lax_allow_unsafe_threshold_age =
      GetLaxAllowUnsafeThresholdAge();

  switch (SameSite()) {
    // If a cookie does not have a SameSite attribute, the effective SameSite
    // mode depends on the access semantics and whether the cookie is
    // recently-created.
    case CookieSameSite::UNSPECIFIED:
      return (access_semantics == CookieAccessSemantics::LEGACY)
                 ? CookieEffectiveSameSite::NO_RESTRICTION
                 : (IsRecentlyCreated(lax_allow_unsafe_threshold_age)
                        ? CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE
                        : CookieEffectiveSameSite::LAX_MODE);
    case CookieSameSite::NO_RESTRICTION:
      return CookieEffectiveSameSite::NO_RESTRICTION;
    case CookieSameSite::LAX_MODE:
      return CookieEffectiveSameSite::LAX_MODE;
    case CookieSameSite::STRICT_MODE:
      return CookieEffectiveSameSite::STRICT_MODE;
  }
}

base::TimeDelta CookieBase::GetLaxAllowUnsafeThresholdAge() const {
  return base::TimeDelta::Min();
}

bool CookieBase::IsRecentlyCreated(base::TimeDelta age_threshold) const {
  return (base::Time::Now() - creation_date_) <= age_threshold;
}

// static
int CookieBase::ValidateAndAdjustSourcePort(int port) {
  if ((port >= 0 && port <= 65535) || port == url::PORT_UNSPECIFIED) {
    // 0 would be really weird as it has a special meaning, but it's still
    // technically a valid tcp/ip port so we're going to accept it here.
    return port;
  }
  return url::PORT_INVALID;
}

}  // namespace net

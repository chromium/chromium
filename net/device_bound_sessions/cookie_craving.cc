// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/cookie_craving.h"

#include <optional>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/strings/strcat.h"
#include "net/base/url_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_params.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "net/device_bound_sessions/proto/storage.pb.h"
#include "net/device_bound_sessions/session_error.h"
#include "net/url_request/url_request.h"
#include "url/url_canon.h"

namespace net::device_bound_sessions {

namespace {

// A one-character value suffices to be non-empty. We avoid using an
// unnecessarily long placeholder so as to not eat into the 4096-char limit for
// a cookie name-value pair.
const char kPlaceholderValue[] = "v";

proto::CookieSameSite ProtoEnumFromCookieSameSite(CookieSameSite same_site) {
  switch (same_site) {
    case CookieSameSite::UNSPECIFIED:
      return proto::CookieSameSite::COOKIE_SAME_SITE_UNSPECIFIED;
    case CookieSameSite::NO_RESTRICTION:
      return proto::CookieSameSite::NO_RESTRICTION;
    case CookieSameSite::LAX_MODE:
      return proto::CookieSameSite::LAX_MODE;
    case CookieSameSite::STRICT_MODE:
      return proto::CookieSameSite::STRICT_MODE;
  }
}

CookieSameSite CookieSameSiteFromProtoEnum(proto::CookieSameSite proto) {
  switch (proto) {
    case proto::CookieSameSite::COOKIE_SAME_SITE_UNSPECIFIED:
      return CookieSameSite::UNSPECIFIED;
    case proto::CookieSameSite::NO_RESTRICTION:
      return CookieSameSite::NO_RESTRICTION;
    case proto::CookieSameSite::LAX_MODE:
      return CookieSameSite::LAX_MODE;
    case proto::CookieSameSite::STRICT_MODE:
      return CookieSameSite::STRICT_MODE;
  }
}

proto::CookieSourceScheme ProtoEnumFromCookieSourceScheme(
    CookieSourceScheme scheme) {
  switch (scheme) {
    case CookieSourceScheme::kUnset:
      return proto::CookieSourceScheme::UNSET;
    case CookieSourceScheme::kNonSecure:
      return proto::CookieSourceScheme::NON_SECURE;
    case CookieSourceScheme::kSecure:
      return proto::CookieSourceScheme::SECURE;
  }
}

CookieSourceScheme CookieSourceSchemeFromProtoEnum(
    proto::CookieSourceScheme proto) {
  switch (proto) {
    case proto::CookieSourceScheme::UNSET:
      return CookieSourceScheme::kUnset;
    case proto::CookieSourceScheme::NON_SECURE:
      return CookieSourceScheme::kNonSecure;
    case proto::CookieSourceScheme::SECURE:
      return CookieSourceScheme::kSecure;
  }
}

}  // namespace

// static
base::expected<CookieCraving, SessionError> CookieCraving::Create(
    const GURL& url,
    const std::string& name,
    const std::string& attributes,
    base::Time creation_time) {
  CHECK(url.is_valid());
  if (creation_time.is_null()) {
    return base::unexpected(
        SessionError{SessionError::kInvalidCredentialsCookieCreationTime});
  }

  // Check the name first individually, otherwise the next step which cobbles
  // together a cookie line may mask issues with the name.
  if (!ParsedCookie::IsValidCookieName(name)) {
    return base::unexpected(
        SessionError{SessionError::kInvalidCredentialsCookieName});
  }

  // Construct an imitation "Set-Cookie" line to feed into ParsedCookie.
  // Make up a value which is an arbitrary a non-empty string, because the
  // "value" of the ParsedCookie will be discarded anyway, and it is valid for
  // a cookie's name to be empty, but not for both name and value to be empty.
  std::string line_to_parse =
      base::StrCat({name, "=", kPlaceholderValue, ";", attributes});

  ParsedCookie parsed_cookie(line_to_parse);
  if (!parsed_cookie.IsValid()) {
    return base::unexpected(
        SessionError{SessionError::kInvalidCredentialsCookieParsing});
  }

  static constexpr auto kPermittedAttributes =
      base::MakeFixedFlatSet<std::string>(
          {"domain", "path", "secure", "httponly", "samesite"});
  if (!parsed_cookie.ForEachAttribute(
          [](std::string_view attribute, std::string_view value) {
            return base::Contains(kPermittedAttributes, attribute);
          })) {
    return base::unexpected(SessionError{
        SessionError::kInvalidCredentialsCookieUnpermittedAttribute});
  }

  // `domain` is the domain key for storing the CookieCraving, determined
  // from the domain attribute value (if any) and the URL. A domain cookie is
  // marked by a preceding dot, as per CookieBase::Domain(), whereas a host
  // cookie has no leading dot.
  std::string domain_attribute_value;
  if (parsed_cookie.Domain()) {
    domain_attribute_value = parsed_cookie.Domain().value();
  }
  CookieInclusionStatus ignored_status;
  std::optional<std::string> domain = cookie_util::GetCookieDomainWithString(
      url, domain_attribute_value, ignored_status);
  // Note: This is a deviation from CanonicalCookie. Here, we also require that
  // domain is non-empty, which CanonicalCookie does not. See comment below in
  // IsValid().
  if (!domain || domain->empty()) {
    return base::unexpected(
        SessionError{SessionError::kInvalidCredentialsCookieInvalidDomain});
  }

  std::string path =
      cookie_util::CanonPathWithString(url, parsed_cookie.Path().value_or(""));

  CookiePrefix prefix = cookie_util::GetCookiePrefix(name);
  if (!cookie_util::IsCookiePrefixValid(prefix, url, parsed_cookie)) {
    return base::unexpected(
        SessionError{SessionError::kInvalidCredentialsCookiePrefix});
  }

  // Note: This is a deviation from CanonicalCookie::Create(), which allows
  // cookies with a Secure attribute to be created as if they came from a
  // cryptographic URL, even if the URL is not cryptographic, on the basis that
  // the URL might be trustworthy. CookieCraving makes the simplifying
  // assumption to ignore this case.
  CookieSourceScheme source_scheme = url.SchemeIsCryptographic()
                                         ? CookieSourceScheme::kSecure
                                         : CookieSourceScheme::kNonSecure;
  int source_port = url.EffectiveIntPort();

  CookieCraving cookie_craving{parsed_cookie.Name(),
                               std::move(domain).value(),
                               std::move(path),
                               creation_time,
                               parsed_cookie.IsSecure(),
                               parsed_cookie.IsHttpOnly(),
                               parsed_cookie.SameSite().first,
                               source_scheme,
                               source_port};

  CHECK(cookie_craving.IsValid());

  CookieInclusionStatus status;
  if (!cookie_craving.CreateCanonicalCookieForRequest(url, &status)) {
    SCOPED_CRASH_KEY_STRING256("CookieCraving", "Create",
                               status.GetDebugString());
    base::debug::DumpWithoutCrashing();
    // If we're not able to create a canonical cookie here, then we likely won't
    // be able to in `CookieCraving::ShouldIncludeForRequest` later, so there's
    // no point in creating the craving.
    // TODO(crbug.com/435221694): See related TODO below for plan for
    // longer-term fix.
    return base::unexpected(
        SessionError{SessionError::kInvalidCredentialsCookie});
  }

  return cookie_craving;
}

// TODO(crbug.com/438792839): Much of this function is copied directly from
// CanonicalCookie. Try to deduplicate it.
bool CookieCraving::IsValid() const {
  if (ParsedCookie::ParseTokenString(Name()) != Name() ||
      !ParsedCookie::IsValidCookieName(Name())) {
    return false;
  }

  if (CreationDate().is_null()) {
    return false;
  }

  url::CanonHostInfo ignored_info;
  std::string canonical_domain = CanonicalizeHost(Domain(), &ignored_info);
  // Note: This is a deviation from CanonicalCookie. CookieCraving does not
  // allow Domain() to be empty, whereas CanonicalCookie does (perhaps
  // erroneously).
  if (Domain().empty() || Domain() != canonical_domain) {
    return false;
  }

  if (Path().empty() || Path().front() != '/') {
    return false;
  }

  CookiePrefix prefix = cookie_util::GetCookiePrefix(Name());
  switch (prefix) {
    case COOKIE_PREFIX_HOST:
      if (!SecureAttribute() || Path() != "/" || !IsHostCookie()) {
        return false;
      }
      break;
    case COOKIE_PREFIX_SECURE:
      if (!SecureAttribute()) {
        return false;
      }
      break;
    case COOKIE_PREFIX_HTTP:
      if (!SecureAttribute() || !IsHttpOnly()) {
        return false;
      }
      break;
    case COOKIE_PREFIX_HOSTHTTP:
      if (!SecureAttribute() || Path() != "/" || !IsHostCookie() ||
          !IsHttpOnly()) {
        return false;
      }
      break;
    case COOKIE_PREFIX_NONE:
    case COOKIE_PREFIX_LAST:
      break;
  }

  if (IsPartitioned()) {
    if (CookiePartitionKey::HasNonce(PartitionKey())) {
      return true;
    }
    if (!SecureAttribute()) {
      return false;
    }
  }

  return true;
}

bool CookieCraving::IsSatisfiedBy(
    const CanonicalCookie& canonical_cookie) const {
  CHECK(IsValid());
  CHECK(canonical_cookie.IsCanonicalForFromStorage());

  // Note: Creation time is not required to match. DBSC configs may be set at
  // different times from the cookies they reference. DBSC also does not require
  // expiry time to match, for similar reasons. Source scheme and port are also
  // not required to match. DBSC does not require the config and its required
  // cookie to come from the same URL (and the source host does not matter as
  // long as the Domain attribute value matches), so it doesn't make sense to
  // compare the source scheme and port either.
  // TODO(crbug.com/353772143): Decide more carefully how nonced partition keys
  // should be compared.
  auto make_required_members_tuple = [](const CookieBase& c) {
    return std::make_tuple(c.Name(), c.Domain(), c.Path(), c.SecureAttribute(),
                           c.IsHttpOnly(), c.SameSite(), c.PartitionKey());
  };

  return make_required_members_tuple(*this) ==
         make_required_members_tuple(canonical_cookie);
}

std::string CookieCraving::DebugString() const {
  auto bool_to_string = [](bool b) { return b ? "true" : "false"; };
  return base::StrCat({"Name: ", Name(), "; Domain: ", Domain(),
                       "; Path: ", Path(),
                       "; SecureAttribute: ", bool_to_string(SecureAttribute()),
                       "; IsHttpOnly: ", bool_to_string(IsHttpOnly()),
                       "; SameSite: ", CookieSameSiteToString(SameSite()),
                       "; IsPartitioned: ", bool_to_string(IsPartitioned())});
  // Source scheme and port, and creation date omitted for brevity.
}

// static
CookieCraving CookieCraving::CreateUnsafeForTesting(
    std::string name,
    std::string domain,
    std::string path,
    base::Time creation,
    bool secure,
    bool httponly,
    CookieSameSite same_site,
    CookieSourceScheme source_scheme,
    int source_port) {
  return CookieCraving{
      std::move(name), std::move(domain), std::move(path), creation,   secure,
      httponly,        same_site,         source_scheme,   source_port};
}

CookieCraving::CookieCraving() = default;

CookieCraving::CookieCraving(std::string name,
                             std::string domain,
                             std::string path,
                             base::Time creation,
                             bool secure,
                             bool httponly,
                             CookieSameSite same_site,
                             CookieSourceScheme source_scheme,
                             int source_port)
    : CookieBase(std::move(name),
                 std::move(domain),
                 std::move(path),
                 creation,
                 secure,
                 httponly,
                 same_site,
                 /*partition_key=*/std::nullopt,
                 source_scheme,
                 source_port) {}

CookieCraving::CookieCraving(const CookieCraving& other) = default;

CookieCraving::CookieCraving(CookieCraving&& other) = default;

CookieCraving& CookieCraving::operator=(const CookieCraving& other) = default;

CookieCraving& CookieCraving::operator=(CookieCraving&& other) = default;

CookieCraving::~CookieCraving() = default;

bool CookieCraving::IsEqualForTesting(const CookieCraving& other) const {
  return Name() == other.Name() && Domain() == other.Domain() &&
         Path() == other.Path() &&
         SecureAttribute() == other.SecureAttribute() &&
         IsHttpOnly() == other.IsHttpOnly() && SameSite() == other.SameSite() &&
         SourceScheme() == other.SourceScheme() &&
         SourcePort() == other.SourcePort() &&
         CreationDate() == other.CreationDate() &&
         PartitionKey() == other.PartitionKey();
}

std::ostream& operator<<(std::ostream& os, const CookieCraving& cc) {
  os << cc.DebugString();
  return os;
}

proto::CookieCraving CookieCraving::ToProto() const {
  CHECK(IsValid());

  proto::CookieCraving proto;
  proto.set_name(Name());
  proto.set_domain(Domain());
  proto.set_path(Path());
  proto.set_secure(SecureAttribute());
  proto.set_httponly(IsHttpOnly());
  proto.set_source_port(SourcePort());
  proto.set_creation_time(
      CreationDate().ToDeltaSinceWindowsEpoch().InMicroseconds());
  proto.set_same_site(ProtoEnumFromCookieSameSite(SameSite()));
  proto.set_source_scheme(ProtoEnumFromCookieSourceScheme(SourceScheme()));
  return proto;
}

// static
std::optional<CookieCraving> CookieCraving::CreateFromProto(
    const proto::CookieCraving& proto) {
  if (!proto.has_name() || !proto.has_domain() || !proto.has_path() ||
      !proto.has_secure() || !proto.has_httponly() ||
      !proto.has_source_port() || !proto.has_creation_time() ||
      !proto.has_same_site() || !proto.has_source_scheme()) {
    return std::nullopt;
  }

  CookieCraving cookie_craving{
      proto.name(),
      proto.domain(),
      proto.path(),
      base::Time::FromDeltaSinceWindowsEpoch(
          base::Microseconds(proto.creation_time())),
      proto.secure(),
      proto.httponly(),
      CookieSameSiteFromProtoEnum(proto.same_site()),
      CookieSourceSchemeFromProtoEnum(proto.source_scheme()),
      proto.source_port()};

  if (!cookie_craving.IsValid()) {
    return std::nullopt;
  }

  return cookie_craving;
}

bool CookieCraving::ShouldIncludeForRequest(
    DbscRequest& request,
    const FirstPartySetMetadata& first_party_set_metadata,
    const CookieOptions& options,
    const CookieAccessParams& params) const {
  if (!IncludeForRequestURL(request.url(), options, params)
           .status.IsInclude()) {
    return false;
  }

  CookieInclusionStatus status;
  std::unique_ptr<CanonicalCookie> canonical_cookie =
      CreateCanonicalCookieForRequest(request.url(), &status);

  if (!canonical_cookie) {
    SCOPED_CRASH_KEY_STRING256("CookieCraving", "ShouldInclude",
                               status.GetDebugString());
    base::debug::DumpWithoutCrashing();
    // If we're not able to create a canonical cookie here, return false instead
    // of crashing below.
    // TODO(crbug.com/435221694): See related TODO below for plan for
    // longer-term fix.
    return false;
  }

  CookieAccessResultList included_cravings;
  included_cravings.emplace_back(std::move(*canonical_cookie));
  CookieAccessResultList excluded_cravings;
  // The use of `unnormalized_request()` here is potentially unsafe since
  // accessing the URL could drop the normalization of WebSocket schemes. But
  // cookie inclusion logic has to handle this already when deciding
  // whether to include cookies on the WebSocket handshake. That makes
  // it safe in this very limited context to expose the `URLRequest`.
  return request.network_delegate()->AnnotateAndMoveUserBlockedCookies(
      *request.unnormalized_request(), first_party_set_metadata,
      included_cravings, excluded_cravings);
}

bool CookieCraving::CanSetBoundCookie(
    DbscRequest& request,
    const FirstPartySetMetadata& first_party_set_metadata,
    CookieOptions* options) const {
  // TODO(crbug.com/438783631): Refactor this.
  // The below is all copied from
  // UrlRequestHttpJob::SaveCookiesAndNotifyHeadersComplete. We should refactor
  // it.
  CookieInclusionStatus status;
  std::unique_ptr<CanonicalCookie> canonical_cookie =
      CreateCanonicalCookieForRequest(request.url(), &status);
  if (!canonical_cookie || !status.IsInclude()) {
    return false;
  }

  if (!request.network_delegate()) {
    return false;
  }

  // The use of `unnormalized_request()` here is potentially unsafe since
  // accessing the URL could drop the normalization of WebSocket schemes. But
  // cookie inclusion logic has to handle this already when deciding
  // whether to include cookies on the WebSocket handshake. That makes
  // it safe in this very limited context to expose the `URLRequest`.
  if (!request.network_delegate()->CanSetCookie(
          *request.unnormalized_request(), *canonical_cookie, options,
          first_party_set_metadata, &status)) {
    return false;
  }

  return IsSetPermittedInContext(
             request.url(), *options,
             CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                                CookieScopeSemantics::UNKNOWN,
                                /* delegate_treats_url_as_trustworthy=*/false),
             {"https", "http"}, std::nullopt)
      .status.IsInclude();
}

std::unique_ptr<CanonicalCookie> CookieCraving::CreateCanonicalCookieForRequest(
    const GURL& url,
    CookieInclusionStatus* status) const {
  // The `NetworkDelegate` can also reject cookies for any reason
  // (e.g. user preferences). So we need to synthesize a
  // `CanonicalCookie` and make sure it would be included to check those
  // conditions too.
  base::Time now = base::Time::Now();
  std::string domain = Domain();
  // This fix is needed because non-IP address __Host- prefix cookies are
  // considered invalid if they pass through a domain, but Domain() is defined
  // even for __Host- prefix cookies. This fix is very limited in scope for now
  // (only __Host- prefix cookies).
  // TODO(crbug.com/435221694): re-implement the way we call into
  // `AnnotateAndMoveUserBlockedCookies` so that it is not possible for a
  // validation to fail in this method. Some ideas:
  //  1) Is it needed for `CookieCraving` creation validation and
  //     `CanonicalCookie` creation validation to be different in the first
  //     place?
  //  2) Can we refactor `AnnotateAndMoveUserBlockedCookies` to input a
  //     `CookieBase` instead?
  if (!url.HostIsIPAddress() &&
      cookie_util::GetCookiePrefix(Name()) == COOKIE_PREFIX_HOST) {
    domain = "";
  }
  return CanonicalCookie::CreateSanitizedCookie(
      url, Name(), /*value=*/"", domain, Path(), CreationDate(),
      now + base::Days(1), now, IsSecure(), IsHttpOnly(), SameSite(),
      COOKIE_PRIORITY_DEFAULT, PartitionKey(), status);
}

}  // namespace net::device_bound_sessions

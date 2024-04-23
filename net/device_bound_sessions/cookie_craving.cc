// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/cookie_craving.h"

#include "base/strings/strcat.h"
#include "net/base/url_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "url/url_canon.h"

namespace net {

namespace {

// A one-character value suffices to be non-empty. We avoid using an
// unnecessarily long placeholder so as to not eat into the 4096-char limit for
// a cookie name-value pair.
const char kPlaceholderValue[] = "v";

}  // namespace

// static
std::optional<CookieCraving> CookieCraving::Create(
    const GURL& url,
    const std::string& name,
    const std::string& attributes,
    base::Time creation_time,
    std::optional<CookiePartitionKey> cookie_partition_key) {
  if (!url.is_valid() || creation_time.is_null()) {
    return std::nullopt;
  }

  // Check the name first individually, otherwise the next step which cobbles
  // together a cookie line may mask issues with the name.
  if (!ParsedCookie::IsValidCookieName(name)) {
    return std::nullopt;
  }

  // Construct an imitation "Set-Cookie" line to feed into ParsedCookie.
  // Make up a value which is an arbitrary a non-empty string, because the
  // "value" of the ParsedCookie will be discarded anyway, and it is valid for
  // a cookie's name to be empty, but not for both name and value to be empty.
  std::string line_to_parse =
      base::StrCat({name, "=", kPlaceholderValue, ";", attributes});

  ParsedCookie parsed_cookie(line_to_parse);
  if (!parsed_cookie.IsValid()) {
    return std::nullopt;
  }

  // `domain` is the domain key for storing the CookieCraving, determined
  // from the domain attribute value (if any) and the URL. A domain cookie is
  // marked by a preceding dot, as per CookieBase::Domain(), whereas a host
  // cookie has no leading dot.
  std::string domain_attribute_value;
  if (parsed_cookie.HasDomain()) {
    domain_attribute_value = parsed_cookie.Domain();
  }
  std::string domain;
  CookieInclusionStatus ignored_status;
  // Note: This is a deviation from CanonicalCookie. Here, we also require that
  // domain is non-empty, which CanonicalCookie does not. See comment below in
  // IsValid().
  if (!cookie_util::GetCookieDomainWithString(url, domain_attribute_value,
                                              ignored_status, &domain) ||
      domain.empty()) {
    return std::nullopt;
  }

  std::string path = cookie_util::CanonPathWithString(
      url, parsed_cookie.HasPath() ? parsed_cookie.Path() : "");

  // Note: This is a deviation from CanonicalCookie. In CookieCraving, we do not
  // honor non-default values for the kCaseInsensitiveCookiePrefix feature.
  CookiePrefix prefix =
      cookie_util::GetCookiePrefix(name, /*check_insensitively=*/true);
  if (!cookie_util::IsCookiePrefixValid(prefix, url, parsed_cookie)) {
    return std::nullopt;
  }

  // TODO(chlily): Determine whether nonced partition keys should be supported
  // for CookieCravings.
  bool partition_has_nonce = CookiePartitionKey::HasNonce(cookie_partition_key);
  if (!cookie_util::IsCookiePartitionedValid(url, parsed_cookie,
                                             partition_has_nonce)) {
    return std::nullopt;
  }
  if (!parsed_cookie.IsPartitioned() && !partition_has_nonce) {
    cookie_partition_key = std::nullopt;
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
                               std::move(domain),
                               std::move(path),
                               creation_time,
                               parsed_cookie.IsSecure(),
                               parsed_cookie.IsHttpOnly(),
                               parsed_cookie.SameSite(),
                               std::move(cookie_partition_key),
                               source_scheme,
                               source_port};

  CHECK(cookie_craving.IsValid());
  return cookie_craving;
}

// TODO(chlily): Much of this function is copied directly from CanonicalCookie.
// Try to deduplicate it.
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

  // Note: This is a deviation from CanonicalCookie. Here, we always check case
  // insensitively. See comment above in Create().
  CookiePrefix prefix =
      cookie_util::GetCookiePrefix(Name(), /*check_insensitively=*/true);
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
    default:
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
  CHECK(canonical_cookie.IsCanonical());

  // Note: Creation time is not required to match. DBSC configs may be set at
  // different times from the cookies they reference. DBSC also does not require
  // expiry time to match, for similar reasons. Source scheme and port are also
  // not required to match. DBSC does not require the config and its required
  // cookie to come from the same URL (and the source host does not matter as
  // long as the Domain attribute value matches), so it doesn't make sense to
  // compare the source scheme and port either.
  // TODO(chlily): Decide more carefully how nonced partition keys should be
  // compared.
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
    std::optional<CookiePartitionKey> partition_key,
    CookieSourceScheme source_scheme,
    int source_port) {
  return CookieCraving{std::move(name), std::move(domain),
                       std::move(path), creation,
                       secure,          httponly,
                       same_site,       std::move(partition_key),
                       source_scheme,   source_port};
}

CookieCraving::CookieCraving() = default;

CookieCraving::CookieCraving(std::string name,
                             std::string domain,
                             std::string path,
                             base::Time creation,
                             bool secure,
                             bool httponly,
                             CookieSameSite same_site,
                             std::optional<CookiePartitionKey> partition_key,
                             CookieSourceScheme source_scheme,
                             int source_port)
    : CookieBase(std::move(name),
                 std::move(domain),
                 std::move(path),
                 creation,
                 secure,
                 httponly,
                 same_site,
                 std::move(partition_key),
                 source_scheme,
                 source_port) {}

CookieCraving::CookieCraving(const CookieCraving& other) = default;

CookieCraving::CookieCraving(CookieCraving&& other) = default;

CookieCraving& CookieCraving::operator=(const CookieCraving& other) = default;

CookieCraving& CookieCraving::operator=(CookieCraving&& other) = default;

CookieCraving::~CookieCraving() = default;

std::ostream& operator<<(std::ostream& os, const CookieCraving& cc) {
  os << cc.DebugString();
  return os;
}

}  // namespace net

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_COOKIE_CRAVING_H_
#define NET_DEVICE_BOUND_SESSIONS_COOKIE_CRAVING_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cookies/cookie_base.h"
#include "net/cookies/cookie_constants.h"
#include "net/device_bound_sessions/dbsc_request.h"
#include "net/device_bound_sessions/session_error.h"

namespace net {
class CanonicalCookie;
class FirstPartySetMetadata;
}  // namespace net

namespace net::device_bound_sessions {

namespace proto {
class CookieCraving;
}

// This class represents the need for a certain cookie to be present. It is not
// a cookie itself, but rather represents a requirement which can be satisfied
// by a real cookie (i.e. a CanonicalCookie). Each CookieCraving is specified by
// and associated with a DBSC (Device Bound Session Credentials) session.
//
// In general, CookieCraving behavior is intended to be as close as possible to
// CanonicalCookie, especially the inclusion logic, since they need to be
// matched up. However, some notable differences include:
//
// CookieCraving does not have a value field, i.e. they only have a name (and
// other attributes). The name can be the empty string. The name of a cookie is
// needed to identify it, but the value of a cookie is not relevant to its
// inclusion or exclusion, so CookieCraving omits it.
//
// CookieCraving does not have an expiry date. The expiry date of a
// CanonicalCookie often depends upon the creation time (if it is set via
// Max-Age), and a DBSC session config is not necessarily created at the same
// time as its corresponding Set-Cookie header, so we cannot guarantee that
// they'd match. DBSC also does not require a specific expiry date for the
// cookies whose presence it guarantees.
//
// CookieCraving does not implement lax-allow-unsafe behavior (it does not
// set a non-zero age threshold for it). The default CanonicalCookie
// lax-allow-unsafe behavior is problematic because it can result in two
// identical set-cookie lines (set from the same URL) exhibiting different
// inclusion results, if they happen to be on opposite sides of the
// lax-allow-unsafe age threshold. By not implementing lax-allow-unsafe,
// CookieCraving may sometimes be excluded even when a corresponding
// CanonicalCookie would be included for being under its lax-allow-unsafe age
// threshold. This means that servers deploying DBSC with SameSite-unspecified
// cookies SHOULD NOT rely on the presence of SameSite-unspecified cookies
// within 2 minutes of their creation time on cross-site POST and other unsafe
// request types, as DBSC cannot make any such guarantee.
class NET_EXPORT CookieCraving : public CookieBase {
 public:
  // Creates a new CookieCraving in the context of `url`, given a `name` and
  // associated cookie `attributes`. (Note that CookieCravings do not have a
  // "value".) `url` must be valid. `creation_time` may not be null. May return
  // a SessionError if the CookieCraving is invalid, such as if an attribute
  // value is invalid. If a CookieCraving is returned, it will satisfy
  // IsValid(). If there is leading or trailing whitespace in `name`, it will
  // get trimmed.
  //
  // Partitioned cookies are not supported. Attempts to create a
  // partitioned CookieCraving will fail.
  //
  // SameSite and HttpOnly related parameters are not checked here,
  // so creation of CookieCravings with e.g. SameSite=Strict from a cross-site
  // context is allowed. Create() also does not check whether `url` has a secure
  // scheme if attempting to create a Secure cookie. The Secure, SameSite, and
  // HttpOnly related parameters should be checked when deciding CookieCraving
  // inclusion for a given request/context.
  //
  // In general this is intended to closely mirror CanonicalCookie::Create.
  // However, there are some simplifying assumptions made*, and metrics are not
  // (currently) logged so as to not interfere with CanonicalCookie metrics.
  // There is also no (current) need for a CookieInclusionStatus to be returned.
  //
  // * Simplifying assumptions (differing from CanonicalCookie):
  //  - The Domain() member of a CookieCraving is required to be non-empty,
  //    which CanonicalCookie does not require.
  //  - Cookie name prefixes (__Host- and __Secure-) are always checked
  //    case-insensitively, unlike CanonicalCookie which reads a Feature value
  //    to decide whether to check insensitively.
  //  - CanonicalCookie allows non-cryptographic URLs to create a cookie with a
  //    secure source_scheme, if that cookie was Secure, on the basis that that
  //    URL might be trustworthy when checked later. CookieCraving does not
  //    allow this.
  static base::expected<CookieCraving, SessionError> Create(
      const GURL& url,
      const std::string& name,
      const std::string& attributes,
      base::Time creation_time);

  CookieCraving(const CookieCraving& other);
  CookieCraving(CookieCraving&& other);
  CookieCraving& operator=(const CookieCraving& other);
  CookieCraving& operator=(CookieCraving&& other);
  ~CookieCraving() override;

  // Returns whether all CookieCraving fields are consistent, in canonical form,
  // etc. (Mostly analogous to CanonicalCookie::IsCanonical, except without
  // checks for access time.) Essentially, if this returns true, then this
  // CookieCraving instance could have been created by Create().
  // Other public methods of this class may not be called if IsValid() is false.
  bool IsValid() const;

  // Returns whether the given "real" cookie satisfies this CookieCraving, in
  // the sense that DBSC will consider the required cookie present.
  // The provided CanonicalCookie must be canonical.
  bool IsSatisfiedBy(const CanonicalCookie& canonical_cookie) const;

  std::string DebugString() const;

  bool IsEqualForTesting(const CookieCraving& other) const;

  // May return an invalid instance.
  static CookieCraving CreateUnsafeForTesting(std::string name,
                                              std::string domain,
                                              std::string path,
                                              base::Time creation,
                                              bool secure,
                                              bool httponly,
                                              CookieSameSite same_site,
                                              CookieSourceScheme source_scheme,
                                              int source_port);

  // Returns a protobuf object. May only be called for
  // a valid CookieCraving object.
  proto::CookieCraving ToProto() const;

  // Creates a CookieCraving object from a protobuf
  // object. If the protobuf contents are invalid,
  // a std::nullopt is returned.
  static std::optional<CookieCraving> CreateFromProto(
      const proto::CookieCraving& proto);

  // Whether the craving applies to the given `request`, with other
  // arguments providing context for the access.
  bool ShouldIncludeForRequest(
      DbscRequest& request,
      const FirstPartySetMetadata& first_party_set_metadata,
      const CookieOptions& options,
      const CookieAccessParams& params) const;

  // Whether the craving could be modified by `request`, with other
  // arguments providing context for the access.
  bool CanSetBoundCookie(DbscRequest& request,
                         const FirstPartySetMetadata& first_party_set_metadata,
                         CookieOptions* options) const;

 private:
  // Creates a CanonicalCookie for this craving in the context of a request to
  // `url`. Fills in `status` with any exclusion reasons, which answer why this
  // function may return null.
  std::unique_ptr<CanonicalCookie> CreateCanonicalCookieForRequest(
      const GURL& url,
      CookieInclusionStatus* status) const;

  CookieCraving();

  // Prefer Create() over this constructor. This may return non-valid instances.
  CookieCraving(std::string name,
                std::string domain,
                std::string path,
                base::Time creation,
                bool secure,
                bool httponly,
                CookieSameSite same_site,
                CookieSourceScheme source_scheme,
                int source_port);

  using CookieBase::IncludeForRequestURL;
};

// Outputs a debug string, e.g. for more helpful test failure messages.
NET_EXPORT std::ostream& operator<<(std::ostream& os, const CookieCraving& cc);

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_COOKIE_CRAVING_H_

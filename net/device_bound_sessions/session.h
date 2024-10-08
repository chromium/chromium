// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_H_

#include <memory>
#include <optional>
#include <string>

#include "base/types/strong_alias.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "net/base/net_export.h"
#include "net/device_bound_sessions/cookie_craving.h"
#include "net/device_bound_sessions/session_inclusion_rules.h"
#include "net/device_bound_sessions/session_params.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
}

namespace net::device_bound_sessions {

namespace proto {
class Session;
}

// This class represents a DBSC (Device Bound Session Credentials) session.
class NET_EXPORT Session {
 public:
  using Id = base::StrongAlias<class IdTag, std::string>;

  ~Session();

  static std::unique_ptr<Session> CreateIfValid(const SessionParams& params,
                                                GURL url);
  static std::unique_ptr<Session> CreateFromProto(const proto::Session& proto);
  proto::Session ToProto() const;

  // Used to set the unexportable session binding key associated with this
  // session. This method can be called when a session is first bound with
  // a brand new key. It can also be called when restoring a session after
  // browser restart.
  void set_unexportable_key_id(
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
          key_id_or_error) {
    key_id_or_error_ = std::move(key_id_or_error);
  }

  const unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>&
  unexportable_key_id() const {
    return key_id_or_error_;
  }

  // this bool could also be an enum for UMA, eventually devtools, etc.
  bool ShouldDeferRequest(URLRequest* request) const;

  const Id& id() const { return id_; }

  const GURL& refresh_url() const { return refresh_url_; }

  const std::optional<std::string>& cached_challenge() const {
    return cached_challenge_;
  }

  const base::Time& expiry_date() const { return expiry_date_; }

  bool should_defer_when_expired() const { return should_defer_when_expired_; }

  bool IsEqualForTesting(const Session& other) const;

  void set_cached_challenge(std::string challenge) {
    cached_challenge_ = std::move(challenge);
  }

  void set_expiry_date(base::Time expiry_date) { expiry_date_ = expiry_date; }

 private:
  Session(Id id, url::Origin origin, GURL refresh);
  Session(Id id,
          GURL refresh,
          SessionInclusionRules inclusion_rules,
          std::vector<CookieCraving> cookie_cravings,
          bool should_defer_when_expired,
          base::Time expiry_date);
  Session(const Session& other) = delete;
  Session& operator=(const Session& other) = delete;
  Session(Session&& other) = delete;
  Session& operator=(Session&& other) = delete;

  // The unique server-issued identifier of the session.
  const Id id_;
  // The URL to use for refresh requests made on behalf of this session.
  // Note: This probably also needs to store its IsolationInfo, so that the
  // correct IsolationInfo can be used when sending refresh requests.
  // If requests are not deferred when missing a craving, this should still
  // be set as this URL must be able to set all cravings.
  const GURL refresh_url_;
  // Determines which requests are potentially subject to deferral on behalf of
  // this session.
  SessionInclusionRules inclusion_rules_;
  // The set of credentials required by this session. Derived from the
  // "credentials" array in the session config.
  std::vector<CookieCraving> cookie_cravings_;
  // If this session should defer requests when cookies are not present.
  // Default is true, and strongly recommended.
  // If this is false, requests will still be sent when cookies are not present,
  // and will be signed using the cached challenge if present, if not signed
  // using a default value for challenge.
  bool should_defer_when_expired_ = true;
  // Expiry date for session, 400 days from last refresh similar to cookies.
  base::Time expiry_date_;
  // Unexportable key for this session.
  // NOTE: The key may not be available for sometime after a browser restart.
  // This is because the key needs to be restored from a corresponding
  // "wrapped" value that is persisted to disk. This restoration takes time
  // and can be done lazily. The "wrapped" key and the restore process are
  // transparent to this class. Once restored, the key can be set using
  // `set_unexportable_key_id`
  unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
      key_id_or_error_ =
          base::unexpected(unexportable_keys::ServiceError::kKeyNotReady);
  // Precached challenge, if any. Should not be persisted.
  std::optional<std::string> cached_challenge_;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_H_

#include <optional>
#include <string>

#include "base/types/strong_alias.h"
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

// This class represents a DBSC (Device Bound Session Credentials) session.
class NET_EXPORT Session {
 public:
  using Id = base::StrongAlias<class IdTag, std::string>;

  static std::unique_ptr<Session> CreateIfValid(const SessionParams& params,
                                                GURL url);

  // this bool could also be an enum for UMA, eventually devtools, etc.
  bool ShouldDeferRequest(URLRequest* request) const;

  const Id& id() const { return id_; }

  const GURL& refresh_url() const { return refresh_url_; }

  ~Session();

 private:
  Session(Id id, url::Origin origin, GURL refresh);
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
  // Unexportable key for this session, this will never change for a given
  // session.
  unexportable_keys::UnexportableKeyId key_id_;
  // Precached challenge, if any. Should not be persisted.
  std::optional<std::string> cached_challenge_;
  // If this session should defer requests when cookies are not present.
  // Default is true, and strongly recommended.
  // If this is false, requests will still be sent when cookies are not present,
  // and will be signed using the cached challenge if present, if not signed
  // using a default value for challenge.
  bool should_defer_when_expired = true;
  // Expiry date for session, 400 days from last refresh similar to cookies.
  base::Time expiry_date_;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_H_

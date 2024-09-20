// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_CHALLENGE_PARAM_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_CHALLENGE_PARAM_H_

#include <optional>
#include <string>
#include <vector>

#include "net/base/net_export.h"
#include "net/http/structured_headers.h"

// Forward declarations.
class GURL;
namespace net {
class HttpResponseHeaders;
}

namespace net::device_bound_sessions {

// Class to parse Sec-Session-Challenge header.
// See explainer for details:
// https://github.com/WICG/dbsc/blob/main/README.md.
// It is a RFC 8941 list of challenges for the associated DBSC sessions.
// Example:
// Sec-Session-Challenge: "challenge";id="session_id".
// Sec-Session-Challenge: "challenge";id="session_id", "challenge1";id="id1".
// The session id may be unknown during the session registration, hence it can
// be omitted:
// Sec-Session-Challenge: "challenge".
// It is possible to have multiple Sec-Session-Challenge headers in
// one response. If multiple challenges are given for one specific session,
// the last one will take effect.
class NET_EXPORT SessionChallengeParam {
 public:
  SessionChallengeParam(SessionChallengeParam&& other) noexcept;
  SessionChallengeParam& operator=(SessionChallengeParam&& other) noexcept;

  ~SessionChallengeParam();

  // Returns a vector of valid instances from the headers.
  static std::vector<SessionChallengeParam> CreateIfValid(
      const GURL& request_url,
      const HttpResponseHeaders* headers);

  const std::optional<std::string>& session_id() const { return session_id_; }
  const std::string& challenge() const { return challenge_; }

 private:
  SessionChallengeParam(std::optional<std::string> session_id,
                        std::string challenge);

  static std::optional<SessionChallengeParam> ParseItem(
      const structured_headers::ParameterizedMember& session_challenge);

  std::optional<std::string> session_id_;
  std::string challenge_;
};
}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_CHALLENGE_PARAM_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_ACCESS_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_ACCESS_H_

#include "net/device_bound_sessions/session_key.h"

namespace net::device_bound_sessions {

// LINT.IfChange
struct NET_EXPORT SessionAccess {
  enum class AccessType {
    kCreation = 0,
    kUpdate = 1,
    kTermination = 2,
  };

  SessionAccess();
  SessionAccess(AccessType access, SessionKey key);
  SessionAccess(AccessType access,
                SessionKey key,
                const std::vector<std::string>& cookies);

  ~SessionAccess();

  SessionAccess(const SessionAccess&);
  SessionAccess& operator=(const SessionAccess&);

  SessionAccess(SessionAccess&&) noexcept;
  SessionAccess& operator=(SessionAccess&&) noexcept;

  // Type of access
  AccessType access_type;

  // Key of accessed session
  SessionKey session_key;

  // Cookies bound by this session. Only populated when `access_type` is
  // `kTermination`.
  std::vector<std::string> cookies;

  bool operator==(const SessionAccess& other) const;
};
// LINT.ThenChange(//services/network/public/mojom/device_bound_sessions.mojom)

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_ACCESS_H_

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

  // Type of access
  AccessType access_type;

  // Key of accessed session
  SessionKey session_key;

  bool operator==(const SessionAccess& other) const;
};
// LINT.ThenChange(//services/network/public/mojom/device_bound_sessions.mojom)

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_ACCESS_H_

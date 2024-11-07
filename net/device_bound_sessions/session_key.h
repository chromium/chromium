// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_KEY_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_KEY_H_

#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/session.h"

namespace net::device_bound_sessions {

// Unique identifier for a `Session`.
struct NET_EXPORT SessionKey {
  SessionKey();
  SessionKey(SchemefulSite site, Session::Id id);
  ~SessionKey();

  SessionKey(const SessionKey&);
  SessionKey& operator=(const SessionKey&);

  SessionKey(SessionKey&&);
  SessionKey& operator=(SessionKey&&);

  SchemefulSite site;
  Session::Id id;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_KEY_H_

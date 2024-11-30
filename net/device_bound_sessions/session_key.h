// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_KEY_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_KEY_H_

#include "base/types/strong_alias.h"
#include "net/base/schemeful_site.h"

namespace net::device_bound_sessions {

// Unique identifier for a `Session`.
struct NET_EXPORT SessionKey {
  using Id = base::StrongAlias<class IdTag, std::string>;

  SessionKey();
  SessionKey(SchemefulSite site, Id id);
  ~SessionKey();

  SessionKey(const SessionKey&);
  SessionKey& operator=(const SessionKey&);

  SessionKey(SessionKey&&);
  SessionKey& operator=(SessionKey&&);

  SchemefulSite site;
  Id id;

  bool operator==(const SessionKey& other) const;
  bool operator<(const SessionKey& other) const;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_KEY_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_STORE_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_STORE_H_

#include <memory>
#include <string>

#include "net/device_bound_sessions/session.h"

namespace net {
class SchemefulSite;
}

namespace net::device_bound_sessions {

// Interface SessionStore abstracts out the interaction with a
// persistent store for device bound session state.
class NET_EXPORT SessionStore {
 public:
  SessionStore() = default;
  virtual ~SessionStore() = default;

  SessionStore(const SessionStore&) = delete;
  SessionStore& operator=(const SessionStore&) = delete;

  using SessionsMap = std::multimap<SchemefulSite, std::unique_ptr<Session>>;
  using LoadSessionsCallback = base::OnceCallback<void(SessionsMap)>;
  virtual void LoadSessions(LoadSessionsCallback callback) = 0;

  virtual void SaveSession(const SchemefulSite& site,
                           const Session& session) = 0;

  virtual void DeleteSession(const SchemefulSite& site,
                             const Session::Id& session_id) = 0;

  // Returns session objects created from currently cached store data.
  virtual SessionsMap GetAllSessions() const = 0;

  // Asynchronously retrieves the unwrapped session binding key
  // from its persistent form saved in the store.
  using RestoreSessionBindingKeyCallback = base::OnceCallback<void(
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>)>;
  virtual void RestoreSessionBindingKey(
      const SchemefulSite& site,
      const Session::Id& session_id,
      RestoreSessionBindingKeyCallback callback) = 0;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_STORE_H_

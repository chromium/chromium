// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_MOCK_SESSION_STORE_H_
#define NET_DEVICE_BOUND_SESSIONS_MOCK_SESSION_STORE_H_

#include "base/functional/callback.h"
#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/session.h"
#include "net/device_bound_sessions/session_store.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net::device_bound_sessions {

class SessionStoreMock : public SessionStore {
 public:
  SessionStoreMock();
  ~SessionStoreMock() override;

  MOCK_METHOD(void, LoadSessions, (LoadSessionsCallback callback), (override));
  MOCK_METHOD(void,
              SaveSession,
              (const SchemefulSite& site, const Session& session),
              (override));
  MOCK_METHOD(void, DeleteSession, (const SessionKey& key), (override));
  MOCK_METHOD(SessionStore::SessionsMap, GetAllSessions, (), (const, override));
  MOCK_METHOD(void,
              RestoreSessionBindingKey,
              (const SessionKey& session_key,
               RestoreSessionBindingKeyCallback callback),
              (override));
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_MOCK_SESSION_STORE_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_TEST_UTIL_H_
#define NET_DEVICE_BOUND_SESSIONS_TEST_UTIL_H_

#include <string>
#include <utility>

#include "base/containers/span.h"
#include "net/device_bound_sessions/registration_fetcher_param.h"
#include "net/device_bound_sessions/session_challenge_param.h"
#include "net/device_bound_sessions/session_service.h"
#include "net/device_bound_sessions/session_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

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
  MOCK_METHOD(void,
              DeleteSession,
              (const SchemefulSite& site, const Session::Id& session_id),
              (override));
  MOCK_METHOD(SessionStore::SessionsMap, GetAllSessions, (), (const, override));
  MOCK_METHOD(void,
              RestoreSessionBindingKey,
              (const SchemefulSite& site,
               const Session::Id& session_id,
               RestoreSessionBindingKeyCallback callback),
              (override));
};

class SessionServiceMock : public SessionService {
 public:
  SessionServiceMock();
  ~SessionServiceMock() override;

  MOCK_METHOD(void,
              RegisterBoundSession,
              (OnAccessCallback on_access_callback,
               RegistrationFetcherParam registration_params,
               const IsolationInfo& isolation_info),
              (override));
  MOCK_METHOD(std::optional<Session::Id>,
              GetAnySessionRequiringDeferral,
              (URLRequest * request),
              (override));
  MOCK_METHOD(void,
              DeferRequestForRefresh,
              (URLRequest * request,
               Session::Id session_id,
               RefreshCompleteCallback restart_callback,
               RefreshCompleteCallback continue_callback),
              (override));
  MOCK_METHOD(void,
              SetChallengeForBoundSession,
              (OnAccessCallback on_access_callback,
               const GURL& request_url,
               const SessionChallengeParam& challenge_param),
              (override));
};

// Return a hard-coded RS256 public key's SPKI bytes and JWK string for testing.
std::pair<base::span<const uint8_t>, std::string>
GetRS256SpkiAndJwkForTesting();

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_TEST_UTIL_H_

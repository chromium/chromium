// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_MOCK_SESSION_SERVICE_H_
#define NET_DEVICE_BOUND_SESSIONS_MOCK_SESSION_SERVICE_H_

#include <string>
#include <utility>

#include "base/containers/span.h"
#include "net/device_bound_sessions/registration_fetcher_param.h"
#include "net/device_bound_sessions/session_challenge_param.h"
#include "net/device_bound_sessions/session_service.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace net::device_bound_sessions {

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
  MOCK_METHOD(
      void,
      GetAllSessionsAsync,
      (base::OnceCallback<void(const std::vector<SessionKey>&)> callback),
      (override));
  MOCK_METHOD(void,
              DeleteSession,
              (const SchemefulSite& site, const Session::Id& id),
              (override));
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_MOCK_SESSION_SERVICE_H_

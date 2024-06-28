// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_TEST_UTIL_H_
#define NET_DEVICE_BOUND_SESSIONS_TEST_UTIL_H_

#include "net/device_bound_sessions/registration_fetcher_param.h"
#include "net/device_bound_sessions/session_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net::device_bound_sessions {

class SessionServiceMock : public SessionService {
 public:
  SessionServiceMock();
  ~SessionServiceMock() override;

  MOCK_METHOD(void,
              RegisterBoundSession,
              (RegistrationFetcherParam registration_params,
               const IsolationInfo& isolation_info),
              (override));
  MOCK_METHOD(std::optional<std::string>,
              GetAnySessionRequiringDeferral,
              (URLRequest * request),
              (override));
  MOCK_METHOD(void,
              DeferRequestForRefresh,
              (std::string session_id,
               RefreshCompleteCallback restart_callback,
               RefreshCompleteCallback continue_callback),
              (override));
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_TEST_UTIL_H_

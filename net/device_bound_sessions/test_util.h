// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_TEST_UTIL_H_
#define NET_DEVICE_BOUND_SESSIONS_TEST_UTIL_H_

#include "net/device_bound_sessions/device_bound_session_registration_fetcher_param.h"
#include "net/device_bound_sessions/device_bound_session_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {

class DeviceBoundSessionServiceMock : public DeviceBoundSessionService {
 public:
  DeviceBoundSessionServiceMock();
  ~DeviceBoundSessionServiceMock() override;

  MOCK_METHOD(
      void,
      RegisterBoundSession,
      (const DeviceBoundSessionRegistrationFetcherParam& registration_params),
      (override));
};

}  // namespace net

#endif  // NET_DEVICE_BOUND_SESSIONS_TEST_UTIL_H_

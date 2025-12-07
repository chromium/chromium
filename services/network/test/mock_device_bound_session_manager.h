// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_MOCK_DEVICE_BOUND_SESSION_MANAGER_H_
#define SERVICES_NETWORK_TEST_MOCK_DEVICE_BOUND_SESSION_MANAGER_H_

#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/device_bound_sessions.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace network {

class MockDeviceBoundSessionManager : public mojom::DeviceBoundSessionManager {
 public:
  MockDeviceBoundSessionManager();
  ~MockDeviceBoundSessionManager() override;

  MOCK_METHOD(void,
              GetAllSessions,
              (GetAllSessionsCallback callback),
              (override));
  MOCK_METHOD(void,
              DeleteSession,
              (net::device_bound_sessions::DeletionReason reason,
               const net::device_bound_sessions::SessionKey& session_key),
              (override));
  MOCK_METHOD(void,
              DeleteAllSessions,
              (net::device_bound_sessions::DeletionReason reason,
               std::optional<base::Time> created_after_time,
               std::optional<base::Time> created_before_time,
               network::mojom::ClearDataFilterPtr filter,
               base::OnceClosure completion_callback),
              (override));
  MOCK_METHOD(
      void,
      AddObserver,
      (const GURL& url,
       mojo::PendingRemote<mojom::DeviceBoundSessionAccessObserver> observer),
      (override));
  MOCK_METHOD(void,
              CreateBoundSessions,
              (std::vector<net::device_bound_sessions::SessionParams> params,
               const std::vector<uint8_t>& wrapped_key,
               const std::vector<net::CanonicalCookie>& cookies_to_set,
               const net::CookieOptions& cookie_options,
               CreateBoundSessionsCallback callback),
              (override));
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_MOCK_DEVICE_BOUND_SESSION_MANAGER_H_

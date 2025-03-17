// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_MOCK_SESSION_AUTHZ_SERVICE_CLIENT_H_
#define REMOTING_BASE_MOCK_SESSION_AUTHZ_SERVICE_CLIENT_H_

#include "base/functional/callback.h"
#include "remoting/base/session_authz_service_client.h"
#include "remoting/proto/session_authz_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

class MockSessionAuthzServiceClient : public SessionAuthzServiceClient {
 public:
  MockSessionAuthzServiceClient();
  ~MockSessionAuthzServiceClient() override;

  MOCK_METHOD(void, GenerateHostToken, (GenerateHostTokenCallback callback));
  MOCK_METHOD(void,
              VerifySessionToken,
              (std::string_view session_token,
               VerifySessionTokenCallback callback));
  MOCK_METHOD(void,
              ReauthorizeHost,
              (std::string_view session_reauth_token,
               std::string_view session_id,
               ReauthorizeHostCallback callback));
};

}  // namespace remoting

#endif  // REMOTING_BASE_MOCK_SESSION_AUTHZ_SERVICE_CLIENT_H_

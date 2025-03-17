// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_SESSION_AUTHZ_SERVICE_CLIENT_H_
#define REMOTING_BASE_SESSION_AUTHZ_SERVICE_CLIENT_H_

#include <memory>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "remoting/base/http_status.h"
#include "remoting/proto/session_authz_service.h"

namespace remoting {

// Interface for communicating with the SessionAuthz service. For internal
// details, see go/crd-sessionauthz.
class SessionAuthzServiceClient {
 public:
  using GenerateHostTokenCallback = base::OnceCallback<void(
      const HttpStatus&,
      std::unique_ptr<internal::GenerateHostTokenResponseStruct>)>;
  using VerifySessionTokenCallback = base::OnceCallback<void(
      const HttpStatus&,
      std::unique_ptr<internal::VerifySessionTokenResponseStruct>)>;
  using ReauthorizeHostCallback = base::OnceCallback<void(
      const HttpStatus&,
      std::unique_ptr<internal::ReauthorizeHostResponseStruct>)>;

  virtual ~SessionAuthzServiceClient() = default;

  virtual void GenerateHostToken(GenerateHostTokenCallback callback) = 0;
  virtual void VerifySessionToken(std::string_view session_token,
                                  VerifySessionTokenCallback callback) = 0;
  virtual void ReauthorizeHost(std::string_view session_reauth_token,
                               std::string_view session_id,
                               ReauthorizeHostCallback callback) = 0;

 protected:
  SessionAuthzServiceClient() = default;
};

}  // namespace remoting

#endif  // REMOTING_BASE_SESSION_AUTHZ_SERVICE_CLIENT_H_

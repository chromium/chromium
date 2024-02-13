// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_SESSION_AUTHZ_SERVICE_CLIENT_H_
#define REMOTING_BASE_SESSION_AUTHZ_SERVICE_CLIENT_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/proto/session_authz_service.h"

namespace remoting {

// Interface for communicating with the SessionAuthz service. For internal
// details, see go/crd-sessionauthz.
class SessionAuthzServiceClient {
 public:
  using GenerateHostTokenCallback = base::OnceCallback<void(
      const ProtobufHttpStatus&,
      std::unique_ptr<internal::GenerateHostTokenResponseStruct>)>;
  using VerifySessionTokenCallback = base::OnceCallback<void(
      const ProtobufHttpStatus&,
      std::unique_ptr<internal::VerifySessionTokenResponseStruct>)>;
  using ReauthorizeHostCallback = base::OnceCallback<void(
      const ProtobufHttpStatus&,
      std::unique_ptr<internal::ReauthorizeHostResponseStruct>)>;

  virtual ~SessionAuthzServiceClient() = default;

  virtual void GenerateHostToken(GenerateHostTokenCallback callback) = 0;
  virtual void VerifySessionToken(
      const internal::VerifySessionTokenRequestStruct& request,
      VerifySessionTokenCallback callback) = 0;
  virtual void ReauthorizeHost(
      const internal::ReauthorizeHostRequestStruct& request,
      ReauthorizeHostCallback callback) = 0;

 protected:
  SessionAuthzServiceClient() = default;
};

}  // namespace remoting

#endif  // REMOTING_BASE_SESSION_AUTHZ_SERVICE_CLIENT_H_

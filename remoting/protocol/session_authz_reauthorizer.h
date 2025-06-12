// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_SESSION_AUTHZ_REAUTHORIZER_H_
#define REMOTING_PROTOCOL_SESSION_AUTHZ_REAUTHORIZER_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/base/http_status.h"
#include "remoting/base/session_authz_service_client.h"
#include "remoting/protocol/authenticator.h"

namespace remoting {
namespace internal {
struct ReauthorizeHostResponseStruct;
}  // namespace internal

namespace protocol {

// SessionReauthorizer implementation that reauthorizes using the SessionAuthz
// service.
class SessionAuthzReauthorizer {
 public:
  using OnReauthorizationFailedCallback =
      base::OnceCallback<void(HttpStatus::Code,
                              const Authenticator::RejectionDetails&)>;

  // |service_client| must outlive |this|.
  SessionAuthzReauthorizer(
      SessionAuthzServiceClient* service_client,
      std::string_view session_id,
      std::string_view session_reauth_token,
      base::TimeDelta session_reauth_token_lifetime,
      OnReauthorizationFailedCallback on_reauthorization_failed);
  ~SessionAuthzReauthorizer();

  SessionAuthzReauthorizer(const SessionAuthzReauthorizer&) = delete;
  SessionAuthzReauthorizer& operator=(const SessionAuthzReauthorizer&) = delete;

  void Start();

  const std::string& session_reauth_token() const {
    return session_reauth_token_;
  }

 private:
  void ScheduleNextReauth();
  void Reauthorize();
  void OnReauthorizeResult(
      const HttpStatus& status,
      std::unique_ptr<internal::ReauthorizeHostResponseStruct> response);
  void NotifyReauthorizationFailed(
      HttpStatus::Code error_code,
      const Authenticator::RejectionDetails& details);

  raw_ptr<SessionAuthzServiceClient> service_client_;
  std::string session_id_;
  std::string session_reauth_token_;
  base::TimeTicks token_expire_time_;
  base::OneShotTimer reauthorize_timer_;
  OnReauthorizationFailedCallback on_reauthorization_failed_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_SESSION_AUTHZ_REAUTHORIZER_H_

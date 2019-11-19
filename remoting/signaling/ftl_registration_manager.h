// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_REGISTRATION_MANAGER_H_
#define REMOTING_SIGNALING_FTL_REGISTRATION_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"
#include "remoting/signaling/registration_manager.h"

namespace grpc {
class Status;
}  // namespace grpc

namespace remoting {

namespace ftl {

class SignInGaiaRequest;
class SignInGaiaResponse;

}  // namespace ftl

class FtlDeviceIdProvider;
class OAuthTokenGetter;

// Class for registering the user with FTL service.
// TODO(yuweih): Add unittest
class FtlRegistrationManager final : public RegistrationManager {
 public:
  // |token_getter| must outlive |this|.
  FtlRegistrationManager(
      OAuthTokenGetter* token_getter,
      std::unique_ptr<FtlDeviceIdProvider> device_id_provider);
  ~FtlRegistrationManager() override;

  // RegistrationManager implementations.
  void SignInGaia(DoneCallback on_done) override;
  void SignOut() override;
  bool IsSignedIn() const override;
  std::string GetRegistrationId() const override;
  std::string GetFtlAuthToken() const override;

 private:
  using SignInGaiaResponseCallback =
      base::OnceCallback<void(const grpc::Status&,
                              const ftl::SignInGaiaResponse&)>;

  friend class FtlRegistrationManagerTest;

  class RegistrationClient {
   public:
    virtual ~RegistrationClient() = default;
    virtual void SignInGaia(const ftl::SignInGaiaRequest& request,
                            SignInGaiaResponseCallback on_done) = 0;
    virtual void CancelPendingRequests() = 0;
  };
  class RegistrationClientImpl;

  void DoSignInGaia(DoneCallback on_done);
  void OnSignInGaiaResponse(DoneCallback on_done,
                            const grpc::Status& status,
                            const ftl::SignInGaiaResponse& response);

  std::unique_ptr<RegistrationClient> registration_client_;
  std::unique_ptr<FtlDeviceIdProvider> device_id_provider_;
  base::OneShotTimer sign_in_backoff_timer_;
  base::OneShotTimer sign_in_refresh_timer_;
  std::string registration_id_;
  std::string ftl_auth_token_;
  net::BackoffEntry sign_in_backoff_;

  DISALLOW_COPY_AND_ASSIGN(FtlRegistrationManager);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_REGISTRATION_MANAGER_H_
